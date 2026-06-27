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
int runMainWindowHelpDocumentTest(int argc, char **argv);
int runMainWindowRecentFilesServiceTest(int argc, char **argv);
int runMainWindowRecentProjectsServiceTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runMainWindowHelpDocumentTest(argc, argv);
    status |= runMainWindowProjectLifecycleServiceTest(argc, argv);
    status |= runMainWindowProjectOrchestrationServiceTest(argc, argv);
    status |= runMainWindowProjectUiFlowServiceTest(argc, argv);
    status |= runMainWindowProjectWorkspaceServiceTest(argc, argv);
    status |= runMainWindowRecentFilesServiceTest(argc, argv);
    status |= runMainWindowRecentProjectsServiceTest(argc, argv);
    status |= runMainWindowSessionDocumentServiceTest(argc, argv);
    status |= runMainWindowSessionProjectServiceTest(argc, argv);
    status |= runMainWindowSessionRestoreOrchestrationServiceTest(argc, argv);
    status |= runMainWindowSessionRestoreUiFlowServiceTest(argc, argv);
    status |= runMainWindowSessionStateServiceTest(argc, argv);
    status |= runMainWindowSessionWindowRestoreServiceTest(argc, argv);
    status |= runMainWindowStructureNameOverridesServiceTest(argc, argv);
    return status;
}
