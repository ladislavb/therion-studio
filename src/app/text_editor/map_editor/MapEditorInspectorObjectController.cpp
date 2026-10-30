#include "MapEditorInspectorObjectController.h"
#include "MapEditorSceneItems.h"

#include "../TextEditorTab.h"
#include "MapEditorAreaReferenceResolver.h"
#include "MapEditorInspectorData.h"
#include "MapEditorObjectDeletePlanner.h"
#include "MapEditorObjectMovePlanner.h"
#include "MapEditorSceneSupport.h"
#include "../../../core/ProjectStructureIndex.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceLogicalDocument.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTreeView>

#include <functional>
#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;
constexpr int kInspectorSourceFileRole = Qt::UserRole + 701;
constexpr int kInspectorObjectCategoryRole = Qt::UserRole + 702;
constexpr int kInspectorObjectDeleteBlockedRole = Qt::UserRole + 703;
constexpr int kInspectorObjectNameColumn = 0;
constexpr int kInspectorObjectDragColumn = 1;
constexpr int kInspectorObjectVisibilityColumn = 2;
constexpr int kInspectorObjectDeleteColumn = 3;
constexpr int kInspectorObjectColumnCount = 4;

bool isScrapCategory(const QString &category)
{
    return category == QStringLiteral("Scraps");
}

bool requireSourceTransaction(const MapEditorInspectorObjectContext &context, const QString &message)
{
    if (context.applySourceTextChangeWithSnapshot) {
        return true;
    }

    Q_ASSERT(context.applySourceTextChangeWithSnapshot);
    if (context.toolbarStatusNote != nullptr) {
        *context.toolbarStatusNote = message;
    }
    if (context.refreshToolbarSummary) {
        context.refreshToolbarSummary();
    }
    if (context.refreshObjectDetailsPanel) {
        context.refreshObjectDetailsPanel();
    }
    return false;
}

TextEditorSourceTransactionResult applySourceTransaction(const MapEditorInspectorObjectContext &context,
                                                         const QString &label,
                                                         const QString &beforeText,
                                                         const QString &afterText,
                                                         int targetLineNumber,
                                                         std::function<void()> postApplyHook = {})
{
    return context.applySourceTextChangeWithSnapshot(label,
                                                     beforeText,
                                                     afterText,
                                                     targetLineNumber,
                                                     std::move(postApplyHook));
}

class InspectorObjectTreeVisualGuard final
{
public:
    explicit InspectorObjectTreeVisualGuard(QTreeView *tree)
        : tree_(tree)
        , updatesEnabled_(tree != nullptr && tree->updatesEnabled())
        , animated_(tree != nullptr && tree->isAnimated())
    {
        if (tree_ == nullptr) {
            return;
        }

        tree_->setUpdatesEnabled(false);
        tree_->setAnimated(false);
    }

    ~InspectorObjectTreeVisualGuard()
    {
        if (tree_ == nullptr) {
            return;
        }

        tree_->setAnimated(animated_);
        tree_->setUpdatesEnabled(updatesEnabled_);
        if (updatesEnabled_ && tree_->viewport() != nullptr) {
            tree_->viewport()->update();
        }
    }

private:
    QTreeView *tree_ = nullptr;
    bool updatesEnabled_ = true;
    bool animated_ = true;
};
}

MapEditorInspectorObjectController::MapEditorInspectorObjectController(MapEditorInspectorObjectContext context)
    : context_(std::move(context))
{
}

QString MapEditorInspectorObjectController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorInspectorObjectController", text);
}

