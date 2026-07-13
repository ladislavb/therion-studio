#include "MapEditorTab.h"

#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QLayout>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>

#include "../TextEditorTab.h"

namespace TherionStudio
{
bool MapEditorTab::loadFile(const QString &filePath, QString *errorMessage)
{
    const QString currentFilePath = this->filePath();
    if (!currentFilePath.isEmpty() && QFileInfo(currentFilePath).canonicalFilePath() != QFileInfo(filePath).canonicalFilePath()) {
        clearDraftGeometryItems();
        clearBackgroundImageItems();
    }
    const bool loadedWhileHidden = mapView_ == nullptr
        || mapView_->viewport() == nullptr
        || !isVisible()
        || !mapView_->isVisible()
        || !mapView_->viewport()->isVisible();

    const bool loaded = textEditor_->loadFile(filePath, errorMessage);
    if (!loaded) {
        return false;
    }

    // TextEditorTab emits documentTextChanged while loading, which schedules
    // the normal debounced source-edit refresh. The load workflow performs an
    // explicit scene refresh below, so leaving that timer active would rebuild
    // the scene again after callers have already started interacting with it.
    if (sourceDrivenMapRefreshTimer_ != nullptr) {
        sourceDrivenMapRefreshTimer_->stop();
    }

    resetUndoOwnerState();
    refreshMapScene();
    loadBackgroundLayersFromSession();
    loadBackgroundLayersFromDocumentMetadata();
    fitMapToView();
    rebuildInspectorObjectsTree();
    refreshInspectorBackgroundPanel();
    refreshTitle();
    refreshStatus();
    resetUndoOwnerState();
    mapSceneRefreshWhenVisiblePending_ = loadedWhileHidden;
    return true;
}

bool MapEditorTab::save(QString *errorMessage)
{
    return textEditor_->save(errorMessage);
}

void MapEditorTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
    textEditor_->setProjectRootPath(projectRootPath);
    refreshStatus();
}

void MapEditorTab::showFindBar(bool replaceMode)
{
    textEditor_->showFindBar(replaceMode);
}

void MapEditorTab::showFindBarWithText(const QString &findText,
                                       bool replaceMode,
                                       bool wholeWord,
                                       bool matchCase)
{
    textEditor_->showFindBarWithText(findText, replaceMode, wholeWord, matchCase);
}

void MapEditorTab::hideFindBar()
{
    textEditor_->hideFindBar();
}

void MapEditorTab::goToLine(int lineNumber)
{
    selectionSyncState_.pendingNavigationLineNumber_ = lineNumber > 0 ? lineNumber : 0;
    textEditor_->goToLine(lineNumber);
    applyPendingNavigationSelection(false);
}

QString MapEditorTab::filePath() const
{
    return textEditor_->filePath();
}

QString MapEditorTab::displayName() const
{
    return textEditor_->displayName();
}

bool MapEditorTab::isDirty() const
{
    return textEditor_->isDirty();
}

int MapEditorTab::currentLineNumber() const
{
    return textEditor_->currentLineNumber();
}

QString MapEditorTab::text() const
{
    return textEditor_ != nullptr ? textEditor_->text() : QString();
}

QString MapEditorTab::statusPathText() const
{
    return textEditor_ != nullptr ? textEditor_->statusPathText() : tr("No file open");
}

QString MapEditorTab::statusEncodingText() const
{
    return textEditor_ != nullptr ? textEditor_->statusEncodingText() : tr("UTF-8");
}

QString MapEditorTab::statusModeText() const
{
    return interactiveDrawState_.mode_ == InteractiveDrawMode::None
        ? tr("Map mode: Select")
        : tr("Map mode: Insert");
}

QString MapEditorTab::statusHintText() const
{
    return toolbarStatusNote_;
}

int MapEditorTab::zoomPercent() const
{
    return qRound(zoomFactor_ * 100.0);
}

bool MapEditorTab::isMapPaneDetached() const
{
    return detachedPaneState_.detached_;
}

QString MapEditorTab::mapPaneWindowActionText() const
{
    return detachedPaneState_.detached_ ? tr("Return Map") : tr("Separate Map");
}

QString MapEditorTab::mapPaneWindowActionToolTip() const
{
    return detachedPaneState_.detached_
        ? tr("Return the map pane from the detached window into this tab.")
        : tr("Open the map pane in a separate window (for multi-monitor workflows).");
}

MapEditorTab::WorkspaceMode MapEditorTab::workspaceMode() const
{
    return workspaceMode_;
}

void MapEditorTab::setInlineWorkspaceModeSelectorVisible(bool visible)
{
    detachedPaneState_.inlineWorkspaceModeSelectorVisible_ = visible;
    if (workspaceModeRow_ != nullptr) {
        workspaceModeRow_->setVisible(detachedPaneState_.inlineWorkspaceModeSelectorVisible_);
        workspaceModeRow_->setMaximumHeight(detachedPaneState_.inlineWorkspaceModeSelectorVisible_ ? QWIDGETSIZE_MAX : 0);
        if (!detachedPaneState_.inlineWorkspaceModeSelectorVisible_) {
            workspaceModeRow_->setMinimumHeight(0);
        } else {
            workspaceModeRow_->setMinimumHeight(workspaceModeRow_->sizeHint().height());
        }
    }
    if (QLayout *rootLayout = layout(); rootLayout != nullptr) {
        rootLayout->invalidate();
        rootLayout->activate();
    }
}

