#include "../src/app/text_editor/map_editor/MapEditorInteractiveDrawController.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>

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
    const QString filePath = tempDir.path() + QStringLiteral("/interactive-draw-transaction.th2");
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

int runPointInsertStaleTransactionStatusTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir,
                                            "scrap s1 -projection plan\n"
                                            "endscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create interactive-draw transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load interactive-draw transaction test tab.")) {
        return 1;
    }

    QString toolbarStatus;
    bool selectModeActive = true;
    bool commandApplyInProgress = false;
    bool panActive = false;
    QVector<QPointF> sourceVertices;
    QVector<QPointF> sceneVertices;
    QVector<MapEditorInteractiveLineDraftVertex> lineVertices;
    bool strokeActive = false;
    bool anchorPressActive = false;
    QPointF anchorPressScenePoint;
    bool anchorDragActive = false;
    QPointF anchorDragScenePoint;
    bool controlDragActive = false;
    bool hoverActive = false;
    QPointF hoverScenePoint;
    bool hoverSnapActive = false;
    QPointF hoverSnapScenePoint;
    QGraphicsPathItem *previewPath = nullptr;
    QVector<QGraphicsItem *> previewMarkers;

    MapEditorInteractiveDrawMode mode = MapEditorInteractiveDrawMode::None;
    int refreshCount = 0;
    int commandSurfaceUpdateCount = 0;
    int transactionCallCount = 0;
    const QString staleStatus = QStringLiteral("Source transaction skipped: document changed.");

    MapEditorInteractiveDrawContext context;
    context.textEditor = &tab;
    context.toolbarStatusNote = &toolbarStatus;
    context.selectModeActive = &selectModeActive;
    context.commandApplyInProgress = &commandApplyInProgress;
    context.panActive = &panActive;
    context.sourceVertices = &sourceVertices;
    context.sceneVertices = &sceneVertices;
    context.lineVertices = &lineVertices;
    context.strokeActive = &strokeActive;
    context.anchorPressActive = &anchorPressActive;
    context.anchorPressScenePoint = &anchorPressScenePoint;
    context.anchorDragActive = &anchorDragActive;
    context.anchorDragScenePoint = &anchorDragScenePoint;
    context.controlDragActive = &controlDragActive;
    context.hoverActive = &hoverActive;
    context.hoverScenePoint = &hoverScenePoint;
    context.hoverSnapActive = &hoverSnapActive;
    context.hoverSnapScenePoint = &hoverSnapScenePoint;
    context.previewPath = &previewPath;
    context.previewMarkers = &previewMarkers;

    context.drawMode = [&mode]() {
        return mode;
    };
    context.setDrawMode = [&mode](MapEditorInteractiveDrawMode newMode) {
        mode = newMode;
    };
    context.translate = [](const char *text) {
        return QString::fromUtf8(text);
    };
    context.emitModeStatusChanged = []() {};
    context.sourcePointFromScenePosition = [](const QPointF &point) {
        return point;
    };
    context.applySourceTextChangeWithSnapshot = [&](const QString &label,
                                                    const QString &beforeText,
                                                    const QString &afterText,
                                                    int insertedLineNumber,
                                                    std::function<void()> selectionRestoreHook) {
        Q_UNUSED(label);
        Q_UNUSED(beforeText);
        Q_UNUSED(afterText);
        Q_UNUSED(insertedLineNumber);
        Q_UNUSED(selectionRestoreHook);
        ++transactionCallCount;
        toolbarStatus = staleStatus;
        return TextEditorSourceTransactionResult::Stale;
    };
    context.draftObjectOptions = [](const QString &) {
        return TherionDraftObjectOptions{};
    };
    context.initialAreaAdjustRectForDraftInsertion = []() -> std::optional<QRectF> {
        return std::nullopt;
    };
    context.lineCoordinateRowsForInteractiveDraft = []() {
        return QStringList{};
    };
    context.areaCoordinateRowsForInteractiveDraft = []() {
        return QStringList{};
    };
    context.captureInteractiveLineAnchor = [](const QPointF &, const std::optional<QPointF> &) {};
    context.previewSmartAreaAt = [](const QPointF &) {
        return false;
    };
    context.hasSmartAreaPreview = []() {
        return false;
    };
    context.commitSmartAreaPreview = []() {
        return false;
    };
    context.hasCompletableInteractiveDrawSession = []() {
        return false;
    };
    context.refreshToolbarSummary = [&refreshCount]() {
        ++refreshCount;
    };
    context.updateCommandSurfaceState = [&commandSurfaceUpdateCount]() {
        ++commandSurfaceUpdateCount;
    };
    context.updateHelpPanel = []() {};

    MapEditorInteractiveDrawController controller(context);
    controller.setInteractiveDrawMode(MapEditorInteractiveDrawMode::Point);

    const QString beforeText = tab.text();
    const bool handled = controller.handleInteractiveDrawClick(QPointF(10.0, 20.0));
    pumpEvents();

    if (!expect(handled, "Interactive point insert should report handled click.")) {
        return 1;
    }
    if (!expect(transactionCallCount == 1, "Interactive point insert should invoke source transaction callback once.")) {
        return 1;
    }
    if (!expect(tab.text() == beforeText, "Stale transaction path should not change source text.")) {
        return 1;
    }
    if (!expect(toolbarStatus == staleStatus, "Interactive point insert should preserve stale transaction status feedback.")) {
        return 1;
    }
    if (!expect(refreshCount > 0, "Interactive point insert should refresh toolbar summary after stale transaction callback.")) {
        return 1;
    }

    return 0;
}

int runStationPointInsertUsesXviSnapTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir,
                                            "scrap s1 -projection plan\n"
                                            "endscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create interactive-draw snap test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load interactive-draw snap test tab.")) {
        return 1;
    }

    QString toolbarStatus;
    bool selectModeActive = true;
    bool commandApplyInProgress = false;
    bool panActive = false;
    QVector<QPointF> sourceVertices;
    QVector<QPointF> sceneVertices;
    QVector<MapEditorInteractiveLineDraftVertex> lineVertices;
    bool strokeActive = false;
    bool anchorPressActive = false;
    QPointF anchorPressScenePoint;
    bool anchorDragActive = false;
    QPointF anchorDragScenePoint;
    bool controlDragActive = false;
    bool hoverActive = false;
    QPointF hoverScenePoint;
    bool hoverSnapActive = false;
    QPointF hoverSnapScenePoint;
    QGraphicsPathItem *previewPath = nullptr;
    QVector<QGraphicsItem *> previewMarkers;
    MapEditorInteractiveDrawMode mode = MapEditorInteractiveDrawMode::None;
    QString plannedAfterText;
    TherionDraftObjectOptions committedOptions;

    MapEditorInteractiveDrawContext context;
    context.textEditor = &tab;
    context.toolbarStatusNote = &toolbarStatus;
    context.selectModeActive = &selectModeActive;
    context.commandApplyInProgress = &commandApplyInProgress;
    context.panActive = &panActive;
    context.sourceVertices = &sourceVertices;
    context.sceneVertices = &sceneVertices;
    context.lineVertices = &lineVertices;
    context.strokeActive = &strokeActive;
    context.anchorPressActive = &anchorPressActive;
    context.anchorPressScenePoint = &anchorPressScenePoint;
    context.anchorDragActive = &anchorDragActive;
    context.anchorDragScenePoint = &anchorDragScenePoint;
    context.controlDragActive = &controlDragActive;
    context.hoverActive = &hoverActive;
    context.hoverScenePoint = &hoverScenePoint;
    context.hoverSnapActive = &hoverSnapActive;
    context.hoverSnapScenePoint = &hoverSnapScenePoint;
    context.previewPath = &previewPath;
    context.previewMarkers = &previewMarkers;
    context.drawMode = [&mode]() { return mode; };
    context.setDrawMode = [&mode](MapEditorInteractiveDrawMode newMode) { mode = newMode; };
    context.translate = [](const char *text) { return QString::fromUtf8(text); };
    context.emitModeStatusChanged = []() {};
    context.sourcePointFromScenePosition = [](const QPointF &point) { return point; };
    context.xviStationSnapAtScenePosition = [](const QPointF &) -> std::optional<MapEditorXviStationSnapCandidate> {
        return MapEditorXviStationSnapCandidate{QStringLiteral("12@survey"), QPointF(100.0, 200.0), QPointF(0.0, 0.0)};
    };
    context.applySourceTextChangeWithSnapshot = [&](const QString &,
                                                    const QString &,
                                                    const QString &afterText,
                                                    int,
                                                    std::function<void()>) {
        plannedAfterText = afterText;
        return TextEditorSourceTransactionResult::Applied;
    };
    context.draftObjectOptions = [](const QString &) {
        TherionDraftObjectOptions options;
        options.type = QStringLiteral("station");
        options.name = QStringLiteral("unrelated");
        options.nameEnabled = true;
        return options;
    };
    context.recordCommittedDraftObjectOptions = [&](const QString &, const TherionDraftObjectOptions &options) {
        committedOptions = options;
    };
    context.initialAreaAdjustRectForDraftInsertion = []() -> std::optional<QRectF> { return std::nullopt; };
    context.lineCoordinateRowsForInteractiveDraft = []() { return QStringList{}; };
    context.areaCoordinateRowsForInteractiveDraft = []() { return QStringList{}; };
    context.captureInteractiveLineAnchor = [](const QPointF &, const std::optional<QPointF> &) {};
    context.previewSmartAreaAt = [](const QPointF &) { return false; };
    context.hasSmartAreaPreview = []() { return false; };
    context.commitSmartAreaPreview = []() { return false; };
    context.hasCompletableInteractiveDrawSession = []() { return false; };
    context.refreshObjectDetailsPanel = []() {};
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};
    context.updateHelpPanel = []() {};

    MapEditorInteractiveDrawController controller(context);
    controller.setInteractiveDrawMode(MapEditorInteractiveDrawMode::Point);
    if (!expect(controller.handleInteractiveDrawClick(QPointF(10.0, 20.0)),
                "Interactive station insert should handle an XVI snap click.")) {
        return 1;
    }
    if (!expect(plannedAfterText.contains(QStringLiteral("-name 12@survey")),
                "Station point insertion should write the snapped XVI station name.")) {
        return 1;
    }
    if (!expect(plannedAfterText.contains(QStringLiteral("100.0 200.0")),
                "Station point insertion should use the snapped coordinate.")) {
        return 1;
    }
    if (!expect(committedOptions.name == QStringLiteral("12@survey") && committedOptions.nameEnabled,
                "Station point insertion should commit the snapped XVI station name.")) {
        return 1;
    }

    return 0;
}