void MapEditorInspectorObjectController::rebuildInspectorObjectsTree()
{
    if (context_.objectsModel == nullptr) {
        return;
    }

    inspectorObjectIndexesByLine_.clear();
    indexedObjectsModel_ = context_.objectsModel;
    inspectorObjectLineIndexValid_ = false;
    const bool autoCollapseExpandScraps =
        context_.autoCollapseExpandScraps != nullptr && *context_.autoCollapseExpandScraps;
    const QSet<int> collapsedScrapLines = collapsedScrapLineNumbers();
    const InspectorObjectTreeVisualGuard visualGuard(autoCollapseExpandScraps ? context_.objectsTree : nullptr);
    *context_.pressedNameIndex = QPersistentModelIndex();
    *context_.pressedWasSelected = false;
    context_.objectsModel->clear();
    context_.objectsModel->setColumnCount(kInspectorObjectColumnCount);
    context_.objectsModel->setHorizontalHeaderLabels({tr("Objects"), QString(), QString(), QString()});
    configureInspectorObjectTreeColumns();

    const QString th2Path = context_.filePath();
    if (context_.textEditor == nullptr || !th2Path.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive)) {
        auto *placeholderItem = new QStandardItem(tr("Open a TH2 document to browse its objects by scrap"));
        placeholderItem->setEditable(false);
        context_.objectsModel->appendRow({placeholderItem, new QStandardItem, new QStandardItem});
        return;
    }

    const QVector<TherionSourceLogicalCommand> logicalCommands = context_.logicalSource.logicalCommandsForCurrentDocument
        ? context_.logicalSource.logicalCommandsForCurrentDocument()
        : QVector<TherionSourceLogicalCommand>();
    std::optional<Th2GeometryProjection> geometryProjection;
    if (context_.logicalSource.geometryProjectionForCurrentDocument) {
        geometryProjection = context_.logicalSource.geometryProjectionForCurrentDocument();
    }
    const bool hasLogicalSource = context_.logicalSource.logicalCommandsForCurrentDocument != nullptr;
    const QVector<TherionParsedLine> parsedLines = hasLogicalSource
        ? QVector<TherionParsedLine>()
        : context_.parsedLinesForCurrentDocument();
    const QVector<ProjectStructureEntry> entries = hasLogicalSource
        ? ProjectStructureIndex::scanTh2Objects(th2Path, logicalCommands)
        : ProjectStructureIndex::scanTh2Objects(th2Path, parsedLines);
    if (entries.isEmpty()) {
        auto *placeholderItem = new QStandardItem(tr("No TH2 scraps, points, lines, or areas were found in the current document"));
        placeholderItem->setEditable(false);
        context_.objectsModel->appendRow({placeholderItem, new QStandardItem, new QStandardItem});
        return;
    }

    QHash<int, TherionParsedLine> parsedLinesByLineNumber;
    if (hasLogicalSource) {
        for (const TherionSourceLogicalCommand &command : logicalCommands) {
            if (command.startLineNumber > 0) {
                parsedLinesByLineNumber.insert(command.startLineNumber, command.parsed);
            }
        }
    } else {
        for (const TherionParsedLine &parsedLine : parsedLines) {
            if (parsedLine.lineNumber > 0) {
                parsedLinesByLineNumber.insert(parsedLine.lineNumber, parsedLine);
            }
        }
    }

    // Built once per rebuild: per-entry lookups rescanned the whole document and froze large maps.
    const auto projectionAreaReferences = geometryProjection.has_value()
        ? mapEditorAreaReferencesByBorderLine(geometryProjection.value())
        : QHash<int, QVector<MapEditorAreaReference>>();
    const auto logicalAreaReferences = mapEditorAreaReferencesByBorderLine(logicalCommands);

    QSet<int> currentObjectLines;
    QVector<QStandardItem *> parentStack;
    for (const ProjectStructureEntry &entry : entries) {
        while (parentStack.size() > entry.depth) {
            parentStack.removeLast();
        }

        const auto parsedLineIt = parsedLinesByLineNumber.constFind(entry.lineNumber);
        const TherionParsedLine *parsedLine = parsedLineIt == parsedLinesByLineNumber.constEnd()
            ? nullptr
            : &parsedLineIt.value();
        auto *entryItem = new QStandardItem(inspectorMapObjectItemText(entry, parsedLine));
        const QString iconName = inspectorMapObjectIconName(entry);
        if (!iconName.isEmpty()) {
            entryItem->setIcon(inspectorActionIcon(iconName));
        }
        entryItem->setEditable(false);
        entryItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        entryItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        entryItem->setData(entry.category, kInspectorObjectCategoryRole);
        if (entry.lineNumber > 0) {
            currentObjectLines.insert(entry.lineNumber);
        }

        auto *dragItem = new QStandardItem;
        dragItem->setEditable(false);
        dragItem->setTextAlignment(Qt::AlignCenter);
        dragItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        dragItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        dragItem->setData(entry.category, kInspectorObjectCategoryRole);
        if (entry.lineNumber > 0 && !isScrapCategory(entry.category)) {
            dragItem->setIcon(inspectorActionIcon(QStringLiteral("grip-vertical")));
            dragItem->setToolTip(tr("Drag to move this object"));
        }

        auto *visibilityItem = new QStandardItem;
        visibilityItem->setEditable(false);
        visibilityItem->setTextAlignment(Qt::AlignCenter);
        visibilityItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        visibilityItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        visibilityItem->setData(entry.category, kInspectorObjectCategoryRole);
        if (entry.lineNumber > 0) {
            const bool visible = !context_.hiddenObjectLines->contains(entry.lineNumber);
            visibilityItem->setIcon(inspectorActionIcon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off")));
            visibilityItem->setToolTip(visible ? tr("Hide object") : tr("Show object"));
        }

        auto *deleteItem = new QStandardItem;
        deleteItem->setEditable(false);
        deleteItem->setTextAlignment(Qt::AlignCenter);
        deleteItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        deleteItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        deleteItem->setData(entry.category, kInspectorObjectCategoryRole);
        if (entry.lineNumber > 0) {
            QVector<MapEditorAreaReference> areaReferences = projectionAreaReferences.value(entry.lineNumber);
            if (areaReferences.isEmpty()) {
                areaReferences = logicalAreaReferences.value(entry.lineNumber);
            }
            const bool deleteBlockedByAreaReference = !areaReferences.isEmpty();
            deleteItem->setIcon(inspectorActionIcon(QStringLiteral("trash-2")));
            deleteItem->setData(deleteBlockedByAreaReference, kInspectorObjectDeleteBlockedRole);
            deleteItem->setEnabled(!deleteBlockedByAreaReference);
            deleteItem->setToolTip(deleteBlockedByAreaReference
                ? tr("This line is used as an area border. Delete the area instead.")
                : tr("Delete object from source"));
        }

        QStandardItem *parentItem = parentStack.isEmpty() ? context_.objectsModel->invisibleRootItem() : parentStack.last();
        parentItem->appendRow({entryItem, dragItem, visibilityItem, deleteItem});
        if (entry.lineNumber > 0) {
            inspectorObjectIndexesByLine_.insert(
                entry.lineNumber, QPersistentModelIndex(context_.objectsModel->indexFromItem(entryItem)));
        }
        parentStack.append(entryItem);
    }
    inspectorObjectLineIndexValid_ = true;
    for (auto it = context_.hiddenObjectLines->begin(); it != context_.hiddenObjectLines->end();) {
        if (!currentObjectLines.contains(*it)) {
            it = context_.hiddenObjectLines->erase(it);
        } else {
            ++it;
        }
    }

    applyInspectorObjectVisibility();
    if (autoCollapseExpandScraps) {
        const int selectedLineNumber = context_.selectedObjectLineNumber != nullptr
            ? *context_.selectedObjectLineNumber
            : 0;
        if (selectedLineNumber > 0) {
            syncInspectorObjectSelectionToLine(selectedLineNumber, true, true);
        } else {
            collapseAllInspectorObjectScraps();
            syncInspectorObjectSelectionToLine(context_.currentLineNumber(), true, false);
        }
    } else {
        restoreInspectorObjectExpansion(collapsedScrapLines);
        syncInspectorObjectSelectionToLine(context_.currentLineNumber(), true, false);
    }
}

