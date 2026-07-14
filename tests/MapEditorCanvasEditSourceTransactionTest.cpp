#include "../src/app/text_editor/map_editor/MapEditorCanvasEditController.h"
#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionDocumentParser.h"
#include "../src/core/TherionSourceLogicalDocument.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QHash>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>

#include <functional>
#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

const MapEditorObjectStyleCatalog &testStyleCatalog()
{
    static const MapEditorObjectStyleCatalog catalog = loadMapEditorObjectStyleCatalog(QString());
    return catalog;
}

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

QString createTestFile(QTemporaryDir &tempDir, const QByteArray &contents)
{
    const QString filePath = tempDir.path() + QStringLiteral("/map-source-transaction.th2");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    file.write(contents);
    file.close();
    return filePath;
}

bool loadTestTab(TextEditorTab *tab, const QString &filePath)
{
    QString errorMessage;
    if (!tab->loadFile(filePath, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << '\n';
        return false;
    }
    tab->resize(640, 480);
    tab->show();
    pumpEvents();
    return true;
}

MapEditorCanvasEditController makeController(TextEditorTab *tab,
                                             QUndoStack *undoStack,
                                             QString *toolbarStatus,
                                             bool *commandApplyInProgress,
                                             int *refreshCount,
                                             int *flushCount,
                                             int *discardCount = nullptr,
                                             QGraphicsScene *scene = nullptr,
                                             QVector<QGraphicsRectItem *> *draftItems = nullptr,
                                             QHash<int, QGraphicsItem *> *itemsByLine = nullptr,
                                             QHash<QString, QGraphicsItem *> *vertexItemsByKey = nullptr,
                                             std::function<QRectF()> mapSourceBoundsForCurrentDocument = {})
{
    MapEditorCanvasEditContext context;
    context.callbackContext = tab;
    context.textEditor = tab;
    context.scene = scene;
    context.undoStack = undoStack;
    context.itemsByLine = itemsByLine;
    context.vertexItemsByKey = vertexItemsByKey;
    context.styleCatalog = &testStyleCatalog();
    context.draftGeometryItems = draftItems;
    context.toolbarStatusNote = toolbarStatus;
    context.commandApplyInProgress = commandApplyInProgress;
    context.refreshToolbarSummary = [refreshCount]() {
        ++(*refreshCount);
    };
    context.flushPendingSceneRefreshAfterCommand = [flushCount]() {
        ++(*flushCount);
    };
    if (discardCount != nullptr) {
        context.discardPendingSceneRefreshAfterCommand = [discardCount]() {
            ++(*discardCount);
        };
    }
    context.logicalSource.logicalCommandsForCurrentDocument = [tab]() {
        TherionSourceDocumentMetadata metadata;
        metadata.sourceType = TherionSourceDocumentType::TherionMap;
        metadata.revisionId = tab != nullptr ? tab->documentRevision() : 0;
        return TherionSourceLogicalDocument::fromText(tab != nullptr ? tab->text() : QString(), metadata).commands();
    };
    context.mapSourceBoundsForCurrentDocument = std::move(mapSourceBoundsForCurrentDocument);
    return MapEditorCanvasEditController(context);
}

int geometryItemCountForLine(const QGraphicsScene &scene, int lineNumber)
{
    int count = 0;
    for (QGraphicsItem *item : scene.items()) {
        if (item != nullptr
            && item->data(kMapItemRole).toInt() == kMapItemGeometryValue
            && item->data(kMapSceneLineNumberRole).toInt() == lineNumber) {
            ++count;
        }
    }
    return count;
}

bool vertexIndexEntriesBelongToSceneLine(const QGraphicsScene &scene,
                                         const QHash<QString, QGraphicsItem *> &vertexItemsByKey,
                                         int lineNumber)
{
    if (vertexItemsByKey.isEmpty()) {
        return false;
    }

    const auto sceneItems = scene.items();
    for (QGraphicsItem *item : vertexItemsByKey) {
        if (item == nullptr
            || !sceneItems.contains(item)
            || item->data(kMapItemRole).toInt() != kMapItemGeometryValue
            || item->data(kMapSceneLineNumberRole).toInt() != lineNumber) {
            return false;
        }
    }
    return true;
}

int runApplySourceTextChangeWithSnapshotTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "scrap s1\nendscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create map source transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load map source transaction test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController controller =
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount);

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("point 1 2 station\n");
    controller.applySourceTextChangeWithSnapshot(QStringLiteral("Insert Map Point"), beforeText, afterText, 3);
    pumpEvents();

    if (!expect(tab.text() == afterText, "applySourceTextChangeWithSnapshot should apply afterText immediately.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "applySourceTextChangeWithSnapshot should push one undo snapshot.")) {
        return 1;
    }
    if (!expect(flushCount == 1, "applySourceTextChangeWithSnapshot should flush pending scene refresh once.")) {
        return 1;
    }
    if (!expect(!commandApplyInProgress, "applySourceTextChangeWithSnapshot should restore commandApplyInProgress.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore source text before map source change.")) {
        return 1;
    }
    if (!expect(toolbarStatus == QStringLiteral("Removed inserted map object at source line 3."),
                "Undo should surface the map source snapshot undo status.")) {
        return 1;
    }
    if (!expect(refreshCount == 1, "Undo status should refresh toolbar summary once.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "Redo should restore source text after map source change.")) {
        return 1;
    }
    if (!expect(toolbarStatus == QStringLiteral("Restored inserted map object at source line 3."),
                "Redo should surface the map source snapshot redo status.")) {
        return 1;
    }
    if (!expect(refreshCount == 2, "Redo status should refresh toolbar summary again.")) {
        return 1;
    }

    return 0;
}

int runRecordSourceTextSnapshotForAlreadyAppliedChangeTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "scrap s1\nendscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create map source snapshot test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load map source snapshot test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    int discardCount = 0;
    MapEditorCanvasEditController controller =
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount, &discardCount);

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("line wall\n  0 0\n  1 1\nendline\n");
    tab.applySourceSnapshotForTransaction(afterText);
    pumpEvents();

    controller.recordSourceTextSnapshot(QStringLiteral("Insert Map Line"), beforeText, afterText, 3);
    if (!expect(undoStack.count() == 1, "recordSourceTextSnapshot should push one undo snapshot.")) {
        return 1;
    }
    if (!expect(flushCount == 1, "recordSourceTextSnapshot should flush pending scene refresh once.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "recordSourceTextSnapshot undo should restore beforeText.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "recordSourceTextSnapshot redo should restore afterText.")) {
        return 1;
    }

    return 0;
}

