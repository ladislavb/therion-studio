#include "QTestSuiteDispatcher.h"

#include <QtTest/QtTest>

int runMapEditorAreaReferenceResolverTest(int argc, char **argv);
int runMapEditorFreehandSimplificationTest(int argc, char **argv);
int runMapEditorInputPolicyTest(int argc, char **argv);
int runMapEditorObjectDeletePlannerTest(int argc, char **argv);
int runMapEditorObjectMovePlannerTest(int argc, char **argv);
int runMapEditorPointSymbolGeometryTest(int argc, char **argv);
int runMapEditorSvgBackgroundMetadataTest(int argc, char **argv);
int runMapEditorUndoArbitrationServiceTest(int argc, char **argv);

int main(int argc, char **argv)
{
    return runSelectedQTestSuites(argc, argv, {
        {"MapEditorAreaReferenceResolverTest", runMapEditorAreaReferenceResolverTest},
        {"MapEditorFreehandSimplificationTest", runMapEditorFreehandSimplificationTest},
        {"MapEditorInputPolicyTest", runMapEditorInputPolicyTest},
        {"MapEditorObjectDeletePlannerTest", runMapEditorObjectDeletePlannerTest},
        {"MapEditorObjectMovePlannerTest", runMapEditorObjectMovePlannerTest},
        {"MapEditorPointSymbolGeometryTest", runMapEditorPointSymbolGeometryTest},
        {"MapEditorSvgBackgroundMetadataTest", runMapEditorSvgBackgroundMetadataTest},
        {"MapEditorUndoArbitrationServiceTest", runMapEditorUndoArbitrationServiceTest}});
}