void MapEditorInspectorObjectController::configureInspectorObjectTreeColumns()
{
    if (context_.objectsTree == nullptr) {
        return;
    }

    if (context_.objectsModel != nullptr && context_.objectsModel->columnCount() < kInspectorObjectColumnCount) {
        context_.objectsModel->setColumnCount(kInspectorObjectColumnCount);
    }
    context_.objectsTree->setMouseTracking(true);
    if (context_.objectsTree->viewport() != nullptr) {
        context_.objectsTree->viewport()->setMouseTracking(true);
    }
    context_.objectsTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (QHeaderView *header = context_.objectsTree->header(); header != nullptr) {
        header->setStretchLastSection(false);
        header->setSectionsMovable(false);
        header->setMinimumSectionSize(22);
        header->setSectionResizeMode(kInspectorObjectNameColumn, QHeaderView::Stretch);
        header->setSectionResizeMode(kInspectorObjectDragColumn, QHeaderView::Fixed);
        header->setSectionResizeMode(kInspectorObjectVisibilityColumn, QHeaderView::Fixed);
        header->setSectionResizeMode(kInspectorObjectDeleteColumn, QHeaderView::Fixed);
        header->resizeSection(kInspectorObjectDragColumn, 24);
        header->resizeSection(kInspectorObjectVisibilityColumn, 26);
        header->resizeSection(kInspectorObjectDeleteColumn, 26);
    }
}

void MapEditorInspectorObjectController::rebuildInspectorObjectLineIndex() const
{
    inspectorObjectIndexesByLine_.clear();
    indexedObjectsModel_ = context_.objectsModel;
    if (context_.objectsModel == nullptr) {
        inspectorObjectLineIndexValid_ = true;
        return;
    }

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = context_.objectsModel->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = context_.objectsModel->index(row, 0, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }
            const int lineNumber = childIndex.data(kInspectorSourceLineRole).toInt();
            if (lineNumber > 0) {
                inspectorObjectIndexesByLine_.insert(lineNumber, QPersistentModelIndex(childIndex));
            }
            visitNode(childIndex);
        }
    };
    visitNode(QModelIndex());
    inspectorObjectLineIndexValid_ = true;
}

QModelIndex MapEditorInspectorObjectController::findInspectorObjectIndexForLine(int lineNumber) const
{
    if (context_.objectsModel == nullptr || lineNumber <= 0) {
        return QModelIndex();
    }
    if (!inspectorObjectLineIndexValid_ || indexedObjectsModel_ != context_.objectsModel) {
        rebuildInspectorObjectLineIndex();
    }

    auto index = inspectorObjectIndexesByLine_.upperBound(lineNumber);
    if (index == inspectorObjectIndexesByLine_.constBegin()) {
        return QModelIndex();
    }
    --index;
    return index.value();
}

void MapEditorInspectorObjectController::syncInspectorObjectSelectionToLine(int lineNumber)
{
    syncInspectorObjectSelectionToLine(lineNumber, true, false);
}

void MapEditorInspectorObjectController::syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection)
{
    syncInspectorObjectSelectionToLine(lineNumber, scrollToSelection, false);
}

void MapEditorInspectorObjectController::syncInspectorObjectSelectionToLineExpanding(int lineNumber, bool scrollToSelection)
{
    syncInspectorObjectSelectionToLine(lineNumber, scrollToSelection, true);
}

void MapEditorInspectorObjectController::syncInspectorObjectSelectionToLine(int lineNumber,
                                                                            bool scrollToSelection,
                                                                            bool expandAncestors)
{
    if (context_.objectsTree == nullptr || context_.objectsTree->selectionModel() == nullptr || lineNumber <= 0) {
        return;
    }
    if (context_.suppressedAutoReselectLineNumbers->contains(lineNumber)) {
        return;
    }
    if (!context_.suppressedAutoReselectLineNumbers->isEmpty()) {
        context_.suppressedAutoReselectLineNumbers->clear();
    }

    const QModelIndex targetIndex = findInspectorObjectIndexForLine(lineNumber);
    if (!targetIndex.isValid()) {
        return;
    }

    *context_.lastClickedLineNumber = 0;
    const QScopedValueRollback<bool> guard(*context_.updatingSelection, true);
    QSignalBlocker blocker(context_.objectsTree->selectionModel());
    if (expandAncestors) {
        if (context_.autoCollapseExpandScraps != nullptr && *context_.autoCollapseExpandScraps) {
            focusInspectorObjectScrap(targetIndex);
        } else {
            expandInspectorObjectAncestors(targetIndex);
        }
    }
    setInspectorObjectCurrentIndex(targetIndex);
    if (scrollToSelection) {
        context_.objectsTree->scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
    }
}

