#include "QTestSuiteDispatcher.h"

#include <QApplication>
#include <QtTest/QtTest>

int runMapEditorBackgroundRoundTripTest(int argc, char **argv);

int main(int argc, char **argv)
{
    // These suites drive real map editor widgets, so the runner needs a
    // QApplication rather than the QCoreApplication the QTest macros create.
    QApplication app(argc, argv);
    return runSelectedQTestSuites(argc, argv, {
        {"MapEditorBackgroundRoundTripTest", runMapEditorBackgroundRoundTripTest}});
}
