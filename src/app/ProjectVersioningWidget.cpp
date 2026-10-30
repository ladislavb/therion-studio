#include "ProjectVersioningWidget.h"
#include "GitHubRepositoryWidget.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QStringDecoder>
#include <QtConcurrent>

#include <utility>

namespace TherionStudio
{
ProjectVersioningWidget::ProjectVersioningWidget(QString projectRootPath, QWidget *parent)
    : QWidget(parent)
    , projectRootPath_(std::move(projectRootPath))
    , applicationDataDirectory_(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
    , ownedSettings_(std::make_unique<QSettings>())
    , settings_(ownedSettings_.get())
{
    initialize();
}

ProjectVersioningWidget::ProjectVersioningWidget(QString projectRootPath, QSettings &settings,
                                                 QString applicationDataDirectory, QWidget *parent)
    : QWidget(parent)
    , projectRootPath_(std::move(projectRootPath))
    , applicationDataDirectory_(std::move(applicationDataDirectory))
    , settings_(&settings)
{
    initialize();
}

ProjectVersioningWidget::~ProjectVersioningWidget() = default;

void ProjectVersioningWidget::initialize()
{
    auto *layout = new QVBoxLayout(this);
    const auto addNote = [this](QVBoxLayout *target, const QString &text) {
        auto *label = new QLabel(text, this);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        target->addWidget(label);
        return label;
    };
    auto *projectRow = new QHBoxLayout;
    auto *projectLabel = new QLabel(tr("Current project"), this);
    auto *projectPath = new QLineEdit(projectRootPath_.isEmpty()
        ? tr("No project open") : QDir::toNativeSeparators(projectRootPath_), this);
    projectPath->setReadOnly(true);
    projectPath->setCursorPosition(0);
    projectPath->setToolTip(projectPath->text());
    projectLabel->setBuddy(projectPath);
    projectRow->addWidget(projectLabel);
    projectRow->addWidget(projectPath, 1);
    layout->addLayout(projectRow);
    addNote(layout, tr("These backup controls apply immediately, independently of OK or Cancel. "
                      "Local history is stored outside the project. The project's Git repository is not changed."));
    auto *pages = new QTabWidget(this);
    pages->setObjectName(QStringLiteral("backupSettingsPages"));
    layout->addWidget(pages, 1);

    auto *local = new QGroupBox(tr("Local backups"), this);
    auto *localLayout = new QVBoxLayout(local);
    enabled_ = new QCheckBox(tr("Enable local history for this project"), local);
    enabled_->setObjectName(QStringLiteral("localHistoryEnabled"));
    localLayout->addWidget(enabled_);
    addNote(localLayout, tr("Only changed, saved files create checkpoints. Unsaved edits are not included. "
                           "Local history is not encrypted; disabling it keeps existing backups."));
    location_ = addNote(localLayout, QString());
    location_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    location_->setObjectName(QStringLiteral("localHistoryLocation"));
    status_ = addNote(layout, QString());
    status_->setObjectName(QStringLiteral("localHistoryStatus"));

    automatic_ = new QCheckBox(tr("Create checkpoints automatically"), local);
    automatic_->setObjectName(QStringLiteral("automaticHistoryEnabled"));
    localLayout->addWidget(automatic_);
    auto *intervalLayout = new QHBoxLayout;
    auto *intervalLabel = new QLabel(tr("Check for changes every"), local);
    interval_ = new QSpinBox(local);
    interval_->setObjectName(QStringLiteral("automaticHistoryMinutes"));
    interval_->setRange(1, 1440);
    interval_->setSuffix(tr(" min"));
    intervalLabel->setBuddy(interval_);
    intervalLayout->addWidget(intervalLabel);
    intervalLayout->addWidget(interval_);
    intervalLayout->addStretch();
    localLayout->addLayout(intervalLayout);
    connect(automatic_, &QCheckBox::clicked, this, &ProjectVersioningWidget::saveAutomaticSettings);
    connect(interval_, &QSpinBox::valueChanged, this, &ProjectVersioningWidget::saveAutomaticSettings);

    auto *actions = new QHBoxLayout;
    checkpoint_ = new QPushButton(tr("Create checkpoint"), local);
    checkpoint_->setObjectName(QStringLiteral("createLocalCheckpoint"));
    refresh_ = new QPushButton(tr("Refresh history"), local);
    refresh_->setObjectName(QStringLiteral("refreshLocalHistory"));
    openFolder_ = new QPushButton(tr("Open history folder"), local);
    actions->addWidget(checkpoint_);
    actions->addWidget(refresh_);
    actions->addWidget(openFolder_);
    actions->addStretch();
    localLayout->addLayout(actions);
    history_ = new QTableWidget(0, 3, local);
    history_->setObjectName(QStringLiteral("localCheckpointHistory"));
    history_->setHorizontalHeaderLabels({tr("Created"), tr("Checkpoint"), tr("ID")});
    history_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    history_->setSelectionBehavior(QAbstractItemView::SelectRows);
    history_->setSelectionMode(QAbstractItemView::SingleSelection);
    history_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    history_->horizontalHeader()->setStretchLastSection(true);
    history_->verticalHeader()->hide();
    history_->setMinimumHeight(100);
    localLayout->addWidget(history_, 1);
    remove_ = new QPushButton(tr("Delete local history..."), local);
    remove_->setObjectName(QStringLiteral("removeLocalHistory"));
    localLayout->addWidget(remove_, 0, Qt::AlignLeft);
    connect(remove_, &QPushButton::clicked, this, &ProjectVersioningWidget::removeHistory);
    pages->addTab(local, tr("Local backups"));

    auto *ignoreGroup = new QGroupBox(tr("Project .gitignore"), this);
    auto *ignoreLayout = new QVBoxLayout(ignoreGroup);
    addNote(ignoreLayout, tr("Exclusions: project .gitignore rules; output/ and outputs/ folders; "
                           "*.3d, *.lox and *.plt exports; Git metadata. Local history is not encrypted. "
                           "Disabling history does not delete existing checkpoints."));
    addNote(ignoreLayout, tr("Edit the root .gitignore. Saving also affects your project's Git tools. "
                             "Nested .gitignore files still apply. Old checkpoints keep previously included files."));
    ignoreEditor_ = new QPlainTextEdit(ignoreGroup);
    ignoreEditor_->setObjectName(QStringLiteral("projectGitignore"));
    ignoreEditor_->setAccessibleName(tr("Project .gitignore"));
    ignoreEditor_->setMinimumHeight(120);
    ignoreLayout->addWidget(ignoreEditor_, 1);
    auto *ignoreActions = new QHBoxLayout;
    saveIgnore_ = new QPushButton(tr("Save .gitignore"), ignoreGroup);
    saveIgnore_->setObjectName(QStringLiteral("saveProjectGitignore"));
    reloadIgnore_ = new QPushButton(tr("Reload .gitignore"), ignoreGroup);
    reloadIgnore_->setObjectName(QStringLiteral("reloadProjectGitignore"));
    ignoreActions->addWidget(saveIgnore_);
    ignoreActions->addWidget(reloadIgnore_);
    ignoreActions->addStretch();
    ignoreLayout->addLayout(ignoreActions);
    connect(saveIgnore_, &QPushButton::clicked, this, &ProjectVersioningWidget::saveIgnore);
    connect(reloadIgnore_, &QPushButton::clicked, this, &ProjectVersioningWidget::reloadIgnore);

    auto *remote = new QGroupBox(tr("GitHub backup"), this);
    remote->setObjectName(QStringLiteral("githubSettingsPage"));
    auto *remoteLayout = new QVBoxLayout(remote);
    remoteLayout->addWidget(new GitHubRepositoryWidget(projectRootPath_, remote));
    remoteLayout->addStretch();
    pages->addTab(remote, tr("GitHub"));
    pages->addTab(ignoreGroup, tr("Excluded files (.gitignore)"));

    watcher_ = new QFutureWatcher<OperationResult>(this);
    connect(watcher_, &QFutureWatcher<OperationResult>::finished, this, [this] {
        const OperationResult result = watcher_->result();
        if (!result.error.isEmpty()) {
            showError(result.error);
        } else {
            history_->setRowCount(result.history.checkpoints.size());
            int row = 0;
            for (const auto &entry : result.history.checkpoints) {
                history_->setItem(row, 0, new QTableWidgetItem(
                    QLocale().toString(entry.createdAt.toLocalTime(), QLocale::ShortFormat)));
                history_->setItem(row, 1, new QTableWidgetItem(entry.label == QStringLiteral("Manual checkpoint")
                    ? tr("Manual checkpoint") : entry.label == QStringLiteral("Automatic checkpoint")
                    ? tr("Automatic checkpoint") : entry.label == QStringLiteral("Safety checkpoint before restore")
                    ? tr("Safety checkpoint before restore") : entry.label.startsWith(QStringLiteral("Restored checkpoint "))
                    ? tr("Restored checkpoint %1").arg(entry.label.mid(20, 12)) : entry.label));
                auto *id = new QTableWidgetItem(entry.id.left(12));
                id->setToolTip(entry.id);
                history_->setItem(row++, 2, id);
            }
            status_->setText(result.historyRemoved ? tr("Local history deleted. Local history is now disabled.")
                : preferences_.enabled
                ? tr("Local history enabled. %1 checkpoint(s).").arg(history_->rowCount())
                : tr("Local history disabled. Existing checkpoints are retained."));
        }
        updateControls();
    });
    connect(enabled_, &QCheckBox::clicked, this, &ProjectVersioningWidget::changeEnabled);
    connect(checkpoint_, &QPushButton::clicked, this, [this] { loadHistory(true); });
    connect(refresh_, &QPushButton::clicked, this, [this] { loadHistory(false); });
    connect(openFolder_, &QPushButton::clicked, this, [this] {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(preferences_.historyDirectory))) {
            showError(tr("Could not open the local history folder."));
        }
    });

