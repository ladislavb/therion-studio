#include "MapEditorCanvasEditController.h"

#include <QCoreApplication>

#include "../TextEditorSourceTransactionController.h"
#include "../TextEditorTab.h"
#include "MapEditorCanvasEditCommandFactory.h"
#include "MapEditorLineSplitPlanner.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentEditor.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTimer>
#include <QObject>
#include <QUndoStack>
#include <QWidget>

#include <memory>
#include <limits>
#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
bool diagnosticMapInputLoggingEnabled()
{
    static const bool enabled = [] {
        const QString value = QString::fromLocal8Bit(qgetenv("THERION_STUDIO_ENABLE_LOG")).trimmed().toLower();
        return value == QStringLiteral("1")
            || value == QStringLiteral("true")
            || value == QStringLiteral("yes")
            || value == QStringLiteral("on");
    }();
    return enabled;
}

int lineVertexIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        if (lineFeature.lineVertices.at(index).anchorSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}

qreal segmentProjectionT(const QPointF &point, const QPointF &segmentStart, const QPointF &segmentEnd)
{
    const QPointF segment = segmentEnd - segmentStart;
    const qreal lengthSquared = (segment.x() * segment.x()) + (segment.y() * segment.y());
    if (lengthSquared <= 1e-9) {
        return 0.5;
    }

    const QPointF pointDelta = point - segmentStart;
    return qBound<qreal>(0.0,
                         ((pointDelta.x() * segment.x()) + (pointDelta.y() * segment.y())) / lengthSquared,
                         1.0);
}

qreal pointDistanceSquared(const QPointF &a, const QPointF &b)
{
    const QPointF delta = a - b;
    return (delta.x() * delta.x()) + (delta.y() * delta.y());
}

QString mapVertexItemKey(int lineNumber, int vertexIndex, const QString &geometryKind)
{
    return QStringLiteral("%1:%2:%3")
        .arg(lineNumber)
        .arg(vertexIndex)
        .arg(geometryKind.trimmed().toLower());
}

QPointF cubicBezierPoint(const QPointF &p0,
                         const QPointF &p1,
                         const QPointF &p2,
                         const QPointF &p3,
                         qreal t)
{
    const qreal clampedT = qBound<qreal>(0.0, t, 1.0);
    const qreal inverseT = 1.0 - clampedT;
    return (p0 * (inverseT * inverseT * inverseT))
        + (p1 * (3.0 * inverseT * inverseT * clampedT))
        + (p2 * (3.0 * inverseT * clampedT * clampedT))
        + (p3 * (clampedT * clampedT * clampedT));
}

qreal distanceSquaredToLineSegment(const QPointF &point,
                                   const QPointF &segmentStart,
                                   const QPointF &segmentEnd,
                                   qreal *projectionT = nullptr)
{
    const qreal t = segmentProjectionT(point, segmentStart, segmentEnd);
    if (projectionT != nullptr) {
        *projectionT = t;
    }
    return pointDistanceSquared(point, segmentStart + ((segmentEnd - segmentStart) * t));
}

qreal distanceSquaredToCubicSegment(const QPointF &point,
                                    const MapGeometryFeature::TH2LineVertex &startVertex,
                                    const MapGeometryFeature::TH2LineVertex &endVertex,
                                    qreal *projectionT = nullptr)
{
    const QPointF p0 = startVertex.anchor;
    const QPointF p1 = startVertex.outgoingControl.value_or(startVertex.anchor);
    const QPointF p2 = endVertex.incomingControl.value_or(endVertex.anchor);
    const QPointF p3 = endVertex.anchor;
    const bool segmentIsCubic = startVertex.outgoingControl.has_value() || endVertex.incomingControl.has_value();
    if (!segmentIsCubic) {
        return distanceSquaredToLineSegment(point, p0, p3, projectionT);
    }

    constexpr int kSampleCount = 64;
    qreal bestT = 0.5;
    qreal bestDistanceSquared = std::numeric_limits<qreal>::max();
    for (int sample = 0; sample <= kSampleCount; ++sample) {
        const qreal t = static_cast<qreal>(sample) / static_cast<qreal>(kSampleCount);
        const qreal distanceSquared = pointDistanceSquared(point, cubicBezierPoint(p0, p1, p2, p3, t));
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestT = t;
        }
    }

    qreal low = qMax<qreal>(0.0, bestT - (1.0 / static_cast<qreal>(kSampleCount)));
    qreal high = qMin<qreal>(1.0, bestT + (1.0 / static_cast<qreal>(kSampleCount)));
    for (int iteration = 0; iteration < 16; ++iteration) {
        const qreal left = low + ((high - low) / 3.0);
        const qreal right = high - ((high - low) / 3.0);
        const qreal leftDistanceSquared = pointDistanceSquared(point, cubicBezierPoint(p0, p1, p2, p3, left));
        const qreal rightDistanceSquared = pointDistanceSquared(point, cubicBezierPoint(p0, p1, p2, p3, right));
        if (leftDistanceSquared < rightDistanceSquared) {
            high = right;
        } else {
            low = left;
        }
    }

    bestT = (low + high) / 2.0;
    bestT = qBound<qreal>(0.001, bestT, 0.999);
    bestDistanceSquared = pointDistanceSquared(point, cubicBezierPoint(p0, p1, p2, p3, bestT));
    if (projectionT != nullptr) {
        *projectionT = bestT;
    }
    return bestDistanceSquared;
}

int lineVertexOwnerIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &vertex = lineFeature.lineVertices.at(index);
        if (vertex.anchorSourceVertexIndex == sourceVertexIndex
            || vertex.incomingSourceVertexIndex == sourceVertexIndex
            || vertex.outgoingSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}

bool confirmLinePointRowDelete(const MapEditorCanvasEditContext &context,
                               const QStringList &rows,
                               int lineNumber,
                               int vertexOneBasedIndex)
{
    if (rows.isEmpty()) {
        return true;
    }

    QWidget *parentWidget = qobject_cast<QWidget *>(context.callbackContext);
    QStringList previewRows = rows.mid(0, 4);
    QString details = previewRows.join(QLatin1Char('\n'));
    if (rows.size() > previewRows.size()) {
        details += QStringLiteral("\n...");
    }
    const QString message = QCoreApplication::translate("TherionStudio::MapEditorCanvasEditController",
                                                         "This vertex has additional line-point options (for example altitude/direction settings).\n\nLine %1, point %2:\n%3\n\nDelete the vertex anyway?")
                                .arg(lineNumber)
                                .arg(vertexOneBasedIndex)
                                .arg(details);
    return QMessageBox::question(parentWidget,
                                 QCoreApplication::translate("TherionStudio::MapEditorCanvasEditController", "Delete Point"),
                                 message,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

QString sourceSnapshotUndoMessage(int lineNumber)
{
    return lineNumber > 0
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Removed inserted map object at source line %1.")
              .arg(lineNumber)
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Removed inserted map object.");
}

QString sourceSnapshotRedoMessage(int lineNumber)
{
    return lineNumber > 0
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Restored inserted map object at source line %1.")
              .arg(lineNumber)
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Restored inserted map object.");
}

QString completedDraftRevertedMessage(int lineNumber)
{
    return lineNumber > 0
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Reverted completed draft at source line %1.")
              .arg(lineNumber)
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Reverted completed draft.");
}

QString completedDraftRestoredMessage(int lineNumber)
{
    return lineNumber > 0
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Restored completed draft at source line %1.")
              .arg(lineNumber)
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Restored completed draft.");
}

TextEditorSourceTransactionRequest sourceTransactionRequest(const MapEditorCanvasEditContext &context,
                                                            const QString &label,
                                                            const QString &beforeText,
                                                            const QString &afterText,
                                                            int insertedLineNumber,
                                                            TextEditorSourceSelectionRestorePolicy selectionRestorePolicy =
                                                                TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection,
                                                            std::function<void()> selectionRestoreHook = {})
{
    const int expectedRevision = context.textEditor != nullptr ? context.textEditor->documentRevision() : 0;
    return {
        .label = label,
        .beforeText = beforeText,
        .afterText = afterText,
        .expectedSourceRevision = expectedRevision,
        .projectionInvalidationPolicy = TextEditorSourceProjectionInvalidationPolicy::FlushPendingRefresh,
        .selectionRestorePolicy = selectionRestorePolicy,
        .selectionRestoreHook = std::move(selectionRestoreHook),
        .undoStatusMessage = sourceSnapshotUndoMessage(insertedLineNumber),
        .redoStatusMessage = sourceSnapshotRedoMessage(insertedLineNumber),
        .staleStatusMessage = QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                                          "Map source change skipped: document changed."),
    };
}

TextEditorSourceTransactionController sourceTransactionController(const MapEditorCanvasEditContext &context)
{
    auto statusCallback = [statusNote = context.toolbarStatusNote,
                           refreshToolbarSummary = context.refreshToolbarSummary](const QString &statusMessage) {
        *statusNote = statusMessage;
        refreshToolbarSummary();
    };

    return TextEditorSourceTransactionController({
        .textEditor = context.textEditor,
        .undoStack = context.undoStack,
        .commandApplyInProgress = context.commandApplyInProgress,
        .flushPendingRefresh = context.flushPendingSceneRefreshAfterCommand,
        .markSourceChangeOriginatedFromTransaction = context.markSourceChangeOriginatedFromMapTransaction,
        .statusCallback = std::move(statusCallback),
    });
}

class MapDraftCompletionLifecycle final
{
public:
    MapDraftCompletionLifecycle(QGraphicsScene *scene,
                                QVector<QGraphicsRectItem *> *draftItems,
                                MapDraftGeometryItem *draftItem)
        : scene_(scene)
        , draftItems_(draftItems)
        , draftItem_(draftItem)
    {
    }

    ~MapDraftCompletionLifecycle()
    {
        if (draftItem_ != nullptr && !isDraftAttached()) {
            delete draftItem_;
            draftItem_ = nullptr;
        }
    }

    void detachDraftItem()
    {
        if (draftItem_ == nullptr) {
            return;
        }
        if (scene_ != nullptr) {
            scene_->removeItem(draftItem_);
        }
        if (draftItems_ != nullptr) {
            draftItems_->removeAll(draftItem_);
        }
    }

    void restoreDraftItem()
    {
        if (draftItem_ == nullptr) {
            return;
        }
        if (scene_ != nullptr && !scene_->items().contains(draftItem_)) {
            scene_->addItem(draftItem_);
            draftItem_->setZValue(20.0);
            draftItem_->setSelected(true);
        }
        if (draftItems_ != nullptr && !draftItems_->contains(draftItem_)) {
            draftItems_->append(draftItem_);
        }
    }

private:
    bool isDraftAttached() const
    {
        const bool inScene = scene_ != nullptr && draftItem_ != nullptr && scene_->items().contains(draftItem_);
        const bool inList = draftItems_ != nullptr && draftItem_ != nullptr && draftItems_->contains(draftItem_);
        return inScene || inList;
    }

    QPointer<QGraphicsScene> scene_;
    QVector<QGraphicsRectItem *> *draftItems_ = nullptr;
    MapDraftGeometryItem *draftItem_ = nullptr;
};

std::optional<QPointF> normalizedLineControlPoint(const QPointF &anchor, const QPointF &control)
{
    if (!mapSourcePointsDiffer(anchor, control)) {
        return std::nullopt;
    }
    return control;
}

QString mapPartialRefreshBoundsLogValue(const QRectF &bounds)
{
    if (!bounds.isValid()) {
        return QStringLiteral("invalid");
    }
    return QStringLiteral("%1,%2,%3,%4")
        .arg(bounds.x(), 0, 'f', 3)
        .arg(bounds.y(), 0, 'f', 3)
        .arg(bounds.width(), 0, 'f', 3)
        .arg(bounds.height(), 0, 'f', 3);
}

QString mapPartialRefreshBoundsLogValue(const std::optional<QRectF> &bounds)
{
    if (!bounds.has_value()) {
        return QStringLiteral("none");
    }
    return mapPartialRefreshBoundsLogValue(bounds.value());
}

std::optional<QPointF> defaultIncomingControlForLineVertex(const QVector<MapGeometryFeature::TH2LineVertex> &vertices,
                                                           int ownerIndex,
                                                           bool closed)
{
    if (ownerIndex < 0 || ownerIndex >= vertices.size()) {
        return std::nullopt;
    }

    const MapGeometryFeature::TH2LineVertex &target = vertices.at(ownerIndex);
    if (target.outgoingControl.has_value()) {
        return mirroredSmoothControlPoint(target.anchor, target.outgoingControl.value(), std::nullopt);
    }
    if (closed && vertices.size() >= 2) {
        const int previousIndex = ownerIndex > 0 ? ownerIndex - 1 : vertices.size() - 1;
        const QPointF previousAnchor = vertices.at(previousIndex).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor + ((previousAnchor - target.anchor) / 3.0));
    }
    if (ownerIndex > 0) {
        const QPointF previousAnchor = vertices.at(ownerIndex - 1).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor + ((previousAnchor - target.anchor) / 3.0));
    }
    if (ownerIndex + 1 < vertices.size()) {
        const QPointF nextAnchor = vertices.at(ownerIndex + 1).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor - ((nextAnchor - target.anchor) / 3.0));
    }
    return std::nullopt;
}

