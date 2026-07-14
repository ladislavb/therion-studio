#include "QTestSuiteDispatcher.h"

#include <QApplication>
#include <QtTest/QtTest>

int runRawEditorCompletionInsertionControllerTest(int argc, char **argv);
int runRawEditorCompletionContextAnalyzerTest(int argc, char **argv);
int runRawEditorCompletionTokenContextTest(int argc, char **argv);
int runRawEditorFindShortcutTest(int argc, char **argv);

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    return runSelectedQTestSuites(argc, argv, {
        {"RawEditorCompletionInsertionControllerTest", runRawEditorCompletionInsertionControllerTest},
        {"RawEditorCompletionContextAnalyzerTest", runRawEditorCompletionContextAnalyzerTest},
        {"RawEditorCompletionTokenContextTest", runRawEditorCompletionTokenContextTest},
        {"RawEditorFindShortcutTest", runRawEditorFindShortcutTest}});
}