void MapEditorInspectorObjectController::setInspectorObjectCurrentIndex(const QModelIndex &index)
{
    if (!index.isValid() || context_.objectsTree == nullptr || context_.objectsTree->selectionModel() == nullptr) {
        return;
    }

    QItemSelectionModel *selectionModel = context_.objectsTree->selectionModel();
    if (selectionModel->currentIndex() == index && selectionModel->isSelected(index)) {
        return;
    }
    selectionModel->clear();
    context_.objectsTree->setCurrentIndex(QModelIndex());
    selectionModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    if (QWidget *viewport = context_.objectsTree->viewport(); viewport != nullptr) {
        viewport->update();
    }
}

void MapEditorInspectorObjectController::clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers)
{
    *context_.pressedNameIndex = QPersistentModelIndex();
    *context_.pressedWasSelected = false;
    *context_.lastClickedLineNumber = 0;
    *context_.suppressedAutoReselectLineNumbers = suppressAutoReselectLineNumbers;
    if (context_.objectsTree != nullptr && context_.objectsTree->selectionModel() != nullptr) {
        const QScopedValueRollback<bool> guard(*context_.updatingSelection, true);
        QSignalBlocker blocker(context_.objectsTree->selectionModel());
        context_.objectsTree->selectionModel()->clearSelection();
        context_.objectsTree->setCurrentIndex(QModelIndex());
    }

    if (context_.scene != nullptr) {
        context_.scene->clearSelection();
        return;
    }

    *context_.selectedObjectLineNumber = 0;
    *context_.selectedObjectVertexIndex = -1;
    context_.selectedObjectKind->clear();
    context_.selectedObjectCoordinate->reset();
    context_.refreshObjectDetailsPanel();
    context_.updateCommandSurfaceState();
}

void MapEditorInspectorObjectController::handleInspectorObjectSelectionChanged(const QModelIndex &current)
{
    if (*context_.updatingSelection || !current.isValid() || context_.textEditor == nullptr) {
        return;
    }

    const QModelIndex objectIndex = current.sibling(current.row(), kInspectorObjectNameColumn);
    const int lineNumber = objectIndex.data(kInspectorSourceLineRole).toInt();
    if (lineNumber > 0) {
        if (context_.autoCollapseExpandScraps != nullptr && *context_.autoCollapseExpandScraps) {
            focusInspectorObjectScrap(objectIndex);
        }
        context_.textEditor->goToLine(lineNumber);
        context_.syncMapSelectionFromTextCursor(lineNumber, context_.textEditor->currentColumnNumber());
        *context_.lastClickedLineNumber = lineNumber;
    }
}

