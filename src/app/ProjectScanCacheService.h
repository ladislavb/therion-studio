#pragma once

#include "../core/ProjectStructureIndex.h"

#include <QMutex>
#include <QString>

#include <optional>

namespace TherionStudio
{
struct ProjectIndexSnapshotCacheEntry
{
    QString sourceRequestKey;
    QString errorMessage;
    ProjectIndexSnapshot snapshot;
};

class ProjectScanCacheService final
{
public:
    [[nodiscard]] std::optional<ProjectIndexSnapshotCacheEntry> projectIndexSnapshot(
        const QString &sourceRequestKey) const;
    void storeProjectIndexSnapshot(const ProjectIndexSnapshotCacheEntry &entry);
    void clearProjectIndexSnapshot();

private:
    mutable QMutex mutex_;
    std::optional<ProjectIndexSnapshotCacheEntry> projectIndexSnapshot_;
};
}
