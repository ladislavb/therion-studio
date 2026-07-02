#pragma once

#include "ProjectSourceSnapshot.h"

#include "../core/ProjectStructureIndex.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>

#include <optional>

namespace TherionStudio
{
struct ProjectIndexSnapshotCacheEntry
{
    QString sourceRequestKey;
    QString errorMessage;
    ProjectIndexSnapshot snapshot;
};

struct ProjectSourceSnapshotCacheEntry
{
    QString sourceRequestKey;
    ProjectSourceSnapshot snapshot;
};

class ProjectScanCacheService final
{
public:
    [[nodiscard]] ProjectSourceSnapshot projectSourceSnapshot(
        const QString &projectRootPath,
        const QString &preferredConfigPath,
        const QHash<QString, QString> &inMemoryContentsByPath,
        qsizetype maximumTextBytes,
        bool *cacheHit = nullptr);
    [[nodiscard]] std::optional<ProjectSourceSnapshotCacheEntry> cachedProjectSourceSnapshot(
        const QString &sourceRequestKey) const;
    void storeProjectSourceSnapshot(const ProjectSourceSnapshotCacheEntry &entry);
    void clearProjectSourceSnapshot();

    [[nodiscard]] std::optional<ProjectIndexSnapshotCacheEntry> projectIndexSnapshot(
        const QString &sourceRequestKey) const;
    void storeProjectIndexSnapshot(const ProjectIndexSnapshotCacheEntry &entry);
    void clearProjectIndexSnapshot();

private:
    mutable QMutex mutex_;
    mutable QVector<ProjectSourceSnapshotCacheEntry> projectSourceSnapshots_;
    mutable QVector<ProjectIndexSnapshotCacheEntry> projectIndexSnapshots_;
};
}
