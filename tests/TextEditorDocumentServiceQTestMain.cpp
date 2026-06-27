#include <QtTest/QtTest>

int runTextEditorDocumentIoServiceTest(int argc, char **argv);
int runTextEditorDocumentPersistenceStateServiceTest(int argc, char **argv);
int runTextEditorDocumentPreconditionsServiceTest(int argc, char **argv);
int runTextEditorDocumentWorkflowControllerTest(int argc, char **argv);
int runTextEditorOptionValidationTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runTextEditorDocumentIoServiceTest(argc, argv);
    status |= runTextEditorDocumentPersistenceStateServiceTest(argc, argv);
    status |= runTextEditorDocumentPreconditionsServiceTest(argc, argv);
    status |= runTextEditorDocumentWorkflowControllerTest(argc, argv);
    status |= runTextEditorOptionValidationTest(argc, argv);
    return status;
}
