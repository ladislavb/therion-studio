#include "MapEditorTab.h"

#include "MapEditorSelectionContext.h"
#include "MapEditorSelectionController.h"

namespace TherionStudio
{
MapEditorSelectionContext MapEditorTab::selectionContext()
{
    return MapEditorSelectionContext{
        .textEditor = textEditor_,
        .scene = mapScene_,
        .view = mapView_,
        .itemsByLine = &mapItemsByLine_,
        .hiddenObjectLines = &hiddenInspectorObjectLines_,
        .suppressedAutoReselectLineNumbers = &suppressedInspectorAutoReselectLineNumbers_,
        .updatingSelection = &updatingSelection_,
        .autoFitEnabled = &autoFitEnabled_,
        .textNavigationInProgress = &selectionSyncState_.textNavigationInProgress_,
        .lastCursorSyncedLine = &selectionSyncState_.lastCursorSyncedLine_,
        .lastCursorSyncedColumn = &selectionSyncState_.lastCursorSyncedColumn_,
        .pendingClickSelection = &selectionSyncState_.pendingClickSelection_,
        .pendingClickScenePosition = &selectionSyncState_.pendingClickScenePosition_,
        .pendingClickElapsed = &selectionSyncState_.pendingClickElapsed_,
        .pendingClickLineNumber = &selectionSyncState_.pendingClickLineNumber_,
        .pendingClickSourceVertexIndex = &selectionSyncState_.pendingClickSourceVertexIndex_,
        .pendingClickGeometryKind = &selectionSyncState_.pendingClickGeometryKind_,
        .sceneRefreshSelectionLineNumber = &selectionSyncState_.sceneRefreshSelectionLineNumber_,
        .sceneRefreshSelectionVertexIndex = &selectionSyncState_.sceneRefreshSelectionVertexIndex_,
        .sceneRefreshSelectionGeometryKind = &selectionSyncState_.sceneRefreshSelectionGeometryKind_,
        .selectedObjectLineNumber = &objectSelectionState_.selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &objectSelectionState_.selectedObjectVertexIndex_,
        .selectedLineSegmentStartVertexIndex = &objectSelectionState_.selectedLineSegmentStartVertexIndex_,
        .selectedLineSegmentEndVertexIndex = &objectSelectionState_.selectedLineSegmentEndVertexIndex_,
        .selectedObjectKind = &objectSelectionState_.selectedObjectKind_,
        .selectedObjectCoordinate = &objectSelectionState_.selectedObjectCoordinate_,
        .currentLineNumber = [this]() {
            return currentLineNumber();
        },
        .parsedLinesForCurrentDocument = [this]() {
            return parsedLinesForCurrentDocument();
        },
        .logicalCommandsForCurrentDocument = [this]() {
            return logicalCommandsForCurrentDocument();
        },
        .sourcePointFromScenePosition = [this](const QPointF &scenePosition) {
            return sourcePointFromScenePosition(scenePosition);
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
        .clearInspectorObjectSelection = [this]() {
            clearInspectorObjectSelection();
        },
        .syncInspectorObjectSelectionToLineExpanding = [this](int lineNumber) {
            syncInspectorObjectSelectionToLineExpanding(lineNumber, true);
        },
    };
}

void MapEditorTab::handleMapSceneSelectionChanged()
{
    MapEditorSelectionController(selectionContext()).handleMapSceneSelectionChanged();
}

void MapEditorTab::syncMapSelectionFromTextCursor(int lineNumber, int columnNumber)
{
    MapEditorSelectionController(selectionContext()).syncMapSelectionFromTextCursor(lineNumber, columnNumber);
}

void MapEditorTab::updateGeometrySelectionPresentation()
{
    MapEditorSelectionController(selectionContext()).updateGeometrySelectionPresentation();
}

void MapEditorTab::selectMapLine(int lineNumber, bool centerOnSelection)
{
    MapEditorSelectionController(selectionContext()).selectMapLine(lineNumber, centerOnSelection);
}

void MapEditorTab::selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection)
{
    MapEditorSelectionController(selectionContext()).selectMapLines(lineNumbers, centerOnSelection);
}
}
