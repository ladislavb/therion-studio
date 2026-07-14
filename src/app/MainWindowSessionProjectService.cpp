#include "MainWindowSessionProjectService.h"

#include <QDir>

namespace TherionStudio
{
MainWindowSessionProjectService::ProjectRestoreDecision MainWindowSessionProjectService::decideProjectRestore(const QString &lastProjectPath)
{
    if (lastProjectPath.isEmpty() || !QDir(lastProjectPath).exists()) {
        return {};
    }

    return {ProjectRestoreStatus::Restored, lastProjectPath};
}
}
