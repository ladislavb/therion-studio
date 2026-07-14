#include "QTestSuiteDispatcher.h"

#include <QtTest/QtTest>

int runTextEditorDocumentIoServiceTest(int argc, char **argv);
int runTextEditorDocumentPersistenceStateServiceTest(int argc, char **argv);
int runTextEditorDocumentPreconditionsServiceTest(int argc, char **argv);
int runTextEditorDocumentWorkflowControllerTest(int argc, char **argv);
int runTextEditorOptionValidationTest(int argc, char **argv);

int main(int argc, char **argv)
{
    return runSelectedQTestSuites(argc, argv, {
        {"TextEditorDocumentIoServiceTest", runTextEditorDocumentIoServiceTest},
        {"TextEditorDocumentPersistenceStateServiceTest", runTextEditorDocumentPersistenceStateServiceTest},
        {"TextEditorDocumentPreconditionsServiceTest", runTextEditorDocumentPreconditionsServiceTest},
        {"TextEditorDocumentWorkflowControllerTest", runTextEditorDocumentWorkflowControllerTest},
        {"TextEditorOptionValidationTest", runTextEditorOptionValidationTest}});
}
