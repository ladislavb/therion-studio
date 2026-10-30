#include "MainWindow.h"

#include "ProjectGitHubBackup.h"
#include "ProjectGitHubSettings.h"
#include "ProjectVersioningNavigationService.h"
#include "ProjectVersioningService.h"
#include "TherionRunnerService.h"

#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QStandardPaths>
#include <QStatusBar>
#include <QtConcurrent>

namespace
{
class GitHubProgress final : public QProgressDialog
{
public:
    using QProgressDialog::QProgressDialog;
    void reject() override {}
protected:
    void closeEvent(QCloseEvent *event) override { event->ignore(); }
};

TherionStudio::ProjectGitHubBackup backupFor(const QString &root)
{
    const QString key = QString::fromLatin1(QCryptographicHash::hash(
        QFileInfo(root).canonicalFilePath().toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString state = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("github-backups/") + key);
    return TherionStudio::ProjectGitHubBackup(root, state);
}

template<typename Task>
TherionStudio::ProjectGitHubBackup::Result runBackupTask(QWidget *parent, const QString &text, Task task)
{
    GitHubProgress progress(text, QString(), 0, 0, parent);
    progress.setObjectName(QStringLiteral("githubBackupProgress"));
    progress.setWindowTitle(MainWindow::tr("GitHub backup"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    QFutureWatcher<TherionStudio::ProjectGitHubBackup::Result> watcher;
    QObject::connect(&watcher, &QFutureWatcher<TherionStudio::ProjectGitHubBackup::Result>::finished,
                     &progress, &QDialog::accept);
    watcher.setFuture(QtConcurrent::run(std::move(task)));
    if (!watcher.isFinished()) {
        progress.exec();
    }
    return watcher.result();
}
}

void MainWindow::uploadProjectToGitHub()
{
    performProjectGitHubBackup(true);
}

bool MainWindow::performProjectGitHubBackup(bool saveEditors)
{
    if (githubBackupInProgress_ || projectRestoreInProgress_) {
        return false;
    }
    const auto settings = TherionStudio::ProjectGitHubSettings::read(projectRootPath_);
    if (!settings.uploadEnabled()) {
        QMessageBox information(QMessageBox::Information, tr("GitHub backup"),
            tr("To upload the whole project:\n"
               "1. Open a project.\n"
               "2. Sign in through your browser or check your GitHub account.\n"
               "3. Select or create a dedicated empty private repository and verify it.\n"
               "4. Enable unencrypted GitHub backups.\n\n"
               "GitHub settings will open next."),
            QMessageBox::Ok, this);
        information.setObjectName(QStringLiteral("githubBackupSetupRequired"));
        information.setTextFormat(Qt::PlainText);
        if (settings.succeeded && !projectRootPath_.isEmpty()) {
            information.setInformativeText(tr("Signing in alone does not enable project uploads. "
                "Save the destination repository and enable GitHub backups for this project once."));
        }
        if (!settings.succeeded) {
            information.setDetailedText(settings.error);
        }
        information.exec();
        showSettingsDialogAtPage(true, true);
        return false;
    }
    if ((therionRunnerService_ && therionRunnerService_->isRunning())
        || (projectVersioningNavigation_ && projectVersioningNavigation_->isBusy())) {
        QMessageBox::warning(this, tr("GitHub backup"), tr("Wait for compilation or project restoration to finish before uploading."));
        return false;
    }
    if (saveEditors && !saveAllOpenDocuments()) {
        return false;
    }
    QString message = QStringLiteral("Project backup %1")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!saveEditors) {
        bool accepted = false;
        message = QInputDialog::getMultiLineText(this, tr("Upload project to GitHub"),
            tr("Private repository: %1\n"
               "Saved included files will be uploaded WITHOUT ENCRYPTION. Exports and .gitignore exclusions apply.\n"
               "Commit message:").arg(settings.repository()),
            tr("Project backup"), &accepted).trimmed();
        if (!accepted) {
            return false;
        }
    }
    if (message.isEmpty() || message.size() > 4000) {
        QMessageBox::warning(this, tr("GitHub backup"), tr("Enter a commit message between 1 and 4000 characters."));
        return false;
    }
    QScopedValueRollback<bool> busy(githubBackupInProgress_, true);
    if (projectVersioningService_) {
        projectVersioningService_->configure({});
    }
    const auto backup = backupFor(projectRootPath_);
    statusBar()->showMessage(tr("GitHub: checking account, repository and project changes..."));
    const auto result = runBackupTask(this, tr("Verifying private repository and uploading project backup..."),
        [backup, repository = settings.repository(), message]() mutable { return backup.upload(repository, message); });
    if (projectVersioningService_) {
        projectVersioningService_->configure(projectRootPath_);
    }
    if (!result.succeeded) {
        statusBar()->showMessage(tr("GitHub error: %1").arg(result.error), 30000);
        QMessageBox::warning(this, tr("GitHub backup failed"), result.error);
        showSettingsDialogAtPage(true, true);
        return false;
    }
    statusBar()->showMessage(result.changed
        ? tr("GitHub OK - project uploaded (%1)").arg(result.commitId.left(12))
        : tr("GitHub OK - project is up to date"), 30000);
    return true;
}

bool MainWindow::confirmGitHubBackupBeforeClose()
{
    if (projectRootPath_.isEmpty()) {
        return true;
    }
    const auto settings = TherionStudio::ProjectGitHubSettings::read(projectRootPath_);
    if (!settings.succeeded) {
        return QMessageBox::warning(this, tr("GitHub backup"),
            tr("GitHub settings could not be checked:\n%1\n\nExit without a GitHub backup?").arg(settings.error),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes;
    }
    if (!settings.promptOnClose()) {
        return true;
    }
    const auto result = [&] {
        QScopedValueRollback<bool> busy(githubBackupInProgress_, true);
        const auto backup = backupFor(projectRootPath_);
        return runBackupTask(this, tr("Checking for project changes not uploaded to GitHub..."),
            [backup, repository = settings.repository()]() mutable { return backup.check(repository); });
    }();
    if (!result.succeeded) {
        return QMessageBox::warning(this, tr("GitHub backup"),
            tr("Could not check unuploaded changes:\n%1\n\nExit without a GitHub backup?").arg(result.error),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes;
    }
    if (!result.changed) {
        return true;
    }
    QMessageBox choice(QMessageBox::Question, tr("Update GitHub before exiting?"),
        tr("Saved project changes have not been uploaded to %1. Upload a backup before exiting?")
            .arg(settings.repository()), QMessageBox::Cancel, this);
    choice.setObjectName(QStringLiteral("githubBackupBeforeClose"));
    choice.setTextFormat(Qt::PlainText);
    auto *upload = choice.addButton(tr("Upload and exit"), QMessageBox::AcceptRole);
    auto *skip = choice.addButton(tr("Exit without uploading"), QMessageBox::DestructiveRole);
    skip->setObjectName(QStringLiteral("exitWithoutGitHubBackup"));
    choice.setDefaultButton(QMessageBox::Cancel);
    choice.exec();
    if (choice.clickedButton() == choice.button(QMessageBox::Cancel) || !choice.clickedButton()) {
        return false;
    }
    return choice.clickedButton() != upload || performProjectGitHubBackup(false);
}