int runPointGeometryMoveUsesSourceEditSnapshotTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "point 1.0 2.0 station\n");
    if (!expect(!filePath.isEmpty(), "Failed to create map point move test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load map point move test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController controller =
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount);

    const QString beforeText = tab.text();
    const QString afterText = QStringLiteral("point 3.0 4.0 station\n");
    controller.recordPointGeometryMove(1, QPointF(1.0, 2.0), QPointF(3.0, 4.0));
    pumpEvents();

    if (!expect(tab.text() == afterText, "Point geometry move should apply source-edit planned coordinates.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Point geometry move should push one undo command.")) {
        return 1;
    }
    if (!expect(flushCount == 1, "Point geometry move should flush pending scene refresh once.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Point geometry move undo should restore the original source text.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "Point geometry move redo should restore the moved source text.")) {
        return 1;
    }

    return 0;
}

int runPointGeometryMoveUsesPartialRefreshTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir,
                                            "point 1.0 2.0 station\n"
                                            "point 10.0 10.0 station\n");
    if (!expect(!filePath.isEmpty(), "Failed to create map point partial refresh test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load map point partial refresh test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    int discardCount = 0;
    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> itemsByLine;
    QHash<QString, QGraphicsItem *> vertexItemsByKey;
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(tab.text());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    renderMapWorkspaceScene(&scene,
                            filePath,
                            collectMapSceneEntries(parsedLines),
                            features,
                            geometryBoundsForFeatures(features),
                            false,
                            &itemsByLine,
                            &vertexItemsByKey,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {},
                            testStyleCatalog());
    const int geometryItemCountBefore = geometryItemCountForLine(scene, 1);
    if (!expect(geometryItemCountBefore > 0 && itemsByLine.contains(1),
                "Point partial refresh test should start with a rendered geometry item group.")) {
        return 1;
    }

    const QString afterText = QStringLiteral("point 3.0 4.0 station\n"
                                             "point 10.0 10.0 station\n");
    makeController(&tab,
                   &undoStack,
                   &toolbarStatus,
                   &commandApplyInProgress,
                   &refreshCount,
                   &flushCount,
                   &discardCount,
                   &scene,
                   nullptr,
                   &itemsByLine,
                   &vertexItemsByKey,
                   [&tab]() {
                       const QVector<TherionParsedLine> currentParsedLines =
                           TherionDocumentParser::parseTokenLines(tab.text());
                       return geometryBoundsForFeatures(collectGeometryFeatures(currentParsedLines));
                   })
        .recordPointGeometryMove(1, QPointF(1.0, 2.0), QPointF(3.0, 4.0));

    if (!expect(tab.text() == afterText, "Point partial refresh should apply source-edit planned coordinates.")) {
        return 1;
    }
    if (!expect(flushCount == 0, "Point partial refresh should defer scene refresh out of the source transaction.")) {
        return 1;
    }
    pumpEvents();
    if (!expect(flushCount == 0, "Point partial refresh should avoid the full scene refresh when one-item refresh succeeds.")) {
        return 1;
    }
    if (!expect(discardCount == 1, "Point partial refresh should discard the pending full scene refresh.")) {
        return 1;
    }
    if (!expect(geometryItemCountForLine(scene, 1) == geometryItemCountBefore,
                "Point partial refresh should replace the complete geometry item group.")) {
        return 1;
    }
    if (!expect(itemsByLine.contains(1),
                "Point partial refresh should restore the live primary geometry index.")) {
        return 1;
    }

    return 0;
}

int runLineVertexMoveUsesSourceEditSnapshotTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "line wall\n  0.0 0.0\n  1.0 1.0\nendline\n");
    if (!expect(!filePath.isEmpty(), "Failed to create map line vertex move test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load map line vertex move test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    int discardCount = 0;
    MapEditorCanvasEditController controller =
        makeController(&tab,
                       &undoStack,
                       &toolbarStatus,
                       &commandApplyInProgress,
                       &refreshCount,
                       &flushCount,
                       &discardCount);

    const QString beforeText = tab.text();
    const QString afterText = QStringLiteral("line wall\n  0.0 0.0\n  2.0 3.0\nendline\n");
    controller.recordLineAreaVertexMove(1,
                                        QStringLiteral("line"),
                                        1,
                                        QPointF(1.0, 1.0),
                                        QPointF(2.0, 3.0));

    if (!expect(tab.text() == afterText, "Line vertex move should apply source-edit planned coordinates.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Line vertex move should push one undo command.")) {
        return 1;
    }
    if (!expect(flushCount == 0, "Line vertex move should defer scene refresh out of the source transaction.")) {
        return 1;
    }
    pumpEvents();
    if (!expect(flushCount == 0, "Line vertex move should not force a full scene refresh after the event loop resumes.")) {
        return 1;
    }
    if (!expect(discardCount == 1, "Line vertex move should discard the pending full scene refresh after restoring selection.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Line vertex move undo should restore the original source text.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "Line vertex move redo should restore the moved source text.")) {
        return 1;
    }

    return 0;
}

int runSegmentStyledLineVertexMoveUsesPartialRefreshTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir,
                                            "line wall\n"
                                            "  0.0 0.0\n"
                                            "  subtype blocks\n"
                                            "  1.0 1.0\n"
                                            "  4.0 4.0\n"
                                            "endline\n");
    if (!expect(!filePath.isEmpty(), "Failed to create segment-styled line vertex move test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load segment-styled line vertex move test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    int discardCount = 0;
    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> itemsByLine;
    QHash<QString, QGraphicsItem *> vertexItemsByKey;
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(tab.text());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    renderMapWorkspaceScene(&scene,
                            filePath,
                            collectMapSceneEntries(parsedLines),
                            features,
                            geometryBoundsForFeatures(features),
                            false,
                            &itemsByLine,
                            &vertexItemsByKey,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {},
                            testStyleCatalog());
    const int geometryItemCountBefore = geometryItemCountForLine(scene, 1);
    if (!expect(geometryItemCountBefore > 1,
                "Segment-styled line vertex move test should start with a multi-item rendered geometry group.")) {
        return 1;
    }

    MapEditorCanvasEditController controller =
        makeController(&tab,
                       &undoStack,
                       &toolbarStatus,
                       &commandApplyInProgress,
                       &refreshCount,
                       &flushCount,
                       &discardCount,
                       &scene,
                       nullptr,
                       &itemsByLine,
                       &vertexItemsByKey,
                       [&tab]() {
                           const QVector<TherionParsedLine> currentParsedLines =
                               TherionDocumentParser::parseTokenLines(tab.text());
                           return geometryBoundsForFeatures(collectGeometryFeatures(currentParsedLines));
                       });

    const QString afterText = QStringLiteral("line wall\n"
                                             "  0.0 0.0\n"
                                             "  subtype blocks\n"
                                             "  2.0 3.0\n"
                                             "  4.0 4.0\n"
                                             "endline\n");
    controller.recordLineAreaVertexMove(1,
                                        QStringLiteral("line"),
                                        1,
                                        QPointF(1.0, 1.0),
                                        QPointF(2.0, 3.0));

    if (!expect(tab.text() == afterText, "Segment-styled line vertex move should apply source-edit planned coordinates.")) {
        return 1;
    }
    if (!expect(flushCount == 0, "Segment-styled line vertex move should defer scene refresh out of the source transaction.")) {
        return 1;
    }
    pumpEvents();
    if (!expect(flushCount == 0, "Segment-styled line vertex move should avoid the full scene refresh when partial refresh succeeds.")) {
        return 1;
    }
    if (!expect(discardCount == 1, "Segment-styled line vertex move should discard the pending full scene refresh after partial refresh.")) {
        return 1;
    }
    if (!expect(geometryItemCountForLine(scene, 1) == geometryItemCountBefore,
                "Segment-styled line vertex move partial refresh should replace the complete geometry item group.")) {
        return 1;
    }
    if (!expect(itemsByLine.contains(1) && vertexIndexEntriesBelongToSceneLine(scene, vertexItemsByKey, 1),
                "Segment-styled line vertex move partial refresh should restore live primary and vertex indexes.")) {
        return 1;
    }

    return 0;
}

int runSegmentStyledLineVertexMoveFallsBackWhenPrimaryIndexMissingTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir,
                                            "line wall\n"
                                            "  0.0 0.0\n"
                                            "  subtype blocks\n"
                                            "  1.0 1.0\n"
                                            "  4.0 4.0\n"
                                            "endline\n");
    if (!expect(!filePath.isEmpty(), "Failed to create missing-index line vertex move test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load missing-index line vertex move test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    int discardCount = 0;
    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> itemsByLine;
    QHash<QString, QGraphicsItem *> vertexItemsByKey;
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(tab.text());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    renderMapWorkspaceScene(&scene,
                            filePath,
                            collectMapSceneEntries(parsedLines),
                            features,
                            geometryBoundsForFeatures(features),
                            false,
                            &itemsByLine,
                            &vertexItemsByKey,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {},
                            testStyleCatalog());
    const int geometryItemCountBefore = geometryItemCountForLine(scene, 1);
    if (!expect(geometryItemCountBefore > 1 && itemsByLine.contains(1),
                "Missing-index fallback test should start with a complete rendered geometry group.")) {
        return 1;
    }
    itemsByLine.remove(1);

    MapEditorCanvasEditController controller =
        makeController(&tab,
                       &undoStack,
                       &toolbarStatus,
                       &commandApplyInProgress,
                       &refreshCount,
                       &flushCount,
                       &discardCount,
                       &scene,
                       nullptr,
                       &itemsByLine,
                       &vertexItemsByKey,
                       [&tab]() {
                           const QVector<TherionParsedLine> currentParsedLines =
                               TherionDocumentParser::parseTokenLines(tab.text());
                           return geometryBoundsForFeatures(collectGeometryFeatures(currentParsedLines));
                       });

    const QString afterText = QStringLiteral("line wall\n"
                                             "  0.0 0.0\n"
                                             "  subtype blocks\n"
                                             "  2.0 3.0\n"
                                             "  4.0 4.0\n"
                                             "endline\n");
    controller.recordLineAreaVertexMove(1,
                                        QStringLiteral("line"),
                                        1,
                                        QPointF(1.0, 1.0),
                                        QPointF(2.0, 3.0));

    if (!expect(tab.text() == afterText, "Missing-index line vertex move should still apply the source edit.")) {
        return 1;
    }
    if (!expect(flushCount == 0, "Missing-index line vertex move should defer scene refresh out of the source transaction.")) {
        return 1;
    }
    pumpEvents();
    if (!expect(flushCount == 1, "Missing-index line vertex move should fall back to a full scene refresh.")) {
        return 1;
    }
    if (!expect(discardCount == 0, "Missing-index line vertex move should keep the pending full scene refresh.")) {
        return 1;
    }
    if (!expect(geometryItemCountForLine(scene, 1) == geometryItemCountBefore,
                "Missing-index fallback should leave the existing geometry item group for the full refresh path.")) {
        return 1;
    }

    return 0;
}

int runSegmentStyledLineVertexMoveFallsBackWhenBoundsChangeTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir,
                                            "line wall\n"
                                            "  0.0 0.0\n"
                                            "  subtype blocks\n"
                                            "  1.0 1.0\n"
                                            "endline\n");
    if (!expect(!filePath.isEmpty(), "Failed to create bounds-changing line vertex move test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load bounds-changing line vertex move test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    int discardCount = 0;
    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> itemsByLine;
    QHash<QString, QGraphicsItem *> vertexItemsByKey;
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(tab.text());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    renderMapWorkspaceScene(&scene,
                            filePath,
                            collectMapSceneEntries(parsedLines),
                            features,
                            geometryBoundsForFeatures(features),
                            false,
                            &itemsByLine,
                            &vertexItemsByKey,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {},
                            testStyleCatalog());
    const int geometryItemCountBefore = geometryItemCountForLine(scene, 1);
    if (!expect(geometryItemCountBefore > 1,
                "Bounds-changing line vertex move test should start with a multi-item rendered geometry group.")) {
        return 1;
    }

    MapEditorCanvasEditController controller =
        makeController(&tab,
                       &undoStack,
                       &toolbarStatus,
                       &commandApplyInProgress,
                       &refreshCount,
                       &flushCount,
                       &discardCount,
                       &scene,
                       nullptr,
                       &itemsByLine,
                       &vertexItemsByKey,
                       [&tab]() {
                           const QVector<TherionParsedLine> currentParsedLines =
                               TherionDocumentParser::parseTokenLines(tab.text());
                           return geometryBoundsForFeatures(collectGeometryFeatures(currentParsedLines));
                       });

    const QString afterText = QStringLiteral("line wall\n"
                                             "  0.0 0.0\n"
                                             "  subtype blocks\n"
                                             "  20.0 20.0\n"
                                             "endline\n");
    controller.recordLineAreaVertexMove(1,
                                        QStringLiteral("line"),
                                        1,
                                        QPointF(1.0, 1.0),
                                        QPointF(20.0, 20.0));

    if (!expect(tab.text() == afterText, "Bounds-changing line vertex move should apply source-edit planned coordinates.")) {
        return 1;
    }
    if (!expect(flushCount == 0, "Bounds-changing line vertex move should defer scene refresh out of the source transaction.")) {
        return 1;
    }
    pumpEvents();
    if (!expect(flushCount == 1, "Bounds-changing line vertex move should fall back to a full scene refresh.")) {
        return 1;
    }
    if (!expect(discardCount == 0, "Bounds-changing line vertex move should not discard the pending full scene refresh.")) {
        return 1;
    }
    if (!expect(geometryItemCountForLine(scene, 1) == geometryItemCountBefore,
                "Bounds-changing fallback should leave the existing geometry item group for the full refresh path.")) {
        return 1;
    }

    return 0;
}

int runStaleSourceChangeIsSkippedTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "scrap s1\nendscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create stale map source transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load stale map source transaction test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController controller =
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount);

    const QString beforeText = tab.text();
    const QString plannedAfterText = beforeText + QStringLiteral("point 1 2 station\n");
    const QString concurrentText = beforeText + QStringLiteral("# concurrent edit\n");
    tab.applySourceSnapshotForTransaction(concurrentText);
    pumpEvents();

    const TextEditorSourceTransactionResult result =
        controller.applySourceTextChangeWithSnapshot(QStringLiteral("Insert Map Point"),
                                                     beforeText,
                                                     plannedAfterText,
                                                     3);
    pumpEvents();

    if (!expect(result == TextEditorSourceTransactionResult::Stale,
                "Stale map source change should report a stale transaction result.")) {
        return 1;
    }
    if (!expect(tab.text() == concurrentText,
                "Stale map source change should not overwrite newer document text.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 0,
                "Stale map source change should not push an undo snapshot.")) {
        return 1;
    }
    if (!expect(flushCount == 0,
                "Stale map source change should not flush map projections.")) {
        return 1;
    }
    if (!expect(toolbarStatus == QStringLiteral("Map source change skipped: document changed."),
                "Stale map source change should surface the map-specific stale status.")) {
        return 1;
    }
    if (!expect(refreshCount == 1,
                "Stale map source status should refresh toolbar summary once.")) {
        return 1;
    }

    return 0;
}

