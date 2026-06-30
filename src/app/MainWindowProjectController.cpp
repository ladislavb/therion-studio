#include "MainWindowProjectController.h"

#include "MainWindowProjectLifecycleService.h"
#include "MainWindowProjectOrchestrationService.h"
#include "MainWindowProjectUiFlowService.h"
#include "MainWindowProjectWorkspaceService.h"
#include "MainWindowRecentProjectsService.h"

namespace TherionStudio
{
namespace
{
void executeOpenProjectSteps(const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> &steps,
                             const MainWindowProjectController::Actions &actions,
                             ISessionStore &sessionStore,
                             const MainWindowProjectWorkspaceService::OpenProjectWorkspaceState &workspaceState)
{
    for (const MainWindowProjectOrchestrationService::OpenProjectStep step : steps) {
        switch (step) {
        case MainWindowProjectOrchestrationService::OpenProjectStep::ApplyProjectRootToBrowser:
            if (actions.applyProjectRootToBrowser) {
                actions.applyProjectRootToBrowser(workspaceState.projectRootPath);
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::LoadStructureNameOverrides:
            if (actions.loadStructureNameOverrides) {
                actions.loadStructureNameOverrides();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::SyncOpenDocumentsToProjectRoot:
            if (actions.syncOpenDocumentsToProjectRoot) {
                actions.syncOpenDocumentsToProjectRoot();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::PersistSessionLastProjectPath:
            sessionStore.setLastProjectPath(workspaceState.sessionLastProjectPath);
            sessionStore.setRecentProjectPaths(
                MainWindowRecentProjectsService::recordOpenedProject(
                    sessionStore.recentProjectPaths(),
                    workspaceState.projectRootPath));
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::RebuildStructureSidebar:
            if (actions.rebuildStructureSidebar) {
                actions.rebuildStructureSidebar();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::RefreshTherionConfigDisplay:
            if (actions.refreshTherionConfigDisplay) {
                actions.refreshTherionConfigDisplay();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::UpdateProjectActionState:
            if (actions.updateProjectActionState) {
                actions.updateProjectActionState();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::EnsureWelcomeTab:
            if (actions.ensureWelcomeTab) {
                actions.ensureWelcomeTab();
            }
            break;
        }
    }
}

void executeCloseProjectSteps(const std::vector<MainWindowProjectOrchestrationService::CloseProjectStep> &steps,
                              const MainWindowProjectController::Actions &actions,
                              ISessionStore &sessionStore,
                              const MainWindowProjectWorkspaceService::CloseProjectWorkspaceState &workspaceState)
{
    for (const MainWindowProjectOrchestrationService::CloseProjectStep step : steps) {
        switch (step) {
        case MainWindowProjectOrchestrationService::CloseProjectStep::ClearDocumentTabs:
            if (actions.clearDocumentTabs) {
                actions.clearDocumentTabs();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::ResetProjectBrowser:
            if (actions.resetProjectBrowser) {
                actions.resetProjectBrowser();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::ClearProjectValidationResults:
            if (actions.clearProjectValidationResults) {
                actions.clearProjectValidationResults();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::PersistSessionLastProjectPath:
            sessionStore.setLastProjectPath(workspaceState.sessionLastProjectPath);
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::PersistOpenDocuments:
            if (actions.persistOpenDocuments) {
                actions.persistOpenDocuments();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::ResetProjectTherionRunContext:
            if (actions.resetProjectTherionRunContext) {
                actions.resetProjectTherionRunContext();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::RebuildStructureSidebar:
            if (actions.rebuildStructureSidebar) {
                actions.rebuildStructureSidebar();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::RefreshTherionConfigDisplay:
            if (actions.refreshTherionConfigDisplay) {
                actions.refreshTherionConfigDisplay();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::UpdateProjectActionState:
            if (actions.updateProjectActionState) {
                actions.updateProjectActionState();
            }
            break;
        }
    }
}
}

void MainWindowProjectController::openProject(const QString &selectedProjectPath,
                                              bool hasNoTabs,
                                              bool hasWelcomeTab,
                                              ISessionStore &sessionStore,
                                              const Actions &actions)
{
    const MainWindowProjectLifecycleService::OpenProjectDecision decision =
        MainWindowProjectLifecycleService::decideOpenProject(selectedProjectPath);
    const MainWindowProjectUiFlowService::DecisionPresentation decisionPresentation =
        MainWindowProjectUiFlowService::presentOpenProjectDecision(decision);
    if (!decisionPresentation.shouldContinueWorkflow) {
        if (decisionPresentation.showWarningDialog && actions.showWarningDialog) {
            actions.showWarningDialog(decisionPresentation.warningDialogTitle,
                                      decisionPresentation.warningDialogMessage);
        }
        if (decisionPresentation.showStatusBarMessage && actions.showStatusBarMessage) {
            actions.showStatusBarMessage(decisionPresentation.statusBarMessage,
                                         decisionPresentation.statusBarTimeoutMs);
        }
        return;
    }

    const MainWindowProjectWorkspaceService::OpenProjectWorkspaceState workspaceState =
        MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(
            decision.projectPath, hasNoTabs, hasWelcomeTab);
    if (actions.setProjectRootPath) {
        actions.setProjectRootPath(workspaceState.projectRootPath);
    }

    executeOpenProjectSteps(
        MainWindowProjectOrchestrationService::buildOpenProjectSteps(workspaceState.shouldEnsureWelcomeTab),
        actions,
        sessionStore,
        workspaceState);

    const MainWindowProjectUiFlowService::SuccessPresentation successPresentation =
        MainWindowProjectUiFlowService::presentOpenProjectSuccess(workspaceState.projectRootPath);
    if (actions.showStatusBarMessage) {
        actions.showStatusBarMessage(successPresentation.statusBarMessage,
                                     successPresentation.statusBarTimeoutMs);
    }
    if (actions.appendConsoleLine) {
        actions.appendConsoleLine(successPresentation.consoleMessage);
    }
}

void MainWindowProjectController::closeProject(const QString &currentProjectPath,
                                               const std::function<bool()> &confirmCloseDirtyDocuments,
                                               ISessionStore &sessionStore,
                                               const Actions &actions)
{
    const MainWindowProjectLifecycleService::CloseProjectDecision decision =
        MainWindowProjectLifecycleService::decideCloseProject(currentProjectPath,
                                                              confirmCloseDirtyDocuments);
    const MainWindowProjectUiFlowService::DecisionPresentation decisionPresentation =
        MainWindowProjectUiFlowService::presentCloseProjectDecision(decision);
    if (!decisionPresentation.shouldContinueWorkflow) {
        if (decisionPresentation.showStatusBarMessage && actions.showStatusBarMessage) {
            actions.showStatusBarMessage(decisionPresentation.statusBarMessage,
                                         decisionPresentation.statusBarTimeoutMs);
        }
        return;
    }

    const MainWindowProjectWorkspaceService::CloseProjectWorkspaceState workspaceState =
        MainWindowProjectWorkspaceService::buildCloseProjectWorkspaceState(
            decision.closedProjectPath);
    if (actions.setProjectRootPath) {
        actions.setProjectRootPath(workspaceState.projectRootPath);
    }

    executeCloseProjectSteps(
        MainWindowProjectOrchestrationService::buildCloseProjectSteps(),
        actions,
        sessionStore,
        workspaceState);

    const MainWindowProjectUiFlowService::SuccessPresentation successPresentation =
        MainWindowProjectUiFlowService::presentCloseProjectSuccess(workspaceState.closedProjectPath);
    if (actions.showStatusBarMessage) {
        actions.showStatusBarMessage(successPresentation.statusBarMessage,
                                     successPresentation.statusBarTimeoutMs);
    }
    if (actions.appendConsoleLine) {
        actions.appendConsoleLine(successPresentation.consoleMessage);
    }
}
}