void MapEditorInspectorObjectController::handleInspectorObjectClicked(const QModelIndex &index)
{
    if (!index.isValid() || context_.objectsModel == nullptr || context_.textEditor == nullptr) {
        return;
    }

    if (index.column() != kInspectorObjectNameColumn
        && index.column() != kInspectorObjectDragColumn
        && index.column() != kInspectorObjectVisibilityColumn
        && index.column() != kInspectorObjectDeleteColumn) {
        return;
    }

    const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
    const int lineNumber = objectIndex.data(kInspectorSourceLineRole).toInt();
    if (lineNumber <= 0) {
        return;
    }

    if (index.column() == kInspectorObjectNameColumn) {
        const bool repeatedClickOnSelectedRow = *context_.pressedWasSelected
            && context_.pressedNameIndex->isValid()
            && *context_.pressedNameIndex == objectIndex;
        *context_.pressedNameIndex = QPersistentModelIndex();
        *context_.pressedWasSelected = false;
        if (repeatedClickOnSelectedRow) {
            QSet<int> suppressedLines;
            std::function<void(const QModelIndex &)> collectSuppressedLines = [&](const QModelIndex &parentIndex) {
                const int sourceLine = parentIndex.data(kInspectorSourceLineRole).toInt();
                if (sourceLine > 0) {
                    suppressedLines.insert(sourceLine);
                }

                const int rowCount = context_.objectsModel->rowCount(parentIndex);
                for (int row = 0; row < rowCount; ++row) {
                    collectSuppressedLines(context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex));
                }
            };
            collectSuppressedLines(objectIndex);
            clearInspectorObjectSelection(suppressedLines);
            return;
        }

        context_.suppressedAutoReselectLineNumbers->clear();
        if (context_.objectsTree != nullptr && context_.objectsTree->selectionModel() != nullptr) {
            setInspectorObjectCurrentIndex(objectIndex);
        }
        context_.textEditor->goToLine(lineNumber);
        context_.syncMapSelectionFromTextCursor(lineNumber, context_.textEditor->currentColumnNumber());
        *context_.lastClickedLineNumber = lineNumber;
        return;
    }

    if (index.column() == kInspectorObjectDragColumn) {
        return;
    }

    *context_.pressedNameIndex = QPersistentModelIndex();
    *context_.pressedWasSelected = false;
    context_.suppressedAutoReselectLineNumbers->clear();
    if (index.column() == kInspectorObjectDeleteColumn) {
        if (index.data(kInspectorObjectDeleteBlockedRole).toBool()) {
            if (context_.toolbarStatusNote != nullptr) {
                *context_.toolbarStatusNote = tr("This line is used as an area border. Delete the area instead.");
            }
            if (context_.refreshToolbarSummary) {
                context_.refreshToolbarSummary();
            }
            return;
        }
        const QString beforeText = context_.textEditor->text();
        const MapEditorObjectDeletePlan deletePlan = planMapEditorObjectDelete(beforeText, lineNumber);
        if (!deletePlan.resolved || !deletePlan.changed) {
            if (context_.toolbarStatusNote != nullptr) {
                *context_.toolbarStatusNote = deletePlan.errorMessage.isEmpty()
                    ? tr("Object deletion failed.")
                    : tr("Object deletion failed: %1").arg(deletePlan.errorMessage);
            }
            if (context_.refreshToolbarSummary) {
                context_.refreshToolbarSummary();
            }
            return;
        }

        if (!requireSourceTransaction(context_, tr("Cannot delete map object without map source transaction support."))) {
            return;
        }
        const auto postDeleteHook = [this, deletePlan]() {
            for (int removedLineNumber : deletePlan.removedLineNumbers) {
                context_.hiddenObjectLines->remove(removedLineNumber);
            }
            *context_.lastClickedLineNumber = 0;
        };

        const TextEditorSourceTransactionResult transactionResult =
            applySourceTransaction(context_,
                                   tr("Delete Map Object"),
                                   beforeText,
                                   deletePlan.updatedText,
                                   deletePlan.focusLineAfterDelete,
                                   postDeleteHook);
        if (transactionResult != TextEditorSourceTransactionResult::Applied) {
            if (context_.refreshToolbarSummary) {
                context_.refreshToolbarSummary();
            }
            if (context_.refreshObjectDetailsPanel) {
                context_.refreshObjectDetailsPanel();
            }
            return;
        }

        if (context_.toolbarStatusNote != nullptr) {
            *context_.toolbarStatusNote = tr("Deleted selected object from source.");
        }
        if (context_.refreshToolbarSummary) {
            context_.refreshToolbarSummary();
        }
        return;
    }

    *context_.lastClickedLineNumber = 0;
    QVector<int> subtreeLineNumbers;
    std::function<void(const QModelIndex &)> collectLineNumbers = [&](const QModelIndex &parentIndex) {
        const int sourceLine = parentIndex.data(kInspectorSourceLineRole).toInt();
        if (sourceLine > 0) {
            subtreeLineNumbers.append(sourceLine);
        }

        const int rowCount = context_.objectsModel->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            collectLineNumbers(context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex));
        }
    };
    collectLineNumbers(objectIndex);

    if (subtreeLineNumbers.isEmpty()) {
        return;
    }

    const bool shouldShow = context_.hiddenObjectLines->contains(lineNumber);
    for (int sourceLine : subtreeLineNumbers) {
        if (shouldShow) {
            context_.hiddenObjectLines->remove(sourceLine);
        } else {
            context_.hiddenObjectLines->insert(sourceLine);
        }
    }

    rebuildInspectorObjectsTree();
    applyInspectorObjectVisibility();
    syncInspectorObjectSelectionToLine(lineNumber, false);
    if (shouldShow) {
        context_.selectMapLines(QSet<int>(subtreeLineNumbers.begin(), subtreeLineNumbers.end()), false);
    }
}

