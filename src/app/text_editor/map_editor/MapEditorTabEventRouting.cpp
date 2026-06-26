#include "MapEditorTab.h"

#include "../TextEditorTab.h"

#include <QApplication>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTabletEvent>
#include <QTreeView>
#include <QWidget>

#include "MapEditorViewportInputController.h"

namespace TherionStudio
{
namespace
{
bool isTextEditingReceiver(QObject *receiver)
{
    for (QObject *object = receiver; object != nullptr; object = object->parent()) {
        if (qobject_cast<QLineEdit *>(object) != nullptr
            || qobject_cast<QPlainTextEdit *>(object) != nullptr
            || qobject_cast<QTextEdit *>(object) != nullptr) {
            return true;
        }
        if (qobject_cast<QAbstractSpinBox *>(object) != nullptr) {
            return true;
        }
        if (auto *combo = qobject_cast<QComboBox *>(object); combo != nullptr && combo->isEditable()) {
            return true;
        }
    }

    return false;
}

bool isMapSecondaryClickEvent(const QEvent *event, QPoint *globalPosition)
{
    if (event == nullptr || globalPosition == nullptr) {
        return false;
    }

    if (event->type() == QEvent::ContextMenu) {
        const auto *contextMenuEvent = static_cast<const QContextMenuEvent *>(event);
        *globalPosition = contextMenuEvent->globalPos();
        return true;
    }

    if (event->type() == QEvent::GraphicsSceneContextMenu) {
        const auto *sceneContextMenuEvent = static_cast<const QGraphicsSceneContextMenuEvent *>(event);
        *globalPosition = sceneContextMenuEvent->screenPos();
        return true;
    }

    return false;
}

bool globalPositionInsideVisibleMenu(const QPoint &globalPosition)
{
    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget *widget : widgets) {
        auto *menu = qobject_cast<QMenu *>(widget);
        if (menu != nullptr && menu->isVisible() && menu->geometry().contains(globalPosition)) {
            return true;
        }
    }
    return false;
}

bool isDescendantOf(QObject *receiver, QObject *ancestor)
{
    for (QObject *object = receiver; object != nullptr; object = object->parent()) {
        if (object == ancestor) {
            return true;
        }
    }
    return false;
}

bool isCursorOverViewport(const QGraphicsView *view)
{
    if (view == nullptr || view->viewport() == nullptr) {
        return false;
    }

    const QPoint viewportPosition = view->viewport()->mapFromGlobal(QCursor::pos());
    return view->viewport()->rect().contains(viewportPosition);
}
}

