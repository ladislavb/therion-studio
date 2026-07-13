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
int runProjectStructureScannerTest(int argc, char **argv);

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    int status = 0;
    status |= runMainWindowHelpDocumentTest(argc, argv);
    status |= runMainWindowProjectLifecycleServiceTest(argc, argv);
    status |= runMainWindowProjectOrchestrationServiceTest(argc, argv);
    status |= runMainWindowProjectUiFlowServiceTest(argc, argv);
    status |= runMainWindowProjectWorkspaceServiceTest(argc, argv);
    status |= runMainWindowRecentFilesServiceTest(argc, argv);
    status |= runMainWindowRecentProjectsServiceTest(argc, argv);
    status |= runProjectFileWatchInventoryTest(argc, argv);
    status |= runProjectFileWatchInventoryServiceTest(argc, argv);
    status |= runProjectFileWatchDeltaTest(argc, argv);
    status |= runProjectOutputsScannerTest(argc, argv);
    status |= runProjectStructureScannerTest(argc, argv);
    status |= runMainWindowSessionDocumentServiceTest(argc, argv);
    status |= runMainWindowSessionProjectServiceTest(argc, argv);
    status |= runMainWindowSessionRestoreOrchestrationServiceTest(argc, argv);
    status |= runMainWindowSessionRestoreUiFlowServiceTest(argc, argv);
    status |= runMainWindowSessionStateServiceTest(argc, argv);
    status |= runMainWindowSessionWindowRestoreServiceTest(argc, argv);
    status |= runMainWindowStructureNameOverridesServiceTest(argc, argv);
    status |= runTherionSqlReportDatabaseTest(argc, argv);
    status |= runTherionSqlReportWorkerTest(argc, argv);
    return status;
}
