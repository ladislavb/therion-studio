#include "QTestSuiteDispatcher.h"

#include <QApplication>
#include <QtTest/QtTest>

int runThreeDViewerProjectionTest(int argc, char **argv);
int runThreeDViewerLayerListModelTest(int argc, char **argv);
int runThreeDViewerInspectorStateTest(int argc, char **argv);
int runThreeDViewerImageExportDialogTest(int argc, char **argv);
int runThreeDViewerInspectorWidgetTest(int argc, char **argv);
int runThreeDViewerViewportWidgetTest(int argc, char **argv);
int runThreeDViewerViewportControllerTest(int argc, char **argv);

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    return runSelectedQTestSuites(argc, argv, {
        {"ThreeDViewerProjectionTest", runThreeDViewerProjectionTest},
        {"ThreeDViewerLayerListModelTest", runThreeDViewerLayerListModelTest},
        {"ThreeDViewerInspectorStateTest", runThreeDViewerInspectorStateTest},
        {"ThreeDViewerImageExportDialogTest", runThreeDViewerImageExportDialogTest},
        {"ThreeDViewerInspectorWidgetTest", runThreeDViewerInspectorWidgetTest},
        {"ThreeDViewerViewportWidgetTest", runThreeDViewerViewportWidgetTest},
        {"ThreeDViewerViewportControllerTest", runThreeDViewerViewportControllerTest}});
}