bool MapEditorTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && watched == qApp) {
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

    if (event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress
        && textEditor_ != nullptr
        && isDescendantOf(watched, textEditor_)) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Undo)
            && nextUndoOwner() == MapEditorUndoOwner::MapCommand) {
            triggerUndo();
            event->accept();
            return true;
        }
        if (keyEvent->matches(QKeySequence::Redo)
            && nextRedoOwner() == MapEditorUndoOwner::MapCommand) {
            triggerRedo();
            event->accept();
            return true;
        }
    }

    if (watched == objectDetailsUiState_.linePointFlagsEdit_
        && event->type() == QEvent::FocusOut
        && objectDetailsUiState_.linePointFlagsDirty_) {
        applyLinePointFlagsEdits();
    }

    QPoint secondaryClickGlobalPosition;
    if (mapView_ != nullptr
        && mapView_->viewport() != nullptr
        && (watched == qApp || isMapEditorEventReceiver(watched) || mapSelectionContextMenu_ != nullptr)
        && isMapSecondaryClickEvent(event, &secondaryClickGlobalPosition)) {
        if (mapSelectionContextMenu_ != nullptr && globalPositionInsideVisibleMenu(secondaryClickGlobalPosition)) {
            return QWidget::eventFilter(watched, event);
        }
        const QPoint viewportPosition = mapView_->viewport()->mapFromGlobal(secondaryClickGlobalPosition);
        if (mapView_->viewport()->rect().contains(viewportPosition)) {
            MapEditorViewportInputController(viewportInputContext())
                .showContextMenuAtViewportPosition(viewportPosition, secondaryClickGlobalPosition);
            event->accept();
            return true;
        }
    }

    if (mapView_ != nullptr
        && mapScene_ != nullptr
        && event->type() == QEvent::KeyPress
        && isMapEditorEventReceiver(watched)) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const Qt::KeyboardModifiers disallowedModifiers =
            keyEvent->modifiers() & ~(Qt::KeyboardModifier::KeypadModifier);
        const bool deleteKeyNoModifier = disallowedModifiers == Qt::NoModifier;
        const bool deleteKeyPressed =
            (keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Delete)
            && deleteKeyNoModifier;
        if (deleteKeyPressed
            && !isTextEditingReceiver(watched)
            && !mapScene_->selectedItems().isEmpty()) {
            if (const std::optional<bool> keyResult =
                    MapEditorViewportInputController(viewportInputContext()).handleEvent(mapView_, event)) {
                return keyResult.value();
            }
        }
    }

    if (backgroundPivotPickActive_
        && event->type() == QEvent::KeyPress
        && isMapEditorEventReceiver(watched)
        && handleBackgroundPivotPickViewportEvent(event)) {
        return true;
    }

    if ((event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        && mapView_ != nullptr
        && mapView_->viewport() != nullptr
        && isMapEditorEventReceiver(watched)
        && !isTextEditingReceiver(watched)) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Space
            && keyEvent->modifiers() == Qt::NoModifier
            && (watched == mapView_
                || watched == mapView_->viewport()
                || watched == mapScene_
                || isCursorOverViewport(mapView_))) {
            if (!keyEvent->isAutoRepeat()) {
                mapSpacePanKeyDown_ = event->type() == QEvent::KeyPress;
            }
            if (mapSpacePanKeyDown_) {
                mapView_->viewport()->setCursor(Qt::OpenHandCursor);
            } else if (!mapPanActive_) {
                if (selectModeActive_) {
                    mapView_->viewport()->setCursor(Qt::CrossCursor);
                } else {
                    mapView_->viewport()->unsetCursor();
                }
            }
            event->accept();
            return true;
        }
    }

    if (handleMapEditorEscapeKeyEvent(watched, event)) {
        return true;
    }

    if (handleMapEditorDrawingShortcutKeyEvent(watched, event)) {
        return true;
    }

    if (watched == mapInspectorTabs_) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
            updateMapInspectorLeftEdgeGeometry();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (mapObjectsTree_ != nullptr && watched == mapObjectsTree_->viewport()) {
        if (const std::optional<bool> inspectorResult = handleInspectorObjectViewportEvent(event)) {
            return inspectorResult.value();
        }
        return QWidget::eventFilter(watched, event);
    }

    if (mapView_ != nullptr && watched == mapView_->viewport()) {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::Show: {
            const QPoint viewportPosition = mapView_->viewport()->mapFromGlobal(QCursor::pos());
            if (mapView_->viewport()->rect().contains(viewportPosition)) {
                scheduleMagnifierOverlayUpdateFromViewportPosition(viewportPosition);
            }
            if (event->type() == QEvent::Show) {
                updateMagnifierOverlayGeometry();
                if (mapSceneRefreshWhenVisiblePending_) {
                    mapSceneRefreshWhenVisiblePending_ = false;
                    QPointer<MapEditorTab> guarded(this);
                    QTimer::singleShot(0, this, [guarded]() {
                        if (guarded == nullptr) {
                            return;
                        }
                        guarded->refreshMapScenePreservingUndoStack();
                    });
                }
            }
            break;
        }
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            scheduleMagnifierOverlayUpdateFromViewportPosition(static_cast<QMouseEvent *>(event)->pos());
            break;
        case QEvent::TabletMove:
        case QEvent::TabletPress:
        case QEvent::TabletRelease:
            scheduleMagnifierOverlayUpdateFromViewportPosition(static_cast<QTabletEvent *>(event)->position().toPoint());
            break;
        case QEvent::Leave:
            hideMagnifierOverlay();
            break;
        case QEvent::Resize:
            updateMagnifierOverlayGeometry();
            break;
        default:
            break;
        }
        if (handleBackgroundPivotPickViewportEvent(event)) {
            return true;
        }
    }

    if (const std::optional<bool> viewportResult = MapEditorViewportInputController(viewportInputContext()).handleEvent(watched, event)) {
        return viewportResult.value();
    }

    return QWidget::eventFilter(watched, event);
}

