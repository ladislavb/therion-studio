#pragma once

#include "MainWindowSessionProjectService.h"

#include <vector>

namespace TherionStudio
{
class MainWindowSessionRestoreOrchestrationService final
{
public:
    enum class Step
    {
        ApplyProjectRootToBrowser,
        AppendRestoredProjectRootConsole,
        LoadStructureNameOverrides,
        SyncOpenDocumentsToProjectRoot,
        RebuildStructureSidebar,
        RefreshTherionConfigDisplay,
        UpdateProjectActionState,
        RestoreOpenDocuments
    };

    struct Plan
    {
        MainWindowSessionProjectService::ProjectRestoreDecision restoreProjectDecision;
        std::vector<Step> steps;
    };

    static Plan buildPlan(const MainWindowSessionProjectService::ProjectRestoreDecision &restoreProjectDecision);
};
}