std::optional<QPointF> defaultOutgoingControlForLineVertex(const QVector<MapGeometryFeature::TH2LineVertex> &vertices,
                                                           int ownerIndex,
                                                           bool closed)
{
    if (ownerIndex < 0 || ownerIndex >= vertices.size()) {
        return std::nullopt;
    }

    const MapGeometryFeature::TH2LineVertex &target = vertices.at(ownerIndex);
    if (target.incomingControl.has_value()) {
        return mirroredSmoothControlPoint(target.anchor, target.incomingControl.value(), std::nullopt);
    }
    if (closed && vertices.size() >= 2) {
        const int nextIndex = ownerIndex + 1 < vertices.size() ? ownerIndex + 1 : 0;
        const QPointF nextAnchor = vertices.at(nextIndex).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor + ((nextAnchor - target.anchor) / 3.0));
    }
    if (ownerIndex + 1 < vertices.size()) {
        const QPointF nextAnchor = vertices.at(ownerIndex + 1).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor + ((nextAnchor - target.anchor) / 3.0));
    }
    if (ownerIndex > 0) {
        const QPointF previousAnchor = vertices.at(ownerIndex - 1).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor - ((previousAnchor - target.anchor) / 3.0));
    }
    return std::nullopt;
}

void ensureLineVertexControlHandles(QVector<MapGeometryFeature::TH2LineVertex> *vertices, int ownerIndex, bool closed)
{
    if (vertices == nullptr || ownerIndex < 0 || ownerIndex >= vertices->size()) {
        return;
    }

    MapGeometryFeature::TH2LineVertex &target = (*vertices)[ownerIndex];
    if (!target.incomingControl.has_value()) {
        target.incomingControl = defaultIncomingControlForLineVertex(*vertices, ownerIndex, closed);
    }
    if (!target.outgoingControl.has_value()) {
        target.outgoingControl = defaultOutgoingControlForLineVertex(*vertices, ownerIndex, closed);
    }
}

void smoothLineVertexControlHandles(MapGeometryFeature::TH2LineVertex *vertex)
{
    if (vertex == nullptr || !vertex->incomingControl.has_value() || !vertex->outgoingControl.has_value()) {
        return;
    }

    constexpr qreal kEpsilon = 1e-6;
    const QPointF anchor = vertex->anchor;
    const QPointF incomingVector = anchor - vertex->incomingControl.value();
    const QPointF outgoingVector = vertex->outgoingControl.value() - anchor;
    const qreal incomingLength = std::hypot(incomingVector.x(), incomingVector.y());
    const qreal outgoingLength = std::hypot(outgoingVector.x(), outgoingVector.y());
    if (incomingLength <= kEpsilon || outgoingLength <= kEpsilon) {
        return;
    }

    QPointF direction = vertex->outgoingControl.value() - vertex->incomingControl.value();
    qreal directionLength = std::hypot(direction.x(), direction.y());
    if (directionLength <= kEpsilon) {
        direction = QPointF(-incomingVector.y(), incomingVector.x());
        directionLength = std::hypot(direction.x(), direction.y());
    }
    if (directionLength <= kEpsilon) {
        return;
    }

    const QPointF unit(direction.x() / directionLength, direction.y() / directionLength);
    vertex->incomingControl = anchor - (unit * incomingLength);
    vertex->outgoingControl = anchor + (unit * outgoingLength);
}

MapEditableGeometryVertexItem *findGeometryVertexItem(QGraphicsScene *scene,
                                                      int lineNumber,
                                                      int sourceVertexIndex,
                                                      const QString &geometryKindPrefix)
{
    if (scene == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return nullptr;
    }

    const auto items = scene->items();
    for (QGraphicsItem *item : items) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
        if (vertexItem == nullptr) {
            continue;
        }
        if (vertexItem->lineNumber() != lineNumber || vertexItem->vertexIndex() != sourceVertexIndex) {
            continue;
        }
        if (!geometryKindPrefix.isEmpty() && !vertexItem->geometryKind().startsWith(geometryKindPrefix)) {
            continue;
        }
        return vertexItem;
    }

    return nullptr;
}

MapEditableGeometryVertexItem *findGeometryVertexItemForContext(const MapEditorCanvasEditContext &context,
                                                                int lineNumber,
                                                                int sourceVertexIndex,
                                                                const QString &geometryKindPrefix)
{
    if (lineNumber <= 0 || sourceVertexIndex < 0) {
        return nullptr;
    }

    if (context.vertexItemsByKey != nullptr && !geometryKindPrefix.isEmpty()) {
        auto itemIt = context.vertexItemsByKey->constFind(mapVertexItemKey(lineNumber,
                                                                           sourceVertexIndex,
                                                                           geometryKindPrefix));
        if (itemIt != context.vertexItemsByKey->constEnd()) {
            if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(itemIt.value())) {
                return vertexItem;
            }
        }
    }

    return findGeometryVertexItem(context.scene, lineNumber, sourceVertexIndex, geometryKindPrefix);
}