bool MapEditorTab::isMapEditorEventReceiver(QObject *receiver) const
{
    if (receiver == nullptr) {
        return false;
    }

    if (auto *widget = qobject_cast<QWidget *>(receiver)) {
        if (widget == this || isAncestorOf(widget)) {
            return true;
        }
        if (isVisible() && window() != nullptr && widget->window() == window()) {
            return true;
        }
        if (detachedPaneState_.window_ != nullptr
            && (widget == detachedPaneState_.window_.data()
                || detachedPaneState_.window_->isAncestorOf(widget))) {
            return true;
        }
    }

    for (QObject *object = receiver; object != nullptr; object = object->parent()) {
        if (object == this
            || object == mapPaneContainer_
            || object == mapView_
            || object == mapScene_
            || object == objectDetailsPanel_
            || object == mapInspectorTabs_) {
            return true;
        }
        if (detachedPaneState_.window_ != nullptr && object == detachedPaneState_.window_.data()) {
            return true;
        }
    }

    return false;
}

bool MapEditorTab::handleMapEditorEscapeKeyEvent(QObject *receiver, QEvent *event)
{
    if (event == nullptr || event->type() != QEvent::KeyPress) {
        return false;
    }
    if (interactiveDrawState_.mode_ == InteractiveDrawMode::None && !interactiveDrawState_.lineExtensionActive_) {
        return false;
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() != Qt::Key_Escape || keyEvent->modifiers() != Qt::NoModifier) {
        return false;
    }
    if (!isMapEditorEventReceiver(receiver)) {
        return false;
    }

    if (!cancelInteractiveDrawingToSelectMode()) {
        return false;
    }

    keyEvent->accept();
    return true;
}

bool MapEditorTab::handleMapEditorDrawingShortcutKeyEvent(QObject *receiver, QEvent *event)
{
    if (event == nullptr || event->type() != QEvent::KeyPress) {
        return false;
    }
    if (!isMapEditorEventReceiver(receiver) || isTextEditingReceiver(receiver)) {
        return false;
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    const Qt::KeyboardModifiers disallowedModifiers =
        keyEvent->modifiers() & ~(Qt::KeyboardModifier::KeypadModifier);
    if (disallowedModifiers != Qt::NoModifier) {
        return false;
    }

    auto acceptHandled = [keyEvent]() {
        keyEvent->accept();
        return true;
    };
    auto toggleCheckBox = [this, acceptHandled](QCheckBox *checkBox, auto toggleHandler) {
        if (checkBox == nullptr || !checkBox->isVisible() || !checkBox->isEnabled()) {
            return false;
        }
        toggleHandler(!checkBox->isChecked());
        return acceptHandled();
    };

    switch (keyEvent->key()) {
    case Qt::Key_P:
        triggerAddPoint();
        return acceptHandled();
    case Qt::Key_L:
        triggerAddLine();
        return acceptHandled();
    case Qt::Key_A:
        triggerAddArea();
        return acceptHandled();
    case Qt::Key_R:
        return toggleCheckBox(objectDetailsUiState_.lineReversedCheck_,
                              [this](bool checked) { handleLineReversedToggled(checked); });
    case Qt::Key_C:
        if (interactiveDrawState_.mode_ == InteractiveDrawMode::Line) {
            if (!hasCompletableInteractiveDrawSession()) {
                return false;
            }
            commitInteractiveDrawSession(true);
            return acceptHandled();
        }
        return toggleCheckBox(objectDetailsUiState_.lineClosedCheck_,
                              [this](bool checked) { handleLineClosedToggled(checked); });
    case Qt::Key_S:
        return toggleCheckBox(objectDetailsUiState_.linePointSmoothCheck_,
                              [this](bool checked) { handleLinePointSmoothToggled(checked); });
    case Qt::Key_Comma:
        return toggleCheckBox(objectDetailsUiState_.linePointPreviousControlCheck_,
                              [this](bool checked) { handleLinePointPreviousControlToggled(checked); });
    case Qt::Key_Period:
        return toggleCheckBox(objectDetailsUiState_.linePointNextControlCheck_,
                              [this](bool checked) { handleLinePointNextControlToggled(checked); });
    default:
        return false;
    }
}

}
