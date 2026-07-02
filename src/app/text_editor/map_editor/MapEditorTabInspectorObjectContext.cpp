#include "MapEditorTab.h"

#include "MapEditorInspectorObjectContext.h"
#include "../TextEditorTab.h"

namespace TherionStudio
{
MapEditorInspectorObjectContext MapEditorTab::inspectorObjectContext()
{
    return MapEditorInspectorObjectContext{
        .textEditor = textEditor_,
        .objectsTree = mapObjectsTree_,
        .objectsModel = mapObjectsModel_,
        .scene = mapScene_,
        .hiddenObjectLines = &hiddenInspectorObjectLines_,
        .pressedNameIndex = &inspectorObjectPressedNameIndex_,
        .pressedWasSelected = &inspectorObjectPressedWasSelected_,
        .updatingSelection = &updatingMapInspectorObjectSelection_,
        .autoCollapseExpandScraps = &objectsInspectorAutoCollapseExpandScrapsEnabled_,
        .lastClickedLineNumber = &lastInspectorClickedObjectLineNumber_,
        .suppressedAutoReselectLineNumbers = &suppressedInspectorAutoReselectLineNumbers_,
        .selectedObjectLineNumber = &objectSelectionState_.selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &objectSelectionState_.selectedObjectVertexIndex_,
        .selectedObjectKind = &objectSelectionState_.selectedObjectKind_,
        .selectedObjectCoordinate = &objectSelectionState_.selectedObjectCoordinate_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .filePath = [this]() {
            return filePath();
        },
        .parsedLinesForCurrentDocument = [this]() {
            return parsedLinesForCurrentDocument();
        },
        .logicalCommandsForCurrentDocument = [this]() {
            return logicalCommandsForCurrentDocument();
        },
        .currentLineNumber = [this]() {
            return currentLineNumber();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
        .updateCommandSurfaceState = [this]() {
            updateCommandSurfaceState();
        },
        .updateGeometrySelectionPresentation = [this]() {
            updateGeometrySelectionPresentation();
        },
        .syncMapSelectionFromTextCursor = [this](int lineNumber, int columnNumber) {
            syncMapSelectionFromTextCursor(lineNumber, columnNumber);
        },
        .selectMapLines = [this](const QSet<int> &lineNumbers, bool centerOnSelection) {
            selectMapLines(lineNumbers, centerOnSelection);
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
    };
}
}
