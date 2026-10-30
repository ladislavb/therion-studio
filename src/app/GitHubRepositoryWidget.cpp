#include "GitHubRepositoryWidget.h"
#include "ProjectGitHubSettings.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace TherionStudio
{
GitHubRepositoryWidget::GitHubRepositoryWidget(const QString &projectRootPath, QWidget *parent, const QString &executable)
    : QWidget(parent)
    , executable_(executable.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("gh")) : executable)
    , projectRootPath_(QFileInfo(projectRootPath).canonicalFilePath())
    , projectOpen_(!projectRootPath.isEmpty())
    , process_(new QProcess(this))
    , timeout_(new QTimer(this))
    , saveTimer_(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *note = new QLabel(tr("Create an empty private repository on GitHub. No project files, paths or "
        "history are uploaded by repository creation. Optional backups upload saved files without encryption. "
        "Sign-in uses GitHub CLI and your browser; credentials must be stored in the system keyring."), this);
    note->setWordWrap(true);
    note->setTextFormat(Qt::PlainText);
    layout->addWidget(note);
    login_ = new QPushButton(tr("Sign in through browser..."), this);
    login_->setObjectName(QStringLiteral("githubSignIn"));
    check_ = new QPushButton(tr("Check GitHub account"), this);
    check_->setObjectName(QStringLiteral("githubCheckAccount"));
    auto *authActions = new QHBoxLayout;
    authActions->addWidget(login_);
    authActions->addWidget(check_);
    layout->addLayout(authActions);
    loginOutput_ = new QPlainTextEdit(this);
    loginOutput_->setObjectName(QStringLiteral("githubLoginOutput"));
    loginOutput_->setReadOnly(true);
    loginOutput_->setMaximumBlockCount(100);
    loginOutput_->setMaximumHeight(100);
    loginOutput_->setAccessibleName(tr("GitHub sign-in instructions"));
    loginOutput_->hide();
    layout->addWidget(loginOutput_);
    auto *form = new QFormLayout;
    owner_ = new QLineEdit(this);
    owner_->setMaxLength(39);
    owner_->setObjectName(QStringLiteral("githubRepositoryOwner"));
    name_ = new QLineEdit(QStringLiteral("survey-backup-") + QUuid::createUuid().toString(QUuid::Id128).left(8), this);
    name_->setObjectName(QStringLiteral("githubRepositoryName"));
    name_->setMaxLength(100);
    form->addRow(tr("Owner (account or organization)"), owner_);
    form->addRow(tr("Repository name"), name_);
    layout->addLayout(form);
    uploadEnabled_ = new QCheckBox(tr("Enable unencrypted backups to this private repository"), this);
    uploadEnabled_->setObjectName(QStringLiteral("githubUploadEnabled"));
    promptOnClose_ = new QCheckBox(tr("Ask before exiting when saved project changes have not been uploaded"), this);
    promptOnClose_->setObjectName(QStringLiteral("githubPromptOnClose"));
    layout->addWidget(uploadEnabled_);
    layout->addWidget(promptOnClose_);
    configurationStatus_ = new QLabel(this);
    configurationStatus_->setObjectName(QStringLiteral("githubConfigurationStatus"));
    configurationStatus_->setWordWrap(true);
    configurationStatus_->setTextFormat(Qt::PlainText);
    layout->addWidget(configurationStatus_);
    create_ = new QPushButton(tr("Create private repository..."), this);
    create_->setObjectName(QStringLiteral("githubCreateRepository"));
    verify_ = new QPushButton(tr("Verify repository"), this);
    verify_->setObjectName(QStringLiteral("githubVerifyRepository"));
    auto *repositoryActions = new QHBoxLayout;
    repositoryActions->addWidget(create_);
    repositoryActions->addWidget(verify_);
    layout->addLayout(repositoryActions);
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("githubBackupStatus"));
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status_);
    process_->setWorkingDirectory(QDir::tempPath());
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const QString &key : environment.keys()) {
        if (key.startsWith(QStringLiteral("GH_")) || key.startsWith(QStringLiteral("GITHUB_"))
            || key.startsWith(QStringLiteral("GIT_"))) {
            environment.remove(key);
        }
    }
    environment.insert(QStringLiteral("GH_HOST"), QStringLiteral("github.com"));
    environment.insert(QStringLiteral("GH_PROMPT_DISABLED"), QStringLiteral("1"));
    environment.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    process_->setProcessEnvironment(environment);
    timeout_->setSingleShot(true);
    connect(process_, &QProcess::started, this, [this] {
        if (operation_ == Operation::Login) {
            process_->write("\n");
        }
        process_->closeWriteChannel();
    });
    connect(timeout_, &QTimer::timeout, this, [this] {
        timedOut_ = true;
        process_->kill();
    });
    connect(process_, &QProcess::readyReadStandardError, this, [this] {
        if (operation_ == Operation::Login) {
            loginOutput_->insertPlainText(QString::fromUtf8(process_->readAllStandardError()));
        }
    });
    connect(process_, &QProcess::readyReadStandardOutput, this, [this] {
        if (operation_ == Operation::Login) {
            loginOutput_->insertPlainText(QString::fromUtf8(process_->readAllStandardOutput()));
        }
    });
    connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) { finished(code); });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            timeout_->stop();
            fail(tr("Could not start GitHub CLI: %1").arg(process_->errorString()));
        }
    });
    connect(login_, &QPushButton::clicked, this, [this] {
        authenticated_ = false;
        loginOutput_->clear();
        loginOutput_->show();
        run(Operation::Login, {QStringLiteral("auth"), QStringLiteral("login"), QStringLiteral("--hostname"),
            QStringLiteral("github.com"), QStringLiteral("--web"), QStringLiteral("--git-protocol"),
            QStringLiteral("https"), QStringLiteral("--scopes"), QStringLiteral("repo")});
    });
    connect(check_, &QPushButton::clicked, this, [this] {
        authenticated_ = false;
        run(Operation::Account, {QStringLiteral("auth"), QStringLiteral("status"),
                                QStringLiteral("--hostname"), QStringLiteral("github.com")});
    });
    connect(create_, &QPushButton::clicked, this, &GitHubRepositoryWidget::createRepository);
    connect(verify_, &QPushButton::clicked, this, [this] {
        repository_ = owner_->text().trimmed() + QLatin1Char('/') + name_->text().trimmed();
        static const QRegularExpression fullName(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9_][A-Za-z0-9_.-]{0,99}$"));
        if (!fullName.match(repository_).hasMatch()) {
            fail(tr("Enter a valid owner and repository name."));
            return;
        }
        run(Operation::Verify, {QStringLiteral("api"), QStringLiteral("repos/") + repository_});
    });
    status_->setText(executable_.isEmpty() ? tr("Install GitHub CLI (gh), then reopen Settings.")
        : !projectOpen_ ? tr("Open a project first.") : tr("Check your GitHub account or sign in to continue."));
    loadConfiguration();
    saveTimer_->setSingleShot(true);
    saveTimer_->setInterval(500);
    connect(saveTimer_, &QTimer::timeout, this, [this] { saveConfiguration(); });
    connect(uploadEnabled_, &QCheckBox::clicked, this, [this](bool enabled) {
        if (enabled && QMessageBox::warning(this, tr("Enable GitHub backup"),
            tr("Project files will be uploaded WITHOUT ENCRYPTION to the private repository. "
               "GitHub and people with repository access can read them. Project .gitignore and export exclusions apply. "
               "Use a dedicated empty repository, not the Therion Studio source repository. Continue?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
            const QSignalBlocker blocker(uploadEnabled_);
            uploadEnabled_->setChecked(false);
            return;
        }
        configurationDirty_ = true;
        saveConfiguration();
        updateControls();
    });
    connect(promptOnClose_, &QCheckBox::clicked, this, [this] {
        configurationDirty_ = true;
        saveConfiguration();
    });
    for (auto *field : {owner_, name_}) {
        connect(field, &QLineEdit::textChanged, this, [this] {
            configurationDirty_ = true;
            configurationStatus_->setText(tr("GitHub settings have unsaved changes."));
            saveTimer_->start();
        });
        connect(field, &QLineEdit::editingFinished, this, [this] { saveConfiguration(); });
    }
    updateControls();
}

GitHubRepositoryWidget::~GitHubRepositoryWidget()
{
    saveConfiguration();
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(3000);
    }
}

