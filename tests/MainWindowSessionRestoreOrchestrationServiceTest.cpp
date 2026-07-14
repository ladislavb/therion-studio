#include "../src/app/MainWindowSessionRestoreOrchestrationService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowSessionRestoreOrchestrationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsRestoredPlan();
    void buildsNoProjectPlan();
};

MainWindowSessionProjectService::ProjectRestoreDecision makeDecision(MainWindowSessionProjectService::ProjectRestoreStatus status,
                                                                     const QString &path)
{
    MainWindowSessionProjectService::ProjectRestoreDecision decision;
    decision.status = status;
    decision.projectPath = path;
    return decision;
}

void MainWindowSessionRestoreOrchestrationServiceTest::buildsRestoredPlan()
{
    const auto plan = MainWindowSessionRestoreOrchestrationService::buildPlan(
        makeDecision(MainWindowSessionProjectService::ProjectRestoreStatus::Restored,
                     QStringLiteral("/tmp/project")));

    const std::vector<MainWindowSessionRestoreOrchestrationService::Step> expected = {
        MainWindowSessionRestoreOrchestrationService::Step::ApplyProjectRootToBrowser,
        MainWindowSessionRestoreOrchestrationService::Step::AppendRestoredProjectRootConsole,
        MainWindowSessionRestoreOrchestrationService::Step::LoadStructureNameOverrides,
        MainWindowSessionRestoreOrchestrationService::Step::SyncOpenDocumentsToProjectRoot,
        MainWindowSessionRestoreOrchestrationService::Step::RebuildStructureSidebar,
        MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay,
        MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState,
        MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments};

    QVERIFY2(plan.steps == expected, "Restored-project session restore plan steps changed unexpectedly.");
}

void MainWindowSessionRestoreOrchestrationServiceTest::buildsNoProjectPlan()
{
    const auto plan = MainWindowSessionRestoreOrchestrationService::buildPlan(
        makeDecision(MainWindowSessionProjectService::ProjectRestoreStatus::NotRestored,
                     QString()));

    const std::vector<MainWindowSessionRestoreOrchestrationService::Step> expected = {
        MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay,
        MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState,
        MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments};

    QVERIFY2(plan.steps == expected, "No-project session restore plan steps changed unexpectedly.");
}
}

int runMainWindowSessionRestoreOrchestrationServiceTest(int argc, char **argv)
{
    MainWindowSessionRestoreOrchestrationServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowSessionRestoreOrchestrationServiceTest.moc"
