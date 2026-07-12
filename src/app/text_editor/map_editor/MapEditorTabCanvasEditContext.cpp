#include "MapEditorTab.h"

#include "MapEditorCanvasEditContext.h"
#include "MapEditorCanvasEditController.h"
#include "../TextEditorTab.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>
#include <QTransform>
#include <QWidget>

namespace TherionStudio
{
namespace
{
struct MapEditorViewportSnapshot
{
    QPointer<QGraphicsScene> scene;
    QPointer<QGraphicsView> view;
    QRectF sceneRect;
    QTransform transform;
    int horizontalScrollValue = 0;
    int verticalScrollValue = 0;

    bool valid() const
    {
        return scene != nullptr && view != nullptr && view->scene() == scene;
    }
};

MapEditorViewportSnapshot captureMapEditorViewportSnapshot(QGraphicsScene *scene, QGraphicsView *view)
{
    MapEditorViewportSnapshot snapshot;
    if (scene == nullptr || view == nullptr || view->scene() != scene) {
        return snapshot;
    }

    snapshot.scene = scene;
    snapshot.view = view;
    snapshot.sceneRect = scene->sceneRect();
    snapshot.transform = view->transform();
    if (QScrollBar *horizontalScrollBar = view->horizontalScrollBar(); horizontalScrollBar != nullptr) {
        snapshot.horizontalScrollValue = horizontalScrollBar->value();
    }
    if (QScrollBar *verticalScrollBar = view->verticalScrollBar(); verticalScrollBar != nullptr) {
        snapshot.verticalScrollValue = verticalScrollBar->value();
    }
    return snapshot;
}

void restoreMapEditorViewportSnapshot(const MapEditorViewportSnapshot &snapshot)
{
    if (!snapshot.valid()) {
        return;
    }

    // This runs after every deferred projection and selection update belonging
    // to the source transaction. Restoring the original scene rect first keeps
    // the raw scrollbar values in the same scene-coordinate system.
    snapshot.scene->setSceneRect(snapshot.sceneRect);
    snapshot.view->setTransform(snapshot.transform);
    if (QScrollBar *horizontalScrollBar = snapshot.view->horizontalScrollBar(); horizontalScrollBar != nullptr) {
        horizontalScrollBar->setValue(qBound(horizontalScrollBar->minimum(),
                                             snapshot.horizontalScrollValue,
                                             horizontalScrollBar->maximum()));
    }
    if (QScrollBar *verticalScrollBar = snapshot.view->verticalScrollBar(); verticalScrollBar != nullptr) {
        verticalScrollBar->setValue(qBound(verticalScrollBar->minimum(),
                                           snapshot.verticalScrollValue,
                                           verticalScrollBar->maximum()));
    }
}
}

MapEditorCanvasEditContext MapEditorTab::canvasEditContext()
{
    return MapEditorCanvasEditContext{
        .callbackContext = this,
        .textEditor = textEditor_,
        .scene = mapScene_,
        .undoStack = undoStack_,
        .itemsByLine = &mapItemsByLine_,
        .vertexItemsByKey = &mapVertexItemsByKey_,
        .draftGeometryItems = &draftGeometryItems_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .commandApplyInProgress = &mapCommandApplyInProgress_,
        .lineVertexSelectionRestoreGeneration = &lineVertexSelectionRestoreGeneration_,
        .updatingSelection = &updatingSelection_,
        .pendingClickSelection = &selectionSyncState_.pendingClickSelection_,
        .pendingClickLineNumber = &selectionSyncState_.pendingClickLineNumber_,
        .pendingClickSourceVertexIndex = &selectionSyncState_.pendingClickSourceVertexIndex_,
        .pendingClickGeometryKind = &selectionSyncState_.pendingClickGeometryKind_,
        .selectedObjectLineNumber = &objectSelectionState_.selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &objectSelectionState_.selectedObjectVertexIndex_,
        .selectedObjectKind = &objectSelectionState_.selectedObjectKind_,
        .selectedObjectCoordinate = &objectSelectionState_.selectedObjectCoordinate_,
        .nextDraftGeometryId = &nextDraftGeometryId_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .markSourceChangeOriginatedFromMapTransaction = [this]() {
            preserveNextSourceDrivenMapRefresh_ = true;
            preserveMapUndoForSourceRevision_ = textEditor_ != nullptr ? textEditor_->documentRevision() : 0;
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .flushPendingSceneRefreshAfterCommand = [this]() {
            flushPendingMapSceneRefreshAfterCommand();
        },
        .discardPendingSceneRefreshAfterCommand = [this]() {
            mapSceneRefreshPending_ = false;
        },
        .sourcePointFromScenePosition = [this](const QPointF &scenePosition) {
            return sourcePointFromScenePosition(scenePosition);
        },
        .updateGeometrySelectionPresentation = [this]() {
            updateGeometrySelectionPresentation();
        },
        .updateCommandSurfaceState = [this]() {
            updateCommandSurfaceState();
        },
        .updateHelpPanel = [this]() {
            updateHelpPanel();
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
        .mapPreviewBounds = [this]() {
            return mapPreviewBounds();
        },
        .mapSourceBoundsForCurrentDocument = [this]() {
            return mapSourceBoundsForCurrentDocument();
        },
        .logicalSource = logicalSourceContext(),
        .restorePointSelectionLater = [this](int lineNumber) {
            restorePointSelection(lineNumber);
        },
        .restoreLineAnchorSelectionLater = [this](int lineNumber, int sourceVertexIndex) {
            restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
        },
        .beginLineExtensionFromSelection = [this](int lineNumber, int sourceVertexIndex, bool prepend) {
            return beginLineExtensionFromSelection(lineNumber, sourceVertexIndex, prepend);
        },
        .recordDraftMove = [this](QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition) {
            recordDraftMove(item, oldPosition, newPosition);
        },
        .recordDraftVisibility = [this](QGraphicsRectItem *item, bool oldVisible, bool newVisible) {
            recordDraftVisibility(item, oldVisible, newVisible);
        },
    };
}

void MapEditorTab::recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition)
{
    MapEditorCanvasEditController(canvasEditContext()).recordCardMove(lineNumber, oldPosition, newPosition);
}

void MapEditorTab::recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible)
{
    MapEditorCanvasEditController(canvasEditContext()).recordCardVisibility(lineNumber, oldVisible, newVisible);
}

void MapEditorTab::recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint)
{
    MapEditorCanvasEditController(canvasEditContext()).recordPointGeometryMove(lineNumber, oldPoint, newPoint);
}