bool restoreLineVertexSourceSelectionForContext(const MapEditorCanvasEditContext &context,
                                                int lineNumber,
                                                int ownerSourceVertexIndex)
{
    if (context.scene == nullptr || lineNumber <= 0 || ownerSourceVertexIndex < 0) {
        return false;
    }

    const bool logTiming = diagnosticMapInputLoggingEnabled();
    QElapsedTimer totalTimer;
    QElapsedTimer stageTimer;
    if (logTiming) {
        totalTimer.start();
        stageTimer.start();
    }

    MapEditableGeometryVertexItem *ownerAnchor =
        findGeometryVertexItemForContext(context,
                                         lineNumber,
                                         ownerSourceVertexIndex,
                                         QStringLiteral("line"));
    const qint64 findMs = logTiming ? stageTimer.restart() : 0;
    if (ownerAnchor == nullptr) {
        if (logTiming) {
            qInfo().noquote()
                << QStringLiteral("map-line-selection-restore line=%1 vertex=%2 result=missing find_ms=%3 total_ms=%4")
                       .arg(lineNumber)
                       .arg(ownerSourceVertexIndex)
                       .arg(findMs)
                       .arg(totalTimer.elapsed());
        }
        return false;
    }
    if (context.updatingSelection == nullptr
        || context.selectedObjectLineNumber == nullptr
        || context.selectedObjectVertexIndex == nullptr
        || context.selectedObjectKind == nullptr
        || context.selectedObjectCoordinate == nullptr
        || !context.sourcePointFromScenePosition
        || !context.updateGeometrySelectionPresentation
        || !context.updateCommandSurfaceState
        || !context.updateHelpPanel
        || !context.refreshObjectDetailsPanel) {
        if (logTiming) {
            qInfo().noquote()
                << QStringLiteral("map-line-selection-restore line=%1 vertex=%2 result=missing-context find_ms=%3 total_ms=%4")
                       .arg(lineNumber)
                       .arg(ownerSourceVertexIndex)
                       .arg(findMs)
                       .arg(totalTimer.elapsed());
        }
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = context.scene->selectedItems();
    const bool alreadySelected = selectedItems.size() == 1
        && selectedItems.first() == ownerAnchor
        && ownerAnchor->isVisible();
    const QPointF restoredSourceCoordinate = context.sourcePointFromScenePosition(ownerAnchor->pos());
    const bool semanticSelectionUnchanged =
        alreadySelected
        && (*context.selectedObjectLineNumber) == lineNumber
        && (*context.selectedObjectVertexIndex) == ownerSourceVertexIndex
        && (*context.selectedObjectKind) == QStringLiteral("line");
    const bool coordinateUnchanged =
        semanticSelectionUnchanged
        && context.selectedObjectCoordinate->has_value()
        && !sourcePointsDifferForCommands(context.selectedObjectCoordinate->value(), restoredSourceCoordinate);
    if (!alreadySelected) {
        const QScopedValueRollback<bool> selectionGuard((*context.updatingSelection), true);
        for (QGraphicsItem *selectedItem : selectedItems) {
            if (selectedItem != nullptr && selectedItem != ownerAnchor) {
                selectedItem->setSelected(false);
            }
        }
        ownerAnchor->setVisible(true);
        ownerAnchor->setSelected(true);
    }
    const qint64 selectMs = logTiming ? stageTimer.restart() : 0;

    (*context.selectedObjectLineNumber) = lineNumber;
    (*context.selectedObjectVertexIndex) = ownerSourceVertexIndex;
    (*context.selectedObjectKind) = QStringLiteral("line");
    (*context.selectedObjectCoordinate) = restoredSourceCoordinate;
    const qint64 stateMs = logTiming ? stageTimer.restart() : 0;
    if (coordinateUnchanged) {
        if (logTiming) {
            qInfo().noquote()
                << QStringLiteral(
                       "map-line-selection-restore line=%1 vertex=%2 result=ok already_selected=%3 skipped_ui=1 find_ms=%4 "
                       "select_ms=%5 state_ms=%6 presentation_ms=0 command_ms=0 help_ms=0 details_ms=0 total_ms=%7")
                       .arg(lineNumber)
                       .arg(ownerSourceVertexIndex)
                       .arg(alreadySelected ? 1 : 0)
                       .arg(findMs)
                       .arg(selectMs)
                       .arg(stateMs)
                       .arg(totalTimer.elapsed());
        }
        return true;
    }
    context.updateGeometrySelectionPresentation();
    const qint64 presentationMs = logTiming ? stageTimer.restart() : 0;
    context.updateCommandSurfaceState();
    const qint64 commandMs = logTiming ? stageTimer.restart() : 0;
    context.updateHelpPanel();
    const qint64 helpMs = logTiming ? stageTimer.restart() : 0;
    context.refreshObjectDetailsPanel();
    const qint64 detailsMs = logTiming ? stageTimer.restart() : 0;
    if (logTiming) {
        qInfo().noquote()
            << QStringLiteral(
                   "map-line-selection-restore line=%1 vertex=%2 result=ok already_selected=%3 find_ms=%4 "
                   "skipped_ui=0 select_ms=%5 state_ms=%6 presentation_ms=%7 command_ms=%8 help_ms=%9 details_ms=%10 total_ms=%11")
                   .arg(lineNumber)
                   .arg(ownerSourceVertexIndex)
                   .arg(alreadySelected ? 1 : 0)
                   .arg(findMs)
                   .arg(selectMs)
                   .arg(stateMs)
                   .arg(presentationMs)
                   .arg(commandMs)
                   .arg(helpMs)
                   .arg(detailsMs)
                   .arg(totalTimer.elapsed());
    }
    return true;
}

bool restoreLineVertexOwnerSelectionForContext(const MapEditorCanvasEditContext &context,
                                               int lineNumber,
                                               int ownerIndex)
{
    if (lineNumber <= 0 || ownerIndex < 0 || !context.logicalSource.logicalCommandsForCurrentDocument) {
        return false;
    }

    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context.logicalSource.logicalCommandsForCurrentDocument(),
                                                                                   lineNumber);
    if (!lineFeature.has_value() || ownerIndex >= lineFeature->lineVertices.size()) {
        return false;
    }

    const MapGeometryFeature::TH2LineVertex &ownerVertex = lineFeature->lineVertices.at(ownerIndex);
    const int ownerSourceVertexIndex = ownerVertex.anchorSourceVertexIndex >= 0
        ? ownerVertex.anchorSourceVertexIndex
        : ownerIndex;
    return restoreLineVertexSourceSelectionForContext(context, lineNumber, ownerSourceVertexIndex);
}

quint64 startLineVertexSelectionRestoreGeneration(const MapEditorCanvasEditContext &context)
{
    return context.lineVertexSelectionRestoreGeneration != nullptr
        ? ++(*context.lineVertexSelectionRestoreGeneration)
        : 0;
}

bool isCurrentLineVertexSelectionRestoreGeneration(const MapEditorCanvasEditContext &context, quint64 restoreGeneration)
{
    return context.lineVertexSelectionRestoreGeneration == nullptr
        || *context.lineVertexSelectionRestoreGeneration == restoreGeneration;
}

void schedulePointSelectionRecovery(const MapEditorCanvasEditContext &context, int lineNumber)
{
    if (!context.restorePointSelectionLater) {
        return;
    }

    auto attemptRestore = [context, lineNumber]() {
        context.restorePointSelectionLater(lineNumber);
    };
    if (context.callbackContext != nullptr) {
        QTimer::singleShot(0, context.callbackContext, attemptRestore);
    } else {
        attemptRestore();
    }
}

void scheduleLineAnchorSelectionRecovery(const MapEditorCanvasEditContext &context,
                                         int lineNumber,
                                         int sourceVertexIndex)
{
    if (!context.restoreLineAnchorSelectionLater) {
        return;
    }

    auto attemptRestore = [context, lineNumber, sourceVertexIndex]() {
        context.restoreLineAnchorSelectionLater(lineNumber, sourceVertexIndex);
    };
    if (context.callbackContext != nullptr) {
        QTimer::singleShot(0, context.callbackContext, attemptRestore);
    } else {
        attemptRestore();
    }
}

void scheduleLineVertexOwnerSelectionRecovery(const MapEditorCanvasEditContext &context,
                                              int lineNumber,
                                              int ownerIndex)
{
    const quint64 restoreGeneration = startLineVertexSelectionRestoreGeneration(context);
    auto attemptRestore = [context, lineNumber, ownerIndex, restoreGeneration]() {
        if (!isCurrentLineVertexSelectionRestoreGeneration(context, restoreGeneration)) {
            return;
        }
        restoreLineVertexOwnerSelectionForContext(context, lineNumber, ownerIndex);
    };
    if (context.callbackContext != nullptr) {
        QTimer::singleShot(0, context.callbackContext, attemptRestore);
    } else {
        attemptRestore();
    }
}

void scheduleLineVertexSourceSelectionRecovery(const MapEditorCanvasEditContext &context,
                                               int lineNumber,
                                               int ownerSourceVertexIndex)
{
    const quint64 restoreGeneration = startLineVertexSelectionRestoreGeneration(context);
    auto attemptRestore = [context, lineNumber, ownerSourceVertexIndex, restoreGeneration]() {
        if (!isCurrentLineVertexSelectionRestoreGeneration(context, restoreGeneration)) {
            return;
        }
        restoreLineVertexSourceSelectionForContext(context, lineNumber, ownerSourceVertexIndex);
    };
    if (context.callbackContext != nullptr) {
        QTimer::singleShot(0, context.callbackContext, attemptRestore);
    } else {
        attemptRestore();
    }
}

std::function<void()> deferredMapSceneRefreshHook(const MapEditorCanvasEditContext &context,
                                                  std::function<void()> afterRefreshHook = {})
{
    return [context, afterRefreshHook = std::move(afterRefreshHook)]() mutable {
        auto refresh = [context, afterRefreshHook = std::move(afterRefreshHook)]() mutable {
            if (context.flushPendingSceneRefreshAfterCommand) {
                context.flushPendingSceneRefreshAfterCommand();
            }
            if (afterRefreshHook) {
                afterRefreshHook();
            }
        };
        if (context.callbackContext != nullptr) {
            QTimer::singleShot(0, context.callbackContext, std::move(refresh));
        } else {
            refresh();
        }
    };
}

std::function<void()> deferredMapSelectionRestoreHook(const MapEditorCanvasEditContext &context,
                                                      std::function<void()> selectionRestoreHook = {})
{
    return [context, selectionRestoreHook = std::move(selectionRestoreHook)]() mutable {
        auto restoreSelection = [context, selectionRestoreHook = std::move(selectionRestoreHook)]() mutable {
            if (context.discardPendingSceneRefreshAfterCommand) {
                context.discardPendingSceneRefreshAfterCommand();
            }
            if (selectionRestoreHook) {
                selectionRestoreHook();
            } else if (context.updateGeometrySelectionPresentation) {
                context.updateGeometrySelectionPresentation();
            }
        };
        if (context.callbackContext != nullptr) {
            QTimer::singleShot(0, context.callbackContext, std::move(restoreSelection));
        } else {
            restoreSelection();
        }
    };
}

