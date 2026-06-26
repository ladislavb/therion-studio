#include "MapEditorTab.h"

#include "MapEditorViewportInputContext.h"
#include "../TextEditorTab.h"

namespace TherionStudio
{
namespace
{
MapEditorInteractiveDrawMode toViewportInteractiveDrawMode(MapEditorTab::InteractiveDrawMode mode)
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
}

MapEditorViewportInputContext MapEditorTab::viewportInputContext()
{
    return MapEditorViewportInputContext{
        .textEditor = textEditor_,
        .scene = mapScene_,
        .view = mapView_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .touchFriendlyControlsEnabled = &touchFriendlyControlsEnabled_,
        .selectModeActive = &selectModeActive_,
        .autoFitEnabled = &autoFitEnabled_,
        .fitBackgroundRequested = &fitBackgroundRequested_,
        .mapPanActive = &mapPanActive_,
        .mapPanMoved = &mapPanMoved_,
        .mapSpacePanKeyDown = &mapSpacePanKeyDown_,
        .mapControlPanActive = &mapControlPanActive_,
        .mapPanStartPosition = &mapPanStartPosition_,
        .mapPanLastPosition = &mapPanLastPosition_,
        .primaryPointerInteractionActive = &primaryPointerInteractionActive_,
        .touchPanCandidate = &touchPanCandidate_,
        .touchPanActive = &touchPanActive_,
        .touchPanStartPosition = &touchPanStartPosition_,
        .touchPanLastPosition = &touchPanLastPosition_,
        .lastTabletInteractionUtc = &lastTabletInteractionUtc_,
        .nativeZoomGestureActive = &nativeZoomGestureActive_,
        .lastNativeZoomGestureUtc = &lastNativeZoomGestureUtc_,
        .pendingClickSelection = &selectionSyncState_.pendingClickSelection_,
        .pendingClickScenePosition = &selectionSyncState_.pendingClickScenePosition_,
        .pendingClickElapsed = &selectionSyncState_.pendingClickElapsed_,
        .pendingClickLineNumber = &selectionSyncState_.pendingClickLineNumber_,
        .pendingClickSourceVertexIndex = &selectionSyncState_.pendingClickSourceVertexIndex_,
        .pendingClickGeometryKind = &selectionSyncState_.pendingClickGeometryKind_,
        .interactiveDrawSourceVertices = &interactiveDrawState_.sourceVertices_,
        .interactiveDrawSceneVertices = &interactiveDrawState_.sceneVertices_,
        .interactiveDrawLineVertices = &interactiveDrawState_.lineVertices_,
        .interactiveDrawStrokeActive = &interactiveDrawState_.strokeActive_,
        .interactiveDrawAnchorPressActive = &interactiveDrawState_.anchorPressActive_,
        .interactiveDrawAnchorPressScenePoint = &interactiveDrawState_.anchorPressScenePoint_,
        .interactiveDrawAnchorDragActive = &interactiveDrawState_.anchorDragActive_,
        .interactiveDrawAnchorDragScenePoint = &interactiveDrawState_.anchorDragScenePoint_,
        .interactiveDrawControlDragActive = &interactiveDrawState_.controlDragActive_,
        .interactiveDrawControlDragHandle = &interactiveDrawState_.controlDragHandle_,
        .interactiveDrawHoverActive = &interactiveDrawState_.hoverActive_,
        .interactiveDrawHoverScenePoint = &interactiveDrawState_.hoverScenePoint_,
        .interactiveDrawHoverSnapActive = &interactiveDrawState_.hoverSnapActive_,
        .interactiveDrawHoverSnapScenePoint = &interactiveDrawState_.hoverSnapScenePoint_,
        .interactiveDrawHoverSnapGuideScenePoints = &interactiveDrawState_.hoverSnapGuideScenePoints_,
        .drawMode = [this]() {
            return toViewportInteractiveDrawMode(interactiveDrawState_.mode_);
        },
        .translate = [this](const char *text) {
            return tr(text);
        },
        .clearInteractiveDrawSession = [this](bool clearMode) {
            clearInteractiveDrawSession(clearMode);
        },
        .sourcePointFromScenePosition = [this](const QPointF &scenePosition) {
            return sourcePointFromScenePosition(scenePosition);
        },
        .updateInteractiveDrawPreview = [this]() {
            updateInteractiveDrawPreview();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .updateCommandSurfaceState = [this]() {
            updateCommandSurfaceState();
        },
        .syncMapSelectionFromScene = [this]() {
            handleMapSceneSelectionChanged();
        },
        .interactiveLineControlAt = [this](const QPointF &scenePosition, qreal sceneRadius) {
            return interactiveLineControlAt(scenePosition, sceneRadius);
        },
        .setInteractiveLineControlScenePoint = [this](const MapEditorInteractiveLineControlHandleRef &handle,
                                                      const QPointF &scenePoint) {
            return setInteractiveLineControlScenePoint(handle, scenePoint);
        },
        .handleInteractiveDrawClick = [this](const QPointF &scenePosition) {
            return handleInteractiveDrawClick(scenePosition);
        },
        .captureInteractiveLineAnchor = [this](const QPointF &anchorScenePoint,
                                               const std::optional<QPointF> &dragScenePoint) {
            captureInteractiveLineAnchor(anchorScenePoint, dragScenePoint);
            refreshObjectDetailsPanel();
        },
        .commitInteractiveDrawVertices = [this](const QString &geometryKind,
                                                const QVector<QPointF> &vertices,
                                                const QString &successLabel) {
            return commitInteractiveDrawVertices(geometryKind, vertices, successLabel);
        },
        .commitInteractiveDrawSession = [this](bool closeLineDraft) {
            return commitInteractiveDrawSession(closeLineDraft);
        },
        .cancelInteractiveDrawingToSelectMode = [this]() {
            return cancelInteractiveDrawingToSelectMode();
        },
        .updateHelpPanel = [this]() {
            updateHelpPanel();
        },
        .syncZoomFactorFromView = [this]() {
            syncZoomFactorFromView();
        },
        .applyZoomAtViewportPosition = [this](qreal factor, const QPointF &viewportPosition) {
            applyZoomAtViewportPosition(factor, viewportPosition);
        },
        .fitMapToView = [this](bool includeBackgroundImages) {
            fitMapToView(includeBackgroundImages);
        },
        .fitMapToViewAfterViewportResize = [this](bool includeBackgroundImages) {
            fitMapToViewAfterViewportResize(includeBackgroundImages);
        },
        .prepareSelectionContextMenuState = [this](int lineNumber,
                                                   int sourceVertexIndex,
                                                   const QString &geometryKind,
                                                   const QPointF &scenePosition) {
            if (lineNumber <= 0) {
                return;
            }

            objectSelectionState_.selectedObjectLineNumber_ = lineNumber;
            objectSelectionState_.selectedObjectVertexIndex_ = sourceVertexIndex;
            objectSelectionState_.selectedObjectCoordinate_.reset();

            QString normalizedKind = geometryKind.trimmed().toLower();
            if (normalizedKind.startsWith(QStringLiteral("line"))) {
                normalizedKind = QStringLiteral("line");
            } else if (normalizedKind.startsWith(QStringLiteral("area"))) {
                normalizedKind = QStringLiteral("area");
            }
            if (normalizedKind.isEmpty()) {
                const QVector<TherionParsedLine> parsedLines = parsedLinesForCurrentDocument();
                for (const TherionParsedLine &parsedLine : parsedLines) {
                    if (parsedLine.lineNumber == lineNumber) {
                        normalizedKind = objectKindForDirective(parsedLine.directive);
                        break;
                    }
                }
            }
            if (normalizedKind.isEmpty()) {
                normalizedKind = QStringLiteral("object");
            }

            objectSelectionState_.selectedObjectKind_ = normalizedKind;
            if (sourceVertexIndex >= 0) {
                objectSelectionState_.selectedObjectCoordinate_ = sourcePointFromScenePosition(scenePosition);
            } else if (normalizedKind == QStringLiteral("line")) {
                objectSelectionState_.selectedObjectCoordinate_ = sourcePointFromScenePosition(scenePosition);
            }
            refreshObjectDetailsPanel();
            updateCommandSurfaceState();
        },
        .showSelectionContextMenu = [this](const QPoint &globalPosition) {
            showMapSelectionContextMenu(globalPosition);
        },
        .insertLineVertexFromSelection = [this]() {
            return insertLineVertexFromSelection(false);
        },
        .removeLineVertexFromSelection = [this]() {
            return removeLineVertexFromSelection();
        },
        .deleteSelectedObjectFromSelection = [this]() {
            if (textEditor_ == nullptr) {
                return false;
            }
            const QString beforeText = textEditor_->text();
            deleteSelectedObjectFromSelection();
            return textEditor_->text() != beforeText;
        },
        .toggleLineVertexSmoothFromSelection = [this]() {
            return toggleLineVertexSmoothFromSelection();
        },
    };
}
}