void MapEditorTab::recordLineAreaVertexMove(int lineNumber,
                                            const QString &kind,
                                            int vertexIndex,
                                            const QPointF &oldPoint,
                                            const QPointF &newPoint)
{
    MapEditorCanvasEditController(canvasEditContext()).recordLineAreaVertexMove(lineNumber, kind, vertexIndex, oldPoint, newPoint);
}

void MapEditorTab::recordPointOrientationHandleChange(int lineNumber, qreal orientationDegrees)
{
    MapEditorCanvasEditController(canvasEditContext()).recordPointOrientationHandleChange(lineNumber, orientationDegrees);
}

void MapEditorTab::recordLinePointLeftHandleChange(int lineNumber,
                                                   int sourceVertexIndex,
                                                   qreal orientationDegrees,
                                                   qreal leftSize)
{
    MapEditorCanvasEditController(canvasEditContext()).recordLinePointLeftHandleChange(lineNumber,
                                                                                       sourceVertexIndex,
                                                                                       orientationDegrees,
                                                                                       leftSize);
}

void MapEditorTab::restorePointSelection(int lineNumber)
{
    MapEditorCanvasEditController(canvasEditContext()).restorePointSelection(lineNumber);
}

void MapEditorTab::restoreLineAnchorSelection(int lineNumber, int sourceVertexIndex)
{
    MapEditorCanvasEditController(canvasEditContext()).restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
}