int runDraftCompletionUsesCentralSnapshotReplayTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "scrap s1\nendscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create map draft completion test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load map draft completion test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QGraphicsScene scene;
    QVector<QGraphicsRectItem *> draftItems;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController controller =
        makeController(&tab,
                       &undoStack,
                       &toolbarStatus,
                       &commandApplyInProgress,
                       &refreshCount,
                       &flushCount,
                       nullptr,
                       &scene,
                       &draftItems);

    auto *draftItem = new MapDraftGeometryItem(1, DraftGeometryKind::Point);
    scene.addItem(draftItem);
    draftItems.append(draftItem);

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("point 1.0 2.0 station\n");
    tab.applySourceSnapshotForTransaction(afterText);
    pumpEvents();

    controller.recordDraftCompletion(draftItem, QStringLiteral("Complete Draft"), beforeText, afterText, 3);
    pumpEvents();

    if (!expect(tab.text() == afterText, "Draft completion record should keep the already-applied source text.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Draft completion should push one undo command.")) {
        return 1;
    }
    if (!expect(flushCount == 1, "Draft completion should flush pending scene refresh once.")) {
        return 1;
    }
    if (!expect(!scene.items().contains(draftItem) && !draftItems.contains(draftItem),
                "Draft completion should detach the completed draft item on initial redo.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Draft completion undo should restore the original source text.")) {
        return 1;
    }
    if (!expect(scene.items().contains(draftItem) && draftItems.contains(draftItem),
                "Draft completion undo should restore the draft item.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "Draft completion redo should restore the completed source text.")) {
        return 1;
    }
    if (!expect(!scene.items().contains(draftItem) && !draftItems.contains(draftItem),
                "Draft completion redo should detach the draft item again.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (runApplySourceTextChangeWithSnapshotTest() != 0) {
        return 1;
    }
    if (runRecordSourceTextSnapshotForAlreadyAppliedChangeTest() != 0) {
        return 1;
    }
    if (runPointGeometryMoveUsesSourceEditSnapshotTest() != 0) {
        return 1;
    }
    if (runPointGeometryMoveUsesPartialRefreshTest() != 0) {
        return 1;
    }
    if (runLineVertexMoveUsesSourceEditSnapshotTest() != 0) {
        return 1;
    }
    if (runSegmentStyledLineVertexMoveUsesPartialRefreshTest() != 0) {
        return 1;
    }
    if (runSegmentStyledLineVertexMoveFallsBackWhenPrimaryIndexMissingTest() != 0) {
        return 1;
    }
    if (runSegmentStyledLineVertexMoveFallsBackWhenBoundsChangeTest() != 0) {
        return 1;
    }
    if (runStaleSourceChangeIsSkippedTest() != 0) {
        return 1;
    }
    if (runDraftCompletionUsesCentralSnapshotReplayTest() != 0) {
        return 1;
    }

    return 0;
}