std::function<void()> deferredMapLinePartialRefreshHook(const MapEditorCanvasEditContext &context,
                                                       int lineNumber,
                                                       const std::optional<QRectF> &previousSourceBounds,
                                                       std::function<void()> selectionRestoreHook = {})
{
    return [context, lineNumber, previousSourceBounds, selectionRestoreHook = std::move(selectionRestoreHook)]() mutable {
        auto refreshLine = [context,
                            lineNumber,
                            previousSourceBounds,
                            selectionRestoreHook = std::move(selectionRestoreHook)]() mutable {
            const bool logTiming = diagnosticMapInputLoggingEnabled();
            QElapsedTimer totalTimer;
            QElapsedTimer stageTimer;
            if (logTiming) {
                totalTimer.start();
                stageTimer.start();
            }
            int removedItems = 0;
            int removedVertexEntries = 0;
            int addedItems = 0;
            int addedVertexEntries = 0;
            bool removedPrimaryItem = false;
            bool addedPrimaryItem = false;
            qint64 resolveMs = 0;
            qint64 removeMs = 0;
            qint64 renderMs = 0;
            qint64 selectionMs = 0;
            QString previousBoundsLog = mapPartialRefreshBoundsLogValue(previousSourceBounds);
            QString currentBoundsLog = QStringLiteral("unset");
            QString renderBoundsLog = QStringLiteral("unset");

            auto logPartialRefresh = [&](bool fallbackFullRefresh, const QString &reason) {
                if (!logTiming) {
                    return;
                }
                qInfo().noquote()
                    << QStringLiteral(
                           "map-line-partial-refresh line=%1 fallback_full_refresh=%2 reason=%3 removed_items=%4 "
                           "added_items=%5 removed_vertex_entries=%6 added_vertex_entries=%7 removed_primary=%8 "
                           "added_primary=%9 previous_bounds=\"%10\" current_bounds=\"%11\" render_bounds=\"%12\" "
                           "resolve_ms=%13 remove_ms=%14 render_ms=%15 selection_ms=%16 total_ms=%17")
                           .arg(lineNumber)
                           .arg(fallbackFullRefresh ? 1 : 0)
                           .arg(reason)
                           .arg(removedItems)
                           .arg(addedItems)
                           .arg(removedVertexEntries)
                           .arg(addedVertexEntries)
                           .arg(removedPrimaryItem ? 1 : 0)
                           .arg(addedPrimaryItem ? 1 : 0)
                           .arg(previousBoundsLog)
                           .arg(currentBoundsLog)
                           .arg(renderBoundsLog)
                           .arg(resolveMs)
                           .arg(removeMs)
                           .arg(renderMs)
                           .arg(selectionMs)
                           .arg(totalTimer.elapsed());
            };
            auto fallbackFullRefresh = [&context, &selectionRestoreHook, &logPartialRefresh](const QString &reason) {
                logPartialRefresh(true, reason);
                if (context.flushPendingSceneRefreshAfterCommand) {
                    context.flushPendingSceneRefreshAfterCommand();
                }
                if (selectionRestoreHook) {
                    selectionRestoreHook();
                } else if (context.updateGeometrySelectionPresentation) {
                    context.updateGeometrySelectionPresentation();
                }
            };

            if (context.scene == nullptr || !context.logicalSource.logicalCommandsForCurrentDocument || context.itemsByLine == nullptr) {
                fallbackFullRefresh(QStringLiteral("missing-context"));
                return;
            }

            const std::optional<MapGeometryFeature> refreshedFeature =
                lineFeatureForLineNumber(context.logicalSource.logicalCommandsForCurrentDocument(), lineNumber);
            resolveMs = logTiming ? stageTimer.restart() : 0;
            if (!refreshedFeature.has_value()) {
                fallbackFullRefresh(QStringLiteral("missing-feature"));
                return;
            }
            const QRectF currentSourceBounds =
                context.mapSourceBoundsForCurrentDocument ? context.mapSourceBoundsForCurrentDocument() : QRectF();
            currentBoundsLog = mapPartialRefreshBoundsLogValue(currentSourceBounds);
            if (previousSourceBounds.has_value()
                && previousSourceBounds->isValid()
                && currentSourceBounds.isValid()
                && !previousSourceBounds->contains(currentSourceBounds)) {
                fallbackFullRefresh(QStringLiteral("bounds-changed"));
                return;
            }
            const QRectF renderSourceBounds = previousSourceBounds.has_value() && previousSourceBounds->isValid()
                ? previousSourceBounds.value()
                : currentSourceBounds;
            renderBoundsLog = mapPartialRefreshBoundsLogValue(renderSourceBounds);

            const MapGeometryItemGroupRemovalResult removalResult =
                removeMapGeometryItemGroupForLine(context.scene,
                                                  lineNumber,
                                                  context.itemsByLine,
                                                  context.vertexItemsByKey);
            removedItems = removalResult.removedItems;
            removedVertexEntries = removalResult.removedVertexIndexEntries;
            removedPrimaryItem = removalResult.removedPrimaryItem;
            removeMs = logTiming ? stageTimer.restart() : 0;
            const MapGeometryItemGroupRenderResult renderResult =
                renderMapGeometryItemGroupForFeature(
                    context.scene,
                    refreshedFeature.value(),
                    renderSourceBounds,
                    context.itemsByLine,
                    context.vertexItemsByKey,
                    {},
                    [context](int changedLineNumber,
                              const QString &geometryKind,
                              int sourceVertexIndex,
                              const QPointF &oldPoint,
                              const QPointF &newPoint) {
                        MapEditorCanvasEditController(context)
                            .recordLineAreaVertexMove(changedLineNumber,
                                                      geometryKind,
                                                      sourceVertexIndex,
                                                      oldPoint,
                                                      newPoint);
                    },
                    {},
                    [context](int changedLineNumber,
                              int sourceVertexIndex,
                              qreal orientationDegrees,
                              qreal leftSize) {
                        MapEditorCanvasEditController(context)
                            .recordLinePointLeftHandleChange(changedLineNumber,
                                                             sourceVertexIndex,
                                                             orientationDegrees,
                                                             leftSize);
                    });
            addedItems = renderResult.addedItems;
            addedVertexEntries = renderResult.addedVertexIndexEntries;
            addedPrimaryItem = renderResult.addedPrimaryItem;
            renderMs = logTiming ? stageTimer.restart() : 0;
            if (removalResult.removedItems <= 0
                || renderResult.addedItems <= 0
                || !renderResult.addedPrimaryItem) {
                fallbackFullRefresh(QStringLiteral("empty-item-group"));
                return;
            }

            if (context.discardPendingSceneRefreshAfterCommand) {
                context.discardPendingSceneRefreshAfterCommand();
            }
            if (selectionRestoreHook) {
                selectionRestoreHook();
            } else if (context.updateGeometrySelectionPresentation) {
                context.updateGeometrySelectionPresentation();
            }
            selectionMs = logTiming ? stageTimer.restart() : 0;
            logPartialRefresh(false, QStringLiteral("ok"));
        };
        if (context.callbackContext != nullptr) {
            QTimer::singleShot(0, context.callbackContext, std::move(refreshLine));
        } else {
            refreshLine();
        }
    };
}

bool lineVertexMoveCanSkipFullSceneRefresh(const MapGeometryFeature &lineFeature)
{
    if (lineFeature.closed || !lineFeature.subtype.trimmed().isEmpty()) {
        return false;
    }

    for (const MapGeometryFeature::LineSegment &segment : lineFeature.lineSegments) {
        if (!segment.subtype.trimmed().isEmpty()) {
            return false;
        }
    }

    return true;
}

MapEditableGeometryVertexItem *resolveSelectedLineVertexItemForContext(const MapEditorCanvasEditContext &context)
{
    if (context.scene == nullptr) {
        return nullptr;
    }

    if (context.selectedObjectLineNumber != nullptr
        && context.selectedObjectVertexIndex != nullptr
        && context.selectedObjectKind != nullptr
        && (*context.selectedObjectLineNumber) > 0
        && (*context.selectedObjectVertexIndex) >= 0
        && (*context.selectedObjectKind) == QStringLiteral("line")) {
        MapEditableGeometryVertexItem *selectedAnchor = findGeometryVertexItem(context.scene,
                                                                               (*context.selectedObjectLineNumber),
                                                                               (*context.selectedObjectVertexIndex),
                                                                               QStringLiteral("line"));
        if (selectedAnchor != nullptr) {
            return selectedAnchor;
        }
    }

    const QList<QGraphicsItem *> selectedItems = context.scene->selectedItems();
    if (!selectedItems.isEmpty()) {
        int resolvedLineNumber = 0;
        int resolvedOwnerVertexIndex = -1;
        MapEditableGeometryVertexItem *fallbackVertex = nullptr;
        for (QGraphicsItem *selectedItem : selectedItems) {
            auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItem);
            if (vertexItem == nullptr || !vertexItem->geometryKind().startsWith(QStringLiteral("line"))) {
                continue;
            }
            const int ownerVertexIndex = selectedItem->data(kMapSceneOwnerVertexRole).toInt();
            const int candidateOwnerVertexIndex = ownerVertexIndex >= 0 ? ownerVertexIndex : vertexItem->vertexIndex();
            if (resolvedLineNumber == 0) {
                resolvedLineNumber = vertexItem->lineNumber();
                resolvedOwnerVertexIndex = candidateOwnerVertexIndex;
                fallbackVertex = vertexItem;
                continue;
            }
            if (resolvedLineNumber != vertexItem->lineNumber()
                || resolvedOwnerVertexIndex != candidateOwnerVertexIndex) {
                return nullptr;
            }
        }
        if (resolvedLineNumber > 0 && resolvedOwnerVertexIndex >= 0) {
            MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(context.scene,
                                                                                resolvedLineNumber,
                                                                                resolvedOwnerVertexIndex,
                                                                                QStringLiteral("line"));
            if (ownerAnchor != nullptr) {
                return ownerAnchor;
            }
        }
        return fallbackVertex;
    }

    return nullptr;
}

bool applyLineCoordinateRowsRewriteEdits(const QString &beforeText,
                                         int lineNumber,
                                         const QStringList &coordinateRows,
                                         QString *afterText,
                                         QString *errorMessage)
{
    if (afterText == nullptr) {
        return false;
    }

    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::lineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &edits, errorMessage)) {
        return false;
    }

    QString updatedText = beforeText;
    if (!TherionDocumentEditor::applySourceTextEdits(&updatedText, edits, errorMessage)) {
        return false;
    }

    *afterText = updatedText;
    return true;
}

}