TextEditorSourceTransactionResult MapEditorTab::recordSourceTextSnapshot(const QString &label,
                                                                         const QString &beforeText,
                                                                         const QString &afterText,
                                                                         int insertedLineNumber)
{
    return MapEditorCanvasEditController(canvasEditContext()).recordSourceTextSnapshot(label,
                                                                                      beforeText,
                                                                                      afterText,
                                                                                      insertedLineNumber);
}

TextEditorSourceTransactionResult MapEditorTab::applySourceTextChangeWithSnapshot(const QString &label,
                                                                                  const QString &beforeText,
                                                                                  const QString &afterText,
                                                                                  int insertedLineNumber,
                                                                                  std::function<void()> selectionRestoreHook)
{
    MapEditorCanvasEditController controller(canvasEditContext());
    if (selectionRestoreHook) {
        return controller.applySourceTextChangeWithSnapshot(label,
                                                            beforeText,
                                                            afterText,
                                                            insertedLineNumber,
                                                            TextEditorSourceSelectionRestorePolicy::CustomHook,
                                                            std::move(selectionRestoreHook));
    }

    return controller.applySourceTextChangeWithSnapshot(label, beforeText, afterText, insertedLineNumber);
}

TextEditorSourceTransactionResult MapEditorTab::applySourceTextChangeWithSnapshotDeferredProjection(
    const QString &label,
    const QString &beforeText,
    const QString &afterText,
    int insertedLineNumber,
    std::function<void()> afterProjectionHook)
{
    // Source rewriting may emit cursor changes after the command guard has
    // unwound. Keep the map/text selection bridge from treating those internal
    // signals as user navigation until the corresponding preserved-viewport
    // scene refresh has completed.
    mapViewportPreservationInProgress_ = true;
    const QPointer<QGraphicsView> preservedView(mapView_);
    const MapEditorViewportSnapshot viewportSnapshot = captureMapEditorViewportSnapshot(mapScene_, preservedView);
    const QPointer<QWidget> preservedViewport = preservedView != nullptr ? preservedView->viewport() : nullptr;
    const bool viewportUpdatesWereEnabled = preservedViewport != nullptr && preservedViewport->updatesEnabled();
    if (viewportUpdatesWereEnabled) {
        preservedViewport->setUpdatesEnabled(false);
    }
    auto deferredProjectionRefresh = [guarded = QPointer<MapEditorTab>(this),
                                      viewportSnapshot,
                                      preservedViewport,
                                      viewportUpdatesWereEnabled,
                                      afterProjectionHook = std::move(afterProjectionHook)]() mutable {
        QTimer::singleShot(0, guarded, [guarded,
                                        viewportSnapshot,
                                        preservedViewport,
                                        viewportUpdatesWereEnabled,
                                        afterProjectionHook = std::move(afterProjectionHook)]() mutable {
            if (guarded == nullptr) {
                return;
            }
            guarded->flushPendingMapSceneRefreshAfterCommand();
            if (afterProjectionHook) {
                afterProjectionHook();
            }
            restoreMapEditorViewportSnapshot(viewportSnapshot);
            if (viewportUpdatesWereEnabled && preservedViewport != nullptr) {
                preservedViewport->setUpdatesEnabled(true);
                preservedViewport->update();
            }
            guarded->mapViewportPreservationInProgress_ = false;
        });
    };
    const TextEditorSourceTransactionResult result = MapEditorCanvasEditController(canvasEditContext())
        .applySourceTextChangeWithSnapshotDeferredProjection(label,
                                                             beforeText,
                                                             afterText,
                                                             insertedLineNumber,
                                                             std::move(deferredProjectionRefresh));
    if (result != TextEditorSourceTransactionResult::Applied) {
        if (viewportUpdatesWereEnabled && preservedViewport != nullptr) {
            preservedViewport->setUpdatesEnabled(true);
            preservedViewport->update();
        }
        mapViewportPreservationInProgress_ = false;
    }
    return result;
}