int runDraftAnchorMovePreservesControlOffsetsTest()
{
    QVector<MapEditorInteractiveLineDraftVertex> vertices;
    MapEditorInteractiveLineDraftVertex vertex;
    vertex.anchorScene = QPointF(10.0, 20.0);
    vertex.anchorSource = QPointF(1.0, 2.0);
    vertex.incomingControlScene = QPointF(7.0, 18.0);
    vertex.incomingControlSource = QPointF(0.7, 1.8);
    vertex.outgoingControlScene = QPointF(14.0, 24.0);
    vertex.outgoingControlSource = QPointF(1.4, 2.4);
    vertices.append(vertex);

    const auto handle = interactiveLineControlAt(vertices, QPointF(11.0, 20.5), 4.0);
    if (!expect(handle.has_value(), "Expected draft anchor hit testing to find a handle.")) {
        return 1;
    }
    if (!expect(handle->kind == MapEditorInteractiveLineControlHandleRef::Kind::Anchor,
                "Expected draft anchor hit testing to return an anchor handle.")) {
        return 1;
    }

    const auto sceneToSource = [](const QPointF &scenePoint) {
        return QPointF(scenePoint.x() / 10.0, scenePoint.y() / 10.0);
    };
    if (!expect(setInteractiveLineControlScenePoint(&vertices,
                                                    handle.value(),
                                                    QPointF(30.0, 45.0),
                                                    sceneToSource),
                "Expected moving a draft anchor to succeed.")) {
        return 1;
    }

    const MapEditorInteractiveLineDraftVertex &moved = vertices.first();
    if (!expect(moved.anchorScene == QPointF(30.0, 45.0)
                    && moved.anchorSource == QPointF(3.0, 4.5),
                "Expected draft anchor move to update anchor scene/source positions.")) {
        return 1;
    }
    if (!expect(moved.incomingControlScene == QPointF(27.0, 43.0)
                    && moved.incomingControlSource == QPointF(2.7, 4.3),
                "Expected draft anchor move to preserve incoming control offset.")) {
        return 1;
    }
    if (!expect(moved.outgoingControlScene == QPointF(34.0, 49.0)
                    && moved.outgoingControlSource == QPointF(3.4, 4.9),
                "Expected draft anchor move to preserve outgoing control offset.")) {
        return 1;
    }

    return 0;
}