bool MapEditorInspectorObjectController::moveInspectorObject(const QModelIndex &sourceIndex, const QModelIndex &targetIndex, bool afterTarget)
{
    if (!sourceIndex.isValid() || !targetIndex.isValid() || context_.textEditor == nullptr) {
        return false;
    }

    const QModelIndex sourceObjectIndex = sourceIndex.sibling(sourceIndex.row(), kInspectorObjectNameColumn);
    const QModelIndex targetObjectIndex = targetIndex.sibling(targetIndex.row(), kInspectorObjectNameColumn);
    const int sourceLineNumber = sourceObjectIndex.data(kInspectorSourceLineRole).toInt();
    const int targetLineNumber = targetObjectIndex.data(kInspectorSourceLineRole).toInt();
    if (sourceLineNumber <= 0 || targetLineNumber <= 0 || sourceLineNumber == targetLineNumber) {
        return false;
    }

    const bool targetIsScrap = isScrapCategory(targetObjectIndex.data(kInspectorObjectCategoryRole).toString());
    const QString beforeText = context_.textEditor->text();
    const MapEditorObjectMovePlan movePlan = MapEditorObjectMovePlanner::planMove(beforeText,
                                                                                  sourceLineNumber,
                                                                                  targetLineNumber,
                                                                                  targetIsScrap
                                                                                      ? MapEditorObjectMovePosition::IntoTargetScrap
                                                                                      : (afterTarget
                                                                                             ? MapEditorObjectMovePosition::AfterTarget
                                                                                             : MapEditorObjectMovePosition::BeforeTarget));
    if (!movePlan.resolved) {
        if (context_.toolbarStatusNote != nullptr) {
            *context_.toolbarStatusNote = movePlan.errorMessage.isEmpty()
                ? tr("Object move failed.")
                : tr("Object move failed: %1").arg(movePlan.errorMessage);
        }
        if (context_.refreshToolbarSummary) {
            context_.refreshToolbarSummary();
        }
        return false;
    }

    if (!movePlan.changed) {
        if (context_.toolbarStatusNote != nullptr) {
            *context_.toolbarStatusNote = tr("Object already at requested position.");
        }
        if (context_.refreshToolbarSummary) {
            context_.refreshToolbarSummary();
        }
        return false;
    }

    if (!requireSourceTransaction(context_, tr("Cannot move map object without map source transaction support."))) {
        return false;
    }
    const auto postMoveHook = [this, movePlan]() {
        context_.hiddenObjectLines->clear();
        context_.textEditor->goToLine(movePlan.insertBeforeLineAfterRemoval);
        context_.syncMapSelectionFromTextCursor(movePlan.insertBeforeLineAfterRemoval,
                                                context_.textEditor->currentColumnNumber());
        rebuildInspectorObjectsTree();
        syncInspectorObjectSelectionToLine(movePlan.insertBeforeLineAfterRemoval, true);
    };

    const TextEditorSourceTransactionResult transactionResult =
        applySourceTransaction(context_,
                               tr("Move Map Object"),
                               beforeText,
                               movePlan.movedText,
                               movePlan.insertBeforeLineAfterRemoval,
                               postMoveHook);
    if (transactionResult != TextEditorSourceTransactionResult::Applied) {
        if (context_.refreshToolbarSummary) {
            context_.refreshToolbarSummary();
        }
        if (context_.refreshObjectDetailsPanel) {
            context_.refreshObjectDetailsPanel();
        }
        return false;
    }

    if (context_.toolbarStatusNote != nullptr) {
        *context_.toolbarStatusNote = tr("Moved map object to line %1.").arg(movePlan.insertBeforeLineAfterRemoval);
    }
    if (context_.refreshToolbarSummary) {
        context_.refreshToolbarSummary();
    }
    return true;
}

void MapEditorInspectorObjectController::applyInspectorObjectVisibility()
{
    if (context_.scene == nullptr) {
        return;
    }

    const qsizetype selectedCount = context_.scene->selectedItems().size();
    QSignalBlocker selectionSignals(context_.scene);
    for (QGraphicsItem *item : unsortedSceneItems(context_.scene)) {
        if (item == nullptr) {
            continue;
        }

        bool ok = false;
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt(&ok);
        if (!ok || lineNumber <= 0) {
            continue;
        }

        const bool hidden = context_.hiddenObjectLines->contains(lineNumber);
        if (item->data(kMapSceneSelectionGatedRole).toBool()) {
            if (hidden) {
                item->setVisible(false);
            }
            continue;
        }

        item->setVisible(!hidden);
    }

    selectionSignals.unblock();
    if (context_.scene->selectedItems().size() != selectedCount) {
        Q_EMIT context_.scene->selectionChanged();
    }
    context_.updateGeometrySelectionPresentation();
}

void MapEditorInspectorObjectController::hideAllObjects()
{
    if (context_.hiddenObjectLines == nullptr) {
        return;
    }

    *context_.lastClickedLineNumber = 0;
    if (context_.objectsModel != nullptr) {
        std::function<void(const QModelIndex &)> collectLines = [&](const QModelIndex &parentIndex) {
            const int count = context_.objectsModel->rowCount(parentIndex);
            for (int row = 0; row < count; ++row) {
                const QModelIndex idx = context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex);
                const int line = idx.data(kInspectorSourceLineRole).toInt();
                if (line > 0) {
                    context_.hiddenObjectLines->insert(line);
                }
                collectLines(idx);
            }
        };
        collectLines(QModelIndex());
    }
    if (context_.scene != nullptr) {
        for (QGraphicsItem *item : unsortedSceneItems(context_.scene)) {
            if (item == nullptr) {
                continue;
            }
            bool ok = false;
            const int line = item->data(kMapSceneLineNumberRole).toInt(&ok);
            if (ok && line > 0) {
                context_.hiddenObjectLines->insert(line);
            }
        }
    }

    clearInspectorObjectSelection();
    rebuildInspectorObjectsTree();
}

void MapEditorInspectorObjectController::showAllObjects()
{
    if (context_.hiddenObjectLines == nullptr) {
        return;
    }

    context_.hiddenObjectLines->clear();
    rebuildInspectorObjectsTree();
}

void MapEditorInspectorObjectController::selectPreviousObject()
{
    selectNextObject(false);
}