bool MapEditorTab::insertLineVertexFromSelection(bool before)
{
    return MapEditorCanvasEditController(canvasEditContext())
        .insertLineVertexFromSelection(before ? MapEditorLineVertexInsertPlacement::Before
                                              : MapEditorLineVertexInsertPlacement::After);
}

bool MapEditorTab::insertLineVertexAtSelectionCoordinate()
{
    return MapEditorCanvasEditController(canvasEditContext()).insertLineVertexAtSelectionCoordinate();
}

bool MapEditorTab::splitLineAtSelection()
{
    return MapEditorCanvasEditController(canvasEditContext()).splitLineAtSelection();
}

bool MapEditorTab::removeLineVertexFromSelection()
{
    return MapEditorCanvasEditController(canvasEditContext()).removeLineVertexFromSelection();
}

bool MapEditorTab::toggleLineVertexSmoothFromSelection()
{
    return MapEditorCanvasEditController(canvasEditContext()).toggleLineVertexSmoothFromSelection();
}

bool MapEditorTab::setLineVertexSmoothForSelection(bool smooth)
{
    return MapEditorCanvasEditController(canvasEditContext()).setLineVertexSmoothForSelection(smooth);
}

bool MapEditorTab::setLineVertexControlHandleForSelection(bool incoming, bool enabled)
{
    return MapEditorCanvasEditController(canvasEditContext()).setLineVertexControlHandleForSelection(incoming, enabled);
}

QGraphicsRectItem *MapEditorTab::selectedDraftGeometryItem() const
{
    return MapEditorCanvasEditController(const_cast<MapEditorTab *>(this)->canvasEditContext()).selectedDraftGeometryItem();
}

QGraphicsRectItem *MapEditorTab::createDraftGeometryItem(DraftGeometryKind kind)
{
    return MapEditorCanvasEditController(canvasEditContext()).createDraftGeometryItem(kind);
}

void MapEditorTab::addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position)
{
    MapEditorCanvasEditController(canvasEditContext()).addDraftGeometryItem(item, position);
}

void MapEditorTab::removeDraftGeometryItem(QGraphicsRectItem *item)
{
    MapEditorCanvasEditController(canvasEditContext()).removeDraftGeometryItem(item);
}

QVector<QPointF> MapEditorTab::sourceVerticesForDraft(const QGraphicsRectItem *item) const
{
    return MapEditorCanvasEditController(const_cast<MapEditorTab *>(this)->canvasEditContext()).sourceVerticesForDraft(item);
}

QPointF MapEditorTab::previewToSourcePoint(const QPointF &previewPoint, const QRectF &sourceBounds, const QRectF &previewBounds) const
{
    return MapEditorCanvasEditController(const_cast<MapEditorTab *>(this)->canvasEditContext())
        .previewToSourcePoint(previewPoint, sourceBounds, previewBounds);
}

void MapEditorTab::recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    MapEditorCanvasEditController(canvasEditContext()).recordDraftMove(item, oldPosition, newPosition);
}

void MapEditorTab::recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible)
{
    MapEditorCanvasEditController(canvasEditContext()).recordDraftVisibility(item, oldVisible, newVisible);
}

void MapEditorTab::recordDraftCompletion(QGraphicsRectItem *item,
                                         const QString &label,
                                         const QString &beforeText,
                                         const QString &afterText,
                                         int insertedLineNumber)
{
    MapEditorCanvasEditController(canvasEditContext())
        .recordDraftCompletion(item, label, beforeText, afterText, insertedLineNumber);
}
}
