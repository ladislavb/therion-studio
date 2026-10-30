#include "MapEditorTab.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QWidget>

#include "MapEditorInspectorBackgroundController.h"
#include "MapEditorInspectorObjectController.h"
#include "MapEditorObjectDetailsEditController.h"
#include "MapEditorObjectDetailsPanelController.h"
#include "../../../core/ISessionStore.h"

#include <optional>

namespace TherionStudio
{

void MapEditorTab::updateHelpPanel()
{
    // Map-specific help was removed; source contextual help is owned by the embedded TextEditorTab.
}

void MapEditorTab::rebuildInspectorObjectsTree()
{
    // A synchronous rebuild supersedes any deferred one.
    inspectorTreeRebuildPending_ = false;
    MapEditorInspectorObjectController(inspectorObjectContext()).rebuildInspectorObjectsTree();
}

void MapEditorTab::scheduleInspectorObjectsTreeRebuild()
{
    if (inspectorTreeRebuildPending_) {
        return;
    }
    inspectorTreeRebuildPending_ = true;
    // Run after the pending paint of the refreshed scene so the canvas responds first.
    QTimer::singleShot(0, this, [this]() {
        flushDeferredInspectorTreeRebuild();
    });
}

void MapEditorTab::flushDeferredInspectorTreeRebuild()
{
    if (!inspectorTreeRebuildPending_) {
        return;
    }
    rebuildInspectorObjectsTree();
}

void MapEditorTab::configureInspectorObjectTreeColumns()
{
    MapEditorInspectorObjectController(inspectorObjectContext()).configureInspectorObjectTreeColumns();
}

QModelIndex MapEditorTab::findInspectorObjectIndexForLine(int lineNumber) const
{
    // Source lines shift after inserts and deletes, so a stale tree would map to the wrong row.
    const_cast<MapEditorTab *>(this)->flushDeferredInspectorTreeRebuild();
    return MapEditorInspectorObjectController(const_cast<MapEditorTab *>(this)->inspectorObjectContext()).findInspectorObjectIndexForLine(lineNumber);
}

void MapEditorTab::syncInspectorObjectSelectionToLine(int lineNumber)
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).syncInspectorObjectSelectionToLine(lineNumber);
}

void MapEditorTab::syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection)
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).syncInspectorObjectSelectionToLine(lineNumber, scrollToSelection);
}

void MapEditorTab::syncInspectorObjectSelectionToLineExpanding(int lineNumber, bool scrollToSelection)
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).syncInspectorObjectSelectionToLineExpanding(lineNumber, scrollToSelection);
}

void MapEditorTab::syncInspectorObjectSelectionToLineForNavigation(int lineNumber, bool scrollToSelection)
{
    if (objectsInspectorAutoCollapseExpandScrapsEnabled_) {
        syncInspectorObjectSelectionToLineExpanding(lineNumber, scrollToSelection);
    } else {
        syncInspectorObjectSelectionToLine(lineNumber, scrollToSelection);
    }
}

void MapEditorTab::setInspectorObjectCurrentIndex(const QModelIndex &index)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).setInspectorObjectCurrentIndex(index);
}

void MapEditorTab::clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).clearInspectorObjectSelection(suppressAutoReselectLineNumbers);
}

void MapEditorTab::handleInspectorObjectSelectionChanged(const QModelIndex &current)
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).handleInspectorObjectSelectionChanged(current);
}

void MapEditorTab::handleInspectorObjectClicked(const QModelIndex &index)
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).handleInspectorObjectClicked(index);
}

void MapEditorTab::setObjectsInspectorAutoCollapseExpandScrapsEnabled(bool enabled)
{
    if (objectsInspectorAutoCollapseExpandScrapsEnabled_ == enabled) {
        return;
    }

    objectsInspectorAutoCollapseExpandScrapsEnabled_ = enabled;
    if (sessionStore_ != nullptr) {
        sessionStore_->setTherionMapObjectsAutoCollapseExpandScrapsEnabled(enabled);
    }

    if (mapObjectsAutoCollapseExpandScrapsCheck_ != nullptr
        && mapObjectsAutoCollapseExpandScrapsCheck_->isChecked() != enabled) {
        QSignalBlocker blocker(mapObjectsAutoCollapseExpandScrapsCheck_);
        mapObjectsAutoCollapseExpandScrapsCheck_->setChecked(enabled);
    }

    if (enabled && objectSelectionState_.selectedObjectLineNumber_ > 0) {
        syncInspectorObjectSelectionToLineExpanding(objectSelectionState_.selectedObjectLineNumber_, true);
    }
}

void MapEditorTab::applyInspectorObjectVisibility()
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).applyInspectorObjectVisibility();
}

void MapEditorTab::hideAllObjects()
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).hideAllObjects();
}

void MapEditorTab::showAllObjects()
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).showAllObjects();
}

void MapEditorTab::selectNextObject()
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).selectNextObject(true);
}