void MapEditorInspectorObjectController::selectNextObject(bool forward)
{
    if (context_.objectsModel == nullptr || context_.objectsModel->rowCount() <= 0) {
        return;
    }

    QModelIndex currentIndex;
    if (context_.objectsTree != nullptr && context_.objectsTree->currentIndex().isValid()) {
        currentIndex = context_.objectsTree->currentIndex();
    } else {
        const int selectedLine = context_.selectedObjectLineNumber != nullptr ? *context_.selectedObjectLineNumber : 0;
        if (selectedLine > 0) {
            currentIndex = findInspectorObjectIndexForLine(selectedLine);
        } else {
            currentIndex = findInspectorObjectIndexForLine(context_.currentLineNumber());
        }
    }
    if (currentIndex.isValid()) {
        currentIndex = currentIndex.sibling(currentIndex.row(), kInspectorObjectNameColumn);
    }
    if (!currentIndex.isValid()) {
        currentIndex = context_.objectsModel->index(0, kInspectorObjectNameColumn);
    }
    if (!currentIndex.isValid()) {
        return;
    }

    auto collectSubtreeLines = [&](const QModelIndex &rootIndex) -> QVector<int> {
        QVector<int> lines;
        const int line = rootIndex.data(kInspectorSourceLineRole).toInt();
        if (line > 0) {
            lines.append(line);
        }
        std::function<void(const QModelIndex &)> recurse = [&](const QModelIndex &parent) {
            const int count = context_.objectsModel->rowCount(parent);
            for (int r = 0; r < count; ++r) {
                QModelIndex child = context_.objectsModel->index(r, kInspectorObjectNameColumn, parent);
                const int childLine = child.data(kInspectorSourceLineRole).toInt();
                if (childLine > 0) {
                    lines.append(childLine);
                }
                recurse(child);
            }
        };
        recurse(rootIndex);
        return lines;
    };

    auto isItemVisible = [&](const QModelIndex &idx) -> bool {
        const int line = idx.data(kInspectorSourceLineRole).toInt();
        if (line <= 0 || context_.hiddenObjectLines == nullptr) {
            return true;
        }
        return !context_.hiddenObjectLines->contains(line);
    };

    QVector<QModelIndex> candidates;
    const bool isScrap = !currentIndex.parent().isValid()
        || isScrapCategory(currentIndex.data(kInspectorObjectCategoryRole).toString());

    if (isScrap) {
        for (int r = 0; r < context_.objectsModel->rowCount(); ++r) {
            QModelIndex scrapIdx = context_.objectsModel->index(r, kInspectorObjectNameColumn);
            if (scrapIdx.isValid() && isScrapCategory(scrapIdx.data(kInspectorObjectCategoryRole).toString())) {
                candidates.append(scrapIdx);
            }
        }
    } else {
        const QModelIndex parentScrap = currentIndex.parent();
        const QString category = currentIndex.data(kInspectorObjectCategoryRole).toString();
        const QString text = currentIndex.data(Qt::DisplayRole).toString();

        QString targetType;
        if (category == QStringLiteral("Lines")) {
            targetType = text.section(QLatin1Char(':'), 0, 0).section(QLatin1Char(' '), 0, 0).trimmed().toLower();
        } else if (category == QStringLiteral("Stations") || category == QStringLiteral("Points")) {
            targetType = (category == QStringLiteral("Stations"))
                ? QStringLiteral("station")
                : text.section(QLatin1Char(':'), 0, 0).section(QLatin1Char(' '), 0, 0).trimmed().toLower();
        }

        const int childCount = context_.objectsModel->rowCount(parentScrap);
        for (int r = 0; r < childCount; ++r) {
            QModelIndex childIdx = context_.objectsModel->index(r, kInspectorObjectNameColumn, parentScrap);
            if (!childIdx.isValid()) {
                continue;
            }
            const QString childCat = childIdx.data(kInspectorObjectCategoryRole).toString();
            if (category == QStringLiteral("Areas")) {
                if (childCat == QStringLiteral("Areas")) {
                    candidates.append(childIdx);
                }
            } else if (category == QStringLiteral("Lines")) {
                if (childCat == QStringLiteral("Lines")) {
                    const QString childText = childIdx.data(Qt::DisplayRole).toString();
                    const QString childType = childText.section(QLatin1Char(':'), 0, 0).section(QLatin1Char(' '), 0, 0).trimmed().toLower();
                    if (childType == targetType) {
                        candidates.append(childIdx);
                    }
                }
            } else if (category == QStringLiteral("Stations") || category == QStringLiteral("Points")) {
                if (childCat == QStringLiteral("Stations") || childCat == QStringLiteral("Points")) {
                    const QString childText = childIdx.data(Qt::DisplayRole).toString();
                    const QString childType = (childCat == QStringLiteral("Stations"))
                        ? QStringLiteral("station")
                        : childText.section(QLatin1Char(':'), 0, 0).section(QLatin1Char(' '), 0, 0).trimmed().toLower();
                    if (childType == targetType) {
                        candidates.append(childIdx);
                    }
                }
            } else {
                if (childCat == category) {
                    candidates.append(childIdx);
                }
            }
        }
        if (candidates.isEmpty()) {
            for (int r = 0; r < childCount; ++r) {
                candidates.append(context_.objectsModel->index(r, kInspectorObjectNameColumn, parentScrap));
            }
        }
    }

    if (candidates.isEmpty()) {
        return;
    }

    int currentPos = -1;
    const int currentLine = currentIndex.data(kInspectorSourceLineRole).toInt();
    for (int i = 0; i < candidates.size(); ++i) {
        if (candidates.at(i).data(kInspectorSourceLineRole).toInt() == currentLine) {
            currentPos = i;
            break;
        }
    }
    if (currentPos < 0) {
        currentPos = 0;
    }

    const int targetPos = forward
        ? (currentPos + 1) % candidates.size()
        : (currentPos - 1 + candidates.size()) % candidates.size();
    const QModelIndex targetIndex = candidates.at(targetPos);
    const int targetLine = targetIndex.data(kInspectorSourceLineRole).toInt();
    if (targetLine <= 0) {
        return;
    }

    bool allCandidatesVisible = true;
    for (const QModelIndex &cand : candidates) {
        if (!isItemVisible(cand)) {
            allCandidatesVisible = false;
            break;
        }
    }

    const QVector<int> currentLines = collectSubtreeLines(currentIndex);
    const QVector<int> targetLines = collectSubtreeLines(targetIndex);

    if (!allCandidatesVisible && context_.hiddenObjectLines != nullptr) {
        for (int l : currentLines) {
            context_.hiddenObjectLines->insert(l);
        }
        for (int l : targetLines) {
            context_.hiddenObjectLines->remove(l);
        }
        if (!isScrap && currentIndex.parent().isValid()) {
            const int scrapLine = currentIndex.parent().data(kInspectorSourceLineRole).toInt();
            if (scrapLine > 0) {
                context_.hiddenObjectLines->remove(scrapLine);
            }
        }

        rebuildInspectorObjectsTree();
        applyInspectorObjectVisibility();
    }

    if (context_.textEditor != nullptr) {
        context_.textEditor->goToLine(targetLine);
        context_.syncMapSelectionFromTextCursor(targetLine, context_.textEditor->currentColumnNumber());
    }

    syncInspectorObjectSelectionToLine(targetLine, true, true);

    QVector<int> selectedLines;
    if (isScrap) {
        selectedLines = targetLines;
    } else {
        selectedLines.append(targetLine);
    }
    context_.selectMapLines(QSet<int>(selectedLines.begin(), selectedLines.end()), true);

    if (context_.refreshToolbarSummary) {
        context_.refreshToolbarSummary();
    }
    if (context_.refreshObjectDetailsPanel) {
        context_.refreshObjectDetailsPanel();
    }
}

