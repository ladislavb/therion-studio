#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "ProjectVersioningNavigationService.h"
#include "ProjectVersioningService.h"
#include "ProjectVersioningToolbar.h"
#include "TherionRunnerService.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "text_editor/DocumentFileInspector.h"

#include <QDir>
#include <QCloseEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTabWidget>

namespace
{
class ProjectRestoreProgress final : public QProgressDialog
{
public:
    using QProgressDialog::QProgressDialog;
    void reject() override {}

protected:
    void closeEvent(QCloseEvent *event) override { event->ignore(); }
};
}

void MainWindow::initializeProjectVersioningNavigation()
{
    projectVersioningNavigation_ = new TherionStudio::ProjectVersioningNavigationService(this);
}

void MainWindow::refreshProjectVersioningInspectors()
{
    if (projectVersioningNavigation_ == nullptr) {
        return;
    }
    QList<TherionStudio::DocumentFileInspector *> inspectors;
    if (auto *document = currentDocumentWidget()) {
        inspectors = document->findChildren<TherionStudio::DocumentFileInspector *>();
    }
    for (auto *tab : detachedMapEditorTabs()) {
        for (auto *inspector : tab->findChildren<TherionStudio::DocumentFileInspector *>()) {
            if (!inspectors.contains(inspector)) {
                inspectors.append(inspector);
            }
        }
    }
    bool added = false;
    for (auto *inspector : inspectors) {
        if (inspector->findChild<TherionStudio::ProjectVersioningToolbar *>() != nullptr) {
            continue;
        }
        auto *backups = new TherionStudio::ProjectVersioningToolbar(*projectVersioningNavigation_, inspector);
        inspector->setBackupWidget(backups);
        connect(backups, &TherionStudio::ProjectVersioningToolbar::settingsRequested,
                this, &MainWindow::showBackupSettingsDialog);
        connect(backups, &TherionStudio::ProjectVersioningToolbar::restoreRequested,
                this, &MainWindow::restoreProjectCheckpoint);
        connect(backups, &TherionStudio::ProjectVersioningToolbar::githubBackupRequested,
                this, &MainWindow::uploadProjectToGitHub);
        added = true;
    }
    if (added) {
        projectVersioningNavigation_->refresh();
    }
}

