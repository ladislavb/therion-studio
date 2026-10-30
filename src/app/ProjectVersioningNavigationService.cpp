#include "ProjectVersioningNavigationService.h"

#include <QFutureWatcher>
#include <QtConcurrent>

namespace TherionStudio
{
ProjectVersioningNavigationService::ProjectVersioningNavigationService(QObject *parent)
    : QObject(parent)
    , history_(new QFutureWatcher<ProjectVersioningStore::HistoryResult>(this))
    , restore_(new QFutureWatcher<ProjectVersioningStore::RestoreResult>(this))
{
    connect(history_, &QFutureWatcher<ProjectVersioningStore::HistoryResult>::finished, this, [this] {
        if (requestedRoot_ == root_) {
            const auto result = history_->result();
            if (!result.succeeded) {
                qWarning("Project history navigation failed: %s", qPrintable(result.error));
            }
            emit historyReady(result, preserveSelection_);
        }
        emit busyChanged(isBusy());
        if (refreshPending_) {
            refreshPending_ = false;
            refresh();
        }
    });
    connect(restore_, &QFutureWatcher<ProjectVersioningStore::RestoreResult>::finished, this, [this] {
        const auto result = restore_->result();
        if (!result.succeeded) {
            qWarning("Project restore failed: %s", qPrintable(result.error));
        }
        emit busyChanged(isBusy());
        emit restoreFinished(result);
    });
}

bool ProjectVersioningNavigationService::isBusy() const
{
    return history_->isRunning() || restore_->isRunning();
}

bool ProjectVersioningNavigationService::isEnabled() const
{
    return enabled_ && !root_.isEmpty() && !historyDirectory_.isEmpty();
}

void ProjectVersioningNavigationService::configure(const QString &root, const QString &historyDirectory, bool enabled)
{
    const bool changed = root_ != root || historyDirectory_ != historyDirectory || enabled_ != enabled;
    root_ = root;
    historyDirectory_ = historyDirectory;
    enabled_ = enabled;
    if (changed) {
        refresh(false);
    }
}

void ProjectVersioningNavigationService::refresh(bool preserveSelection)
{
    if (isBusy()) {
        refreshPending_ = true;
        return;
    }
    if (!isEnabled()) {
        ProjectVersioningStore::HistoryResult empty;
        empty.succeeded = true;
        emit historyReady(empty, false);
        return;
    }
    preserveSelection_ = preserveSelection;
    requestedRoot_ = root_;
    const ProjectVersioningStore store(root_, historyDirectory_);
    history_->setFuture(QtConcurrent::run([store] { return store.history(); }));
    emit busyChanged(true);
}

void ProjectVersioningNavigationService::restore(const QString &checkpointId)
{
    if (isBusy() || !isEnabled()) {
        ProjectVersioningStore::RestoreResult result;
        result.error = tr("Local history is disabled or busy. Wait and try again.");
        emit restoreFinished(result);
        return;
    }
    ProjectVersioningStore store(root_, historyDirectory_);
    restore_->setFuture(QtConcurrent::run([store, checkpointId]() mutable {
        return store.restoreCheckpoint(checkpointId);
    }));
    emit busyChanged(true);
}
}