void GitHubRepositoryWidget::hideEvent(QHideEvent *event)
{
    saveConfiguration();
    QWidget::hideEvent(event);
}

void GitHubRepositoryWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!autoChecked_ && configurationAvailable_ && configurationExisted_
        && !executable_.isEmpty() && !owner_->text().trimmed().isEmpty()
        && operation_ == Operation::None) {
        autoChecked_ = true;
        check_->click();
    }
}

bool GitHubRepositoryWidget::configurationError(const QString &message)
{
    configurationAvailable_ = false;
    configurationStatus_->setText(message);
    qWarning("GitHub project settings failed: %s", qPrintable(message));
    updateControls();
    return false;
}

bool GitHubRepositoryWidget::readConfiguration(QByteArray &contents, bool &exists)
{
    const auto result = ProjectGitHubSettings::read(projectRootPath_);
    contents = result.contents;
    exists = result.exists;
    return result.succeeded || configurationError(QCoreApplication::translate(
        "TherionStudio::GitHubRepositoryWidget", result.error.toUtf8().constData()));
}

void GitHubRepositoryWidget::loadConfiguration()
{
    if (!projectOpen_ || !readConfiguration(configurationContents_, configurationExisted_)) {
        return;
    }
    if (configurationExisted_) {
        const QJsonDocument document = QJsonDocument::fromJson(configurationContents_);
        const QJsonObject object = document.object();
        owner_->setText(object.value(QStringLiteral("owner")).toString());
        name_->setText(object.value(QStringLiteral("repository")).toString());
        uploadEnabled_->setChecked(object.value(QStringLiteral("uploadEnabled")).toBool());
        promptOnClose_->setChecked(object.value(QStringLiteral("promptOnClose")).toBool());
        configurationStatus_->setText(tr("GitHub settings loaded from the project. Check your account before remote operations."));
    } else {
        configurationStatus_->setText(tr("Owner and repository name are saved automatically in .therion-studio-github.json. "
            "No passwords or tokens are stored in the project."));
    }
    configurationAvailable_ = true;
}