int runPointInsertPrefersDeferredProjectionTransactionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir,
                                            "scrap s1 -projection plan\n"
                                            "endscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create interactive-draw deferred transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load interactive-draw deferred transaction test tab.")) {
        return 1;
    }

    QString toolbarStatus;
    bool selectModeActive = true;
    bool commandApplyInProgress = false;
    bool panActive = false;
    QVector<QPointF> sourceVertices;
    QVector<QPointF> sceneVertices;
    QVector<MapEditorInteractiveLineDraftVertex> lineVertices;
    bool strokeActive = false;
    bool anchorPressActive = false;
    QPointF anchorPressScenePoint;
    bool anchorDragActive = false;
    QPointF anchorDragScenePoint;
    bool controlDragActive = false;
    bool hoverActive = false;
    QPointF hoverScenePoint;
    bool hoverSnapActive = false;
    QPointF hoverSnapScenePoint;
    QGraphicsPathItem *previewPath = nullptr;
    QVector<QGraphicsItem *> previewMarkers;

    MapEditorInteractiveDrawMode mode = MapEditorInteractiveDrawMode::None;
    int synchronousTransactionCalls = 0;
    int deferredTransactionCalls = 0;

    MapEditorInteractiveDrawContext context;
    context.textEditor = &tab;
    context.toolbarStatusNote = &toolbarStatus;
    context.selectModeActive = &selectModeActive;
    context.commandApplyInProgress = &commandApplyInProgress;
    context.panActive = &panActive;
    context.sourceVertices = &sourceVertices;
    context.sceneVertices = &sceneVertices;
    context.lineVertices = &lineVertices;
    context.strokeActive = &strokeActive;
    context.anchorPressActive = &anchorPressActive;
    context.anchorPressScenePoint = &anchorPressScenePoint;
    context.anchorDragActive = &anchorDragActive;
    context.anchorDragScenePoint = &anchorDragScenePoint;
    context.controlDragActive = &controlDragActive;
    context.hoverActive = &hoverActive;
    context.hoverScenePoint = &hoverScenePoint;
    context.hoverSnapActive = &hoverSnapActive;
    context.hoverSnapScenePoint = &hoverSnapScenePoint;
    context.previewPath = &previewPath;
    context.previewMarkers = &previewMarkers;

    context.drawMode = [&mode]() {
        return mode;
    };
    context.setDrawMode = [&mode](MapEditorInteractiveDrawMode newMode) {
        mode = newMode;
    };
    context.translate = [](const char *text) {
        return QString::fromUtf8(text);
    };
    context.emitModeStatusChanged = []() {};
    context.sourcePointFromScenePosition = [](const QPointF &point) {
        return point;
    };
    context.applySourceTextChangeWithSnapshot = [&](const QString &,
                                                    const QString &,
                                                    const QString &,
                                                    int,
                                                    std::function<void()>) {
        ++synchronousTransactionCalls;
        return TextEditorSourceTransactionResult::Applied;
    };
    context.applySourceTextChangeWithSnapshotDeferredProjection = [&](const QString &,
                                                                      const QString &,
                                                                      const QString &,
                                                                      int,
                                                                      std::function<void()> afterProjectionHook) {
        ++deferredTransactionCalls;
        if (afterProjectionHook) {
            afterProjectionHook();
        }
        return TextEditorSourceTransactionResult::Applied;
    };
    context.draftObjectOptions = [](const QString &) {
        return TherionDraftObjectOptions{};
    };
    context.initialAreaAdjustRectForDraftInsertion = []() -> std::optional<QRectF> {
        return std::nullopt;
    };
    context.lineCoordinateRowsForInteractiveDraft = []() {
        return QStringList{};
    };
    context.areaCoordinateRowsForInteractiveDraft = []() {
        return QStringList{};
    };
    context.captureInteractiveLineAnchor = [](const QPointF &, const std::optional<QPointF> &) {};
    context.previewSmartAreaAt = [](const QPointF &) {
        return false;
    };
    context.hasSmartAreaPreview = []() {
        return false;
    };
    context.commitSmartAreaPreview = []() {
        return false;
    };
    context.hasCompletableInteractiveDrawSession = []() {
        return false;
    };
    context.refreshObjectDetailsPanel = []() {};
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};
    context.updateHelpPanel = []() {};

    MapEditorInteractiveDrawController controller(context);
    controller.setInteractiveDrawMode(MapEditorInteractiveDrawMode::Point);

    if (!expect(controller.handleInteractiveDrawClick(QPointF(10.0, 20.0)),
                "Interactive point insert should report handled click with deferred projection support.")) {
        return 1;
    }
    if (!expect(deferredTransactionCalls == 1,
                "Interactive point insert should use the deferred projection transaction callback when available.")) {
        return 1;
    }
    if (!expect(synchronousTransactionCalls == 0,
                "Interactive point insert should not fall back to the synchronous transaction callback when deferred projection is available.")) {
        return 1;
    }

    return 0;
}

