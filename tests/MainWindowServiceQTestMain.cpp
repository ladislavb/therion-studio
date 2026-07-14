#include "QTestSuiteDispatcher.h"

#include <QCoreApplication>
#include <QtTest/QtTest>

int runMainWindowProjectLifecycleServiceTest(int argc, char **argv);
int runMainWindowProjectOrchestrationServiceTest(int argc, char **argv);
int runMainWindowProjectUiFlowServiceTest(int argc, char **argv);
int runMainWindowProjectWorkspaceServiceTest(int argc, char **argv);
int runMainWindowSessionDocumentServiceTest(int argc, char **argv);
int runMainWindowSessionProjectServiceTest(int argc, char **argv);
int runMainWindowSessionRestoreOrchestrationServiceTest(int argc, char **argv);
int runMainWindowSessionRestoreUiFlowServiceTest(int argc, char **argv);
int runMainWindowSessionStateServiceTest(int argc, char **argv);
int runMainWindowSessionWindowRestoreServiceTest(int argc, char **argv);
int runMainWindowStructureNameOverridesServiceTest(int argc, char **argv);
int runTherionSqlReportDatabaseTest(int argc, char **argv);
int runTherionSqlReportWorkerTest(int argc, char **argv);
int runMainWindowHelpDocumentTest(int argc, char **argv);
int runMainWindowRecentFilesServiceTest(int argc, char **argv);
int runMainWindowRecentProjectsServiceTest(int argc, char **argv);
int runProjectFileWatchInventoryTest(int argc, char **argv);
int runProjectFileWatchInventoryServiceTest(int argc, char **argv);
int runProjectFileWatchDeltaTest(int argc, char **argv);
int runProjectOutputsScannerTest(int argc, char **argv);
int runProjectValidationScannerQTest(int argc, char **argv);
int runProjectStructureScannerTest(int argc, char **argv);

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    return runSelectedQTestSuites(argc, argv, {
        {"MainWindowHelpDocumentTest", runMainWindowHelpDocumentTest},
        {"MainWindowProjectLifecycleServiceTest", runMainWindowProjectLifecycleServiceTest},
        {"MainWindowProjectOrchestrationServiceTest", runMainWindowProjectOrchestrationServiceTest},
        {"MainWindowProjectUiFlowServiceTest", runMainWindowProjectUiFlowServiceTest},
        {"MainWindowProjectWorkspaceServiceTest", runMainWindowProjectWorkspaceServiceTest},
        {"MainWindowRecentFilesServiceTest", runMainWindowRecentFilesServiceTest},
        {"MainWindowRecentProjectsServiceTest", runMainWindowRecentProjectsServiceTest},
        {"ProjectFileWatchInventoryTest", runProjectFileWatchInventoryTest},
        {"ProjectFileWatchInventoryServiceTest", runProjectFileWatchInventoryServiceTest},
        {"ProjectFileWatchDeltaTest", runProjectFileWatchDeltaTest},
        {"ProjectOutputsScannerTest", runProjectOutputsScannerTest},
        {"ProjectValidationScannerQTest", runProjectValidationScannerQTest},
        {"ProjectStructureScannerTest", runProjectStructureScannerTest},
        {"MainWindowSessionDocumentServiceTest", runMainWindowSessionDocumentServiceTest},
        {"MainWindowSessionProjectServiceTest", runMainWindowSessionProjectServiceTest},
        {"MainWindowSessionRestoreOrchestrationServiceTest", runMainWindowSessionRestoreOrchestrationServiceTest},
        {"MainWindowSessionRestoreUiFlowServiceTest", runMainWindowSessionRestoreUiFlowServiceTest},
        {"MainWindowSessionStateServiceTest", runMainWindowSessionStateServiceTest},
        {"MainWindowSessionWindowRestoreServiceTest", runMainWindowSessionWindowRestoreServiceTest},
        {"MainWindowStructureNameOverridesServiceTest", runMainWindowStructureNameOverridesServiceTest},
        {"TherionSqlReportDatabaseTest", runTherionSqlReportDatabaseTest},
        {"TherionSqlReportWorkerTest", runTherionSqlReportWorkerTest}});
}