void MapEditorTab::selectPreviousObject()
{
    flushDeferredInspectorTreeRebuild();
    MapEditorInspectorObjectController(inspectorObjectContext()).selectPreviousObject();
}

void MapEditorTab::configureInspectorBackgroundLayerTreeColumns()
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).configureInspectorBackgroundLayerTreeColumns();
}

void MapEditorTab::handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current)
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).handleInspectorBackgroundLayerSelectionChanged(current);
}

void MapEditorTab::handleInspectorBackgroundLayerClicked(const QModelIndex &index)
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).handleInspectorBackgroundLayerClicked(index);
}

void MapEditorTab::refreshInspectorBackgroundPanel()
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).refreshInspectorBackgroundPanel();
}

void MapEditorTab::refreshInspectorBackgroundSelectionControls()
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).refreshInspectorBackgroundSelectionControls();
}

void MapEditorTab::refreshObjectDetailsPanel()
{
    MapEditorObjectDetailsPanelController(objectDetailsContext()).refreshObjectDetailsPanel();
}

void MapEditorTab::handleObjectOrientationValueChanged(double value)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleObjectOrientationValueChanged(value);
}

void MapEditorTab::handleObjectOrientationEnabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleObjectOrientationEnabledToggled(checked);
}

void MapEditorTab::handleLinePointLeftSizeValueChanged(double value)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointLeftSizeValueChanged(value);
}

void MapEditorTab::handleLinePointLeftSizeEnabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointLeftSizeEnabledToggled(checked);
}

void MapEditorTab::handleLinePointSegmentSubtypeChanged()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointSegmentSubtypeChanged();
}

void MapEditorTab::handleLinePointAltitudeAutoToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointAltitudeAutoToggled(checked);
}

void MapEditorTab::deleteSelectedObjectFromSelection()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).deleteSelectedObjectFromSelection();
}

void MapEditorTab::applyObjectQuickFieldEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyObjectQuickFieldEdits();
}

void MapEditorTab::applyScrapProjectionEdit()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyScrapProjectionEdit();
}

void MapEditorTab::updateObjectQuickSubtypeChoices()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).updateObjectQuickSubtypeChoices();
}

void MapEditorTab::insertVertexBeforeFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).insertVertexBeforeFromSelectionPanel();
}

void MapEditorTab::insertVertexAfterFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).insertVertexAfterFromSelectionPanel();
}

void MapEditorTab::splitLineFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).splitLineFromSelectionPanel();
}

void MapEditorTab::deleteVertexFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).deleteVertexFromSelectionPanel();
}

void MapEditorTab::handleLinePointPreviousControlToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointPreviousControlToggled(checked);
}

void MapEditorTab::handleLinePointSmoothToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointSmoothToggled(checked);
}

void MapEditorTab::handleLinePointNextControlToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointNextControlToggled(checked);
}

void MapEditorTab::applyLinePointFlagsEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyLinePointFlagsEdits();
    objectDetailsUiState_.linePointFlagsDirty_ = false;
}

void MapEditorTab::populateScrapScaleFromSourceBounds()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).populateScrapScaleFromSourceBounds();
}

void MapEditorTab::applyScrapScaleEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyScrapScaleEdits();
}

void MapEditorTab::handleConfigureObjectSettingsTriggered()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleConfigureObjectSettingsTriggered();
}