    if (projectRootPath_.isEmpty()) {
        status_->setText(tr("Open a project using File > Open Project, then return to Settings > Backups and versioning."));
    } else {
        ProjectVersioningSettings preferences(*settings_, applicationDataDirectory_);
        const auto result = preferences.projectSettings(projectRootPath_);
        settingsAvailable_ = result.succeeded;
        if (!result.succeeded) {
            showError(result.error);
        } else {
            preferences_ = result.settings;
            status_->setText(preferences_.enabled ? tr("Local history enabled.") : tr("Local history disabled."));
        }
    }
    updateControls();
    if (settingsAvailable_) {
        reloadIgnore();
    }
    if (settingsAvailable_ && QFileInfo::exists(preferences_.historyDirectory)) {
        loadHistory(false);
    }
}

void ProjectVersioningWidget::showGitHubPage()
{
    findChild<QTabWidget *>(QStringLiteral("backupSettingsPages"))->setCurrentWidget(
        findChild<QGroupBox *>(QStringLiteral("githubSettingsPage")));
}

void ProjectVersioningWidget::changeEnabled(bool enabled)
{
    if (enabled && QMessageBox::question(this, tr("Enable local history"),
            tr("Back up saved files under this project root?\n%1\n\n"
               "Project .gitignore rules and the exclusions shown in this tab apply. "
               "Existing plaintext checkpoints remain readable on this computer. "
               "An initial checkpoint will be created now; unsaved editor changes are not included.")
                .arg(QDir::toNativeSeparators(projectRootPath_)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        updateControls();
        return;
    }
    ProjectVersioningSettings preferences(*settings_, applicationDataDirectory_);
    const auto result = preferences.setEnabled(projectRootPath_, enabled);
    if (!result.succeeded) {
        showError(result.error);
        updateControls();
        return;
    }
    preferences_ = result.settings;
    emit preferencesChanged();
    status_->setText(enabled ? tr("Local history enabled.")
                            : tr("Local history disabled. Existing checkpoints are retained."));
    updateControls();
    if (enabled) {
        loadHistory(true);
    }
}

void ProjectVersioningWidget::loadHistory(bool createCheckpoint)
{
    if (watcher_->isRunning() || !settingsAvailable_
        || (createCheckpoint && !preferences_.enabled)) {
        return;
    }
    status_->setText(createCheckpoint ? tr("Creating checkpoint from saved files...")
                                      : tr("Loading local history..."));
    ProjectVersioningStore store(projectRootPath_, preferences_.historyDirectory);
    watcher_->setFuture(QtConcurrent::run([store, createCheckpoint]() mutable {
        OperationResult result;
        if (createCheckpoint) {
            const auto checkpoint = store.createCheckpoint(QStringLiteral("Manual checkpoint"));
            if (!checkpoint.succeeded) {
                result.error = checkpoint.error;
                return result;
            }
        }
        result.history = store.history();
        if (!result.history.succeeded) {
            result.error = result.history.error;
        }
        return result;
    }));
    updateControls();
}

void ProjectVersioningWidget::updateControls()
{
    const bool busy = watcher_ && watcher_->isRunning();
    const QSignalBlocker blocker(enabled_);
    const QSignalBlocker automaticBlocker(automatic_);
    const QSignalBlocker intervalBlocker(interval_);
    enabled_->setChecked(preferences_.enabled);
    enabled_->setEnabled(settingsAvailable_ && !busy);
    checkpoint_->setEnabled(settingsAvailable_ && preferences_.enabled && !busy);
    refresh_->setEnabled(settingsAvailable_ && !preferences_.projectId.isEmpty() && !busy);
    openFolder_->setEnabled(!busy && !preferences_.historyDirectory.isEmpty()
                           && QFileInfo::exists(preferences_.historyDirectory));
    ignoreEditor_->setEnabled(settingsAvailable_ && ignoreLoaded_ && !busy);
    saveIgnore_->setEnabled(settingsAvailable_ && ignoreLoaded_ && !busy);
    reloadIgnore_->setEnabled(settingsAvailable_ && !busy);
    automatic_->setChecked(preferences_.automaticEnabled);
    automatic_->setEnabled(settingsAvailable_ && preferences_.enabled && !busy);
    interval_->setValue(preferences_.intervalMinutes);
    interval_->setEnabled(settingsAvailable_ && preferences_.enabled && !busy);
    remove_->setEnabled(settingsAvailable_ && !busy && !preferences_.historyDirectory.isEmpty()
                        && QFileInfo::exists(preferences_.historyDirectory));
    location_->setText(preferences_.historyDirectory.isEmpty()
        ? tr("History location: assigned after enabling local history.")
        : tr("History location: %1").arg(QDir::toNativeSeparators(preferences_.historyDirectory)));
}

void ProjectVersioningWidget::showError(const QString &error)
{
    status_->setText(tr("Local history error: %1").arg(error));
    qWarning("Local project history operation failed: %s", qPrintable(error));
}

void ProjectVersioningWidget::reloadIgnore()
{
    const QString path = QDir(projectRootPath_).filePath(QStringLiteral(".gitignore"));
    const QFileInfo info(path);
    ignoreLoaded_ = false;
    if (info.isSymLink() || (info.exists() && !info.isFile())) {
        showError(tr("Cannot edit a linked or non-regular .gitignore file."));
        updateControls();
        return;
    }
    QFile file(path);
    if (info.exists() && !file.open(QIODevice::ReadOnly)) {
        showError(tr("Could not read .gitignore: %1").arg(file.errorString()));
        updateControls();
        return;
    }
    const QByteArray bytes = info.exists() ? file.readAll() : QByteArray();
    if (file.isOpen() && file.error() != QFileDevice::NoError) {
        showError(tr("Could not read .gitignore: %1").arg(file.errorString()));
        updateControls();
        return;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(bytes);
    if (decoder.hasError() || bytes.contains('\0')) {
        showError(tr("The .gitignore file must be UTF-8 text to edit it here."));
        updateControls();
        return;
    }
    ignoreContents_ = bytes;
    ignoreExisted_ = info.exists();
    ignoreLoaded_ = true;
    ignoreEditor_->setPlainText(text);
    ignoreEditor_->document()->setModified(false);
    updateControls();
}

void ProjectVersioningWidget::saveIgnore()
{
    const QString path = QDir(projectRootPath_).filePath(QStringLiteral(".gitignore"));
    const QFileInfo info(path);
    if (!ignoreLoaded_ || info.isSymLink() || (info.exists() && !info.isFile())) {
        showError(tr("Cannot edit a linked or non-regular .gitignore file."));
        return;
    }
    QFile current(path);
    if (info.exists() && !current.open(QIODevice::ReadOnly)) {
        showError(tr("Could not read .gitignore: %1").arg(current.errorString()));
        return;
    }
    const QByteArray currentContents = info.exists() ? current.readAll() : QByteArray();
    if (current.isOpen() && current.error() != QFileDevice::NoError) {
        showError(tr("Could not read .gitignore: %1").arg(current.errorString()));
        return;
    }
    current.close();
    if (info.exists() != ignoreExisted_ || currentContents != ignoreContents_) {
        showError(tr("The project .gitignore changed on disk. Reload it before saving."));
        return;
    }
    QByteArray contents = ignoreEditor_->toPlainText().toUtf8();
    if (ignoreContents_.contains("\r\n")) {
        contents.replace("\n", "\r\n");
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(contents) != contents.size()
        || !output.commit()) {
        showError(tr("Could not save .gitignore: %1").arg(output.errorString()));
        return;
    }
    ignoreContents_ = contents;
    ignoreExisted_ = true;
    ignoreEditor_->document()->setModified(false);
    status_->setText(tr("Project .gitignore saved. New rules apply to future checkpoints."));
}

void ProjectVersioningWidget::saveAutomaticSettings()
{
    ProjectVersioningSettings preferences(*settings_, applicationDataDirectory_);
    const auto result = preferences.setAutomatic(projectRootPath_, automatic_->isChecked(), interval_->value());
    if (result.succeeded) {
        preferences_ = result.settings;
        emit preferencesChanged();
        status_->setText(preferences_.automaticEnabled
            ? tr("Automatic checks every %1 minute(s) while this project is open.").arg(preferences_.intervalMinutes)
            : tr("Automatic checkpoints disabled."));
    } else {
        showError(result.error);
    }
    updateControls();
}

void ProjectVersioningWidget::removeHistory()
{
    if (watcher_->isRunning() || QMessageBox::warning(this, tr("Delete local history"),
            tr("Permanently delete all local checkpoints for this project?\n%1\n\n"
               "Project files and the project's Git repository will NOT be changed. "
               "Local history will be disabled. This cannot be undone.")
                .arg(QDir::toNativeSeparators(preferences_.historyDirectory)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return;
    }
    ProjectVersioningSettings preferences(*settings_, applicationDataDirectory_);
    const auto disabled = preferences.setEnabled(projectRootPath_, false);
    if (!disabled.succeeded) {
        showError(disabled.error);
        return;
    }
    preferences_ = disabled.settings;
    emit preferencesChanged();
    status_->setText(tr("Deleting local history..."));
    ProjectVersioningStore store(projectRootPath_, preferences_.historyDirectory);
    watcher_->setFuture(QtConcurrent::run([store]() mutable {
        OperationResult result;
        const auto removed = store.removeHistory();
        result.error = removed.error;
        result.history.succeeded = removed.succeeded;
        result.historyRemoved = removed.succeeded;
        return result;
    }));
    updateControls();
}
} // namespace TherionStudio
