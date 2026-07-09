#include "MapEditorTab.h"

#include "MapEditorSceneLifecycleController.h"
#include "MapEditorSceneRefreshController.h"
#include "../TextEditorTab.h"

namespace TherionStudio
{
MapEditorSceneLifecycleContext MapEditorTab::sceneLifecycleContext() const
{
    auto *self = const_cast<MapEditorTab *>(this);
    return MapEditorSceneLifecycleContext{
        .scene = &self->mapScene_,
        .view = self->mapView_,
        .itemsByLine = &self->mapItemsByLine_,
        .vertexItemsByKey = &self->mapVertexItemsByKey_,
        .draftGeometryItems = &self->draftGeometryItems_,
        .backgroundImageItems = &self->backgroundImageItems_,
        .interactiveDrawStrokeActive = &self->interactiveDrawState_.strokeActive_,
        .interactiveDrawPreviewPath = &self->interactiveDrawState_.previewPath_,
        .interactiveDrawPreviewMarkers = &self->interactiveDrawState_.previewMarkers_,
        .updatingSelection = &self->updatingSelection_,
        .nextDraftGeometryId = &self->nextDraftGeometryId_,
        .selectedBackgroundLayerIndex = &self->selectedBackgroundLayerIndex_,
        .autoFitEnabled = &self->autoFitEnabled_,
        .zoomFactor = &self->zoomFactor_,
        .refreshBackgroundLayerControls = [self]() {
            self->refreshBackgroundLayerControls();
        },
        .applyBackgroundLayerStackingOrder = [self]() {
            self->applyBackgroundLayerStackingOrder();
        },
        .setSelectedBackgroundLayerIndexInternal = [self](int index) {
            self->setSelectedBackgroundLayerIndexInternal(index);
        },
        .reprojectMetadataBackgroundLayersForCurrentDocument = [self]() {
            self->reprojectMetadataBackgroundLayersForCurrentDocument();
        },
        .mapBackgroundFitBounds = [self]() {
            return self->mapBackgroundFitBounds();
        },
        .updateCommandSurfaceState = [self]() {
            self->updateCommandSurfaceState();
        },
        .zoomPercent = [self]() {
            return self->zoomPercent();
        },
        .emitZoomStatusChanged = [self](int zoomPercent) {
            emit self->zoomStatusChanged(zoomPercent);
        },
    };
}

MapEditorSceneRefreshContext MapEditorTab::sceneRefreshContext()
{
    const QPointer<MapEditorTab> self(this);
    return MapEditorSceneRefreshContext{
        .sceneParent = this,
        .selectionConnectionContext = this,
        .scene = &mapScene_,
        .view = mapView_,
        .undoStack = undoStack_,
        .itemsByLine = &mapItemsByLine_,
        .vertexItemsByKey = &mapVertexItemsByKey_,
        .commandApplyInProgress = &mapCommandApplyInProgress_,
        .sceneRefreshPending = &mapSceneRefreshPending_,
        .autoFitEnabled = &autoFitEnabled_,
        .fitBackgroundRequested = &fitBackgroundRequested_,
        .lineVertexSelectionRestoreGeneration = &lineVertexSelectionRestoreGeneration_,
        .orientationApplicabilityByCommand = &orientationApplicabilityByCommand_,
        .logicalSource = logicalSourceContext(),
        .documentText = [self]() {
            return self != nullptr && self->textEditor_ != nullptr ? self->textEditor_->text() : QString();
        },
        .parsedLinesForCurrentDocument = [self]() {
            return self != nullptr ? self->parsedLinesForCurrentDocument() : QVector<TherionParsedLine>{};
        },
        .currentLineNumber = [self]() {
            return self != nullptr && self->textEditor_ != nullptr ? self->textEditor_->currentLineNumber() : 0;
        },
        .currentColumnNumber = [self]() {
            return self != nullptr && self->textEditor_ != nullptr ? self->textEditor_->currentColumnNumber() : 1;
        },
        .sceneRefreshSelectionLineNumber = [self]() {
            return self != nullptr ? self->selectionSyncState_.sceneRefreshSelectionLineNumber_ : 0;
        },
        .sceneRefreshSelectionVertexIndex = [self]() {
            return self != nullptr ? self->selectionSyncState_.sceneRefreshSelectionVertexIndex_ : -1;
        },
        .sceneRefreshSelectionKind = [self]() {
            return self != nullptr ? self->selectionSyncState_.sceneRefreshSelectionGeometryKind_ : QString();
        },
        .filePath = [self]() {
            return self != nullptr ? self->filePath() : QString();
        },
        .handleSceneSelectionChanged = [self]() {
            if (self != nullptr) {
                self->handleMapSceneSelectionChanged();
            }
        },
        .updateCommandSurfaceState = [self]() {
            if (self != nullptr) {
                self->updateCommandSurfaceState();
            }
        },
        .clearMapScene = [self]() {
            if (self != nullptr) {
                self->clearMapScene();
            }
        },
        .mapSourceBoundsForCurrentDocument = [self]() {
            return self != nullptr ? self->mapSourceBoundsForCurrentDocument() : QRectF();
        },
        .mapBackgroundFitBounds = [self]() {
            return self != nullptr ? self->mapBackgroundFitBounds() : QRectF();
        },
        .recordCardMove = [self](int lineNumber, const QPointF &oldPosition, const QPointF &newPosition) {
            if (self != nullptr) {
                self->recordCardMove(lineNumber, oldPosition, newPosition);
            }
        },
        .recordCardVisibility = [self](int lineNumber, bool oldVisible, bool newVisible) {
            if (self != nullptr) {
                self->recordCardVisibility(lineNumber, oldVisible, newVisible);
            }
        },
        .recordPointGeometryMove = [self](int lineNumber, const QPointF &oldPoint, const QPointF &newPoint) {
            if (self != nullptr) {
                self->recordPointGeometryMove(lineNumber, oldPoint, newPoint);
            }
        },
        .recordLineAreaVertexMove = [self](int lineNumber,
                                           const QString &kind,
                                           int vertexIndex,
                                           const QPointF &oldPoint,
                                           const QPointF &newPoint) {
            if (self != nullptr) {
                self->recordLineAreaVertexMove(lineNumber, kind, vertexIndex, oldPoint, newPoint);
            }
        },
        .recordPointOrientationHandleChange = [self](int lineNumber, qreal orientationDegrees) {
            if (self != nullptr) {
                self->recordPointOrientationHandleChange(lineNumber, orientationDegrees);
            }
        },
        .recordLinePointLeftHandleChange = [self](int lineNumber, int sourceVertexIndex, qreal orientationDegrees, qreal leftSize) {
            if (self != nullptr) {
                self->recordLinePointLeftHandleChange(lineNumber, sourceVertexIndex, orientationDegrees, leftSize);
            }
        },
        .restoreBackgroundImageItems = [self]() {
            if (self != nullptr) {
                self->restoreBackgroundImageItems();
            }
        },
        .reprojectMetadataBackgroundLayersForCurrentDocument = [self]() {
            if (self != nullptr) {
                self->reprojectMetadataBackgroundLayersForCurrentDocument();
            }
        },
        .restoreDraftGeometryItems = [self]() {
            if (self != nullptr) {
                self->restoreDraftGeometryItems();
            }
        },
        .restorePointSelection = [self](int lineNumber) {
            if (self != nullptr) {
                self->restorePointSelection(lineNumber);
            }
        },
        .restoreLineAnchorSelection = [self](int lineNumber, int sourceVertexIndex) {
            if (self != nullptr) {
                self->restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
            }
        },
        .selectMapLine = [self](int lineNumber, bool centerOnSelection) {
            if (self != nullptr) {
                self->selectMapLine(lineNumber, centerOnSelection);
            }
        },
        .applyInspectorObjectVisibility = [self]() {
            if (self != nullptr) {
                self->applyInspectorObjectVisibility();
            }
        },
        .updateGeometrySelectionPresentation = [self]() {
            if (self != nullptr) {
                self->updateGeometrySelectionPresentation();
            }
        },
        .fitMapToView = [self](bool includeBackgroundImages) {
            if (self != nullptr) {
                self->fitMapToView(includeBackgroundImages);
            }
        },
        .syncZoomFactorFromView = [self]() {
            if (self != nullptr) {
                self->syncZoomFactorFromView();
            }
        },
        .updateInteractiveDrawPreview = [self]() {
            if (self != nullptr) {
                self->updateInteractiveDrawPreview();
            }
        },
        .refreshStatus = [self]() {
            if (self != nullptr) {
                self->refreshStatus();
            }
        },
        .refreshObjectDetailsPanel = [self]() {
            if (self != nullptr) {
                self->refreshObjectDetailsPanel();
            }
        },
        .updateHelpPanel = [self]() {
            if (self != nullptr) {
                self->updateHelpPanel();
            }
        },
    };
}
}