int runClosedLineCommitDefersSelectionUntilProjectionRefreshTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir,
                                            "scrap s1 -projection plan\n"
                                            "endscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create closed-line deferred transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load closed-line deferred transaction test tab.")) {
        return 1;
    }

    QString toolbarStatus;
    bool selectModeActive = false;
    bool commandApplyInProgress = false;
    bool panActive = false;
    QVector<QPointF> sourceVertices;
    QVector<QPointF> sceneVertices;
    QVector<MapEditorInteractiveLineDraftVertex> lineVertices;
    MapEditorInteractiveLineDraftVertex firstVertex;
    firstVertex.anchorScene = QPointF(0.0, 0.0);
    firstVertex.anchorSource = QPointF(0.0, 0.0);
    lineVertices.append(firstVertex);
    MapEditorInteractiveLineDraftVertex secondVertex;
    secondVertex.anchorScene = QPointF(1.0, 1.0);
    secondVertex.anchorSource = QPointF(1.0, 1.0);
    lineVertices.append(secondVertex);
    bool strokeActive = false;
    bool anchorPressActive = false;
    QPointF anchorPressScenePoint;
    bool anchorDragActive = false;
    QPointF anchorDragScenePoint;
    bool controlDragActive = false;
    bool hoverActive = false;
    QPointF hoverScenePoint;
    bool hoverSnapActive = false;
    QPointF hoverSnapScenePoint;
    QGraphicsPathItem *previewPath = nullptr;
    QVector<QGraphicsItem *> previewMarkers;

    MapEditorInteractiveDrawMode mode = MapEditorInteractiveDrawMode::Line;
    int synchronousTransactionCalls = 0;
    int deferredTransactionCalls = 0;
    int selectedLineNumber = 0;
    std::function<void()> deferredProjectionHook;

    MapEditorInteractiveDrawContext context;
    context.textEditor = &tab;
    context.toolbarStatusNote = &toolbarStatus;
    context.selectModeActive = &selectModeActive;
    context.commandApplyInProgress = &commandApplyInProgress;
    context.panActive = &panActive;
    context.sourceVertices = &sourceVertices;
    context.sceneVertices = &sceneVertices;
    context.lineVertices = &lineVertices;
    context.strokeActive = &strokeActive;
    context.anchorPressActive = &anchorPressActive;
    context.anchorPressScenePoint = &anchorPressScenePoint;
    context.anchorDragActive = &anchorDragActive;
    context.anchorDragScenePoint = &anchorDragScenePoint;
    context.controlDragActive = &controlDragActive;
    context.hoverActive = &hoverActive;
    context.hoverScenePoint = &hoverScenePoint;
    context.hoverSnapActive = &hoverSnapActive;
    context.hoverSnapScenePoint = &hoverSnapScenePoint;
    context.previewPath = &previewPath;
    context.previewMarkers = &previewMarkers;

    context.drawMode = [&mode]() {
        return mode;
    };
    context.setDrawMode = [&mode](MapEditorInteractiveDrawMode newMode) {
        mode = newMode;
    };
    context.translate = [](const char *text) {
        return QString::fromUtf8(text);
    };
    context.emitModeStatusChanged = []() {};
    context.sourcePointFromScenePosition = [](const QPointF &point) {
        return point;
    };
    context.applySourceTextChangeWithSnapshot = [&](const QString &,
                                                    const QString &,
                                                    const QString &,
                                                    int,
                                                    std::function<void()>) {
        ++synchronousTransactionCalls;
        return TextEditorSourceTransactionResult::Applied;
    };
    context.applySourceTextChangeWithSnapshotDeferredProjection = [&](const QString &,
                                                                      const QString &,
                                                                      const QString &,
                                                                      int,
                                                                      std::function<void()> afterProjectionHook) {
        ++deferredTransactionCalls;
        deferredProjectionHook = std::move(afterProjectionHook);
        return TextEditorSourceTransactionResult::Applied;
    };
    context.draftObjectOptions = [](const QString &) {
        return TherionDraftObjectOptions{};
    };
    context.initialAreaAdjustRectForDraftInsertion = []() -> std::optional<QRectF> {
        return std::nullopt;
    };
    context.lineCoordinateRowsForInteractiveDraft = []() {
        return QStringList{QStringLiteral("0 0"), QStringLiteral("1 1")};
    };
    context.areaCoordinateRowsForInteractiveDraft = []() {
        return QStringList{};
    };
    context.captureInteractiveLineAnchor = [](const QPointF &, const std::optional<QPointF> &) {};
    context.previewSmartAreaAt = [](const QPointF &) {
        return false;
    };
    context.hasSmartAreaPreview = []() {
        return false;
    };
    context.commitSmartAreaPreview = []() {
        return false;
    };
    context.hasCompletableInteractiveDrawSession = []() {
        return true;
    };
    context.selectCommittedDraftObject = [&selectedLineNumber](int lineNumber) {
        selectedLineNumber = lineNumber;
    };
    context.refreshObjectDetailsPanel = []() {};
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};
    context.updateHelpPanel = []() {};

    MapEditorInteractiveDrawController controller(context);
    if (!expect(controller.commitInteractiveDrawSession(true),
                "Closed line commit should be handled by the interactive draw controller.")) {
        return 1;
    }
    if (!expect(deferredTransactionCalls == 1 && synchronousTransactionCalls == 0,
                "Closed line commit should use the deferred projection transaction path.")) {
        return 1;
    }
    if (!expect(selectedLineNumber == 0,
                "Closed line commit should not select the inserted object before the deferred projection refresh runs.")) {
        return 1;
    }
    if (!expect(static_cast<bool>(deferredProjectionHook),
                "Closed line commit should provide a post-projection selection hook.")) {
        return 1;
    }

    deferredProjectionHook();
    if (!expect(selectedLineNumber > 0,
                "Closed line commit should select the inserted object after the deferred projection refresh hook runs.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (runPointInsertStaleTransactionStatusTest() != 0) {
        return 1;
    }
    if (runStationPointInsertUsesXviSnapTest() != 0) {
        return 1;
    }
    if (runDraftAnchorMovePreservesControlOffsetsTest() != 0) {
        return 1;
    }
    if (runPointInsertPrefersDeferredProjectionTransactionTest() != 0) {
        return 1;
    }
    if (runClosedLineCommitDefersSelectionUntilProjectionRefreshTest() != 0) {
        return 1;
    }

    return 0;
}
