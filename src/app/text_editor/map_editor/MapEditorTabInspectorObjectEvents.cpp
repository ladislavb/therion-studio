#include "MapEditorTab.h"

#include "MapEditorInspectorObjectController.h"
#include "MapEditorObjectMovePlanner.h"
#include "../TextEditorTab.h"

#include <QColor>
#include <QContextMenuEvent>
#include <QEvent>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTreeView>
#include <QWidget>

#include <algorithm>

namespace TherionStudio
{
namespace
{
constexpr int kInspectorObjectCategoryRole = Qt::UserRole + 702;
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;
constexpr int kInspectorObjectNameColumn = 0;
constexpr int kInspectorObjectDragColumn = 1;
constexpr int kInspectorObjectVisibilityColumn = 2;
constexpr int kInspectorObjectDeleteColumn = 3;
constexpr int kInspectorDragAutoScrollMargin = 24;

void hideDropIndicator(QWidget *indicator)
{
    if (indicator != nullptr) {
        const QRect dirtyRect = indicator->geometry().adjusted(-1, -1, 1, 1);
        QWidget *parent = indicator->parentWidget();
        indicator->hide();
        indicator->setGeometry(QRect());
        if (parent != nullptr) {
            parent->update(dirtyRect);
        }
    }
}

void clearPressedObjectState(QPersistentModelIndex *pressedIndex, bool *pressedWasSelected)
{
    if (pressedIndex != nullptr) {
        *pressedIndex = QPersistentModelIndex();
    }
    if (pressedWasSelected != nullptr) {
        *pressedWasSelected = false;
    }
}

bool isScrapCategory(const QModelIndex &index)
{
    return index.data(kInspectorObjectCategoryRole).toString() == QStringLiteral("Scraps");
}

bool isMovableObjectIndex(const QModelIndex &index)
{
    return index.isValid()
        && index.data(kInspectorSourceLineRole).toInt() > 0
        && !isScrapCategory(index);
}

bool isValidDropTargetIndex(const QModelIndex &sourceIndex, const QModelIndex &targetIndex)
{
    return sourceIndex.isValid()
        && targetIndex.isValid()
        && targetIndex.data(kInspectorSourceLineRole).toInt() > 0
        && targetIndex != sourceIndex;
}

bool isDragAffordanceColumn(int column)
{
    return column == kInspectorObjectNameColumn || column == kInspectorObjectDragColumn;
}

bool isActionColumn(int column)
{
    return column == kInspectorObjectVisibilityColumn || column == kInspectorObjectDeleteColumn;
}

void autoscrollDragViewport(QTreeView *tree, const QPoint &viewportPos)
{
    if (tree == nullptr || tree->viewport() == nullptr || tree->verticalScrollBar() == nullptr) {
        return;
    }

    QScrollBar *scrollBar = tree->verticalScrollBar();
    const int step = std::max(1, scrollBar->singleStep());
    if (viewportPos.y() < kInspectorDragAutoScrollMargin) {
        scrollBar->setValue(scrollBar->value() - step);
    } else if (viewportPos.y() > tree->viewport()->height() - kInspectorDragAutoScrollMargin) {
        scrollBar->setValue(scrollBar->value() + step);
    }
}

QString dropIndicatorStyleSheet(const QColor &color)
{
    return QStringLiteral("background-color: rgba(%1, %2, %3, 230);"
                          "border: 1px solid rgba(%1, %2, %3, 255);"
                          "border-radius: 1px;")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue());
}

bool isSelfDropTarget(const QModelIndex &sourceIndex, const QModelIndex &targetIndex)
{
    return sourceIndex.isValid() && targetIndex.isValid() && targetIndex == sourceIndex;
}

MapEditorObjectMovePosition dropPositionForTarget(const QModelIndex &targetIndex, bool afterTarget)
{
    if (isScrapCategory(targetIndex)) {
        return MapEditorObjectMovePosition::IntoTargetScrap;
    }
    return afterTarget ? MapEditorObjectMovePosition::AfterTarget : MapEditorObjectMovePosition::BeforeTarget;
}

bool isCurrentVisibleObjectSlot(const QModelIndex &sourceIndex, const QModelIndex &targetIndex, bool afterTarget)
{
    if (!sourceIndex.isValid() || !targetIndex.isValid() || isScrapCategory(targetIndex)) {
        return false;
    }
    if (sourceIndex.parent() != targetIndex.parent()) {
        return false;
    }
    return afterTarget
        ? sourceIndex.row() == targetIndex.row() + 1
        : sourceIndex.row() + 1 == targetIndex.row();
}

bool isCurrentObjectLocation(TextEditorTab *textEditor,
                             const QModelIndex &sourceIndex,
                             const QModelIndex &targetIndex,
                             bool afterTarget)
{
    if (!sourceIndex.isValid() || !targetIndex.isValid()) {
        return false;
    }

    if (isCurrentVisibleObjectSlot(sourceIndex, targetIndex, afterTarget)) {
        return true;
    }

    if (textEditor == nullptr) {
        return false;
    }

    const int sourceLineNumber = sourceIndex.data(kInspectorSourceLineRole).toInt();
    const int targetLineNumber = targetIndex.data(kInspectorSourceLineRole).toInt();
    if (sourceLineNumber <= 0 || targetLineNumber <= 0) {
        return false;
    }

    const MapEditorObjectMovePlan movePlan = MapEditorObjectMovePlanner::planMove(textEditor->text(),
                                                                                  sourceLineNumber,
                                                                                  targetLineNumber,
                                                                                  dropPositionForTarget(targetIndex, afterTarget));
    return movePlan.resolved && !movePlan.changed;
}
}

