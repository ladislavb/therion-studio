#include "ProjectScanCacheService.h"

#include <QMutexLocker>

namespace TherionStudio
{
std::optional<ProjectIndexSnapshotCacheEntry> ProjectScanCacheService::projectIndexSnapshot(
    const QString &sourceRequestKey) const
{
    const QMutexLocker locker(&mutex_);
    if (projectIndexSnapshot_.has_value()
        && projectIndexSnapshot_->sourceRequestKey == sourceRequestKey) {
        return projectIndexSnapshot_;
    }
    return std::nullopt;
}

void ProjectScanCacheService::storeProjectIndexSnapshot(const ProjectIndexSnapshotCacheEntry &entry)
{
    const QMutexLocker locker(&mutex_);
    projectIndexSnapshot_ = entry;
}

void ProjectScanCacheService::clearProjectIndexSnapshot()
{
    const QMutexLocker locker(&mutex_);
    projectIndexSnapshot_.reset();
}
}
