#include <QtTest/QtTest>

int runMapEditorAreaReferenceResolverTest(int argc, char **argv);
int runMapEditorFreehandSimplificationTest(int argc, char **argv);
int runMapEditorObjectDeletePlannerTest(int argc, char **argv);
int runMapEditorObjectMovePlannerTest(int argc, char **argv);
int runMapEditorPointSymbolGeometryTest(int argc, char **argv);
int runMapEditorSvgBackgroundMetadataTest(int argc, char **argv);
int runMapEditorUndoArbitrationServiceTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runMapEditorAreaReferenceResolverTest(argc, argv);
    status |= runMapEditorFreehandSimplificationTest(argc, argv);
    status |= runMapEditorObjectDeletePlannerTest(argc, argv);
    status |= runMapEditorObjectMovePlannerTest(argc, argv);
    status |= runMapEditorPointSymbolGeometryTest(argc, argv);
    status |= runMapEditorSvgBackgroundMetadataTest(argc, argv);
    status |= runMapEditorUndoArbitrationServiceTest(argc, argv);
    return status;
}
