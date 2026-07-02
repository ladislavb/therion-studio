#include "ProjectScanCacheService.h"

#include <QMutexLocker>

namespace TherionStudio
{
namespace
{
constexpr qsizetype kRetainedProjectScanCacheEntries = 4;

void trimProjectSourceSnapshotCache(QVector<ProjectSourceSnapshotCacheEntry> *entries)
{
    while (entries != nullptr && entries->size() > kRetainedProjectScanCacheEntries) {
        entries->removeLast();
    }
}

void trimProjectIndexSnapshotCache(QVector<ProjectIndexSnapshotCacheEntry> *entries)
{
    while (entries != nullptr && entries->size() > kRetainedProjectScanCacheEntries) {
        entries->removeLast();
    }
}
}

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
    for (qsizetype index = 0; index < projectSourceSnapshots_.size(); ++index) {
        if (projectSourceSnapshots_.at(index).sourceRequestKey != sourceRequestKey) {
            continue;
        }
        ProjectSourceSnapshotCacheEntry entry = projectSourceSnapshots_.at(index);
        if (index > 0) {
            projectSourceSnapshots_.removeAt(index);
            projectSourceSnapshots_.prepend(entry);
        }
        return entry;
    }
    return std::nullopt;
}

void ProjectScanCacheService::storeProjectSourceSnapshot(const ProjectSourceSnapshotCacheEntry &entry)
{
    const QMutexLocker locker(&mutex_);
    for (qsizetype index = 0; index < projectSourceSnapshots_.size(); ++index) {
        if (projectSourceSnapshots_.at(index).sourceRequestKey != entry.sourceRequestKey) {
            continue;
        }
        projectSourceSnapshots_.removeAt(index);
        break;
    }
    projectSourceSnapshots_.prepend(entry);
    trimProjectSourceSnapshotCache(&projectSourceSnapshots_);
}

void ProjectScanCacheService::clearProjectSourceSnapshot()
{
    const QMutexLocker locker(&mutex_);
    projectSourceSnapshots_.clear();
}

std::optional<ProjectIndexSnapshotCacheEntry> ProjectScanCacheService::projectIndexSnapshot(
    const QString &sourceRequestKey) const
{
    const QMutexLocker locker(&mutex_);
    for (qsizetype index = 0; index < projectIndexSnapshots_.size(); ++index) {
        if (projectIndexSnapshots_.at(index).sourceRequestKey != sourceRequestKey) {
            continue;
        }
        ProjectIndexSnapshotCacheEntry entry = projectIndexSnapshots_.at(index);
        if (index > 0) {
            projectIndexSnapshots_.removeAt(index);
            projectIndexSnapshots_.prepend(entry);
        }
        return entry;
    }
    return std::nullopt;
}

void ProjectScanCacheService::storeProjectIndexSnapshot(const ProjectIndexSnapshotCacheEntry &entry)
{
    const QMutexLocker locker(&mutex_);
    for (qsizetype index = 0; index < projectIndexSnapshots_.size(); ++index) {
        if (projectIndexSnapshots_.at(index).sourceRequestKey != entry.sourceRequestKey) {
            continue;
        }
        projectIndexSnapshots_.removeAt(index);
        break;
    }
    projectIndexSnapshots_.prepend(entry);
    trimProjectIndexSnapshotCache(&projectIndexSnapshots_);
}

void ProjectScanCacheService::clearProjectIndexSnapshot()
{
    const QMutexLocker locker(&mutex_);
    projectIndexSnapshots_.clear();
}
}