QSet<int> MapEditorInspectorObjectController::collapsedScrapLineNumbers() const
{
    QSet<int> collapsedScrapLines;
    if (context_.objectsTree == nullptr || context_.objectsModel == nullptr || context_.objectsModel->rowCount() <= 0) {
        return collapsedScrapLines;
    }

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = context_.objectsModel->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const int lineNumber = childIndex.data(kInspectorSourceLineRole).toInt();
            const QString category = childIndex.data(kInspectorObjectCategoryRole).toString();
            if (lineNumber > 0 && isScrapCategory(category) && !context_.objectsTree->isExpanded(childIndex)) {
                collapsedScrapLines.insert(lineNumber);
            }

            visitNode(childIndex);
        }
    };
    visitNode(QModelIndex());
    return collapsedScrapLines;
}

void MapEditorInspectorObjectController::restoreInspectorObjectExpansion(const QSet<int> &collapsedScrapLines)
{
    if (context_.objectsTree == nullptr || context_.objectsModel == nullptr) {
        return;
    }

    context_.objectsTree->expandAll();
    if (collapsedScrapLines.isEmpty()) {
        return;
    }

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = context_.objectsModel->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const int lineNumber = childIndex.data(kInspectorSourceLineRole).toInt();
            const QString category = childIndex.data(kInspectorObjectCategoryRole).toString();
            if (lineNumber > 0 && isScrapCategory(category) && collapsedScrapLines.contains(lineNumber)) {
                context_.objectsTree->collapse(childIndex);
            }

            visitNode(childIndex);
        }
    };
    visitNode(QModelIndex());
}

void MapEditorInspectorObjectController::expandInspectorObjectAncestors(const QModelIndex &index)
{
    if (context_.objectsTree == nullptr || !index.isValid()) {
        return;
    }

    QModelIndex parentIndex = index.parent();
    while (parentIndex.isValid()) {
        context_.objectsTree->expand(parentIndex);
        parentIndex = parentIndex.parent();
    }
}

void MapEditorInspectorObjectController::focusInspectorObjectScrap(const QModelIndex &index)
{
    if (context_.objectsTree == nullptr || context_.objectsModel == nullptr || !index.isValid()) {
        return;
    }

    QSet<int> focusedScrapLines;
    QModelIndex ancestorIndex = index;
    while (ancestorIndex.isValid()) {
        const int lineNumber = ancestorIndex.data(kInspectorSourceLineRole).toInt();
        const QString category = ancestorIndex.data(kInspectorObjectCategoryRole).toString();
        if (lineNumber > 0 && isScrapCategory(category)) {
            focusedScrapLines.insert(lineNumber);
        }
        ancestorIndex = ancestorIndex.parent();
    }

    expandInspectorObjectAncestors(index);

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = context_.objectsModel->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const int lineNumber = childIndex.data(kInspectorSourceLineRole).toInt();
            const QString category = childIndex.data(kInspectorObjectCategoryRole).toString();
            if (lineNumber > 0 && isScrapCategory(category)) {
                if (focusedScrapLines.contains(lineNumber)) {
                    context_.objectsTree->expand(childIndex);
                } else {
                    context_.objectsTree->collapse(childIndex);
                }
            }

            visitNode(childIndex);
        }
    };
    visitNode(QModelIndex());
}

void MapEditorInspectorObjectController::collapseAllInspectorObjectScraps()
{
    if (context_.objectsTree == nullptr || context_.objectsModel == nullptr) {
        return;
    }

    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = context_.objectsModel->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const int lineNumber = childIndex.data(kInspectorSourceLineRole).toInt();
            const QString category = childIndex.data(kInspectorObjectCategoryRole).toString();
            if (lineNumber > 0 && isScrapCategory(category)) {
                context_.objectsTree->collapse(childIndex);
            }

            visitNode(childIndex);
        }
    };
    visitNode(QModelIndex());
}
}