QString MapEditorCanvasEditController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorCanvasEditController", text);
}

void MapEditorCanvasEditController::resetPendingClickSelection()
{
    (*context_.pendingClickSelection) = false;
    (*context_.pendingClickLineNumber) = 0;
    (*context_.pendingClickSourceVertexIndex) = -1;
    (*context_.pendingClickGeometryKind).clear();
}

void MapEditorCanvasEditController::restoreLineVertexOwnerSelection(int lineNumber, int ownerIndex)
{
    restoreLineVertexOwnerSelectionForContext(context_, lineNumber, ownerIndex);
}

MapEditorCanvasEditController::MapEditorCanvasEditController(MapEditorCanvasEditContext context)
    : context_(std::move(context))
{
}

void MapEditorCanvasEditController::recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition)
{
    if (context_.undoStack == nullptr || oldPosition == newPosition) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>((*context_.itemsByLine).value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    context_.undoStack->push(createMapCardMoveCommand(card, oldPosition, newPosition));
}

void MapEditorCanvasEditController::recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible)
{
    if (context_.undoStack == nullptr || oldVisible == newVisible) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>((*context_.itemsByLine).value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    context_.undoStack->push(createMapCardVisibilityCommand(card, oldVisible, newVisible));
}

void MapEditorCanvasEditController::recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint)
{
    if (lineNumber <= 0 || !sourcePointsDifferForCommands(oldPoint, newPoint) || context_.textEditor == nullptr) {
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QVector<TherionSourceTextEdit> sourceEdits;
    QString errorMessage;
    if (!TherionDocumentEditor::pointCoordinateRewriteEdits(afterText,
                                                            lineNumber,
                                                            newPoint,
                                                            &sourceEdits,
                                                            &errorMessage)
        || !TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Point move failed.")
            : tr("Point move failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    auto selectionRestoreHook = [this, lineNumber]() {
        restorePointSelection(lineNumber);
        schedulePointSelectionRecovery(context_, lineNumber);
    };
    TextEditorSourceTransactionRequest request =
        sourceTransactionRequest(context_,
                                 tr("Move Point"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook));
    request.sourceEdits = std::move(sourceEdits);
    sourceTransactionController(context_).applyChangeWithSnapshot(request);
    (*context_.toolbarStatusNote) = tr("Updated point geometry at source line %1.").arg(lineNumber);
    context_.refreshToolbarSummary();
}

void MapEditorCanvasEditController::recordLineAreaVertexMove(int lineNumber,
                                            const QString &kind,
                                            int vertexIndex,
                                            const QPointF &oldPoint,
                                            const QPointF &newPoint)
{
    if (lineNumber <= 0 || vertexIndex < 0 || !sourcePointsDifferForCommands(oldPoint, newPoint) || context_.textEditor == nullptr) {
        return;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    const QString rewriteKind = normalizedKind.startsWith(QStringLiteral("line"))
        ? QStringLiteral("line")
        : (normalizedKind.startsWith(QStringLiteral("area")) ? QStringLiteral("area") : normalizedKind);
    if (rewriteKind != QStringLiteral("line") && rewriteKind != QStringLiteral("area")) {
        (*context_.toolbarStatusNote) = tr("Geometry move failed: unsupported geometry kind '%1'.").arg(kind);
        context_.refreshToolbarSummary();
        return;
    }

    QVector<MapLineAreaVertexSecondaryMove> secondaryMoves;
    int lineOwnerIndexToRestore = -1;
    int lineOwnerSourceVertexIndexToRestore = -1;
    bool canSkipFullSceneRefresh = false;
    const std::optional<QRectF> previousSourceBounds =
        context_.mapSourceBoundsForCurrentDocument
            ? std::optional<QRectF>(context_.mapSourceBoundsForCurrentDocument())
            : std::nullopt;
    if (rewriteKind == QStringLiteral("line")) {
        const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), lineNumber);
        if (lineFeature.has_value()) {
            lineOwnerIndexToRestore = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), vertexIndex);
            if (lineOwnerIndexToRestore >= 0 && lineOwnerIndexToRestore < lineFeature->lineVertices.size()) {
                const MapGeometryFeature::TH2LineVertex &ownerVertex =
                    lineFeature->lineVertices.at(lineOwnerIndexToRestore);
                lineOwnerSourceVertexIndexToRestore = ownerVertex.anchorSourceVertexIndex >= 0
                    ? ownerVertex.anchorSourceVertexIndex
                    : lineOwnerIndexToRestore;
            }
            canSkipFullSceneRefresh = lineVertexMoveCanSkipFullSceneRefresh(lineFeature.value());
            secondaryMoves = coupledLineVertexMoveSet(lineFeature.value(), vertexIndex, oldPoint, newPoint).secondaryMoves;
            for (auto it = secondaryMoves.begin(); it != secondaryMoves.end();) {
                if (it->vertexIndex == vertexIndex) {
                    it = secondaryMoves.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QVector<TherionSourceTextEdit> sourceEdits;
    QVector<TherionSourceTextEdit> primarySourceEdits;
    QString errorMessage;
    if (!TherionDocumentEditor::lineAreaVertexRewriteEdits(afterText,
                                                           lineNumber,
                                                           rewriteKind,
                                                           vertexIndex,
                                                           newPoint,
                                                           &sourceEdits,
                                                           &errorMessage)
        || !TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("%1 vertex move failed.").arg(rewriteKind)
            : tr("%1 vertex move failed: %2").arg(rewriteKind, errorMessage);
        context_.refreshToolbarSummary();
        return;
    }
    primarySourceEdits = sourceEdits;

    for (const MapLineAreaVertexSecondaryMove &move : secondaryMoves) {
        if (move.vertexIndex < 0) {
            continue;
        }
        sourceEdits.clear();
        if (!TherionDocumentEditor::lineAreaVertexRewriteEdits(afterText,
                                                               lineNumber,
                                                               rewriteKind,
                                                               move.vertexIndex,
                                                               move.newPoint,
                                                               &sourceEdits,
                                                               &errorMessage)
            || !TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
            (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                ? tr("%1 coupled vertex move failed.").arg(rewriteKind)
                : tr("%1 coupled vertex move failed: %2").arg(rewriteKind, errorMessage);
            context_.refreshToolbarSummary();
            return;
        }
    }

    if (afterText == beforeText) {
        return;
    }

    auto selectionRestoreHook = [context = context_, lineNumber, lineOwnerSourceVertexIndexToRestore]() {
        if (lineOwnerSourceVertexIndexToRestore < 0) {
            return;
        }
        if (!restoreLineVertexSourceSelectionForContext(context, lineNumber, lineOwnerSourceVertexIndexToRestore)) {
            scheduleLineVertexSourceSelectionRecovery(context, lineNumber, lineOwnerSourceVertexIndexToRestore);
        }
    };
    TextEditorSourceTransactionRequest request =
        sourceTransactionRequest(context_,
                                 tr("Move %1 Vertex").arg(rewriteKind),
                                 beforeText,
                                 afterText,
                                 lineNumber);
    request.projectionInvalidationPolicy = TextEditorSourceProjectionInvalidationPolicy::CustomHook;
    if (canSkipFullSceneRefresh) {
        request.projectionInvalidationHook = deferredMapSelectionRestoreHook(context_, std::move(selectionRestoreHook));
    } else if (rewriteKind == QStringLiteral("line") && lineOwnerSourceVertexIndexToRestore >= 0) {
        request.projectionInvalidationHook =
            deferredMapLinePartialRefreshHook(context_, lineNumber, previousSourceBounds, std::move(selectionRestoreHook));
    } else {
        request.projectionInvalidationHook = deferredMapSceneRefreshHook(context_, std::move(selectionRestoreHook));
    }
    if (secondaryMoves.isEmpty()) {
        request.sourceEdits = std::move(primarySourceEdits);
    }
    sourceTransactionController(context_).applyChangeWithSnapshot(request);
    (*context_.toolbarStatusNote) = tr("Updated %1 vertex %2 at source line %3.")
        .arg(rewriteKind)
        .arg(vertexIndex + 1)
        .arg(lineNumber);
    context_.refreshToolbarSummary();
}

void MapEditorCanvasEditController::recordPointOrientationHandleChange(int lineNumber, qreal orientationDegrees)
{
    if (lineNumber <= 0 || context_.textEditor == nullptr) {
        return;
    }

    resetPendingClickSelection();

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QVector<TherionSourceTextEdit> sourceEdits;
    QString errorMessage;
    if (!TherionDocumentEditor::pointOrientationRewriteEdits(afterText,
                                                             lineNumber,
                                                             true,
                                                             orientationDegrees,
                                                             &sourceEdits,
                                                             &errorMessage)
        || !TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Point orientation update failed.")
            : tr("Point orientation update failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    auto selectionRestoreHook = [this, lineNumber]() {
        restorePointSelection(lineNumber);
        schedulePointSelectionRecovery(context_, lineNumber);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Edit Point Orientation"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));
    (*context_.toolbarStatusNote) = tr("Updated point orientation to %1 degrees.")
        .arg(QString::number(normalizeOrientationDegreesForMapDetails(orientationDegrees), 'f', 1));
    context_.refreshToolbarSummary();
}

void MapEditorCanvasEditController::recordLinePointLeftHandleChange(int lineNumber,
                                                   int sourceVertexIndex,
                                                   qreal orientationDegrees,
                                                   qreal leftSize)
{
    if (lineNumber <= 0 || sourceVertexIndex < 0 || context_.textEditor == nullptr) {
        return;
    }

    resetPendingClickSelection();

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QVector<TherionSourceTextEdit> sourceEdits;
    QString errorMessage;
    if (!TherionDocumentEditor::linePointOrientationRewriteEdits(afterText,
                                                                 lineNumber,
                                                                 sourceVertexIndex,
                                                                 true,
                                                                 orientationDegrees,
                                                                 &sourceEdits,
                                                                 &errorMessage)
        || !TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Line point orientation update failed.")
            : tr("Line point orientation update failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    sourceEdits.clear();
    if (!TherionDocumentEditor::linePointLeftSizeRewriteEdits(afterText,
                                                              lineNumber,
                                                              sourceVertexIndex,
                                                              true,
                                                              leftSize,
                                                              &sourceEdits,
                                                              &errorMessage)
        || !TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Line point l-size update failed.")
            : tr("Line point l-size update failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    auto selectionRestoreHook = [this, lineNumber, sourceVertexIndex]() {
        restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
        scheduleLineAnchorSelectionRecovery(context_, lineNumber, sourceVertexIndex);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Edit Line Point Options"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));
    (*context_.toolbarStatusNote) = tr("Updated line point orientation %1 deg and l-size %2.")
        .arg(QString::number(orientationDegrees, 'f', 1),
             QString::number(leftSize, 'f', 1));
    context_.refreshToolbarSummary();
}

void MapEditorCanvasEditController::restorePointSelection(int lineNumber)
{
    if (context_.scene == nullptr || lineNumber <= 0) {
        return;
    }

    auto selectedItemIt = (*context_.itemsByLine).find(lineNumber);
    if (selectedItemIt == (*context_.itemsByLine).end()) {
        return;
    }

    auto *pointItem = dynamic_cast<MapEditablePointItem *>(selectedItemIt.value());
    if (pointItem == nullptr) {
        return;
    }

    {
        const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
        context_.scene->clearSelection();
        pointItem->setVisible(true);
        pointItem->setSelected(true);
    }

    (*context_.selectedObjectLineNumber) = lineNumber;
    (*context_.selectedObjectVertexIndex) = -1;
    (*context_.selectedObjectKind) = QStringLiteral("point");
    (*context_.selectedObjectCoordinate) = pointItem->sourcePoint();
    context_.updateGeometrySelectionPresentation();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
}

void MapEditorCanvasEditController::restoreLineAnchorSelection(int lineNumber, int sourceVertexIndex)
{
    if (context_.scene == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return;
    }

    MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(context_.scene,
                                                                        lineNumber,
                                                                        sourceVertexIndex,
                                                                        QStringLiteral("line"));
    if (ownerAnchor == nullptr) {
        return;
    }

    {
        const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
        context_.scene->clearSelection();
        ownerAnchor->setVisible(true);
        ownerAnchor->setSelected(true);
    }

    (*context_.selectedObjectLineNumber) = lineNumber;
    (*context_.selectedObjectVertexIndex) = sourceVertexIndex;
    (*context_.selectedObjectKind) = QStringLiteral("line");
    (*context_.selectedObjectCoordinate) = context_.sourcePointFromScenePosition(ownerAnchor->pos());
    context_.updateGeometrySelectionPresentation();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
}

TextEditorSourceTransactionResult MapEditorCanvasEditController::recordSourceTextSnapshot(const QString &label,
                                                                                          const QString &beforeText,
                                                                                          const QString &afterText,
                                                                                          int insertedLineNumber)
{
    return sourceTransactionController(context_).recordSnapshot(
        sourceTransactionRequest(context_, label, beforeText, afterText, insertedLineNumber));
}

TextEditorSourceTransactionResult MapEditorCanvasEditController::applySourceTextChangeWithSnapshot(
    const QString &label,
    const QString &beforeText,
    const QString &afterText,
    int insertedLineNumber,
    TextEditorSourceSelectionRestorePolicy selectionRestorePolicy,
    std::function<void()> selectionRestoreHook)
{
    return sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 label,
                                 beforeText,
                                 afterText,
                                 insertedLineNumber,
                                 selectionRestorePolicy,
                                 std::move(selectionRestoreHook)));
}

TextEditorSourceTransactionResult MapEditorCanvasEditController::applySourceTextChangeWithSnapshotDeferredProjection(
    const QString &label,
    const QString &beforeText,
    const QString &afterText,
    int insertedLineNumber,
    std::function<void()> projectionInvalidationHook,
    TextEditorSourceSelectionRestorePolicy selectionRestorePolicy,
    std::function<void()> selectionRestoreHook)
{
    TextEditorSourceTransactionRequest request = sourceTransactionRequest(context_,
                                                                          label,
                                                                          beforeText,
                                                                          afterText,
                                                                          insertedLineNumber,
                                                                          selectionRestorePolicy,
                                                                          std::move(selectionRestoreHook));
    request.projectionInvalidationPolicy = TextEditorSourceProjectionInvalidationPolicy::CustomHook;
    request.projectionInvalidationHook = std::move(projectionInvalidationHook);
    return sourceTransactionController(context_).applyChangeWithSnapshot(request);
}

TextEditorSourceTransactionResult MapEditorCanvasEditController::applySourceEditsWithSnapshotDeferredProjection(
    const QString &label,
    const QString &beforeText,
    QVector<TherionSourceTextEdit> sourceEdits,
    int insertedLineNumber,
    std::function<void()> projectionInvalidationHook,
    TextEditorSourceSelectionRestorePolicy selectionRestorePolicy,
    std::function<void()> selectionRestoreHook)
{
    TextEditorSourceTransactionRequest request = sourceTransactionRequest(context_,
                                                                          label,
                                                                          beforeText,
                                                                          QString(),
                                                                          insertedLineNumber,
                                                                          selectionRestorePolicy,
                                                                          std::move(selectionRestoreHook));
    request.sourceEdits = std::move(sourceEdits);
    request.projectionInvalidationPolicy = TextEditorSourceProjectionInvalidationPolicy::CustomHook;
    request.projectionInvalidationHook = std::move(projectionInvalidationHook);
    request.rebuildBlocksCanvasOnApply = false;
    return sourceTransactionController(context_).applyChangeWithSnapshot(request);
}

bool MapEditorCanvasEditController::insertLineVertexFromSelection(MapEditorLineVertexInsertPlacement placement)
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    int lineNumber = 0;
    int selectedSourceVertexIndex = -1;
    if (MapEditableGeometryVertexItem *vertexItem = resolveSelectedLineVertexItemForContext(context_);
        vertexItem != nullptr && vertexItem->geometryKind().startsWith(QStringLiteral("line"))) {
        lineNumber = vertexItem->lineNumber();
        selectedSourceVertexIndex = vertexItem->vertexIndex();
    }
    if (lineNumber <= 0
        && context_.selectedObjectLineNumber != nullptr
        && context_.selectedObjectVertexIndex != nullptr
        && context_.selectedObjectKind != nullptr
        && (*context_.selectedObjectKind) == QStringLiteral("line")
        && (*context_.selectedObjectLineNumber) > 0
        && (*context_.selectedObjectVertexIndex) >= 0) {
        lineNumber = (*context_.selectedObjectLineNumber);
        selectedSourceVertexIndex = (*context_.selectedObjectVertexIndex);
    }
    if (lineNumber <= 0 || selectedSourceVertexIndex < 0) {
        return false;
    }

    const QString beforeText = context_.textEditor->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int anchorIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), selectedSourceVertexIndex);
    if (anchorIndex < 0) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: selected line anchor could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }
    const int anchorSourceVertexIndex = lineFeature->lineVertices.at(anchorIndex).anchorSourceVertexIndex >= 0
        ? lineFeature->lineVertices.at(anchorIndex).anchorSourceVertexIndex
        : anchorIndex;

    const bool insertBefore = placement == MapEditorLineVertexInsertPlacement::Before;
    const bool prependExtension = insertBefore && anchorIndex == 0;
    const bool appendExtension = !insertBefore && anchorIndex == lineFeature->lineVertices.size() - 1;
    const bool endpointExtension = prependExtension || appendExtension;
    if (endpointExtension) {
        if (!context_.beginLineExtensionFromSelection
            || !context_.beginLineExtensionFromSelection(lineNumber, anchorSourceVertexIndex, prependExtension)) {
            (*context_.toolbarStatusNote) = tr("Extend line failed: selected endpoint could not start continuation mode.");
            context_.refreshToolbarSummary();
        }
        return true;
    }

    int segmentStartIndex = insertBefore ? anchorIndex - 1 : anchorIndex;
    if (segmentStartIndex < 0 || segmentStartIndex + 1 >= lineFeature->lineVertices.size()) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: selected line vertex has no adjacent segment in that direction.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    int insertedIndex = -1;
    if (!insertLineVertexByDeCasteljau(&editedVertices, segmentStartIndex, 0.5, &insertedIndex)) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: segment split could not be computed.");
        context_.refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);
    QString afterText;
    QString errorMessage;
    if (!applyLineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &afterText, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Insert vertex failed.")
            : tr("Insert vertex failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }

    auto selectionRestoreHook = [this, lineNumber, insertedIndex]() {
        restoreLineVertexOwnerSelection(lineNumber, insertedIndex);
        scheduleLineVertexOwnerSelectionRecovery(context_, lineNumber, insertedIndex);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Insert Line Vertex"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));

    (*context_.toolbarStatusNote) = tr("Inserted line vertex %1 on source line %2.").arg(insertedIndex + 1).arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::insertLineVertexAtSelectionCoordinate()
{
    if (context_.textEditor == nullptr
        || context_.selectedObjectLineNumber == nullptr
        || context_.selectedObjectKind == nullptr
        || context_.selectedObjectCoordinate == nullptr
        || (*context_.selectedObjectKind) != QStringLiteral("line")
        || (*context_.selectedObjectLineNumber) <= 0
        || !context_.selectedObjectCoordinate->has_value()) {
        return false;
    }

    const int lineNumber = *context_.selectedObjectLineNumber;
    const QPointF insertionPoint = context_.selectedObjectCoordinate->value();
    const QString beforeText = context_.textEditor->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineNumber);
    if (!lineFeature.has_value() || lineFeature->lineVertices.size() < 2) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    int segmentStartIndex = -1;
    qreal splitT = 0.5;
    qreal bestDistanceSquared = std::numeric_limits<qreal>::max();
    for (int index = 0; index + 1 < lineFeature->lineVertices.size(); ++index) {
        qreal candidateT = 0.5;
        const qreal distanceSquared = distanceSquaredToCubicSegment(insertionPoint,
                                                                    lineFeature->lineVertices.at(index),
                                                                    lineFeature->lineVertices.at(index + 1),
                                                                    &candidateT);
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            segmentStartIndex = index;
            splitT = candidateT;
        }
    }

    if (segmentStartIndex < 0) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: no line segment could be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    int insertedIndex = -1;
    if (!insertLineVertexByDeCasteljau(&editedVertices, segmentStartIndex, splitT, &insertedIndex)) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: segment split could not be computed.");
        context_.refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);
    QString afterText;
    QString errorMessage;
    if (!applyLineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &afterText, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Insert vertex failed.")
            : tr("Insert vertex failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }

    auto selectionRestoreHook = [this, lineNumber, insertedIndex]() {
        restoreLineVertexOwnerSelection(lineNumber, insertedIndex);
        scheduleLineVertexOwnerSelectionRecovery(context_, lineNumber, insertedIndex);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Insert Line Vertex"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));

    (*context_.toolbarStatusNote) = tr("Inserted line vertex %1 on source line %2.").arg(insertedIndex + 1).arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::splitLineAtSelection()
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    int lineNumber = 0;
    int selectedSourceVertexIndex = -1;
    if (MapEditableGeometryVertexItem *vertexItem = resolveSelectedLineVertexItemForContext(context_);
        vertexItem != nullptr && vertexItem->geometryKind().startsWith(QStringLiteral("line"))) {
        lineNumber = vertexItem->lineNumber();
        selectedSourceVertexIndex = vertexItem->vertexIndex();
    }
    if (lineNumber <= 0
        && context_.selectedObjectLineNumber != nullptr
        && context_.selectedObjectVertexIndex != nullptr
        && context_.selectedObjectKind != nullptr
        && (*context_.selectedObjectKind) == QStringLiteral("line")
        && (*context_.selectedObjectLineNumber) > 0
        && (*context_.selectedObjectVertexIndex) >= 0) {
        lineNumber = (*context_.selectedObjectLineNumber);
        selectedSourceVertexIndex = (*context_.selectedObjectVertexIndex);
    }
    if (lineNumber <= 0 || selectedSourceVertexIndex < 0) {
        return false;
    }

    const QString beforeText = context_.textEditor->text();

    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Split line failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }
    if (lineFeature->closed) {
        (*context_.toolbarStatusNote) = tr("Split line failed: closed lines cannot be split yet.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int anchorIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), selectedSourceVertexIndex);
    if (anchorIndex <= 0 || anchorIndex >= lineFeature->lineVertices.size() - 1) {
        (*context_.toolbarStatusNote) = tr("Split line failed: select an interior line vertex.");
        context_.refreshToolbarSummary();
        return true;
    }

    const QVector<MapGeometryFeature::TH2LineVertex> firstVertices =
        lineFeature->lineVertices.mid(0, anchorIndex + 1);
    const QVector<MapGeometryFeature::TH2LineVertex> secondVertices =
        lineFeature->lineVertices.mid(anchorIndex);
    const QStringList firstRows = coordinateRowsForLineVertices(firstVertices);
    const QStringList secondRows = coordinateRowsForLineVertices(secondVertices);
    if (firstRows.isEmpty() || secondRows.isEmpty()) {
        (*context_.toolbarStatusNote) = tr("Split line failed: split produced invalid line geometry.");
        context_.refreshToolbarSummary();
        return true;
    }

    const MapEditorLineSplitPlan splitPlan =
        MapEditorLineSplitPlanner::planSplit(beforeText, lineNumber, firstRows, secondRows);
    if (!splitPlan.resolved) {
        (*context_.toolbarStatusNote) = splitPlan.errorMessage.isEmpty()
            ? tr("Split line failed.")
            : tr("Split line failed: %1").arg(splitPlan.errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }
    if (!splitPlan.changed) {
        return true;
    }

    const QString afterText = splitPlan.updatedText;
    auto selectionRestoreHook = [this, lineNumber, anchorIndex]() {
        restoreLineVertexOwnerSelection(lineNumber, anchorIndex);
        scheduleLineVertexOwnerSelectionRecovery(context_, lineNumber, anchorIndex);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Split Line"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));

    (*context_.toolbarStatusNote) = splitPlan.areaReferencesUpdated
        ? tr("Split line at vertex %1 on source line %2 and updated area references.").arg(anchorIndex + 1).arg(lineNumber)
        : tr("Split line at vertex %1 on source line %2.").arg(anchorIndex + 1).arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::removeLineVertexFromSelection()
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    int lineNumber = 0;
    int selectedSourceVertexIndex = -1;
    if (MapEditableGeometryVertexItem *vertexItem = resolveSelectedLineVertexItemForContext(context_);
        vertexItem != nullptr && vertexItem->geometryKind().startsWith(QStringLiteral("line"))) {
        lineNumber = vertexItem->lineNumber();
        selectedSourceVertexIndex = vertexItem->vertexIndex();
    }
    if (lineNumber <= 0
        && context_.selectedObjectLineNumber != nullptr
        && context_.selectedObjectVertexIndex != nullptr
        && context_.selectedObjectKind != nullptr
        && (*context_.selectedObjectKind) == QStringLiteral("line")
        && (*context_.selectedObjectLineNumber) > 0
        && (*context_.selectedObjectVertexIndex) >= 0) {
        lineNumber = (*context_.selectedObjectLineNumber);
        selectedSourceVertexIndex = (*context_.selectedObjectVertexIndex);
    }
    if (lineNumber <= 0 || selectedSourceVertexIndex < 0) {
        return false;
    }

    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Delete vertex failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int ownerIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), selectedSourceVertexIndex);
    if (ownerIndex < 0) {
        (*context_.toolbarStatusNote) = tr("Delete vertex failed: selected line anchor could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    const QStringList standaloneRows = editedVertices.at(ownerIndex).standaloneOptionRows;
    if (!confirmLinePointRowDelete(context_, standaloneRows, lineNumber, ownerIndex + 1)) {
        (*context_.toolbarStatusNote) = tr("Vertex delete canceled.");
        context_.refreshToolbarSummary();
        return true;
    }
    if (ownerIndex == 0 || ownerIndex == editedVertices.size() - 1) {
        if (editedVertices.size() <= 2) {
            (*context_.toolbarStatusNote) = tr("Delete vertex failed: line needs at least two vertices.");
            context_.refreshToolbarSummary();
            return true;
        }
        editedVertices.removeAt(ownerIndex);
    } else {
        if (!removeLineVertexWithReconnect(&editedVertices, ownerIndex)) {
            (*context_.toolbarStatusNote) = tr("Delete vertex failed: selected anchor could not be removed while keeping a valid line.");
            context_.refreshToolbarSummary();
            return true;
        }
    }

    int restoredOwnerIndex = ownerIndex;
    if (restoredOwnerIndex >= editedVertices.size()) {
        restoredOwnerIndex = editedVertices.size() - 1;
    }
    if (restoredOwnerIndex < 0) {
        restoredOwnerIndex = 0;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);
    const QString beforeText = context_.textEditor->text();
    QString afterText;
    QString errorMessage;
    if (!applyLineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &afterText, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Delete vertex failed.")
            : tr("Delete vertex failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }

    auto selectionRestoreHook = [this, lineNumber, restoredOwnerIndex]() {
        restoreLineVertexOwnerSelection(lineNumber, restoredOwnerIndex);
        scheduleLineVertexOwnerSelectionRecovery(context_, lineNumber, restoredOwnerIndex);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Delete Line Vertex"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));
    (*context_.toolbarStatusNote) = tr("Removed line vertex %1 on source line %2.").arg(ownerIndex + 1).arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::toggleLineVertexSmoothFromSelection()
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    MapEditableGeometryVertexItem *vertexItem = resolveSelectedLineVertexItemForContext(context_);
    if (vertexItem == nullptr) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const QString beforeText = context_.textEditor->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Toggle smooth failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int ownerIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (ownerIndex < 0 || ownerIndex >= lineFeature->lineVertices.size()) {
        (*context_.toolbarStatusNote) = tr("Toggle smooth failed: selected line vertex could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    MapGeometryFeature::TH2LineVertex &target = editedVertices[ownerIndex];
    target.isSmooth = !target.isSmooth;
    if (target.isSmooth) {
        ensureLineVertexControlHandles(&editedVertices, ownerIndex, lineFeature->closed);
        smoothLineVertexControlHandles(&target);
    } else {
        target.incomingControl.reset();
        target.outgoingControl.reset();
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);
    QString afterText;
    QString errorMessage;
    if (!applyLineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &afterText, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Toggle smooth failed.")
            : tr("Toggle smooth failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }
    auto selectionRestoreHook = [this, lineNumber, ownerIndex]() {
        restoreLineVertexOwnerSelection(lineNumber, ownerIndex);
        scheduleLineVertexOwnerSelectionRecovery(context_, lineNumber, ownerIndex);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Toggle Line Vertex Smooth"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));

    (*context_.toolbarStatusNote) = target.isSmooth
        ? tr("Line vertex %1 on source line %2 set to smooth.").arg(ownerIndex + 1).arg(lineNumber)
        : tr("Line vertex %1 on source line %2 set to corner (smooth off).").arg(ownerIndex + 1).arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::setLineVertexSmoothForSelection(bool smooth)
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    MapEditableGeometryVertexItem *vertexItem = resolveSelectedLineVertexItemForContext(context_);
    if (vertexItem == nullptr) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const QString beforeText = context_.textEditor->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Set smooth failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int ownerIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (ownerIndex < 0 || ownerIndex >= lineFeature->lineVertices.size()) {
        (*context_.toolbarStatusNote) = tr("Set smooth failed: selected line vertex could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    MapGeometryFeature::TH2LineVertex &target = editedVertices[ownerIndex];
    if (target.isSmooth == smooth
        && (!smooth || (target.incomingControl.has_value() && target.outgoingControl.has_value()))) {
        return true;
    }

    target.isSmooth = smooth;
    if (target.isSmooth) {
        ensureLineVertexControlHandles(&editedVertices, ownerIndex, lineFeature->closed);
        smoothLineVertexControlHandles(&target);
    } else {
        target.incomingControl.reset();
        target.outgoingControl.reset();
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);
    QString afterText;
    QString errorMessage;
    if (!applyLineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &afterText, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Set smooth failed.")
            : tr("Set smooth failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }
    auto selectionRestoreHook = [this, lineNumber, ownerIndex]() {
        restoreLineVertexOwnerSelection(lineNumber, ownerIndex);
        scheduleLineVertexOwnerSelectionRecovery(context_, lineNumber, ownerIndex);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Set Line Vertex Smooth"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));

    (*context_.toolbarStatusNote) = target.isSmooth
        ? tr("Line vertex %1 on source line %2 set to smooth.").arg(ownerIndex + 1).arg(lineNumber)
        : tr("Line vertex %1 on source line %2 set to corner (smooth off).").arg(ownerIndex + 1).arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::setLineVertexControlHandleForSelection(bool incoming, bool enabled)
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    MapEditableGeometryVertexItem *vertexItem = resolveSelectedLineVertexItemForContext(context_);
    if (vertexItem == nullptr) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const QString beforeText = context_.textEditor->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Set control handle failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int ownerIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (ownerIndex < 0 || ownerIndex >= lineFeature->lineVertices.size()) {
        (*context_.toolbarStatusNote) = tr("Set control handle failed: selected line vertex could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    MapGeometryFeature::TH2LineVertex &target = editedVertices[ownerIndex];
    const bool smoothWasActive = target.isSmooth
        && target.incomingControl.has_value()
        && target.outgoingControl.has_value();
    std::optional<QPointF> &control = incoming ? target.incomingControl : target.outgoingControl;
    const bool currentlyEnabled = control.has_value();
    if (currentlyEnabled == enabled) {
        return true;
    }

    if (enabled) {
        control = incoming
            ? defaultIncomingControlForLineVertex(editedVertices, ownerIndex, lineFeature->closed)
            : defaultOutgoingControlForLineVertex(editedVertices, ownerIndex, lineFeature->closed);
        if (smoothWasActive) {
            ensureLineVertexControlHandles(&editedVertices, ownerIndex, lineFeature->closed);
            smoothLineVertexControlHandles(&target);
        } else {
            target.isSmooth = false;
        }
    } else {
        control.reset();
        target.isSmooth = false;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);
    QString afterText;
    QString errorMessage;
    if (!applyLineCoordinateRowsRewriteEdits(beforeText, lineNumber, coordinateRows, &afterText, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Set control handle failed.")
            : tr("Set control handle failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }
    auto selectionRestoreHook = [this, lineNumber, ownerIndex]() {
        restoreLineVertexOwnerSelection(lineNumber, ownerIndex);
        scheduleLineVertexOwnerSelectionRecovery(context_, lineNumber, ownerIndex);
    };
    sourceTransactionController(context_).applyChangeWithSnapshot(
        sourceTransactionRequest(context_,
                                 tr("Set Line Vertex Control Handle"),
                                 beforeText,
                                 afterText,
                                 lineNumber,
                                 TextEditorSourceSelectionRestorePolicy::CustomHook,
                                 std::move(selectionRestoreHook)));

    const QString handleLabel = incoming ? tr("previous") : tr("next");
    (*context_.toolbarStatusNote) = enabled
        ? tr("Line vertex %1 on source line %2 now uses %3 control handle.").arg(ownerIndex + 1).arg(lineNumber).arg(handleLabel)
        : tr("Line vertex %1 on source line %2 no longer uses %3 control handle.").arg(ownerIndex + 1).arg(lineNumber).arg(handleLabel);
    context_.refreshToolbarSummary();
    return true;
}

QGraphicsRectItem *MapEditorCanvasEditController::selectedDraftGeometryItem() const
{
    if (context_.scene == nullptr) {
        return nullptr;
    }

    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
    for (QGraphicsItem *item : selectedItems) {
        if (auto *draftItem = dynamic_cast<QGraphicsRectItem *>(item)) {
            if (dynamic_cast<MapDraftGeometryItem *>(draftItem) != nullptr) {
                return draftItem;
            }
        }
    }

    return nullptr;
}

QGraphicsRectItem *MapEditorCanvasEditController::createDraftGeometryItem(DraftGeometryKind kind)
{
    return new MapDraftGeometryItem((*context_.nextDraftGeometryId)++, kind);
}

void MapEditorCanvasEditController::addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    draftItem->setPos(position);
    draftItem->setMoveCommittedCallback([recordDraftMove = context_.recordDraftMove,
                                         draftItem](int, const QPointF &oldPosition, const QPointF &newPosition) {
        recordDraftMove(draftItem, oldPosition, newPosition);
    });
    draftItem->setVisibilityCommittedCallback([recordDraftVisibility = context_.recordDraftVisibility,
                                               draftItem](int, bool oldVisible, bool newVisible) {
        recordDraftVisibility(draftItem, oldVisible, newVisible);
    });
    if (context_.undoStack != nullptr) {
        context_.undoStack->push(createMapDraftAddCommand(context_.scene, context_.draftGeometryItems, draftItem, position));
    } else {
        if (context_.scene != nullptr) {
            context_.scene->addItem(draftItem);
        }
        (*context_.draftGeometryItems).append(draftItem);
    }

    context_.updateCommandSurfaceState();
}

void MapEditorCanvasEditController::removeDraftGeometryItem(QGraphicsRectItem *item)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    (*context_.draftGeometryItems).removeAll(draftItem);
    if (context_.scene != nullptr) {
        context_.scene->removeItem(draftItem);
    }
    delete draftItem;
}

QVector<QPointF> MapEditorCanvasEditController::sourceVerticesForDraft(const QGraphicsRectItem *item) const
{
    auto *draftItem = dynamic_cast<const MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return {};
    }

    const QRectF itemRect = draftItem->rect();
    const QPointF itemPosition = draftItem->pos();
    const QPointF itemCenter = itemPosition + itemRect.center();
    QVector<QPointF> previewVertices;

    switch (draftItem->kind()) {
    case DraftGeometryKind::Point:
        previewVertices.append(itemCenter);
        break;
    case DraftGeometryKind::Line:
        previewVertices.append(itemPosition + QPointF(itemRect.left() + 16.0, itemRect.center().y()));
        previewVertices.append(itemPosition + QPointF(itemRect.right() - 16.0, itemRect.center().y()));
        break;
    case DraftGeometryKind::Area:
        previewVertices.append(itemPosition + QPointF(itemRect.center().x(), itemRect.top() + 16.0));
        previewVertices.append(itemPosition + QPointF(itemRect.right() - 16.0, itemRect.bottom() - 16.0));
        previewVertices.append(itemPosition + QPointF(itemRect.left() + 16.0, itemRect.bottom() - 16.0));
        break;
    }

    if (previewVertices.isEmpty()) {
        return {};
    }

    const QRectF previewBounds = context_.mapPreviewBounds();
    const QRectF sourceBounds = context_.mapSourceBoundsForCurrentDocument();
    if (!previewBounds.isValid() || !sourceBounds.isValid() || sourceBounds.width() < 0.001 || sourceBounds.height() < 0.001) {
        return previewVertices;
    }

    QVector<QPointF> sourceVertices;
    sourceVertices.reserve(previewVertices.size());
    for (const QPointF &previewVertex : std::as_const(previewVertices)) {
        sourceVertices.append(previewToSourcePoint(previewVertex, sourceBounds, previewBounds));
    }

    return sourceVertices;
}

QPointF MapEditorCanvasEditController::previewToSourcePoint(const QPointF &previewPoint, const QRectF &sourceBounds, const QRectF &previewBounds) const
{
    if (!sourceBounds.isValid() || !previewBounds.isValid() || previewBounds.width() < 0.001 || previewBounds.height() < 0.001) {
        return previewPoint;
    }

    // Use the shared inverse transform without clamping so drafted points
    // outside the fitted geometry strip keep their relative shape instead of
    // collapsing onto one boundary axis.
    return mapGeometryPreviewToSource(previewPoint, sourceBounds, previewBounds);
}

void MapEditorCanvasEditController::recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || context_.undoStack == nullptr || oldPosition == newPosition) {
        return;
    }

    context_.undoStack->push(createMapDraftMoveCommand(draftItem, oldPosition, newPosition));
}

void MapEditorCanvasEditController::recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || context_.undoStack == nullptr || oldVisible == newVisible) {
        return;
    }

    context_.undoStack->push(createMapDraftVisibilityCommand(draftItem, oldVisible, newVisible));
}

void MapEditorCanvasEditController::recordDraftCompletion(QGraphicsRectItem *item,
                                                          const QString &label,
                                                          const QString &beforeText,
                                                          const QString &afterText,
                                                          int insertedLineNumber)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || context_.textEditor == nullptr || beforeText == afterText) {
        return;
    }

    if (context_.undoStack == nullptr) {
        removeDraftGeometryItem(draftItem);
        return;
    }

    auto lifecycle = std::make_shared<MapDraftCompletionLifecycle>(context_.scene,
                                                                   context_.draftGeometryItems,
                                                                   draftItem);
    TextEditorSourceTransactionRequest request = sourceTransactionRequest(context_,
                                                                          label,
                                                                          beforeText,
                                                                          afterText,
                                                                          insertedLineNumber);
    request.initialRedoHook = [lifecycle]() { lifecycle->detachDraftItem(); };
    request.undoHook = [lifecycle]() { lifecycle->restoreDraftItem(); };
    request.redoHook = [lifecycle]() { lifecycle->detachDraftItem(); };
    request.undoStatusMessage = completedDraftRevertedMessage(insertedLineNumber);
    request.redoStatusMessage = completedDraftRestoredMessage(insertedLineNumber);

    sourceTransactionController(context_).recordSnapshot(request);
}

}
