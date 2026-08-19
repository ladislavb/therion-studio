#include "../QTestSuiteDispatcher.h"

#include <QtTest/QtTest>

int runCommandCatalogStoreTest(int argc, char **argv);
int runPocketTopoImportTest(int argc, char **argv);
int runTh2GeometryProjectionTest(int argc, char **argv);
int runTherionSourceTextTest(int argc, char **argv);
int runTherionSourceDocumentTest(int argc, char **argv);
int runTherionSourceFormatterTest(int argc, char **argv);
int runTherionSourceLogicalDocumentTest(int argc, char **argv);
int runTherionSourceSnapshotCacheTest(int argc, char **argv);
int runTherionTokenRulesTest(int argc, char **argv);
int runTherionDocumentEditorDraftInsertionTest(int argc, char **argv);
int runTherionSourceValidatorFixTest(int argc, char **argv);
int runTherionSourceValidatorProjectionTest(int argc, char **argv);
int runProjectStructureIndexQTest(int argc, char **argv);
int runThreeDViewerLoxLoaderTest(int argc, char **argv);
int runThreeDViewerLoxCorpusPolicyTest(int argc, char **argv);
int runThreeDViewerCameraTest(int argc, char **argv);
int runThreeDViewerSceneModelTest(int argc, char **argv);
int runThreeDViewerSceneStatisticsTest(int argc, char **argv);
int runTherionFileTypesTest(int argc, char **argv);
int runTherionXviPassageTest(int argc, char **argv);

int main(int argc, char **argv)
{
    return runSelectedQTestSuites(argc, argv, {
        {"CommandCatalogStoreTest", runCommandCatalogStoreTest},
        {"PocketTopoImportTest", runPocketTopoImportTest},
        {"Th2GeometryProjectionTest", runTh2GeometryProjectionTest},
        {"TherionFileTypesTest", runTherionFileTypesTest},
        {"TherionXviPassageTest", runTherionXviPassageTest},
        {"ThreeDViewerCameraTest", runThreeDViewerCameraTest},
        {"ThreeDViewerLoxLoaderTest", runThreeDViewerLoxLoaderTest},
        {"ThreeDViewerLoxCorpusPolicyTest", runThreeDViewerLoxCorpusPolicyTest},
        {"ThreeDViewerSceneModelTest", runThreeDViewerSceneModelTest},
        {"ThreeDViewerSceneStatisticsTest", runThreeDViewerSceneStatisticsTest},
        {"TherionDocumentEditorDraftInsertionTest", runTherionDocumentEditorDraftInsertionTest},
        {"TherionSourceTextTest", runTherionSourceTextTest},
        {"TherionSourceDocumentTest", runTherionSourceDocumentTest},
        {"TherionSourceFormatterTest", runTherionSourceFormatterTest},
        {"TherionSourceLogicalDocumentTest", runTherionSourceLogicalDocumentTest},
        {"TherionSourceSnapshotCacheTest", runTherionSourceSnapshotCacheTest},
        {"TherionSourceValidatorFixTest", runTherionSourceValidatorFixTest},
        {"TherionSourceValidatorProjectionTest", runTherionSourceValidatorProjectionTest},
        {"ProjectStructureIndexQTest", runProjectStructureIndexQTest},
        {"TherionTokenRulesTest", runTherionTokenRulesTest}});
}
