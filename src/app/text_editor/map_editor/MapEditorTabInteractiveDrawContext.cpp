#include "MapEditorTab.h"

#include "MapEditorInteractiveDrawContext.h"
#include "MapEditorInteractiveDrawController.h"
#include "../TextEditorTab.h"

#include <QScopedValueRollback>

namespace TherionStudio
{
namespace
{
MapEditorInteractiveDrawMode toInteractiveDrawControllerMode(MapEditorTab::InteractiveDrawMode mode)
{
    switch (mode) {
    case MapEditorTab::InteractiveDrawMode::None:
        return MapEditorInteractiveDrawMode::None;
    case MapEditorTab::InteractiveDrawMode::Point:
        return MapEditorInteractiveDrawMode::Point;
    case MapEditorTab::InteractiveDrawMode::Line:
        return MapEditorInteractiveDrawMode::Line;
    case MapEditorTab::InteractiveDrawMode::Freehand:
        return MapEditorInteractiveDrawMode::Freehand;
    case MapEditorTab::InteractiveDrawMode::Area:
        return MapEditorInteractiveDrawMode::Area;
    case MapEditorTab::InteractiveDrawMode::SmartArea:
        return MapEditorInteractiveDrawMode::SmartArea;
    }
    return MapEditorInteractiveDrawMode::None;
}

MapEditorTab::InteractiveDrawMode toTabInteractiveDrawMode(MapEditorInteractiveDrawMode mode)
{
    switch (mode) {
    case MapEditorInteractiveDrawMode::None:
        return MapEditorTab::InteractiveDrawMode::None;
    case MapEditorInteractiveDrawMode::Point:
        return MapEditorTab::InteractiveDrawMode::Point;
    case MapEditorInteractiveDrawMode::Line:
        return MapEditorTab::InteractiveDrawMode::Line;
    case MapEditorInteractiveDrawMode::Freehand:
        return MapEditorTab::InteractiveDrawMode::Freehand;
    case MapEditorInteractiveDrawMode::Area:
        return MapEditorTab::InteractiveDrawMode::Area;
    case MapEditorInteractiveDrawMode::SmartArea:
        return MapEditorTab::InteractiveDrawMode::SmartArea;
    }
    return MapEditorTab::InteractiveDrawMode::None;
}
}

MapEditorInteractiveDrawContext MapEditorTab::interactiveDrawContext()
{
    return MapEditorInteractiveDrawContext{
        .textEditor = textEditor_,
        .scene = mapScene_,
        .view = mapView_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .selectModeActive = &selectModeActive_,
        .commandApplyInProgress = &mapCommandApplyInProgress_,
        .panActive = &mapPanActive_,
        .sourceVertices = &interactiveDrawState_.sourceVertices_,
        .sceneVertices = &interactiveDrawState_.sceneVertices_,
        .lineVertices = &interactiveDrawState_.lineVertices_,
        .strokeActive = &interactiveDrawState_.strokeActive_,
        .anchorPressActive = &interactiveDrawState_.anchorPressActive_,
        .anchorPressScenePoint = &interactiveDrawState_.anchorPressScenePoint_,
        .anchorDragActive = &interactiveDrawState_.anchorDragActive_,
        .anchorDragScenePoint = &interactiveDrawState_.anchorDragScenePoint_,
        .controlDragActive = &interactiveDrawState_.controlDragActive_,
        .hoverActive = &interactiveDrawState_.hoverActive_,
        .hoverScenePoint = &interactiveDrawState_.hoverScenePoint_,
        .hoverSnapActive = &interactiveDrawState_.hoverSnapActive_,
        .hoverSnapScenePoint = &interactiveDrawState_.hoverSnapScenePoint_,
        .hoverSnapGuideScenePoints = &interactiveDrawState_.hoverSnapGuideScenePoints_,
        .previewPath = &interactiveDrawState_.previewPath_,
        .previewMarkers = &interactiveDrawState_.previewMarkers_,
        .drawMode = [this]() {
            return toInteractiveDrawControllerMode(interactiveDrawState_.mode_);
        },
        .setDrawMode = [this](MapEditorInteractiveDrawMode mode) {
            interactiveDrawState_.mode_ = toTabInteractiveDrawMode(mode);
        },
        .translate = [this](const char *text) {
            return tr(text);
        },
        .emitModeStatusChanged = [this]() {
            emit modeStatusChanged();
        },
        .sourcePointFromScenePosition = [this](const QPointF &scenePosition) {
            return sourcePointFromScenePosition(scenePosition);
        },
        .applySourceTextChangeWithSnapshot = [this](const QString &label,
                                                    const QString &beforeText,
                                                    const QString &afterText,
                                                    int insertedLineNumber,
                                                    std::function<void()> selectionRestoreHook) {
            return applySourceTextChangeWithSnapshot(label,
                                                     beforeText,
                                                     afterText,
                                                     insertedLineNumber,
                                                     std::move(selectionRestoreHook));
        },
        .draftObjectOptions = [this](const QString &commandKind) {
            return pendingDraftObjectOptions(commandKind);
        },
        .recordCommittedDraftObjectOptions = [this](const QString &commandKind,
                                                    const TherionDraftObjectOptions &options) {
            recordCommittedPendingDraftObjectOptions(commandKind, options);
        },
        .initialAreaAdjustRectForDraftInsertion = [this]() {
            return initialAreaAdjustRectForDraftInsertion();
        },
        .lineCoordinateRowsForInteractiveDraft = [this]() {
            return lineCoordinateRowsForInteractiveDraft();
        },
        .areaCoordinateRowsForInteractiveDraft = [this]() {
            return areaCoordinateRowsForInteractiveDraft();
        },
        .captureInteractiveLineAnchor = [this](const QPointF &anchorScenePoint,
                                               const std::optional<QPointF> &dragScenePoint) {
            captureInteractiveLineAnchor(anchorScenePoint, dragScenePoint);
            refreshObjectDetailsPanel();
        },
        .previewSmartAreaAt = [this](const QPointF &scenePosition) {
            return previewSmartAreaAt(scenePosition);
        },
        .hasSmartAreaPreview = [this]() {
            return hasSmartAreaPreview();
        },
        .commitSmartAreaPreview = [this]() {
            return commitSmartAreaPreview();
        },
        .hasCompletableInteractiveDrawSession = [this]() {
            return hasCompletableInteractiveDrawSession();
        },
        .clearPendingInsertObject = [this]() {
            clearPendingInsertObject();
        },
        .selectCommittedDraftObject = [this](int lineNumber) {
            if (textEditor_ != nullptr) {
                const QScopedValueRollback<bool> syncGuard(selectionSyncState_.textNavigationInProgress_, true);
                textEditor_->goToLine(lineNumber);
            }
            selectMapLine(lineNumber, false);
            syncInspectorObjectSelectionToLine(lineNumber, false);
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .updateCommandSurfaceState = [this]() {
            updateCommandSurfaceState();
        },
        .updateHelpPanel = [this]() {
            updateHelpPanel();
        },
    };
}

void MapEditorTab::setInteractiveDrawMode(InteractiveDrawMode mode)
{
    MapEditorInteractiveDrawController(interactiveDrawContext()).setInteractiveDrawMode(toInteractiveDrawControllerMode(mode));
}

bool MapEditorTab::handleInteractiveDrawClick(const QPointF &scenePosition)
{
    return MapEditorInteractiveDrawController(interactiveDrawContext()).handleInteractiveDrawClick(scenePosition);
}

bool MapEditorTab::commitInteractiveDrawSession(bool closeLineDraft)
{
    if (interactiveDrawState_.lineExtensionActive_) {
        return commitLineExtensionSession();
    }
    return MapEditorInteractiveDrawController(interactiveDrawContext()).commitInteractiveDrawSession(closeLineDraft);
}

void MapEditorTab::clearInteractiveDrawSession(bool clearMode)
{
    MapEditorInteractiveDrawController(interactiveDrawContext()).clearInteractiveDrawSession(clearMode);
    if (clearMode) {
        interactiveDrawState_.lineExtensionActive_ = false;
        interactiveDrawState_.lineExtensionLineNumber_ = 0;
        interactiveDrawState_.lineExtensionPrepend_ = false;
        clearPendingInsertObject();
        refreshObjectDetailsPanel();
    }
}

void MapEditorTab::updateInteractiveDrawPreview()
{
    MapEditorInteractiveDrawController(interactiveDrawContext()).updateInteractiveDrawPreview();
}

bool MapEditorTab::hasUndoableInteractiveDrawStep() const
{
    const bool lineLikeDraft = interactiveDrawState_.mode_ == InteractiveDrawMode::Line
        || interactiveDrawState_.mode_ == InteractiveDrawMode::Area;
    return lineLikeDraft && !interactiveDrawState_.lineVertices_.isEmpty();
}

bool MapEditorTab::undoInteractiveDrawStep()
{
    if (!hasUndoableInteractiveDrawStep()) {
        return false;
    }

    interactiveDrawState_.lineVertices_.removeLast();
    if (!interactiveDrawState_.lineVertices_.isEmpty()) {
        MapEditorInteractiveLineDraftVertex &tail = interactiveDrawState_.lineVertices_.last();
        tail.outgoingControlScene.reset();
        tail.outgoingControlSource.reset();
    }

    updateInteractiveDrawPreview();
    toolbarStatusNote_ = tr("Vertex removed from current draft (%1 remaining).")
                             .arg(interactiveDrawState_.lineVertices_.size());
    refreshToolbarSummary();
    updateCommandSurfaceState();
    return true;
}

bool MapEditorTab::cancelInteractiveDrawingToSelectMode()
{
    if (interactiveDrawState_.lineExtensionActive_) {
        if (hasCompletableInteractiveDrawSession()) {
            return commitLineExtensionSession();
        }
        clearInteractiveDrawSession(true);
        toolbarStatusNote_ = tr("Selection mode: line extension canceled.");
        refreshToolbarSummary();
        updateCommandSurfaceState();
        updateHelpPanel();
        return true;
    }
    const bool handled = MapEditorInteractiveDrawController(interactiveDrawContext()).cancelInteractiveDrawingToSelectMode();
    if (handled) {
        clearPendingInsertObject();
        refreshObjectDetailsPanel();
    }
    return handled;
}
}