std::optional<bool> MapEditorTab::handleInspectorObjectViewportEvent(QEvent *event)
{
    if (event == nullptr || mapObjectsTree_ == nullptr) {
        return std::nullopt;
    }

    if (event->type() == QEvent::ContextMenu) {
        showInspectorObjectContextMenu(static_cast<QContextMenuEvent *>(event)->pos());
        event->accept();
        return true;
    }

    if (event->type() == QEvent::MouseMove && inspectorObjectPressedNameIndex_.isValid()) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (!(mouseEvent->buttons() & Qt::LeftButton)) {
            clearPressedObjectState(&inspectorObjectPressedNameIndex_, &inspectorObjectPressedWasSelected_);
            hideDropIndicator(inspectorObjectDropIndicator_);
            mapObjectsTree_->viewport()->unsetCursor();
            return true;
        }

        mapObjectsTree_->viewport()->setCursor(Qt::ClosedHandCursor);
        autoscrollDragViewport(mapObjectsTree_, mouseEvent->pos());
        const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
        const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
        if (isValidDropTargetIndex(inspectorObjectPressedNameIndex_, objectIndex)) {
            const QRect targetRect = mapObjectsTree_->visualRect(objectIndex);
            const bool targetIsScrap = isScrapCategory(objectIndex);
            const bool afterTarget = targetRect.isValid() && mouseEvent->pos().y() > targetRect.center().y();
            if (isCurrentObjectLocation(textEditor_, inspectorObjectPressedNameIndex_, objectIndex, afterTarget)) {
                hideDropIndicator(inspectorObjectDropIndicator_);
                mapObjectsTree_->viewport()->setCursor(Qt::ForbiddenCursor);
                toolbarStatusNote_ = tr("Move object: already at this position.");
                refreshToolbarSummary();
                event->accept();
                return true;
            }
            if (inspectorObjectDropIndicator_ == nullptr
                || inspectorObjectDropIndicator_->parentWidget() != mapObjectsTree_->viewport()) {
                inspectorObjectDropIndicator_ = new QWidget(mapObjectsTree_->viewport());
                inspectorObjectDropIndicator_->setAttribute(Qt::WA_TransparentForMouseEvents);
                inspectorObjectDropIndicator_->setAutoFillBackground(false);
                inspectorObjectDropIndicator_->setObjectName(QStringLiteral("mapObjectsTreeDropIndicator"));
            }
            inspectorObjectDropIndicator_->setStyleSheet(dropIndicatorStyleSheet(
                mapObjectsTree_->palette().color(QPalette::Highlight)));
            const int indicatorHeight = 2;
            const int indicatorX = std::max(0, targetRect.left());
            const int indicatorY = std::clamp(((afterTarget || targetIsScrap) ? targetRect.bottom() : targetRect.top())
                                                  - (indicatorHeight / 2),
                                              0,
                                              std::max(0, mapObjectsTree_->viewport()->height() - indicatorHeight));
            inspectorObjectDropIndicator_->setGeometry(indicatorX,
                                                       indicatorY,
                                                       std::max(0, mapObjectsTree_->viewport()->width() - indicatorX),
                                                       indicatorHeight);
            inspectorObjectDropIndicator_->raise();
            inspectorObjectDropIndicator_->show();
            toolbarStatusNote_ = targetIsScrap
                ? tr("Move object: release to place inside target scrap.")
                : (afterTarget
                       ? tr("Move object: release to place after target object.")
                       : tr("Move object: release to place before target object."));
            refreshToolbarSummary();
            event->accept();
            return true;
        }

        hideDropIndicator(inspectorObjectDropIndicator_);
        mapObjectsTree_->viewport()->setCursor(Qt::ForbiddenCursor);
        toolbarStatusNote_ = isSelfDropTarget(inspectorObjectPressedNameIndex_, objectIndex)
            ? tr("Move object: cannot drop onto itself.")
            : tr("Move object: release over another object or scrap row.");
        refreshToolbarSummary();
        event->accept();
        return true;
    }

    if (event->type() == QEvent::MouseMove) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
        const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
        if (isDragAffordanceColumn(index.column()) && isMovableObjectIndex(objectIndex)) {
            mapObjectsTree_->viewport()->setCursor(Qt::OpenHandCursor);
        } else if (isActionColumn(index.column()) && objectIndex.data(kInspectorSourceLineRole).toInt() > 0) {
            mapObjectsTree_->viewport()->setCursor(Qt::PointingHandCursor);
        } else {
            mapObjectsTree_->viewport()->unsetCursor();
        }
        return std::nullopt;
    }

    if (event->type() == QEvent::Leave) {
        clearPressedObjectState(&inspectorObjectPressedNameIndex_, &inspectorObjectPressedWasSelected_);
        hideDropIndicator(inspectorObjectDropIndicator_);
        mapObjectsTree_->viewport()->unsetCursor();
        return std::nullopt;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return std::nullopt;
        }
        hideDropIndicator(inspectorObjectDropIndicator_);
        mapObjectsTree_->viewport()->unsetCursor();

        const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
        const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
        const QPersistentModelIndex sourceIndex = inspectorObjectPressedNameIndex_;
        inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
        inspectorObjectPressedWasSelected_ = false;
        if (sourceIndex.isValid() && !isValidDropTargetIndex(sourceIndex, objectIndex)) {
            toolbarStatusNote_ = isSelfDropTarget(sourceIndex, objectIndex)
                ? tr("Move object: cannot drop onto itself.")
                : tr("Move object: release over another object or scrap row.");
            refreshToolbarSummary();
            event->accept();
            return true;
        }
        if (sourceIndex.isValid() && objectIndex.isValid() && sourceIndex != objectIndex) {
            const QRect targetRect = mapObjectsTree_->visualRect(objectIndex);
            const bool afterTarget = targetRect.isValid() && mouseEvent->pos().y() > targetRect.center().y();
            if (isCurrentObjectLocation(textEditor_, sourceIndex, objectIndex, afterTarget)) {
                toolbarStatusNote_ = tr("Move object: already at this position.");
                refreshToolbarSummary();
                event->accept();
                return true;
            }
            if (MapEditorInspectorObjectController(inspectorObjectContext()).moveInspectorObject(sourceIndex, objectIndex, afterTarget)) {
                event->accept();
                return true;
            }
        }

        if (index.column() == kInspectorObjectNameColumn
            && objectIndex.isValid()
            && mapObjectsModel_ != nullptr
            && mapObjectsModel_->rowCount(objectIndex) > 0
            && mouseEvent->pos().x() < mapObjectsTree_->visualRect(objectIndex).left() + mapObjectsTree_->indentation()) {
            return std::nullopt;
        }
        if (index.isValid()
            && (index.column() == kInspectorObjectNameColumn
                || index.column() == kInspectorObjectDragColumn
                || index.column() == kInspectorObjectVisibilityColumn
                || index.column() == kInspectorObjectDeleteColumn)) {
            event->accept();
            return true;
        }
        return std::nullopt;
    }

    if (event->type() != QEvent::MouseButtonPress) {
        return std::nullopt;
    }

    inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
    inspectorObjectPressedWasSelected_ = false;

    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() != Qt::LeftButton || mapObjectsTree_->selectionModel() == nullptr) {
        return std::nullopt;
    }

    const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
    const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
    if (index.column() == kInspectorObjectNameColumn
        && objectIndex.isValid()
        && mapObjectsModel_ != nullptr
        && mapObjectsModel_->rowCount(objectIndex) > 0
        && mouseEvent->pos().x() < mapObjectsTree_->visualRect(objectIndex).left() + mapObjectsTree_->indentation()) {
        return std::nullopt;
    }
    if (!index.isValid()
        || (index.column() != kInspectorObjectNameColumn
            && index.column() != kInspectorObjectDragColumn
            && index.column() != kInspectorObjectVisibilityColumn
            && index.column() != kInspectorObjectDeleteColumn)) {
        return std::nullopt;
    }

    if (index.column() == kInspectorObjectNameColumn || index.column() == kInspectorObjectDragColumn) {
        const QPersistentModelIndex pressedObjectIndex(objectIndex);
        const bool pressedWasSelected = mapObjectsTree_->selectionModel()->isSelected(objectIndex);
        const bool sourceIsScrap = isScrapCategory(objectIndex);
        if (!sourceIsScrap) {
            inspectorObjectPressedNameIndex_ = QPersistentModelIndex(objectIndex);
        }
        inspectorObjectPressedWasSelected_ = pressedWasSelected;
        if (index.column() == kInspectorObjectNameColumn) {
            handleInspectorObjectClicked(index);
        }
        inspectorObjectPressedNameIndex_ = sourceIsScrap ? QPersistentModelIndex() : pressedObjectIndex;
        inspectorObjectPressedWasSelected_ = pressedWasSelected;
    } else {
        handleInspectorObjectClicked(index);
    }
    event->accept();
    return true;
}
}