void MapEditorTab::setWorkspaceMode(WorkspaceMode mode)
{
    if (workspaceMode_ == mode) {
        refreshWorkspaceModeUi();
        return;
    }

    workspaceMode_ = mode;
    refreshWorkspaceModeUi();
    updateWorkspaceVisibility();
    if (textEditor_ != nullptr) {
        textEditor_->setProjectValidationDiagnostics(workspaceMode_ == WorkspaceMode::Raw
                                                         ? projectValidationDiagnostics_
                                                         : QVector<TherionSourceDiagnostic>{});
    }
    emit workspaceModeChanged(workspaceMode_);
}

void MapEditorTab::refreshWorkspaceModeUi()
{
    if (visualModeButton_ == nullptr || rawModeButton_ == nullptr) {
        return;
    }

    visualModeButton_->setChecked(workspaceMode_ == WorkspaceMode::Visual);
    rawModeButton_->setChecked(workspaceMode_ == WorkspaceMode::Raw);
}

void MapEditorTab::handleTextEditorCurrentLineChanged(int lineNumber)
{
    // A map source transaction moves the text cursor to the inserted or
    // rewritten command.  Treating that internal cursor change as user text
    // navigation would select and center the command, overriding the viewport
    // preserved by the map scene refresh.
    if (mapCommandApplyInProgress_ || mapViewportPreservationInProgress_) {
        emit currentLineChanged(lineNumber);
        return;
    }

    if (selectionSyncState_.pendingNavigationLineNumber_ > 0
        && selectionSyncState_.pendingNavigationLineNumber_ != lineNumber) {
        selectionSyncState_.pendingNavigationLineNumber_ = 0;
    }
    if (!selectionSyncState_.textNavigationInProgress_) {
        selectionSyncState_.sceneRefreshSelectionLineNumber_ = 0;
        selectionSyncState_.sceneRefreshSelectionVertexIndex_ = -1;
        selectionSyncState_.sceneRefreshSelectionGeometryKind_.clear();
        syncMapSelectionFromTextCursor(lineNumber, textEditor_ != nullptr ? textEditor_->currentColumnNumber() : 1);
    }
    syncInspectorObjectSelectionToLineForNavigation(lineNumber);
    emit currentLineChanged(lineNumber);
}

void MapEditorTab::handleTextEditorCursorPositionChanged(int lineNumber, int columnNumber)
{
    if (mapCommandApplyInProgress_
        || mapViewportPreservationInProgress_
        || selectionSyncState_.textNavigationInProgress_) {
        return;
    }

    if (lineNumber == selectionSyncState_.lastCursorSyncedLine_ && columnNumber == selectionSyncState_.lastCursorSyncedColumn_) {
        return;
    }

    selectionSyncState_.sceneRefreshSelectionLineNumber_ = 0;
    selectionSyncState_.sceneRefreshSelectionVertexIndex_ = -1;
    selectionSyncState_.sceneRefreshSelectionGeometryKind_.clear();
    syncMapSelectionFromTextCursor(lineNumber, columnNumber);
}

void MapEditorTab::handleZoomInTriggered()
{
    adjustMapZoom(1.2);
}

void MapEditorTab::handleZoomOutTriggered()
{
    adjustMapZoom(1.0 / 1.2);
}

void MapEditorTab::handleFitTriggered()
{
    fitBackgroundRequested_ = false;
    toolbarStatusNote_ = tr("Fit geometry: centered visible map content.");
    fitMapToView();
    refreshToolbarSummary();
}

void MapEditorTab::updateWorkspaceVisibility()
{
    if (textEditor_ == nullptr) {
        refreshStatus();
        return;
    }

    if (detachedPaneState_.detached_) {
        if (workspaceModeRow_ != nullptr) {
            workspaceModeRow_->setEnabled(false);
            workspaceModeRow_->setToolTip(tr("Map pane is detached: raw editor remains in this tab while visual map stays in the detached window."));
        }
        textEditor_->show();
        if (mapPaneContainer_ != nullptr) {
            // Detached window always hosts the visual map pane, even if main-tab mode is Raw.
            mapPaneContainer_->show();
        }
        if (mapPaneTopSeparator_ != nullptr) {
            mapPaneTopSeparator_->hide();
        }
        refreshStatus();
        return;
    }

    if (workspaceModeRow_ != nullptr) {
        workspaceModeRow_->setEnabled(true);
        workspaceModeRow_->setToolTip(QString());
    }

    const bool visualMode = workspaceMode_ == WorkspaceMode::Visual;
    textEditor_->setVisible(!visualMode);
    if (mapPaneContainer_ != nullptr) {
        mapPaneContainer_->setVisible(visualMode);
    }
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->setVisible(visualMode);
    }

    if (splitter_ != nullptr) {
        if (visualMode) {
            splitter_->setSizes(QList<int>{1, 0});
        } else {
            splitter_->setSizes(QList<int>{0, 1});
        }
    }

    refreshStatus();
}

void MapEditorTab::refreshTitle()
{
    emit titleChanged();
}
}
