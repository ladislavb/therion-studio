#include "MainWindowSessionController.h"

#include "MainWindowSessionProjectService.h"
#include "MainWindowSessionRestoreOrchestrationService.h"
#include "MainWindowSessionRestoreUiFlowService.h"
#include "MainWindowSessionWindowRestoreService.h"

namespace TherionStudio
{
namespace
{
void executeRestoreSteps(const MainWindowSessionRestoreOrchestrationService::Plan &plan,
                         const MainWindowSessionController::RestoreSessionActions &actions)
{
    const QString &projectPath = plan.restoreProjectDecision.projectPath;
    for (const MainWindowSessionRestoreOrchestrationService::Step step : plan.steps) {
        switch (step) {
        case MainWindowSessionRestoreOrchestrationService::Step::ApplyProjectRootToBrowser:
            if (actions.applyProjectRootToBrowser) {
                actions.applyProjectRootToBrowser(projectPath);
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::AppendRestoredProjectRootConsole:
            if (actions.appendConsoleLine) {
                actions.appendConsoleLine(MainWindowSessionRestoreUiFlowService::restoredProjectRootConsoleLine(projectPath));
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::LoadStructureNameOverrides:
            if (actions.loadStructureNameOverrides) {
                actions.loadStructureNameOverrides();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::SyncOpenDocumentsToProjectRoot:
            if (actions.syncOpenDocumentsToProjectRoot) {
                actions.syncOpenDocumentsToProjectRoot();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::RebuildStructureSidebar:
            if (actions.rebuildStructureSidebar) {
                actions.rebuildStructureSidebar();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay:
            if (actions.refreshTherionConfigDisplay) {
                actions.refreshTherionConfigDisplay();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState:
            if (actions.updateProjectActionState) {
                actions.updateProjectActionState();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments:
            if (actions.restoreOpenDocuments) {
                actions.restoreOpenDocuments();
            }
            break;
        }
    }
}
}

void MainWindowSessionController::restoreSession(const ISessionStore &sessionStore,
                                                 const RestoreSessionActions &actions)
{
    const QByteArray geometry = sessionStore.mainWindowGeometry();
    const QByteArray state = sessionStore.mainWindowState();
    const MainWindowSessionWindowRestoreService::Plan windowRestorePlan =
        MainWindowSessionWindowRestoreService::buildPlan(geometry, state);

    bool geometryRestoreSucceeded = false;
    if (windowRestorePlan.shouldAttemptRestoreGeometry && actions.restoreGeometry) {
        geometryRestoreSucceeded = actions.restoreGeometry(geometry);
    }
    if (MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(
            windowRestorePlan, geometryRestoreSucceeded)
        && actions.resizeToDefault) {
        actions.resizeToDefault();
    }
    if (windowRestorePlan.shouldAttemptRestoreState && actions.restoreState) {
        actions.restoreState(state);
    }
    if (windowRestorePlan.shouldEnsureUsableWindowSize && actions.ensureUsableWindowSize) {
        actions.ensureUsableWindowSize();
    }

    const MainWindowSessionProjectService::ProjectRestoreDecision restoreProjectDecision =
        MainWindowSessionProjectService::decideProjectRestore(sessionStore.lastProjectPath());
    const MainWindowSessionRestoreOrchestrationService::Plan restorePlan =
        MainWindowSessionRestoreOrchestrationService::buildPlan(restoreProjectDecision);
    executeRestoreSteps(restorePlan, actions);
}
}
