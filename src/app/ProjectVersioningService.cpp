#include "ProjectVersioningService.h"
#include "ProjectVersioningSettings.h"

#include <QFutureWatcher>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>

#include <utility>

namespace TherionStudio
{
ProjectVersioningService::ProjectVersioningService(QObject *parent)
    : QObject(parent)
    , applicationDataDirectory_(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
    , ownedSettings_(std::make_unique<QSettings>())
    , settings_(ownedSettings_.get())
{
    initialize();
}

ProjectVersioningService::ProjectVersioningService(QSettings &settings, QString applicationDataDirectory,
                                                   QObject *parent)
    : QObject(parent)
    , applicationDataDirectory_(std::move(applicationDataDirectory))
    , settings_(&settings)
{
    initialize();
}

ProjectVersioningService::~ProjectVersioningService() = default;

void ProjectVersioningService::initialize()
{
    timer_ = new QTimer(this);
    timer_->setObjectName(QStringLiteral("automaticCheckpointTimer"));
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &ProjectVersioningService::checkNow);
    watcher_ = new QFutureWatcher<ProjectVersioningStore::CheckpointResult>(this);
    connect(watcher_, &QFutureWatcher<ProjectVersioningStore::CheckpointResult>::finished, this, [this] {
        const auto result = watcher_->result();
        if (!result.succeeded) {
            qWarning("Automatic project checkpoint failed: %s", qPrintable(result.error));
        }
        emit checkpointFinished(activeRoot_, result.succeeded, result.checkpointId, result.error);
    });
}

void ProjectVersioningService::configure(const QString &projectRootPath)
{
    const bool sameRoot = projectRootPath_ == projectRootPath;
    projectRootPath_ = projectRootPath;
    if (projectRootPath.isEmpty()) {
        timer_->stop();
        return;
    }
    settings_->sync();
    ProjectVersioningSettings preferences(*settings_, applicationDataDirectory_);
    const auto result = preferences.projectSettings(projectRootPath_);
    if (!result.succeeded) {
        timer_->stop();
        qWarning("Automatic project history settings failed: %s", qPrintable(result.error));
        emit checkpointFinished(projectRootPath_, false, {}, result.error);
        return;
    }
    if (!result.settings.enabled || !result.settings.automaticEnabled) {
        timer_->stop();
        return;
    }
    const int interval = result.settings.intervalMinutes * 60 * 1000;
    if (!sameRoot || !timer_->isActive() || timer_->interval() != interval) {
        timer_->start(interval);
    }
}

void ProjectVersioningService::checkNow()
{
    configure(projectRootPath_);
    if (!timer_->isActive() || watcher_->isRunning()) {
        return;
    }
    ProjectVersioningSettings preferences(*settings_, applicationDataDirectory_);
    const auto result = preferences.projectSettings(projectRootPath_);
    if (!result.succeeded) {
        timer_->stop();
        qWarning("Automatic project history settings failed: %s", qPrintable(result.error));
        emit checkpointFinished(projectRootPath_, false, {}, result.error);
        return;
    }
    activeRoot_ = projectRootPath_;
    ProjectVersioningStore store(activeRoot_, result.settings.historyDirectory);
    watcher_->setFuture(QtConcurrent::run([store]() mutable {
        return store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    }));
}
} // namespace TherionStudio
