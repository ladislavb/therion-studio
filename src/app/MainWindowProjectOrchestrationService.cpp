#include "MainWindowProjectOrchestrationService.h"

namespace TherionStudio
{
std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> MainWindowProjectOrchestrationService::buildOpenProjectSteps(bool shouldEnsureWelcomeTab)
{
    std::vector<OpenProjectStep> steps = {
        OpenProjectStep::ApplyProjectRootToBrowser,
        OpenProjectStep::LoadStructureNameOverrides,
        OpenProjectStep::SyncOpenDocumentsToProjectRoot,
        OpenProjectStep::PersistSessionLastProjectPath,
        OpenProjectStep::RebuildStructureSidebar,
        OpenProjectStep::RefreshTherionConfigDisplay,
        OpenProjectStep::UpdateProjectActionState};

    if (shouldEnsureWelcomeTab) {
        steps.push_back(OpenProjectStep::EnsureWelcomeTab);
    }

    return steps;
}

std::vector<MainWindowProjectOrchestrationService::CloseProjectStep> MainWindowProjectOrchestrationService::buildCloseProjectSteps()
{
    return {
        CloseProjectStep::ClearDocumentTabs,
        CloseProjectStep::ResetProjectBrowser,
        CloseProjectStep::ClearProjectValidationResults,
        CloseProjectStep::PersistSessionLastProjectPath,
        CloseProjectStep::PersistOpenDocuments,
        CloseProjectStep::ResetProjectTherionRunContext,
        CloseProjectStep::RebuildStructureSidebar,
        CloseProjectStep::RefreshTherionConfigDisplay,
        CloseProjectStep::UpdateProjectActionState};
}
}
