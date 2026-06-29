#include "../src/app/text_editor/map_editor/MapEditorCanvasEditController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>

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
                                             QGraphicsScene *scene = nullptr,
                                             QVector<QGraphicsRectItem *> *draftItems = nullptr)
{
    MapEditorCanvasEditContext context;
    context.callbackContext = tab;
    context.textEditor = tab;
    context.scene = scene;
    context.undoStack = undoStack;
    context.draftGeometryItems = draftItems;
    context.toolbarStatusNote = toolbarStatus;
    context.commandApplyInProgress = commandApplyInProgress;
    context.refreshToolbarSummary = [refreshCount]() {
        ++(*refreshCount);
    };
    context.flushPendingSceneRefreshAfterCommand = [flushCount]() {
        ++(*flushCount);
    };
    return MapEditorCanvasEditController(context);
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
    MapEditorCanvasEditController controller =
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount);

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
    MapEditorCanvasEditController controller =
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount);

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
    if (!expect(flushCount == 1, "Line vertex move should flush pending scene refresh once after the event loop resumes.")) {
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
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount, &scene, &draftItems);

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
    if (runLineVertexMoveUsesSourceEditSnapshotTest() != 0) {
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