void MapEditorTab::showMapSelectionContextMenu(const QPoint &globalPosition)
{
    if (QMenu *previousMenu = mapSelectionContextMenu_.data()) {
        mapSelectionContextMenu_.clear();
        previousMenu->close();
    }

    const int segmentContextLineNumber = objectSelectionState_.selectedObjectLineNumber_;
    const bool segmentContextLineSelected = objectSelectionState_.selectedObjectKind_ == QStringLiteral("line")
        && objectSelectionState_.selectedObjectVertexIndex_ < 0
        && objectSelectionState_.selectedObjectCoordinate_.has_value();
    const std::optional<QPointF> segmentContextCoordinate = objectSelectionState_.selectedObjectCoordinate_;

    if (mapScene_ != nullptr && !mapScene_->selectedItems().isEmpty()) {
        handleMapSceneSelectionChanged();
    }
    if (segmentContextLineSelected
        && objectSelectionState_.selectedObjectLineNumber_ == segmentContextLineNumber
        && objectSelectionState_.selectedObjectKind_ == QStringLiteral("line")
        && objectSelectionState_.selectedObjectVertexIndex_ < 0
        && !objectSelectionState_.selectedObjectCoordinate_.has_value()) {
        objectSelectionState_.selectedObjectCoordinate_ = segmentContextCoordinate;
    }

    auto *menu = new QMenu(this);
    mapSelectionContextMenu_ = menu;
    connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
        if (mapSelectionContextMenu_ == menu) {
            mapSelectionContextMenu_.clear();
        }
        menu->deleteLater();
    });

    const bool hasObjectSelection = objectSelectionState_.selectedObjectLineNumber_ > 0
        && !objectSelectionState_.selectedObjectKind_.trimmed().isEmpty();
    if (!hasObjectSelection) {
        QAction *emptyAction = menu->addAction(tr("No map object selected"));
        emptyAction->setEnabled(false);
        menu->popup(globalPosition);
        return;
    }

    auto addComboMenu = [this, menu](const QString &title,
                                     QComboBox *combo,
                                     bool refreshSubtypeChoices,
                                     const QString &emptyValueLabel = QString()) -> QMenu * {
        if (combo == nullptr || !combo->isEnabled() || combo->count() <= 0) {
            return nullptr;
        }
        QMenu *targetMenu = menu->addMenu(title);
        const QString currentValue = combo->currentText().trimmed().toLower();
        for (int index = 0; index < combo->count(); ++index) {
            const QString value = combo->itemText(index);
            const QString actionText = value.isEmpty() && !emptyValueLabel.isEmpty()
                ? emptyValueLabel
                : value;
            QAction *action = targetMenu->addAction(actionText);
            action->setCheckable(true);
            action->setChecked(value.trimmed().toLower() == currentValue);
            connect(action, &QAction::triggered, this, [this, combo, value, refreshSubtypeChoices]() {
                combo->setCurrentText(value);
                if (refreshSubtypeChoices) {
                    updateObjectQuickSubtypeChoices();
                }
                applyObjectQuickFieldEdits();
                refreshObjectDetailsPanel();
            });
        }
        return targetMenu;
    };
    auto addLinePointComboMenu = [this](QMenu *targetMenu,
                                        const QString &title,
                                        QComboBox *combo,
                                        const QString &emptyValueLabel = QString()) -> QMenu * {
        if (targetMenu == nullptr || combo == nullptr || !combo->isEnabled() || combo->count() <= 0) {
            return nullptr;
        }
        QMenu *comboMenu = targetMenu->addMenu(title);
        const QString currentValue = combo->currentText().trimmed().toLower();
        for (int index = 0; index < combo->count(); ++index) {
            const QString value = combo->itemText(index);
            const QString actionText = value.isEmpty() && !emptyValueLabel.isEmpty()
                ? emptyValueLabel
                : value;
            QAction *action = comboMenu->addAction(actionText);
            action->setCheckable(true);
            action->setChecked(value.trimmed().toLower() == currentValue);
            connect(action, &QAction::triggered, this, [this, combo, value]() {
                const QSignalBlocker blocker(combo);
                combo->setCurrentText(value);
                MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointSegmentSubtypeChanged();
            });
        }
        return comboMenu;
    };
    auto addFocusAction = [this](QMenu *targetMenu, const QString &title, QWidget *editor) {
        if (targetMenu == nullptr || editor == nullptr || !editor->isVisible() || !editor->isEnabled()) {
            return;
        }
        QAction *action = targetMenu->addAction(title);
        connect(action, &QAction::triggered, this, [this, editor]() {
            activateSelectionInspector();
            QTimer::singleShot(0, this, [editor]() {
                editor->setFocus(Qt::ShortcutFocusReason);
                if (auto *lineEdit = qobject_cast<QLineEdit *>(editor)) {
                    lineEdit->selectAll();
                } else if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(editor)) {
                    plainTextEdit->selectAll();
                } else if (auto *spin = qobject_cast<QDoubleSpinBox *>(editor)) {
                    spin->selectAll();
                }
            });
        });
    };
    auto applyObjectOrientation = [this](bool enabled, std::optional<qreal> value) {
        if (objectDetailsUiState_.objectOrientationEnabledCheck_ == nullptr
            || objectDetailsUiState_.objectOrientationSpin_ == nullptr) {
            return;
        }
        const QSignalBlocker enabledBlocker(objectDetailsUiState_.objectOrientationEnabledCheck_);
        const QSignalBlocker spinBlocker(objectDetailsUiState_.objectOrientationSpin_);
        objectDetailsUiState_.objectOrientationEnabledCheck_->setChecked(enabled);
        objectDetailsUiState_.objectOrientationSpin_->setEnabled(enabled);
        if (value.has_value()) {
            objectDetailsUiState_.objectOrientationSpin_->setValue(value.value());
        }
        MapEditorObjectDetailsEditController(objectDetailsContext()).applyObjectOrientationEdits();
        refreshObjectDetailsPanel();
    };
    auto addOrientationMenu = [this, applyObjectOrientation, addFocusAction](QMenu *targetMenu) {
        if (targetMenu == nullptr
            || objectDetailsUiState_.objectOrientationEnabledCheck_ == nullptr
            || objectDetailsUiState_.objectOrientationSpin_ == nullptr
            || !objectDetailsUiState_.objectOrientationEnabledCheck_->isVisible()
            || !objectDetailsUiState_.objectOrientationEnabledCheck_->isEnabled()) {
            return;
        }

        QAction *orientationAction = targetMenu->addAction(tr("Orientation override (-orientation)"));
        orientationAction->setCheckable(true);
        orientationAction->setChecked(objectDetailsUiState_.objectOrientationEnabledCheck_->isChecked());
        connect(orientationAction, &QAction::triggered, this, [applyObjectOrientation](bool checked) {
            applyObjectOrientation(checked, std::nullopt);
        });

        QMenu *orientationMenu = targetMenu->addMenu(tr("Orientation"));
        const qreal currentOrientation = objectDetailsUiState_.objectOrientationSpin_->value();
        QAction *currentOrientationAction = orientationMenu->addAction(
            tr("Current (%1 deg)").arg(QString::number(currentOrientation, 'f', 3)));
        currentOrientationAction->setCheckable(true);
        currentOrientationAction->setChecked(objectDetailsUiState_.objectOrientationEnabledCheck_->isChecked());
        connect(currentOrientationAction, &QAction::triggered, this, [applyObjectOrientation, currentOrientation]() {
            applyObjectOrientation(true, currentOrientation);
        });
        orientationMenu->addSeparator();
        for (qreal orientation : {0.0, 90.0, 180.0, 270.0}) {
            QAction *orientationValueAction = orientationMenu->addAction(
                tr("%1 deg").arg(QString::number(orientation, 'f', 0)));
            orientationValueAction->setCheckable(true);
            orientationValueAction->setChecked(objectDetailsUiState_.objectOrientationEnabledCheck_->isChecked()
                                               && qAbs(currentOrientation - orientation) < 0.001);
            connect(orientationValueAction, &QAction::triggered, this, [applyObjectOrientation, orientation]() {
                applyObjectOrientation(true, orientation);
            });
        }
        addFocusAction(orientationMenu, tr("Edit orientation..."), objectDetailsUiState_.objectOrientationSpin_);
    };

    const QString typeText = objectDetailsUiState_.objectQuickTypeCombo_ != nullptr
        ? objectDetailsUiState_.objectQuickTypeCombo_->currentText().trimmed()
        : QString();
    addComboMenu(typeText.isEmpty() ? tr("Type") : tr("Type (%1)").arg(typeText),
                 objectDetailsUiState_.objectQuickTypeCombo_,
                 true);
    addComboMenu(tr("Subtype"),
                 objectDetailsUiState_.objectQuickSubtypeCombo_,
                 false,
                 tr("No subtype"));

    auto addObjectFocusAction = [addFocusAction, menu](const QString &title, QWidget *editor) {
        if (editor == nullptr || !editor->isVisible() || !editor->isEnabled()) {
            return;
        }
        addFocusAction(menu, title, editor);
    };
    addObjectFocusAction(tr("Edit ID..."), objectDetailsUiState_.objectQuickIdentifierEdit_);
    addObjectFocusAction(tr("Edit Name..."), objectDetailsUiState_.objectQuickNameEdit_);
    addObjectFocusAction(tr("Edit Text..."), objectDetailsUiState_.objectQuickTextEdit_);
    addObjectFocusAction(tr("Edit Value..."), objectDetailsUiState_.objectQuickValueEdit_);

    const bool lineVertexSelected = objectSelectionState_.selectedObjectKind_ == QStringLiteral("line")
        && objectSelectionState_.selectedObjectVertexIndex_ >= 0;
    const bool lineSelected = objectSelectionState_.selectedObjectKind_ == QStringLiteral("line");
    const bool pointSelected = objectSelectionState_.selectedObjectKind_ == QStringLiteral("point");
    if (lineSelected && !objectSelectionState_.selectedObjectCoordinate_.has_value()
        && mapView_ != nullptr && mapView_->viewport() != nullptr) {
        const QPoint viewportPosition = mapView_->viewport()->mapFromGlobal(globalPosition);
        if (mapView_->viewport()->rect().contains(viewportPosition)) {
            objectSelectionState_.selectedObjectCoordinate_ =
                sourcePointFromScenePosition(mapView_->mapToScene(viewportPosition));
        }
    }

    const bool objectClipAvailable =
        objectDetailsUiState_.objectClipDisabledCheck_ != nullptr
        && objectDetailsUiState_.objectClipDisabledCheck_->isVisible()
        && objectDetailsUiState_.objectClipDisabledCheck_->isEnabled();
    const bool geometryAvailable = (lineSelected || objectClipAvailable)
        && objectDetailsUiState_.lineClosedCheck_ != nullptr
        && objectDetailsUiState_.lineReversedCheck_ != nullptr
        && objectDetailsUiState_.objectClipDisabledCheck_ != nullptr
        && (objectDetailsUiState_.lineClosedCheck_->isVisible()
            || objectDetailsUiState_.lineReversedCheck_->isVisible()
            || objectDetailsUiState_.objectClipDisabledCheck_->isVisible());
    const bool scrapProjectionAvailable =
        objectDetailsUiState_.objectQuickProjectionCombo_ != nullptr
        && objectDetailsUiState_.objectQuickProjectionCombo_->isVisible()
        && objectDetailsUiState_.objectQuickProjectionCombo_->isEnabled();
    const bool settingsAvailable = objectDetailsUiState_.objectConfigureButton_ != nullptr
        && objectDetailsUiState_.objectConfigureButton_->isEnabled();
    QMenu *objectOptionsMenu = nullptr;
    if (geometryAvailable || scrapProjectionAvailable || settingsAvailable) {
        objectOptionsMenu = menu->addMenu(tr("Options"));
        if (objectDetailsUiState_.lineClosedCheck_ != nullptr
            && objectDetailsUiState_.lineClosedCheck_->isVisible()) {
            QAction *closedAction = objectOptionsMenu->addAction(tr("Closed (-close on)"));
            closedAction->setCheckable(true);
            closedAction->setChecked(objectDetailsUiState_.lineClosedCheck_->isChecked());
            connect(closedAction, &QAction::triggered, this, [this](bool checked) {
                handleLineClosedToggled(checked);
                refreshObjectDetailsPanel();
            });
        }

        if (objectDetailsUiState_.lineReversedCheck_ != nullptr
            && objectDetailsUiState_.lineReversedCheck_->isVisible()) {
            QAction *reversedAction = objectOptionsMenu->addAction(tr("Reversed (-reverse on)"));
            reversedAction->setCheckable(true);
            reversedAction->setChecked(objectDetailsUiState_.lineReversedCheck_->isChecked());
            connect(reversedAction, &QAction::triggered, this, [this](bool checked) {
                handleLineReversedToggled(checked);
                refreshObjectDetailsPanel();
            });
        }

        if (objectClipAvailable) {
            QAction *clipAction = objectOptionsMenu->addAction(tr("Disable clipping (-clip off)"));
            clipAction->setCheckable(true);
            clipAction->setChecked(objectDetailsUiState_.objectClipDisabledCheck_->isChecked());
            connect(clipAction, &QAction::triggered, this, [this](bool checked) {
                handleObjectClipDisabledToggled(checked);
                refreshObjectDetailsPanel();
            });
        }
        if (scrapProjectionAvailable) {
            if (!objectOptionsMenu->actions().isEmpty()) {
                objectOptionsMenu->addSeparator();
            }
            addComboMenu(tr("Projection"),
                         objectDetailsUiState_.objectQuickProjectionCombo_,
                         false);
        }
        if (settingsAvailable) {
            if (!objectOptionsMenu->actions().isEmpty()) {
                objectOptionsMenu->addSeparator();
            }
            QAction *settingsAction = objectOptionsMenu->addAction(tr("Edit All Object Settings..."));
            connect(settingsAction, &QAction::triggered, this, &MapEditorTab::handleConfigureObjectSettingsTriggered);
        }
    }

    const bool previousControlAvailable = lineVertexSelected
        && objectDetailsUiState_.linePointPreviousControlCheck_ != nullptr
        && objectDetailsUiState_.linePointPreviousControlCheck_->isVisible()
        && objectDetailsUiState_.linePointPreviousControlCheck_->isEnabled();
    const bool smoothAvailable = lineVertexSelected
        && objectDetailsUiState_.linePointSmoothCheck_ != nullptr
        && objectDetailsUiState_.linePointSmoothCheck_->isVisible()
        && objectDetailsUiState_.linePointSmoothCheck_->isEnabled();
    const bool nextControlAvailable = lineVertexSelected
        && objectDetailsUiState_.linePointNextControlCheck_ != nullptr
        && objectDetailsUiState_.linePointNextControlCheck_->isVisible()
        && objectDetailsUiState_.linePointNextControlCheck_->isEnabled();
    const bool linePointOrientationAvailable = lineVertexSelected
        && objectDetailsUiState_.objectOrientationEnabledCheck_ != nullptr
        && objectDetailsUiState_.objectOrientationSpin_ != nullptr
        && objectDetailsUiState_.objectOrientationEnabledCheck_->isVisible()
        && objectDetailsUiState_.objectOrientationEnabledCheck_->isEnabled();
    const bool leftSizeAvailable = lineVertexSelected
        && objectDetailsUiState_.linePointLeftSizeEnabledCheck_ != nullptr
        && objectDetailsUiState_.linePointLeftSizeSpin_ != nullptr
        && objectDetailsUiState_.linePointLeftSizeEnabledCheck_->isVisible()
        && objectDetailsUiState_.linePointLeftSizeEnabledCheck_->isEnabled();
    const bool linePointFlagsAvailable = lineVertexSelected
        && objectDetailsUiState_.linePointFlagsEdit_ != nullptr
        && objectDetailsUiState_.linePointFlagsEdit_->isVisible()
        && objectDetailsUiState_.linePointFlagsEdit_->isEnabled();
    const bool linePointSegmentSubtypeAvailable = lineVertexSelected
        && objectDetailsUiState_.linePointSegmentSubtypeCombo_ != nullptr
        && objectDetailsUiState_.linePointSegmentSubtypeCombo_->isVisible()
        && objectDetailsUiState_.linePointSegmentSubtypeCombo_->isEnabled();
    const bool linePointAltitudeAutoAvailable = lineVertexSelected
        && objectDetailsUiState_.linePointAltitudeAutoCheck_ != nullptr
        && objectDetailsUiState_.linePointAltitudeAutoCheck_->isVisible()
        && objectDetailsUiState_.linePointAltitudeAutoCheck_->isEnabled();
    if (previousControlAvailable || smoothAvailable || nextControlAvailable
        || linePointOrientationAvailable || leftSizeAvailable || linePointSegmentSubtypeAvailable
        || linePointAltitudeAutoAvailable || linePointFlagsAvailable) {
        QMenu *linePointMenu = menu->addMenu(tr("Line Point"));
        if (previousControlAvailable) {
            QAction *previousControlAction = linePointMenu->addAction(tr("Previous control (<<)"));
            previousControlAction->setCheckable(true);
            previousControlAction->setChecked(objectDetailsUiState_.linePointPreviousControlCheck_->isChecked());
            connect(previousControlAction, &QAction::triggered, this, [this](bool checked) {
                setLineVertexControlHandleForSelection(true, checked);
                refreshObjectDetailsPanel();
            });
        }

        if (smoothAvailable) {
            QAction *smoothAction = linePointMenu->addAction(tr("Smooth (-smooth)"));
            smoothAction->setCheckable(true);
            smoothAction->setChecked(objectDetailsUiState_.linePointSmoothCheck_->isChecked());
            connect(smoothAction, &QAction::triggered, this, [this](bool checked) {
                setLineVertexSmoothForSelection(checked);
                refreshObjectDetailsPanel();
            });
        }

        if (nextControlAvailable) {
            QAction *nextControlAction = linePointMenu->addAction(tr("Next control (>>)"));
            nextControlAction->setCheckable(true);
            nextControlAction->setChecked(objectDetailsUiState_.linePointNextControlCheck_->isChecked());
            connect(nextControlAction, &QAction::triggered, this, [this](bool checked) {
                setLineVertexControlHandleForSelection(false, checked);
                refreshObjectDetailsPanel();
            });
        }

        if (linePointOrientationAvailable) {
            linePointMenu->addSeparator();
            addOrientationMenu(linePointMenu);
        }

        if (leftSizeAvailable) {
            linePointMenu->addSeparator();
            QAction *leftSizeAction = linePointMenu->addAction(tr("Left size (-l-size)"));
            leftSizeAction->setCheckable(true);
            leftSizeAction->setChecked(objectDetailsUiState_.linePointLeftSizeEnabledCheck_->isChecked());
            connect(leftSizeAction, &QAction::triggered, this, [this](bool checked) {
                if (objectDetailsUiState_.linePointLeftSizeEnabledCheck_ == nullptr
                    || objectDetailsUiState_.linePointLeftSizeSpin_ == nullptr) {
                    return;
                }
                const QSignalBlocker enabledBlocker(objectDetailsUiState_.linePointLeftSizeEnabledCheck_);
                objectDetailsUiState_.linePointLeftSizeEnabledCheck_->setChecked(checked);
                objectDetailsUiState_.linePointLeftSizeSpin_->setEnabled(checked);
                MapEditorObjectDetailsEditController(objectDetailsContext()).applyObjectOrientationEdits();
                refreshObjectDetailsPanel();
            });
            QMenu *leftSizeMenu = linePointMenu->addMenu(tr("Left size"));
            const qreal currentLeftSize = objectDetailsUiState_.linePointLeftSizeSpin_->value();
            QAction *currentLeftSizeAction = leftSizeMenu->addAction(
                tr("Current (%1)").arg(QString::number(currentLeftSize, 'f', 1)));
            currentLeftSizeAction->setCheckable(true);
            currentLeftSizeAction->setChecked(objectDetailsUiState_.linePointLeftSizeEnabledCheck_->isChecked());
            connect(currentLeftSizeAction, &QAction::triggered, this, [this, currentLeftSize]() {
                if (objectDetailsUiState_.linePointLeftSizeEnabledCheck_ == nullptr
                    || objectDetailsUiState_.linePointLeftSizeSpin_ == nullptr) {
                    return;
                }
                const QSignalBlocker enabledBlocker(objectDetailsUiState_.linePointLeftSizeEnabledCheck_);
                const QSignalBlocker spinBlocker(objectDetailsUiState_.linePointLeftSizeSpin_);
                objectDetailsUiState_.linePointLeftSizeEnabledCheck_->setChecked(true);
                objectDetailsUiState_.linePointLeftSizeSpin_->setEnabled(true);
                objectDetailsUiState_.linePointLeftSizeSpin_->setValue(currentLeftSize);
                MapEditorObjectDetailsEditController(objectDetailsContext()).applyObjectOrientationEdits();
                refreshObjectDetailsPanel();
            });
            leftSizeMenu->addSeparator();
            for (qreal leftSize : {10.0, 20.0, 40.0, 80.0}) {
                QAction *leftSizeValueAction = leftSizeMenu->addAction(QString::number(leftSize, 'f', 0));
                leftSizeValueAction->setCheckable(true);
                leftSizeValueAction->setChecked(objectDetailsUiState_.linePointLeftSizeEnabledCheck_->isChecked()
                                                && qAbs(currentLeftSize - leftSize) < 0.001);
                connect(leftSizeValueAction, &QAction::triggered, this, [this, leftSize]() {
                    if (objectDetailsUiState_.linePointLeftSizeEnabledCheck_ == nullptr
                        || objectDetailsUiState_.linePointLeftSizeSpin_ == nullptr) {
                        return;
                    }
                    const QSignalBlocker enabledBlocker(objectDetailsUiState_.linePointLeftSizeEnabledCheck_);
                    const QSignalBlocker spinBlocker(objectDetailsUiState_.linePointLeftSizeSpin_);
                    objectDetailsUiState_.linePointLeftSizeEnabledCheck_->setChecked(true);
                    objectDetailsUiState_.linePointLeftSizeSpin_->setEnabled(true);
                    objectDetailsUiState_.linePointLeftSizeSpin_->setValue(leftSize);
                    MapEditorObjectDetailsEditController(objectDetailsContext()).applyObjectOrientationEdits();
                    refreshObjectDetailsPanel();
                });
            }
            addFocusAction(leftSizeMenu, tr("Edit left size..."), objectDetailsUiState_.linePointLeftSizeSpin_);
        }

        if (linePointSegmentSubtypeAvailable) {
            linePointMenu->addSeparator();
            addLinePointComboMenu(linePointMenu,
                                  tr("Subtype"),
                                  objectDetailsUiState_.linePointSegmentSubtypeCombo_,
                                  tr("No subtype"));
        }

        if (linePointAltitudeAutoAvailable) {
            QAction *altitudeAutoAction = linePointMenu->addAction(tr("Altitude (auto)"));
            altitudeAutoAction->setCheckable(true);
            altitudeAutoAction->setChecked(objectDetailsUiState_.linePointAltitudeAutoCheck_->isChecked());
            connect(altitudeAutoAction, &QAction::triggered, this, [this](bool checked) {
                if (objectDetailsUiState_.linePointAltitudeAutoCheck_ == nullptr) {
                    return;
                }
                const QSignalBlocker blocker(objectDetailsUiState_.linePointAltitudeAutoCheck_);
                objectDetailsUiState_.linePointAltitudeAutoCheck_->setChecked(checked);
                MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointAltitudeAutoToggled(checked);
            });
        }

        if (linePointFlagsAvailable) {
            linePointMenu->addSeparator();
            addFocusAction(linePointMenu, tr("Edit additional line-point options..."), objectDetailsUiState_.linePointFlagsEdit_);
        }
    }

    const bool pointOrientationAvailable = pointSelected
        && objectDetailsUiState_.objectOrientationEnabledCheck_ != nullptr
        && objectDetailsUiState_.objectOrientationSpin_ != nullptr
        && objectDetailsUiState_.objectOrientationEnabledCheck_->isVisible()
        && objectDetailsUiState_.objectOrientationEnabledCheck_->isEnabled();
    const bool pointAlignAvailable = pointSelected
        && objectDetailsUiState_.pointAlignCombo_ != nullptr
        && objectDetailsUiState_.pointAlignCombo_->isVisible()
        && objectDetailsUiState_.pointAlignCombo_->isEnabled();
    auto addPointAlignMenu = [this](QMenu *targetMenu) {
        if (targetMenu == nullptr
            || objectDetailsUiState_.pointAlignCombo_ == nullptr
            || !objectDetailsUiState_.pointAlignCombo_->isVisible()
            || !objectDetailsUiState_.pointAlignCombo_->isEnabled()) {
            return;
        }

        QMenu *alignMenu = targetMenu->addMenu(tr("Align (-align)"));
        const QString currentValue = objectDetailsUiState_.pointAlignCombo_->currentData().toString().trimmed().toLower();
        for (int index = 0; index < objectDetailsUiState_.pointAlignCombo_->count(); ++index) {
            const QString value = objectDetailsUiState_.pointAlignCombo_->itemData(index).toString();
            const QString text = objectDetailsUiState_.pointAlignCombo_->itemText(index);
            QAction *action = alignMenu->addAction(text);
            action->setCheckable(true);
            action->setChecked(value.trimmed().toLower() == currentValue);
            connect(action, &QAction::triggered, this, [this, index]() {
                if (objectDetailsUiState_.pointAlignCombo_ == nullptr) {
                    return;
                }
                const QSignalBlocker blocker(objectDetailsUiState_.pointAlignCombo_);
                objectDetailsUiState_.pointAlignCombo_->setCurrentIndex(index);
                handlePointAlignChanged();
                refreshObjectDetailsPanel();
            });
        }
    };
    if (pointOrientationAvailable) {
        QMenu *pointOptionsMenu = objectOptionsMenu != nullptr ? objectOptionsMenu : menu->addMenu(tr("Options"));
        addOrientationMenu(pointOptionsMenu);
        addPointAlignMenu(pointOptionsMenu);
    } else if (pointAlignAvailable) {
        QMenu *pointOptionsMenu = objectOptionsMenu != nullptr ? objectOptionsMenu : menu->addMenu(tr("Options"));
        addPointAlignMenu(pointOptionsMenu);
    }

    const bool insertBeforeAvailable = lineVertexSelected
        && objectDetailsUiState_.vertexInsertBeforeButton_ != nullptr
        && objectDetailsUiState_.vertexInsertBeforeButton_->isEnabled();
    const bool insertAfterAvailable = lineVertexSelected
        && objectDetailsUiState_.vertexInsertAfterButton_ != nullptr
        && objectDetailsUiState_.vertexInsertAfterButton_->isEnabled();
    const bool insertHereAvailable = lineSelected
        && objectSelectionState_.selectedObjectCoordinate_.has_value();
    const bool splitAvailable = lineVertexSelected
        && objectDetailsUiState_.vertexSplitButton_ != nullptr
        && objectDetailsUiState_.vertexSplitButton_->isEnabled();
    const bool deletePointAvailable = lineVertexSelected
        && objectDetailsUiState_.vertexDeleteButton_ != nullptr
        && objectDetailsUiState_.vertexDeleteButton_->isEnabled();
    if (insertBeforeAvailable || insertAfterAvailable || insertHereAvailable || splitAvailable || deletePointAvailable) {
        QMenu *linePointActionsMenu = menu->addMenu(tr("Line Point Actions"));
        if (insertHereAvailable) {
            QAction *insertHereAction = linePointActionsMenu->addAction(tr("Insert Point Here"));
            connect(insertHereAction, &QAction::triggered, this, &MapEditorTab::insertLineVertexAtSelectionCoordinate);
        }

        if (insertBeforeAvailable) {
            QAction *insertBeforeAction = linePointActionsMenu->addAction(objectDetailsUiState_.vertexInsertBeforeButton_->text());
            connect(insertBeforeAction, &QAction::triggered, this, &MapEditorTab::insertVertexBeforeFromSelectionPanel);
        }

        if (insertAfterAvailable) {
            QAction *insertAfterAction = linePointActionsMenu->addAction(objectDetailsUiState_.vertexInsertAfterButton_->text());
            connect(insertAfterAction, &QAction::triggered, this, &MapEditorTab::insertVertexAfterFromSelectionPanel);
        }

        if (splitAvailable) {
            QAction *splitAction = linePointActionsMenu->addAction(tr("Split Here"));
            connect(splitAction, &QAction::triggered, this, &MapEditorTab::splitLineFromSelectionPanel);
        }

        if (deletePointAvailable) {
            QAction *deleteVertexAction = linePointActionsMenu->addAction(objectDetailsUiState_.vertexDeleteButton_->text());
            connect(deleteVertexAction, &QAction::triggered, this, &MapEditorTab::deleteVertexFromSelectionPanel);
        }
    }

    const bool deleteAvailable = objectDetailsUiState_.objectDeleteButton_ != nullptr
        && objectDetailsUiState_.objectDeleteButton_->isEnabled();
    if (deleteAvailable) {
        QMenu *objectActionsMenu = menu->addMenu(tr("Object Actions"));
        if (deleteAvailable) {
            QAction *deleteAction = objectActionsMenu->addAction(objectDetailsUiState_.objectDeleteButton_ != nullptr
                                                                     ? objectDetailsUiState_.objectDeleteButton_->text()
                                                                     : tr("Delete Object"));
            connect(deleteAction, &QAction::triggered, this, &MapEditorTab::deleteSelectedObjectFromSelection);
        }
    }

    menu->popup(globalPosition);
}

void MapEditorTab::handleLineClosedToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLineClosedToggled(checked);
}

void MapEditorTab::handleLineReversedToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLineReversedToggled(checked);
}

void MapEditorTab::handleObjectClipDisabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleObjectClipDisabledToggled(checked);
}

void MapEditorTab::handlePointAlignChanged()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handlePointAlignChanged();
}

} // namespace TherionStudio
