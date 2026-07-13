#pragma once

#include "ProjectFileWatchInventory.h"

#include <QStringList>

namespace TherionStudio
{

struct ProjectFileWatchDelta
{
    QStringList directoriesToRemove;
    QStringList directoriesToAdd;
    QStringList filesToRemove;
    QStringList filesToAdd;
};

class ProjectFileWatchDeltaPlanner final
{
public:
    static ProjectFileWatchDelta plan(const ProjectFileWatchInventory &inventory,
                                      const QStringList &currentDirectories,
                                      const QStringList &currentFiles);
};

} // namespace TherionStudio