bool GitHubRepositoryWidget::saveConfiguration()
{
    saveTimer_->stop();
    if (!configurationDirty_) {
        return true;
    }
    if (!configurationAvailable_) {
        return false;
    }
    QByteArray current;
    bool exists = false;
    if (!readConfiguration(current, exists)) {
        return false;
    }
    if (exists != configurationExisted_ || current != configurationContents_) {
        return configurationError(tr("GitHub project settings changed on disk. Close and reopen Settings before editing."));
    }
    QJsonObject object{{QStringLiteral("version"), 1},
        {QStringLiteral("owner"), owner_->text().trimmed()},
        {QStringLiteral("repository"), name_->text().trimmed()}};
    if (uploadEnabled_->isChecked() || promptOnClose_->isChecked()) {
        object.insert(QStringLiteral("uploadEnabled"), uploadEnabled_->isChecked());
        object.insert(QStringLiteral("promptOnClose"), promptOnClose_->isChecked());
    }
    const QByteArray contents = QJsonDocument(object).toJson();
    QSaveFile output(QDir(projectRootPath_).filePath(QStringLiteral(".therion-studio-github.json")));
    if (!output.open(QIODevice::WriteOnly) || output.write(contents) != contents.size() || !output.commit()) {
        return configurationError(tr("Could not save GitHub project settings: %1").arg(output.errorString()));
    }
    configurationContents_ = contents;
    configurationExisted_ = true;
    configurationDirty_ = false;
    configurationStatus_->setText(tr("GitHub owner and repository saved in the project. Credentials stay in the system keyring."));
    return true;
}

void GitHubRepositoryWidget::run(Operation operation, const QStringList &arguments)
{
    if (!saveConfiguration()) {
        return;
    }
    operation_ = operation;
    timedOut_ = false;
    status_->setText(operation == Operation::Login
        ? tr("Follow the sign-in instructions below. Closing Settings cancels this operation.")
        : tr("Contacting GitHub..."));
    process_->start(executable_, arguments);
    timeout_->start(operation == Operation::Login ? 600000 : 60000);
    updateControls();
}

void GitHubRepositoryWidget::fail(const QString &message)
{
    operation_ = Operation::None;
    status_->setText(message);
    qWarning("GitHub repository operation failed: %s", qPrintable(message));
    updateControls();
}

