#include "ProjectFileWatchDelta.h"

#include <QSet>

namespace TherionStudio
{
namespace
{
QStringList sortedDifference(const QStringList &left, const QStringList &right)
{
    const QSet<QString> rightSet(right.cbegin(), right.cend());
    QStringList result;
    for (const QString &path : left) {
        if (!rightSet.contains(path)) {
            result.append(path);
        }
    }
    result.removeDuplicates();
    result.sort(Qt::CaseSensitive);
    return result;
}
}

ProjectFileWatchDelta ProjectFileWatchDeltaPlanner::plan(const ProjectFileWatchInventory &inventory,
                                                          const QStringList &currentDirectories,
                                                          const QStringList &currentFiles)
{
    ProjectFileWatchDelta delta;
    delta.directoriesToRemove = sortedDifference(currentDirectories, inventory.directories);
    delta.directoriesToAdd = sortedDifference(inventory.directories, currentDirectories);
    delta.filesToRemove = sortedDifference(currentFiles, inventory.files);
    delta.filesToAdd = sortedDifference(inventory.files, currentFiles);
    return delta;
}

} // namespace TherionStudio
