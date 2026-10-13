#include "QTestSuiteDispatcher.h"

#include <QApplication>
#include <QtTest/QtTest>

int runMapEditorBackgroundRoundTripTest(int argc, char **argv);
int runMapEditorSceneLifecycleTest(int argc, char **argv);
int runMapEditorLinePreviewTest(int argc, char **argv);
int runMapEditorAltitudeUnderlayTest(int argc, char **argv);
int runMapEditorAppearanceTest(int argc, char **argv);
int runMapEditorEndpointSnapTransactionTest(int argc, char **argv);
int runMapEditorViewportCullingTest(int argc, char **argv);
int runMapEditorPlainClickSelectionTest(int argc, char **argv);
int runMapEditorScrapStationHighlightTest(int argc, char **argv);
int runMapEditorInteractionPerformanceTest(int argc, char **argv);
int runMapEditorSpatialIndexRepaintTest(int argc, char **argv);
int runMapEditorBulkVisibilityTest(int argc, char **argv);

int main(int argc, char **argv)
{
    // These suites drive real map editor widgets, so the runner needs a
    // QApplication rather than the QCoreApplication the QTest macros create.
    QApplication app(argc, argv);
    return runSelectedQTestSuites(argc, argv, {
        {"MapEditorAppearanceTest", runMapEditorAppearanceTest},
        {"MapEditorViewportCullingTest", runMapEditorViewportCullingTest},
        {"MapEditorAltitudeUnderlayTest", runMapEditorAltitudeUnderlayTest},
        {"MapEditorEndpointSnapTransactionTest", runMapEditorEndpointSnapTransactionTest},
        {"MapEditorPlainClickSelectionTest", runMapEditorPlainClickSelectionTest},
        {"MapEditorScrapStationHighlightTest", runMapEditorScrapStationHighlightTest},
        {"MapEditorInteractionPerformanceTest", runMapEditorInteractionPerformanceTest},
        {"MapEditorSpatialIndexRepaintTest", runMapEditorSpatialIndexRepaintTest},
        {"MapEditorBulkVisibilityTest", runMapEditorBulkVisibilityTest},
        {"MapEditorBackgroundRoundTripTest", runMapEditorBackgroundRoundTripTest},
        {"MapEditorSceneLifecycleTest", runMapEditorSceneLifecycleTest},
        {"MapEditorLinePreviewTest", runMapEditorLinePreviewTest}});
}
