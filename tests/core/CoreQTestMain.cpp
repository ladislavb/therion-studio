#include <QtTest/QtTest>

int runCommandCatalogStoreTest(int argc, char **argv);
int runTherionSourceTextTest(int argc, char **argv);
int runTherionSourceDocumentTest(int argc, char **argv);
int runTherionSourceLogicalDocumentTest(int argc, char **argv);
int runTherionSourceSnapshotCacheTest(int argc, char **argv);
int runTherionTokenRulesTest(int argc, char **argv);
int runTherionDocumentEditorDraftInsertionTest(int argc, char **argv);
int runTherionSourceValidatorFixTest(int argc, char **argv);
int runTherionSourceValidatorProjectionTest(int argc, char **argv);
int runThreeDViewerLoxLoaderTest(int argc, char **argv);
int runThreeDViewerCameraTest(int argc, char **argv);
int runThreeDViewerSceneModelTest(int argc, char **argv);
int runThreeDViewerSceneStatisticsTest(int argc, char **argv);
int runTherionFileTypesTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runCommandCatalogStoreTest(argc, argv);
    status |= runTherionFileTypesTest(argc, argv);
    status |= runThreeDViewerCameraTest(argc, argv);
    status |= runThreeDViewerLoxLoaderTest(argc, argv);
    status |= runThreeDViewerSceneModelTest(argc, argv);
    status |= runThreeDViewerSceneStatisticsTest(argc, argv);
    status |= runTherionDocumentEditorDraftInsertionTest(argc, argv);
    status |= runTherionSourceTextTest(argc, argv);
    status |= runTherionSourceDocumentTest(argc, argv);
    status |= runTherionSourceLogicalDocumentTest(argc, argv);
    status |= runTherionSourceSnapshotCacheTest(argc, argv);
    status |= runTherionSourceValidatorFixTest(argc, argv);
    status |= runTherionSourceValidatorProjectionTest(argc, argv);
    status |= runTherionTokenRulesTest(argc, argv);
    return status;
}