void GitHubRepositoryWidget::finished(int exitCode)
{
    timeout_->stop();
    const auto operation = operation_;
    const QByteArray output = process_->readAllStandardOutput();
    const QByteArray errors = process_->readAllStandardError();
    operation_ = Operation::None;
    if (timedOut_ || exitCode != 0) {
        fail(tr("GitHub operation failed or timed out. Check sign-in, network access and repository permissions. "
                "If creation was attempted, use Verify repository before retrying; it may already exist."));
        return;
    }
    if (operation == Operation::Login) {
        loginOutput_->clear();
        loginOutput_->hide();
        run(Operation::Account, {QStringLiteral("auth"), QStringLiteral("status"),
                                QStringLiteral("--hostname"), QStringLiteral("github.com")});
    } else if (operation == Operation::Account) {
        const QByteArray report = output + errors;
        if (!report.contains("(keyring)") || report.contains("hosts.yml") || report.contains("(file)")) {
            fail(tr("GitHub CLI must use the system keyring. File-based or environment tokens are not accepted. "
                    "Configure a system credential store and sign in again."));
            return;
        }
        authenticated_ = true;
        status_->setText(tr("GitHub account verified using the system keyring. Enter the owner and repository name."));
        repository_ = owner_->text().trimmed() + QLatin1Char('/') + name_->text().trimmed();
        static const QRegularExpression fullName(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9_][A-Za-z0-9_.-]{0,99}$"));
        if (fullName.match(repository_).hasMatch()) {
            run(Operation::Verify, {QStringLiteral("api"), QStringLiteral("repos/") + repository_});
            return;
        }
        updateControls();
    } else if (operation == Operation::Create) {
        run(Operation::Verify, {QStringLiteral("api"), QStringLiteral("repos/") + repository_});
    } else if (operation == Operation::Verify) {
        const QJsonObject result = QJsonDocument::fromJson(output).object();
        if (result.value(QStringLiteral("private")).toBool() != true
            || result.value(QStringLiteral("visibility")).toString() != QStringLiteral("private")
            || result.value(QStringLiteral("full_name")).toString().compare(repository_, Qt::CaseInsensitive) != 0) {
            fail(tr("GitHub did not confirm a private repository with the requested owner and name. Nothing is linked or uploaded."));
            return;
        }
        status_->setText(tr("Private repository verified: https://github.com/%1\n"
            "No project data was uploaded. Use the GitHub button in the File sidebar for an explicitly enabled backup.").arg(repository_));
        updateControls();
    }
}

void GitHubRepositoryWidget::updateControls()
{
    const bool ready = projectOpen_ && !executable_.isEmpty() && operation_ == Operation::None;
    login_->setEnabled(ready);
    check_->setEnabled(ready);
    owner_->setEnabled(projectOpen_ && configurationAvailable_ && operation_ == Operation::None);
    name_->setEnabled(projectOpen_ && configurationAvailable_ && operation_ == Operation::None);
    uploadEnabled_->setEnabled(projectOpen_ && configurationAvailable_ && operation_ == Operation::None);
    promptOnClose_->setEnabled(uploadEnabled_->isEnabled() && uploadEnabled_->isChecked());
    create_->setEnabled(ready && authenticated_ && configurationAvailable_);
    verify_->setEnabled(ready && authenticated_ && configurationAvailable_);
}

void GitHubRepositoryWidget::createRepository()
{
    const QString owner = owner_->text().trimmed();
    const QString name = name_->text().trimmed();
    static const QRegularExpression validOwner(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{0,38}$"));
    static const QRegularExpression validName(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.-]{0,99}$"));
    if (!validOwner.match(owner).hasMatch() || !validName.match(name).hasMatch()) {
        fail(tr("Enter a valid owner and repository name."));
        return;
    }
    repository_ = owner + QLatin1Char('/') + name;
    if (QMessageBox::question(this, tr("Create private repository"),
        tr("Create the empty PRIVATE repository %1 on GitHub?\n"
           "Only the owner and repository name will be sent. No project files will be uploaded.")
            .arg(repository_), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    run(Operation::Create, {QStringLiteral("repo"), QStringLiteral("create"), repository_, QStringLiteral("--private")});
}
}