void MainWindow::restoreProjectCheckpoint(const QString &checkpointId)
{
    if (projectRestoreInProgress_ || projectVersioningNavigation_ == nullptr) {
        return;
    }
    if (therionRunnerService_ != nullptr && therionRunnerService_->isRunning()) {
        QMessageBox::warning(this, tr("Restore project"), tr("Stop compilation before restoring a project backup."));
        return;
    }
    if (projectVersioningNavigation_->isBusy() || !projectVersioningNavigation_->isEnabled()) {
        QMessageBox::warning(this, tr("Restore project"), tr("Local history is disabled or busy. Wait and try again."));
        return;
    }
    const auto *backups = qobject_cast<TherionStudio::ProjectVersioningToolbar *>(sender());
    const QString description = backups != nullptr ? backups->checkpointDescription(checkpointId) : checkpointId.left(12);
    if (QMessageBox::question(this, tr("Restore project"),
            tr("Restore this project backup?\n%1\n\n"
               "Current editor changes will be saved first, then a safety checkpoint will be created. "
               "Included project files will be replaced; files absent from the backup will be removed. "
               "Ignored exports and the project's Git repository are left untouched.")
                .arg(description),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    if (!saveAllOpenDocuments()) {
        return;
    }
    projectRestoreInProgress_ = true;
    if (projectVersioningService_ != nullptr) {
        projectVersioningService_->configure({});
    }
    auto *progress = new ProjectRestoreProgress(tr("Saving a safety checkpoint and restoring project files..."),
                                        QString(), 0, 0, this);
    progress->setObjectName(QStringLiteral("projectRestoreProgress"));
    progress->setWindowTitle(tr("Restore project"));
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setCancelButton(nullptr);
    progress->setMinimumDuration(0);
    progress->show();
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(projectVersioningNavigation_,
        &TherionStudio::ProjectVersioningNavigationService::restoreFinished, this,
        [this, progress, description, connection](const TherionStudio::ProjectVersioningStore::RestoreResult &result) {
            disconnect(*connection);
            // Failed restores can have rolled back files; reload those views as well.
            const QStringList reloadErrors = reloadRestoredProjectDocuments();
            projectRestoreInProgress_ = false;
            progress->hide();
            progress->deleteLater();
            invalidateProjectScanCache();
            handleProjectFileSystemMutation(projectRootPath_);
            refreshProjectVersioning();
            if (!result.succeeded) {
                QMessageBox message(QMessageBox::Warning, tr("Restore project"),
                    tr("Project restore failed:\n%1\n\nSafety checkpoint: %2")
                        .arg(result.error, result.safetyCheckpointId.isEmpty() ? tr("Not created") : result.safetyCheckpointId),
                    QMessageBox::Ok, this);
                message.setTextFormat(Qt::PlainText);
                message.setDetailedText(reloadErrors.join(QLatin1Char('\n')));
                message.exec();
                projectVersioningNavigation_->refresh();
                return;
            }
            projectVersioningNavigation_->refresh(false);
            QStringList details;
            for (const QString &file : result.addedFiles) {
                details.append(tr("Added: %1").arg(file));
            }
            for (const QString &file : result.changedFiles) {
                details.append(tr("Changed: %1").arg(file));
            }
            for (const QString &file : result.removedFiles) {
                details.append(tr("Removed: %1").arg(file));
            }
            details.append(reloadErrors);
            const QString summary = tr("Restored backup:\n%1\n\n"
                "Compared with the project before restore:\n"
                "Added: %2 file(s)\nChanged: %3 file(s)\nRemoved: %4 file(s)\n\n"
                "Previous project saved in safety checkpoint: %5\n"
                "Use Newer or Choose backup to return to it.")
                    .arg(description).arg(result.addedFiles.size()).arg(result.changedFiles.size())
                    .arg(result.removedFiles.size()).arg(result.safetyCheckpointId.left(12));
            QMessageBox message(reloadErrors.isEmpty() ? QMessageBox::Information : QMessageBox::Warning,
                tr("Project restored"), summary + (reloadErrors.isEmpty() ? QString()
                    : tr("\n\nSome open views could not be reloaded. See details.")),
                QMessageBox::Ok, this);
            message.setObjectName(QStringLiteral("projectRestoreResult"));
            message.setTextFormat(Qt::PlainText);
            if (!details.isEmpty()) {
                message.setDetailedText(details.join(QLatin1Char('\n')));
            }
            message.exec();
        });
    projectVersioningNavigation_->restore(checkpointId);
}

QStringList MainWindow::reloadRestoredProjectDocuments()
{
    QList<QPointer<QWidget>> documents;
    for (int i = 0; i < editorTabs_->count(); ++i) {
        documents.append(editorTabs_->widget(i));
    }
    for (auto *tab : detachedMapEditorTabs()) {
        documents.append(tab);
    }
    QStringList errors;
    for (const auto &document : documents) {
        if (document.isNull()) {
            continue;
        }
        const QString path = documentPathForWidget(document);
        const QString relative = QDir(projectRootPath_).relativeFilePath(path);
        if (path.isEmpty() || QDir::isAbsolutePath(relative) || relative == QStringLiteral("..")
            || relative.startsWith(QStringLiteral("../"))) {
            continue;
        }
        if (!QFileInfo::exists(path)) {
            if (!closeOpenDocumentForFilePath(path)) {
                errors.append(tr("Could not close the removed file: %1").arg(path));
            }
            continue;
        }
        QString error;
        if (!reloadDocumentWidgetFromDisk(document, &error)) {
            const QString detail = tr("Could not reload %1: %2").arg(path, error);
            qWarning("%s", qPrintable(detail));
            errors.append(detail);
        }
    }
    return errors;
}
