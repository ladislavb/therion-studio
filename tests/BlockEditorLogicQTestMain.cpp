#include <QtTest/QtTest>

int runBlockEditorDocumentOutlineBuilderTest(int argc, char **argv);
int runBlockEditorMoveSourceRewriterTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runBlockEditorDocumentOutlineBuilderTest(argc, argv);
    status |= runBlockEditorMoveSourceRewriterTest(argc, argv);
    return status;
}
