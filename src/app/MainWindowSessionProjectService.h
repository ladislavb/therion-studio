#pragma once

#include <QString>

namespace TherionStudio
{
class MainWindowSessionProjectService final
{
public:
    enum class ProjectRestoreStatus
    {
        NotRestored,
        Restored
    };

    struct ProjectRestoreDecision
    {
        ProjectRestoreStatus status = ProjectRestoreStatus::NotRestored;
        QString projectPath;
    };

    static ProjectRestoreDecision decideProjectRestore(const QString &lastProjectPath);
};
}
