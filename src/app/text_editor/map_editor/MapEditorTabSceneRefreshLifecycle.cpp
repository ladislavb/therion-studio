#include "MapEditorTab.h"

#include "MapEditorMagnifierOverlay.h"
#include "MapEditorSceneRefreshController.h"
#include "MapEditorSceneThemePolicy.h"
#include "../TextEditorTab.h"

#include <QEvent>
#include <QGraphicsView>
#include <QTimer>
#include <QWidget>

namespace TherionStudio
{
void MapEditorTab::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        handleApplicationAppearanceChanged();
        break;
    default:
        break;
    }
}

void MapEditorTab::handleApplicationAppearanceChanged()
{
    if (mapView_ != nullptr) {
        mapView_->setBackgroundBrush(mapEditorCanvasViewportBackgroundColor());
        if (QWidget *viewport = mapView_->viewport(); viewport != nullptr) {
            viewport->update();
        }
    }

    if (mapScene_ != nullptr) {
        mapScene_->update();
    }
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->update();
    }
    refreshStatus();
}

void MapEditorTab::buildMapScene()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).buildMapScene();
}

void MapEditorTab::refreshMapScene()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).refreshMapScene();
    applyPendingNavigationSelection(false);
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->update();
    }
}

void MapEditorTab::refreshMapScenePreservingUndoStack()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).refreshMapScenePreservingUndoStack();
    applyPendingNavigationSelection(false);
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->update();
    }
}

void MapEditorTab::flushPendingMapSceneRefreshAfterCommand()
{
    const bool hadPendingRefresh = mapSceneRefreshPending_;
    MapEditorSceneRefreshController(sceneRefreshContext()).flushPendingMapSceneRefreshAfterCommand();
    if (hadPendingRefresh) {
        rebuildInspectorObjectsTree();
        applyPendingNavigationSelection(false);
    }
}

void MapEditorTab::applyPendingNavigationSelection(bool consume)
{
    const int lineNumber = selectionSyncState_.pendingNavigationLineNumber_;
    if (lineNumber <= 0 || textEditor_ == nullptr) {
        return;
    }

    syncMapSelectionFromTextCursor(lineNumber, textEditor_->currentColumnNumber());
    syncInspectorObjectSelectionToLineForNavigation(lineNumber);
    if (consume) {
        selectionSyncState_.pendingNavigationLineNumber_ = 0;
    }
}

void MapEditorTab::scheduleSourceDrivenMapRefresh()
{
    if (mapCommandApplyInProgress_) {
        if (sourceDrivenMapRefreshTimer_ != nullptr) {
            sourceDrivenMapRefreshTimer_->stop();
        }
        refreshMapScene();
        return;
    }

    if (sourceDrivenMapRefreshTimer_ == nullptr) {
        applySourceDrivenMapRefresh();
        return;
    }

    sourceDrivenMapRefreshTimer_->start();
}

void MapEditorTab::applySourceDrivenMapRefresh()
{
    if (mapCommandApplyInProgress_) {
        scheduleSourceDrivenMapRefresh();
        return;
    }

    const bool currentRevisionFromMapTransaction = textEditor_ != nullptr
        && preserveMapUndoForSourceRevision_ > 0
        && textEditor_->documentRevision() == preserveMapUndoForSourceRevision_;
    const bool preserveUndoStack = preserveNextSourceDrivenMapRefresh_
        || currentRevisionFromMapTransaction;
    preserveNextSourceDrivenMapRefresh_ = false;
    if (preserveUndoStack) {
        refreshMapScenePreservingUndoStack();
    } else {
        refreshMapScene();
    }
    rebuildInspectorObjectsTree();
    applyPendingNavigationSelection(true);
    emit sourceDrivenMapRefreshCompleted();
}
}
