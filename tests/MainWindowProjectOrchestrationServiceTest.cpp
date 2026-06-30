#include "../src/app/MainWindowProjectOrchestrationService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowProjectOrchestrationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsOpenProjectSteps();
    void buildsCloseProjectSteps();
};

void MainWindowProjectOrchestrationServiceTest::buildsOpenProjectSteps()
{
    const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> withWelcome =
        MainWindowProjectOrchestrationService::buildOpenProjectSteps(true);
    const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> expectedWithWelcome = {
        MainWindowProjectOrchestrationService::OpenProjectStep::ApplyProjectRootToBrowser,
        MainWindowProjectOrchestrationService::OpenProjectStep::LoadStructureNameOverrides,
        MainWindowProjectOrchestrationService::OpenProjectStep::SyncOpenDocumentsToProjectRoot,
        MainWindowProjectOrchestrationService::OpenProjectStep::PersistSessionLastProjectPath,
        MainWindowProjectOrchestrationService::OpenProjectStep::RebuildStructureSidebar,
        MainWindowProjectOrchestrationService::OpenProjectStep::RefreshTherionConfigDisplay,
        MainWindowProjectOrchestrationService::OpenProjectStep::UpdateProjectActionState,
        MainWindowProjectOrchestrationService::OpenProjectStep::EnsureWelcomeTab};
    QVERIFY2(withWelcome == expectedWithWelcome,
             "Open-project orchestration steps with welcome tab changed unexpectedly.");

    const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> withoutWelcome =
        MainWindowProjectOrchestrationService::buildOpenProjectSteps(false);
    const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> expectedWithoutWelcome = {
        MainWindowProjectOrchestrationService::OpenProjectStep::ApplyProjectRootToBrowser,
        MainWindowProjectOrchestrationService::OpenProjectStep::LoadStructureNameOverrides,
        MainWindowProjectOrchestrationService::OpenProjectStep::SyncOpenDocumentsToProjectRoot,
        MainWindowProjectOrchestrationService::OpenProjectStep::PersistSessionLastProjectPath,
        MainWindowProjectOrchestrationService::OpenProjectStep::RebuildStructureSidebar,
        MainWindowProjectOrchestrationService::OpenProjectStep::RefreshTherionConfigDisplay,
        MainWindowProjectOrchestrationService::OpenProjectStep::UpdateProjectActionState};
    QVERIFY2(withoutWelcome == expectedWithoutWelcome,
             "Open-project orchestration steps without welcome tab changed unexpectedly.");
}

void MainWindowProjectOrchestrationServiceTest::buildsCloseProjectSteps()
{
    const std::vector<MainWindowProjectOrchestrationService::CloseProjectStep> steps =
        MainWindowProjectOrchestrationService::buildCloseProjectSteps();
    const std::vector<MainWindowProjectOrchestrationService::CloseProjectStep> expected = {
        MainWindowProjectOrchestrationService::CloseProjectStep::ClearDocumentTabs,
        MainWindowProjectOrchestrationService::CloseProjectStep::ResetProjectBrowser,
        MainWindowProjectOrchestrationService::CloseProjectStep::ClearProjectValidationResults,
        MainWindowProjectOrchestrationService::CloseProjectStep::PersistSessionLastProjectPath,
        MainWindowProjectOrchestrationService::CloseProjectStep::PersistOpenDocuments,
        MainWindowProjectOrchestrationService::CloseProjectStep::ResetProjectTherionRunContext,
        MainWindowProjectOrchestrationService::CloseProjectStep::RebuildStructureSidebar,
        MainWindowProjectOrchestrationService::CloseProjectStep::RefreshTherionConfigDisplay,
        MainWindowProjectOrchestrationService::CloseProjectStep::UpdateProjectActionState};
    QVERIFY2(steps == expected, "Close-project orchestration steps changed unexpectedly.");
}
}

int runMainWindowProjectOrchestrationServiceTest(int argc, char **argv)
{
    MainWindowProjectOrchestrationServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowProjectOrchestrationServiceTest.moc"
