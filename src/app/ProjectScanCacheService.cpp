#include "ProjectScanCacheService.h"

#include <QMutexLocker>

namespace TherionStudio
{
ProjectSourceSnapshot ProjectScanCacheService::projectSourceSnapshot(
    const QString &projectRootPath,
    const QString &preferredConfigPath,
    const QHash<QString, QString> &inMemoryContentsByPath,
    qsizetype maximumTextBytes,
    bool *cacheHit)
{
    const ProjectSourceRequestKey requestKey =
        projectSourceRequestKey(projectRootPath,
                                preferredConfigPath,
                                inMemoryContentsByPath,
                                projectSourceFilePolicyKey(maximumTextBytes));
    const QString sourceRequestKey = requestKey.stableKey();
    if (const std::optional<ProjectSourceSnapshotCacheEntry> cachedSnapshot =
            cachedProjectSourceSnapshot(sourceRequestKey);
        cachedSnapshot.has_value()) {
        if (cacheHit != nullptr) {
            *cacheHit = true;
        }
        return cachedSnapshot->snapshot;
    }

    ProjectSourceSnapshot snapshot = collectProjectSourceSnapshot(projectRootPath,
                                                                 preferredConfigPath,
                                                                 inMemoryContentsByPath,
                                                                 maximumTextBytes);
    if (cacheHit != nullptr) {
        *cacheHit = false;
    }
    storeProjectSourceSnapshot(ProjectSourceSnapshotCacheEntry{
        sourceRequestKey,
        snapshot,
    });
    return snapshot;
}

std::optional<ProjectSourceSnapshotCacheEntry> ProjectScanCacheService::cachedProjectSourceSnapshot(
    const QString &sourceRequestKey) const
{
    const QMutexLocker locker(&mutex_);
    if (projectSourceSnapshot_.has_value()
        && projectSourceSnapshot_->sourceRequestKey == sourceRequestKey) {
        return projectSourceSnapshot_;
    }
    return std::nullopt;
}

void ProjectScanCacheService::storeProjectSourceSnapshot(const ProjectSourceSnapshotCacheEntry &entry)
{
    const QMutexLocker locker(&mutex_);
    projectSourceSnapshot_ = entry;
}

void ProjectScanCacheService::clearProjectSourceSnapshot()
{
    const QMutexLocker locker(&mutex_);
    projectSourceSnapshot_.reset();
}

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
