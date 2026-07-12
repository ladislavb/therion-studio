#include "MapEditorTab.h"
#include "MapEditorUndoArbitrationService.h"

#include <QScopedValueRollback>
#include <QUndoStack>

#include "../TextEditorTab.h"

namespace TherionStudio
{
MapEditorUndoOwner MapEditorTab::nextUndoOwner() const
{
    const MapEditorUndoAvailability availability{
        .hasInteractiveDrawUndo = hasUndoableInteractiveDrawStep(),
        .hasMapUndo = undoStack_ != nullptr && undoStack_->canUndo(),
        .hasTextUndo = textEditor_ != nullptr && textEditor_->canUndo(),
        .hasMapRedo = undoStack_ != nullptr && undoStack_->canRedo(),
        .hasTextRedo = textEditor_ != nullptr && textEditor_->canRedo(),
        .preferredUndoOwner = undoOwnershipState_.preferredUndoOwner,
        .preferredRedoOwner = undoOwnershipState_.preferredRedoOwner};
    return MapEditorUndoArbitrationService::nextUndoOwner(availability);
}

MapEditorUndoOwner MapEditorTab::nextRedoOwner() const
{
    const MapEditorUndoAvailability availability{
        .hasInteractiveDrawUndo = hasUndoableInteractiveDrawStep(),
        .hasMapUndo = undoStack_ != nullptr && undoStack_->canUndo(),
        .hasTextUndo = textEditor_ != nullptr && textEditor_->canUndo(),
        .hasMapRedo = undoStack_ != nullptr && undoStack_->canRedo(),
        .hasTextRedo = textEditor_ != nullptr && textEditor_->canRedo(),
        .preferredUndoOwner = undoOwnershipState_.preferredUndoOwner,
        .preferredRedoOwner = undoOwnershipState_.preferredRedoOwner};
    return MapEditorUndoArbitrationService::nextRedoOwner(availability);
}

bool MapEditorTab::canUndo() const
{
    return nextUndoOwner() != MapEditorUndoOwner::None;
}

bool MapEditorTab::canRedo() const
{
    return nextRedoOwner() != MapEditorUndoOwner::None;
}

MapEditorTab::InteractiveDrawMode MapEditorTab::interactiveDrawMode() const
{
    return interactiveDrawState_.mode_;
}

bool MapEditorTab::canCompleteDraftAction() const
{
    const bool mapReady = mapScene_ != nullptr;
    return mapReady && (selectedDraftGeometryItem() != nullptr || hasCompletableInteractiveDrawSession());
}

void MapEditorTab::handleDocumentTextChangedForUndoOwner()
{
    if (mapCommandApplyInProgress_) {
        return;
    }

    MapEditorUndoArbitrationService::markTextEdited(undoOwnershipState_);
    updateCommandSurfaceState();
}

void MapEditorTab::handleMapUndoStackIndexChanged(int index)
{
    MapEditorUndoArbitrationService::updateMapStackIndex(undoOwnershipState_, index);
    updateCommandSurfaceState();
}

void MapEditorTab::resetUndoOwnerState()
{
    MapEditorUndoArbitrationService::resetOwnership(undoOwnershipState_,
                                                    undoStack_ != nullptr ? undoStack_->index() : 0);
    updateCommandSurfaceState();
}

void MapEditorTab::triggerUndo()
{
    handleUndoTriggered();
}

void MapEditorTab::triggerRedo()
{
    handleRedoTriggered();
}

void MapEditorTab::triggerFormatDocument()
{
    if (textEditor_ != nullptr) {
        textEditor_->formatDocument();
    }
}

void MapEditorTab::triggerZoomIn()
{
    handleZoomInTriggered();
}

void MapEditorTab::triggerZoomOut()
{
    handleZoomOutTriggered();
}

void MapEditorTab::triggerFit()
{
    handleFitTriggered();
}

void MapEditorTab::triggerFitWithBackground()
{
    handleFitWithBackgroundTriggered();
}

void MapEditorTab::triggerSelectMode()
{
    handleSelectModeTriggered();
}

void MapEditorTab::triggerInsertScrap()
{
    handleInsertScrapTriggered();
}

void MapEditorTab::triggerCompleteDraft()
{
    handleCompleteDraftTriggered();
}

void MapEditorTab::triggerAddPoint()
{
    handleAddPointTriggered();
}

void MapEditorTab::triggerAddLine()
{
    handleAddLineTriggered();
}

void MapEditorTab::triggerAddFreehandLine()
{
    handleAddFreehandLineTriggered();
}

void MapEditorTab::triggerAddArea()
{
    handleAddAreaTriggered();
}

void MapEditorTab::triggerSmartArea()
{
    handleSmartAreaTriggered();
}

bool MapEditorTab::isInsertModeActive() const
{
    return interactiveDrawState_.mode_ != InteractiveDrawMode::None;
}

void MapEditorTab::handleUndoTriggered()
{
    const MapEditorUndoExecutionContext context{
        .preferredUndoOwner = undoOwnershipState_.preferredUndoOwner,
        .undoInteractiveDrawStep = [this]() { return undoInteractiveDrawStep(); },
        .canMapUndo = [this]() { return undoStack_ != nullptr && undoStack_->canUndo(); },
        .undoMapCommand = [this]() {
            if (undoStack_ == nullptr) {
                return;
            }
            const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
            undoStack_->undo();
            flushPendingMapSceneRefreshAfterCommand();
        },
        .canTextUndo = [this]() { return textEditor_ != nullptr && textEditor_->canUndo(); },
        .undoTextEdit = [this]() {
            if (textEditor_ != nullptr) {
                textEditor_->triggerUndo();
                MapEditorUndoArbitrationService::markTextUndoApplied(undoOwnershipState_);
            }
        },
        .canMapRedo = {},
        .redoMapCommand = {},
        .canTextRedo = {},
        .redoTextEdit = {}};
    MapEditorUndoArbitrationService::triggerUndo(context);
}

void MapEditorTab::handleRedoTriggered()
{
    const MapEditorUndoExecutionContext context{
        .preferredRedoOwner = undoOwnershipState_.preferredRedoOwner,
        .undoInteractiveDrawStep = {},
        .canMapUndo = {},
        .undoMapCommand = {},
        .canTextUndo = {},
        .undoTextEdit = {},
        .canMapRedo = [this]() { return undoStack_ != nullptr && undoStack_->canRedo(); },
        .redoMapCommand = [this]() {
            if (undoStack_ == nullptr) {
                return;
            }
            const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
            undoStack_->redo();
            flushPendingMapSceneRefreshAfterCommand();
        },
        .canTextRedo = [this]() { return textEditor_ != nullptr && textEditor_->canRedo(); },
        .redoTextEdit = [this]() {
            if (textEditor_ != nullptr) {
                textEditor_->triggerRedo();
                MapEditorUndoArbitrationService::markTextRedoApplied(undoOwnershipState_);
            }
        }};
    MapEditorUndoArbitrationService::triggerRedo(context);
}
}
