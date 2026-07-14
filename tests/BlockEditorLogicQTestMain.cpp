#include "QTestSuiteDispatcher.h"

#include <QtTest/QtTest>

int runBlockEditorDocumentOutlineBuilderTest(int argc, char **argv);
int runBlockEditorMoveSourceRewriterTest(int argc, char **argv);

int main(int argc, char **argv)
{
    return runSelectedQTestSuites(argc, argv, {
        {"BlockEditorDocumentOutlineBuilderTest", runBlockEditorDocumentOutlineBuilderTest},
        {"BlockEditorMoveSourceRewriterTest", runBlockEditorMoveSourceRewriterTest}});
}
