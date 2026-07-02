#include "MapEditorSelectionController.h"

#include "../TextEditorTab.h"
#include "MapEditorAreaReferenceResolver.h"
#include "MapEditorLineDecorationItem.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceLogicalDocument.h"

#include <QCursor>
#include <QElapsedTimer>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScopedValueRollback>
#include <QTransform>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr qreal kFilledPathInteriorHitDistancePixels = 0.0;
constexpr qreal kMaximumPathPrimaryHitDistancePixels = 5.0;

bool isInteractiveMapSelectionItem(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return false;
    }
    const bool gatedSelectionItem = item->data(kMapSceneSelectionGatedRole).toBool();
    if (!item->isVisible() && !gatedSelectionItem) {
        return false;
    }
    if (!(item->flags() & QGraphicsItem::ItemIsSelectable)) {
        return false;
    }
    if (item->acceptedMouseButtons() == Qt::NoButton) {
        return false;
    }
    return true;
}

int mapSelectionHitPriority(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return 100;
    }

    if (dynamic_cast<const MapEditablePointItem *>(item) != nullptr) {
        return 0;
    }
    if (auto *vertexItem = dynamic_cast<const MapEditableGeometryVertexItem *>(item)) {
        const QString geometryKind = vertexItem->geometryKind().trimmed().toLower();
        if (geometryKind.startsWith(QStringLiteral("line control"))) {
            return 0;
        }
        if (geometryKind == QStringLiteral("line") || geometryKind == QStringLiteral("area")) {
            return 1;
        }
    }

    const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
    switch (subtype) {
    case kMapSceneSelectionSubtypeLineControl:
        return 0;
    case kMapSceneSelectionSubtypeLineAnchor:
    case kMapSceneSelectionSubtypeAreaVertex:
        return 1;
    case kMapSceneSelectionSubtypePointOrientationHandle:
        return 1;
    case kMapSceneSelectionSubtypeLineControlConnector:
        return 3;
    case kMapSceneSelectionSubtypeLineDetail:
        return 4;
    case kMapSceneSelectionSubtypeAreaFill:
        return 5;
    default:
        break;
    }

    return 1;
}

qreal itemUnitToViewPixels(const QGraphicsItem *item, const QTransform &viewTransform)
{
    if (item == nullptr) {
        return 1.0;
    }

    const QPointF originScene = item->mapToScene(QPointF(0.0, 0.0));
    const QPointF xScene = item->mapToScene(QPointF(1.0, 0.0));
    const QPointF yScene = item->mapToScene(QPointF(0.0, 1.0));
    const QPointF originView = viewTransform.map(originScene);
    const QPointF xDelta = viewTransform.map(xScene) - originView;
    const QPointF yDelta = viewTransform.map(yScene) - originView;
    const qreal xScale = std::hypot(xDelta.x(), xDelta.y());
    const qreal yScale = std::hypot(yDelta.x(), yDelta.y());
    return qMax<qreal>(0.001, qMax(xScale, yScale));
}

qreal distanceToSegment(const QPointF &point, const QPointF &segmentStart, const QPointF &segmentEnd)
{
    const QPointF segment = segmentEnd - segmentStart;
    const qreal lengthSquared = (segment.x() * segment.x()) + (segment.y() * segment.y());
    if (lengthSquared <= 1e-9) {
        return std::hypot(point.x() - segmentStart.x(), point.y() - segmentStart.y());
    }

    const QPointF pointDelta = point - segmentStart;
    const qreal projection = ((pointDelta.x() * segment.x()) + (pointDelta.y() * segment.y())) / lengthSquared;
    const QPointF closestPoint = segmentStart + (segment * qBound<qreal>(0.0, projection, 1.0));
    return std::hypot(point.x() - closestPoint.x(), point.y() - closestPoint.y());
}

qreal genericPathItemHitDistancePixels(const QGraphicsItem *item,
                                       const QPointF &scenePosition,
                                       const QTransform &viewTransform)
{
    if (const auto *decorationItem = dynamic_cast<const MapEditorLineDecorationItem *>(item)) {
        return decorationItem->hitDistancePixels(scenePosition, viewTransform);
    }

    if (const auto *lineItem = dynamic_cast<const QGraphicsLineItem *>(item)) {
        const QPointF localPosition = lineItem->mapFromScene(scenePosition);
        const qreal viewScale = itemUnitToViewPixels(lineItem, viewTransform);
        const qreal strokeRadiusPixels = lineItem->pen().widthF() * 0.5;
        const qreal tolerance = (strokeRadiusPixels + 2.0) / viewScale;
        const qreal distance = distanceToSegment(localPosition, lineItem->line().p1(), lineItem->line().p2());
        return distance <= tolerance ? distance * viewScale : std::numeric_limits<qreal>::max();
    }

    const auto *pathItem = dynamic_cast<const QGraphicsPathItem *>(item);
    if (pathItem == nullptr) {
        return 0.0;
    }

    const QPainterPath path = pathItem->path();
    const QPointF localPosition = pathItem->mapFromScene(scenePosition);
    if (pathItem->brush().style() != Qt::NoBrush && path.contains(localPosition)) {
        return kFilledPathInteriorHitDistancePixels;
    }

    const qreal viewScale = itemUnitToViewPixels(pathItem, viewTransform);
    const qreal strokeRadiusPixels = pathItem->pen().widthF() * 0.5;
    const qreal tolerance = (strokeRadiusPixels + 4.0) / viewScale;
    if (!path.boundingRect().adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(localPosition)) {
        return std::numeric_limits<qreal>::max();
    }

    const QList<QPolygonF> polygons = path.toSubpathPolygons();
    if (polygons.isEmpty()) {
        return std::numeric_limits<qreal>::max();
    }

    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (const QPolygonF &polygon : polygons) {
        for (int index = 0; index + 1 < polygon.size(); ++index) {
            bestDistance = qMin(bestDistance,
                                distanceToSegment(localPosition, polygon.at(index), polygon.at(index + 1)));
        }
    }
    return bestDistance <= tolerance ? bestDistance * viewScale : std::numeric_limits<qreal>::max();
}

qreal vertexLikeItemHitDistancePixels(const QGraphicsItem *item,
                                      const QPointF &scenePosition,
                                      const QTransform &viewTransform)
{
    if (item == nullptr) {
        return std::numeric_limits<qreal>::max();
    }

    const QRectF rect = dynamic_cast<const MapEditableGeometryVertexItem *>(item) != nullptr
        ? static_cast<const MapEditableGeometryVertexItem *>(item)->rect()
        : item->boundingRect();
    const QPointF localPosition = item->mapFromScene(scenePosition);
    const QPointF delta = localPosition - rect.center();
    return std::hypot(delta.x(), delta.y()) * itemUnitToViewPixels(item, viewTransform);
}

bool isDistanceRankedPathSubtype(int subtype)
{
    return subtype == kMapSceneSelectionSubtypeGeneric
        || subtype == kMapSceneSelectionSubtypeLineDetail
        || subtype == kMapSceneSelectionSubtypeAreaFill
        || subtype == kMapSceneSelectionSubtypeLineControlConnector;
}

bool isDistanceRankedPathItem(const QGraphicsItem *item, int subtype)
{
    return isDistanceRankedPathSubtype(subtype)
        && (dynamic_cast<const QGraphicsPathItem *>(item) != nullptr
            || dynamic_cast<const QGraphicsLineItem *>(item) != nullptr
            || dynamic_cast<const MapEditorLineDecorationItem *>(item) != nullptr);
}

QRectF sceneProbeRectForViewRadius(const QPointF &scenePosition,
                                   const QTransform &viewTransform,
                                   qreal radiusPixels)
{
    bool invertible = false;
    const QTransform sceneFromView = viewTransform.inverted(&invertible);
    if (!invertible) {
        const qreal fallbackRadius = qMax<qreal>(1.0, radiusPixels);
        return QRectF(scenePosition.x() - fallbackRadius,
                      scenePosition.y() - fallbackRadius,
                      fallbackRadius * 2.0,
                      fallbackRadius * 2.0);
    }

    const QPointF viewPosition = viewTransform.map(scenePosition);
    const QRectF viewRect(viewPosition.x() - radiusPixels,
                          viewPosition.y() - radiusPixels,
                          radiusPixels * 2.0,
                          radiusPixels * 2.0);
    return sceneFromView.mapRect(viewRect);
}

QGraphicsItem *preferredMapHitItem(const QList<QGraphicsItem *> &hitItems,
                                   bool requireSelected = false,
                                   std::optional<QPointF> scenePosition = std::nullopt,
                                   const QTransform &viewTransform = QTransform())
{
    QGraphicsItem *bestItem = nullptr;
    int bestPriority = std::numeric_limits<int>::max();
    qreal bestDistancePixels = std::numeric_limits<qreal>::max();
    bool bestUsesDistanceRanking = false;
    for (QGraphicsItem *item : hitItems) {
        if (!isInteractiveMapSelectionItem(item)) {
            continue;
        }
        if (requireSelected && !item->isSelected()) {
            continue;
        }
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber <= 0) {
            continue;
        }

        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        qreal distancePixels = 0.0;
        const bool usesDistanceRanking = scenePosition.has_value() && isDistanceRankedPathItem(item, subtype);
        if (scenePosition.has_value()) {
            if (usesDistanceRanking) {
                distancePixels = genericPathItemHitDistancePixels(item, scenePosition.value(), viewTransform);
                if (!std::isfinite(distancePixels)
                    || distancePixels == std::numeric_limits<qreal>::max()
                    || distancePixels > kMaximumPathPrimaryHitDistancePixels) {
                    continue;
                }
            } else if (dynamic_cast<const MapEditableGeometryVertexItem *>(item) != nullptr
                       || subtype == kMapSceneSelectionSubtypeLineAnchor
                       || subtype == kMapSceneSelectionSubtypeLineControl
                       || subtype == kMapSceneSelectionSubtypeAreaVertex) {
                distancePixels = vertexLikeItemHitDistancePixels(item, scenePosition.value(), viewTransform);
            }
        }

        const int priority = mapSelectionHitPriority(item);
        const bool betterDistanceRankedPath = usesDistanceRanking
            && ((bestUsesDistanceRanking
                 && (distancePixels < bestDistancePixels
                     || (qFuzzyCompare(distancePixels, bestDistancePixels) && priority < bestPriority)))
                || (!bestUsesDistanceRanking && bestPriority > 0));
        const bool betterPriorityRankedItem = !betterDistanceRankedPath
            && (!bestUsesDistanceRanking || priority == 0)
            && (priority < bestPriority
                || (priority == bestPriority && distancePixels < bestDistancePixels));
        if (betterDistanceRankedPath || betterPriorityRankedItem) {
            bestPriority = priority;
            bestDistancePixels = distancePixels;
            bestUsesDistanceRanking = usesDistanceRanking;
            bestItem = item;
            if (bestPriority == 0) {
                break;
            }
        }
    }
    return bestItem;
}

QList<QGraphicsItem *> expandedMapHitItemsAtScenePosition(QGraphicsScene *scene,
                                                          const QPointF &scenePosition,
                                                          const QTransform &viewTransform,
                                                          bool requireSelected)
{
    if (scene == nullptr) {
        return {};
    }

    QList<QGraphicsItem *> hitItems = scene->items(scenePosition,
                                                   Qt::IntersectsItemShape,
                                                   Qt::DescendingOrder,
                                                   viewTransform);
    const QRectF pathProbeRect =
        sceneProbeRectForViewRadius(scenePosition, viewTransform, kMaximumPathPrimaryHitDistancePixels + 1.0);
    const QList<QGraphicsItem *> nearbyPathCandidates =
        scene->items(pathProbeRect,
                     Qt::IntersectsItemBoundingRect,
                     Qt::DescendingOrder,
                     viewTransform);
    for (QGraphicsItem *candidate : nearbyPathCandidates) {
        if (candidate == nullptr || hitItems.contains(candidate)) {
            continue;
        }

        const int subtype = candidate->data(kMapSceneSelectionSubtypeRole).toInt();
        const bool pathSelectionCandidate = isDistanceRankedPathItem(candidate, subtype);
        if (!pathSelectionCandidate
            || !isInteractiveMapSelectionItem(candidate)
            || (requireSelected && !candidate->isSelected())
            || candidate->data(kMapSceneLineNumberRole).toInt() <= 0) {
            continue;
        }

        const qreal distancePixels = genericPathItemHitDistancePixels(candidate, scenePosition, viewTransform);
        if (std::isfinite(distancePixels)
            && distancePixels != std::numeric_limits<qreal>::max()
            && distancePixels <= kMaximumPathPrimaryHitDistancePixels) {
            hitItems.append(candidate);
        }
    }
    return hitItems;
}

QGraphicsItem *takePendingPrimarySelectionItem(QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return nullptr;
    }

    QGraphicsItem *pendingPrimaryItem = nullptr;
    for (QGraphicsItem *item : scene->items()) {
        if (item == nullptr || !item->data(kMapScenePendingPrimarySelectionRole).toBool()) {
            continue;
        }
        item->setData(kMapScenePendingPrimarySelectionRole, false);
        if (pendingPrimaryItem == nullptr && isInteractiveMapSelectionItem(item)) {
            pendingPrimaryItem = item;
        }
    }
    return pendingPrimaryItem;
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

int lineAnchorSourceVertexIndex(const MapGeometryFeature::TH2LineVertex &vertex, int vertexOrder)
{
    return vertex.anchorSourceVertexIndex >= 0 ? vertex.anchorSourceVertexIndex : vertexOrder;
}

struct LineSegmentSelection
{
    int startSourceVertexIndex = -1;
    int endSourceVertexIndex = -1;
};

std::optional<LineSegmentSelection> nearestLineSegmentSelection(const MapGeometryFeature &lineFeature,
                                                               const QPointF &sourcePoint)
{
    if (lineFeature.lineVertices.size() < 2) {
        return std::nullopt;
    }

    qreal bestDistance = std::numeric_limits<qreal>::max();
    LineSegmentSelection bestSelection;
    const auto considerSegment = [&](int startOrder, int endOrder) {
        if (startOrder < 0 || startOrder >= lineFeature.lineVertices.size()
            || endOrder < 0 || endOrder >= lineFeature.lineVertices.size()) {
            return;
        }

        const MapGeometryFeature::TH2LineVertex &startVertex = lineFeature.lineVertices.at(startOrder);
        const MapGeometryFeature::TH2LineVertex &endVertex = lineFeature.lineVertices.at(endOrder);
        const qreal distance = distanceToSegment(sourcePoint, startVertex.anchor, endVertex.anchor);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestSelection.startSourceVertexIndex = lineAnchorSourceVertexIndex(startVertex, startOrder);
            bestSelection.endSourceVertexIndex = lineAnchorSourceVertexIndex(endVertex, endOrder);
        }
    };

    for (int vertexOrder = 1; vertexOrder < lineFeature.lineVertices.size(); ++vertexOrder) {
        considerSegment(vertexOrder - 1, vertexOrder);
    }
    if (lineFeature.closed) {
        considerSegment(lineFeature.lineVertices.size() - 1, 0);
    }

    if (bestSelection.startSourceVertexIndex < 0 || bestSelection.endSourceVertexIndex < 0) {
        return std::nullopt;
    }
    return bestSelection;
}

void clearSelectedObjectState(const MapEditorSelectionContext &context)
{
    (*context.selectedObjectLineNumber) = 0;
    (*context.selectedObjectVertexIndex) = -1;
    (*context.selectedLineSegmentStartVertexIndex) = -1;
    (*context.selectedLineSegmentEndVertexIndex) = -1;
    context.selectedObjectKind->clear();
    context.selectedObjectCoordinate->reset();
}

QString geometryFeatureKindForSelection(MapGeometryFeature::Kind kind)
{
    switch (kind) {
    case MapGeometryFeature::Kind::Point:
        return QStringLiteral("point");
    case MapGeometryFeature::Kind::Line:
        return QStringLiteral("line");
    case MapGeometryFeature::Kind::Area:
        return QStringLiteral("area");
    }
    return QString();
}

QString objectKindForSourceLine(const QVector<TherionParsedLine> &parsedLines, int lineNumber)
{
    if (lineNumber <= 0) {
        return QString();
    }

    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.lineNumber == lineNumber) {
            if (const QString directiveKind = objectKindForDirective(parsedLine.directive);
                !directiveKind.isEmpty()) {
                return directiveKind;
            }
            break;
        }
    }

    const QVector<MapGeometryFeature> geometryFeatures = collectGeometryFeatures(parsedLines);
    for (const MapGeometryFeature &feature : geometryFeatures) {
        if (feature.lineNumber == lineNumber) {
            return geometryFeatureKindForSelection(feature.kind);
        }
    }

    return QString();
}

void setSelectedObjectStateForSourceLine(const MapEditorSelectionContext &context,
                                         int lineNumber,
                                         QGraphicsItem *item = nullptr)
{
    clearSelectedObjectState(context);
    if (lineNumber <= 0) {
        return;
    }

    QString kind = objectKindForSourceLine(context.parsedLinesForCurrentDocument(), lineNumber);
    if (kind.isEmpty()) {
        if (dynamic_cast<MapEditablePointItem *>(item) != nullptr) {
            kind = QStringLiteral("point");
        }
    }
    if (!isConfigurableMapObjectKind(kind)) {
        return;
    }

    (*context.selectedObjectLineNumber) = lineNumber;
    (*context.selectedObjectKind) = kind;
    if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(item)) {
        (*context.selectedObjectCoordinate) = pointItem->sourcePoint();
    }
}

QSet<int> selectedAreaBorderLineNumbers(const QVector<TherionSourceLogicalCommand> &commands,
                                        const QSet<int> &selectedLines)
{
    QSet<int> borderLineNumbers;
    for (const int selectedLine : selectedLines) {
        borderLineNumbers.unite(mapEditorBorderLineNumbersForArea(commands, selectedLine));
    }
    return borderLineNumbers;
}

void selectAreaBorderLineItems(QGraphicsScene *scene,
                               const QHash<int, QGraphicsItem *> &itemsByLine,
                               const QVector<TherionSourceLogicalCommand> &commands,
                               int areaLineNumber)
{
    if (scene == nullptr || areaLineNumber <= 0) {
        return;
    }

    const QSet<int> borderLineNumbers = mapEditorBorderLineNumbersForArea(commands, areaLineNumber);
    for (const int borderLineNumber : borderLineNumbers) {
        const auto itemIt = itemsByLine.constFind(borderLineNumber);
        if (itemIt != itemsByLine.constEnd() && itemIt.value() != nullptr) {
            itemIt.value()->setSelected(true);
        }
    }
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

}

MapEditorSelectionController::MapEditorSelectionController(MapEditorSelectionContext context)
    : context_(std::move(context))
{
}

void MapEditorSelectionController::handleMapSceneSelectionChanged()
{
    if ((*context_.updatingSelection) || context_.scene == nullptr) {
        context_.updateHelpPanel();
        context_.refreshObjectDetailsPanel();
        return;
    }

    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
    if (selectedItems.isEmpty()) {
        (*context_.selectedObjectLineNumber) = 0;
        (*context_.selectedObjectVertexIndex) = -1;
        (*context_.selectedLineSegmentStartVertexIndex) = -1;
        (*context_.selectedLineSegmentEndVertexIndex) = -1;
        (*context_.selectedObjectKind).clear();
        (*context_.selectedObjectCoordinate).reset();
        if (context_.sceneRefreshSelectionLineNumber != nullptr) {
            (*context_.sceneRefreshSelectionLineNumber) = 0;
        }
        if (context_.sceneRefreshSelectionVertexIndex != nullptr) {
            (*context_.sceneRefreshSelectionVertexIndex) = -1;
        }
        if (context_.sceneRefreshSelectionGeometryKind != nullptr) {
            context_.sceneRefreshSelectionGeometryKind->clear();
        }
        context_.clearInspectorObjectSelection();
        updateGeometrySelectionPresentation();
        context_.updateHelpPanel();
        context_.refreshObjectDetailsPanel();
        return;
    }

    int selectedLineNumber = 0;
    int selectedSourceVertexIndex = -1;
    int selectedLineSegmentStartVertexIndex = -1;
    int selectedLineSegmentEndVertexIndex = -1;
    QString selectedGeometryKind;
    std::optional<QPointF> selectedPointSource;
    MapCardItem *selectedCard = nullptr;
    QList<QGraphicsItem *> selectedLineItems;
    QSet<int> selectedLineNumbers;
    selectedLineItems.reserve(selectedItems.size());
    for (QGraphicsItem *item : selectedItems) {
        if (!isInteractiveMapSelectionItem(item)) {
            continue;
        }
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber <= 0) {
            continue;
        }
        selectedLineItems.append(item);
        selectedLineNumbers.insert(lineNumber);
    }

    QGraphicsItem *primarySelectedItem = nullptr;
    const bool usePendingClickSelection = (*context_.pendingClickSelection)
        && (*context_.pendingClickElapsed).isValid()
        && (*context_.pendingClickElapsed).elapsed() <= 1000;
    if (usePendingClickSelection && (*context_.pendingClickLineNumber) > 0) {
        if (QGraphicsItem *pendingPrimaryItem = takePendingPrimarySelectionItem(context_.scene)) {
            if (pendingPrimaryItem->data(kMapSceneLineNumberRole).toInt() == (*context_.pendingClickLineNumber)) {
                primarySelectedItem = pendingPrimaryItem;
            }
        }
        if (primarySelectedItem == nullptr
            && (*context_.pendingClickSourceVertexIndex) < 0
            && context_.scene != nullptr
            && context_.view != nullptr) {
            const QPointF scenePos = (*context_.pendingClickScenePosition);
            const QList<QGraphicsItem *> hitItems =
                expandedMapHitItemsAtScenePosition(context_.scene, scenePos, context_.view->transform(), false);
            if (QGraphicsItem *pendingHitItem = preferredMapHitItem(hitItems, false, scenePos, context_.view->transform())) {
                if (pendingHitItem->data(kMapSceneLineNumberRole).toInt() == (*context_.pendingClickLineNumber)) {
                    primarySelectedItem = pendingHitItem;
                }
            }
        }
        if (primarySelectedItem == nullptr && (*context_.pendingClickSourceVertexIndex) >= 0) {
            for (QGraphicsItem *item : selectedLineItems) {
                if (item == nullptr) {
                    continue;
                }
                if (item->data(kMapSceneLineNumberRole).toInt() != (*context_.pendingClickLineNumber)) {
                    continue;
                }
                auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
                if (vertexItem == nullptr) {
                    continue;
                }
                if (vertexItem->vertexIndex() != (*context_.pendingClickSourceVertexIndex)) {
                    continue;
                }
                if (!(*context_.pendingClickGeometryKind).isEmpty()
                    && !vertexItem->geometryKind().startsWith((*context_.pendingClickGeometryKind))) {
                    continue;
                }
                primarySelectedItem = item;
                break;
            }
        }
        if (primarySelectedItem == nullptr) {
            for (QGraphicsItem *item : selectedLineItems) {
                if (item != nullptr && item->data(kMapSceneLineNumberRole).toInt() == (*context_.pendingClickLineNumber)) {
                    primarySelectedItem = item;
                    break;
                }
            }
        }
    }
    if (context_.view != nullptr && context_.view->viewport() != nullptr) {
        if (primarySelectedItem == nullptr && usePendingClickSelection) {
            const QPointF scenePos = (*context_.pendingClickScenePosition);
            const QList<QGraphicsItem *> hitItems =
                expandedMapHitItemsAtScenePosition(context_.scene, scenePos, context_.view->transform(), true);
            primarySelectedItem = preferredMapHitItem(hitItems, true, scenePos, context_.view->transform());
            if (primarySelectedItem == nullptr && (*context_.pendingClickLineNumber) > 0) {
                const QList<QGraphicsItem *> unselectedHitItems =
                    expandedMapHitItemsAtScenePosition(context_.scene, scenePos, context_.view->transform(), false);
                if (QGraphicsItem *pendingHitItem = preferredMapHitItem(unselectedHitItems, false, scenePos, context_.view->transform())) {
                    if (pendingHitItem->data(kMapSceneLineNumberRole).toInt() == (*context_.pendingClickLineNumber)) {
                        primarySelectedItem = pendingHitItem;
                    }
                }
            }
        } else if (primarySelectedItem == nullptr) {
            const QPoint viewportPos = context_.view->viewport()->mapFromGlobal(QCursor::pos());
            if (context_.view->viewport()->rect().contains(viewportPos)) {
                const QPointF scenePos = context_.view->mapToScene(viewportPos);
                const QList<QGraphicsItem *> hitItems =
                    expandedMapHitItemsAtScenePosition(context_.scene, scenePos, context_.view->transform(), true);
                primarySelectedItem = preferredMapHitItem(hitItems, true, scenePos, context_.view->transform());
            }
        }
    }

    if (primarySelectedItem == nullptr && !selectedLineItems.isEmpty()) {
        if (selectedLineNumbers.size() == 1 && selectedLineItems.size() > 1) {
            QGraphicsItem *highestZItem = selectedLineItems.first();
            qreal highestZ = highestZItem != nullptr ? highestZItem->zValue() : 0.0;
            for (QGraphicsItem *item : selectedLineItems) {
                if (item != nullptr && item->zValue() > highestZ) {
                    highestZ = item->zValue();
                    highestZItem = item;
                }
            }
            primarySelectedItem = highestZItem;
        } else {
            primarySelectedItem = selectedLineItems.first();
        }
    }

    if (primarySelectedItem != nullptr && !primarySelectedItem->isSelected()) {
        const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
        context_.scene->clearSelection();
        primarySelectedItem->setSelected(true);
    }

    (*context_.pendingClickSelection) = false;
    (*context_.pendingClickLineNumber) = 0;
    (*context_.pendingClickSourceVertexIndex) = -1;
    (*context_.pendingClickGeometryKind).clear();

    if (primarySelectedItem != nullptr) {
        selectedLineNumber = primarySelectedItem->data(kMapSceneLineNumberRole).toInt();
        selectedCard = dynamic_cast<MapCardItem *>(primarySelectedItem);
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem)) {
            selectedSourceVertexIndex = vertexItem->vertexIndex();
            selectedGeometryKind = vertexItem->geometryKind();
            const int subtype = primarySelectedItem->data(kMapSceneSelectionSubtypeRole).toInt();
            if ((subtype == kMapSceneSelectionSubtypeLineControl
                 || subtype == kMapSceneSelectionSubtypeLineControlConnector)
                && vertexItem->lineNumber() > 0) {
                const int ownerVertexIndex = primarySelectedItem->data(kMapSceneOwnerVertexRole).toInt();
                if (ownerVertexIndex >= 0) {
                    selectedSourceVertexIndex = ownerVertexIndex;
                    selectedGeometryKind = QStringLiteral("line");
                }
            }
        } else if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(primarySelectedItem)) {
            selectedPointSource = pointItem->sourcePoint();
        } else if (primarySelectedItem->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypePointOrientationHandle) {
            if (auto selectedItemIt = (*context_.itemsByLine).find(selectedLineNumber);
                selectedItemIt != (*context_.itemsByLine).end()) {
                if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(selectedItemIt.value())) {
                    const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
                    pointItem->setSelected(true);
                    selectedPointSource = pointItem->sourcePoint();
                }
            }
        } else if (primarySelectedItem->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineControlConnector) {
            const int ownerVertexIndex = primarySelectedItem->data(kMapSceneOwnerVertexRole).toInt();
            if (ownerVertexIndex >= 0) {
                selectedSourceVertexIndex = ownerVertexIndex;
                selectedGeometryKind = QStringLiteral("line");
            }
        }
    }
    if (primarySelectedItem != nullptr
        && selectedLineNumber > 0
        && selectedSourceVertexIndex < 0
        && usePendingClickSelection
        && context_.scene != nullptr) {
        const QString sourceLineKind = objectKindForSourceLine(context_.parsedLinesForCurrentDocument(),
                                                               selectedLineNumber);
        const int subtype = primarySelectedItem->data(kMapSceneSelectionSubtypeRole).toInt();
        const bool pathLikeLineSelection = sourceLineKind == QStringLiteral("line")
            && dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem) == nullptr
            && dynamic_cast<MapEditablePointItem *>(primarySelectedItem) == nullptr
            && subtype != kMapSceneSelectionSubtypeLineControlConnector;
        if (pathLikeLineSelection) {
            const QVector<TherionParsedLine> parsedLines = context_.parsedLinesForCurrentDocument();
            const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(parsedLines,
                                                                                           selectedLineNumber);
            if (lineFeature.has_value()) {
                const QPointF clickedSourcePoint =
                    context_.sourcePointFromScenePosition((*context_.pendingClickScenePosition));
                if (const std::optional<LineSegmentSelection> segmentSelection =
                        nearestLineSegmentSelection(lineFeature.value(), clickedSourcePoint)) {
                    selectedSourceVertexIndex = segmentSelection->endSourceVertexIndex;
                    selectedGeometryKind = QStringLiteral("line");
                    selectedLineSegmentStartVertexIndex = segmentSelection->startSourceVertexIndex;
                    selectedLineSegmentEndVertexIndex = segmentSelection->endSourceVertexIndex;
                }
            }
        }
    }

    int selectedSourceLineNumber = selectedLineNumber;
    if (selectedLineNumber > 0 && context_.textEditor != nullptr) {
        const QScopedValueRollback<bool> syncGuard((*context_.textNavigationInProgress), true);
        if (selectedPointSource.has_value()) {
            const QVector<TherionParsedLine> parsedLines = context_.parsedLinesForCurrentDocument();
            const std::optional<int> pointLineNumber =
                sourcePointLineNumberForSelection(parsedLines, selectedPointSource.value());
            if (pointLineNumber.has_value()) {
                selectedSourceLineNumber = pointLineNumber.value();
                context_.textEditor->goToLineColumn(pointLineNumber.value(), 1);
            } else if (selectedLineNumber != context_.currentLineNumber()) {
                context_.textEditor->goToLine(selectedLineNumber);
            }
        } else if (selectedSourceVertexIndex >= 0) {
            const QVector<TherionParsedLine> parsedLines = context_.parsedLinesForCurrentDocument();
            const std::optional<SourceVertexTextReference> sourceReference =
                sourceVertexTextReferenceForSelection(parsedLines,
                                                      selectedLineNumber,
                                                      selectedGeometryKind,
                                                      selectedSourceVertexIndex);
            if (sourceReference.has_value()) {
                if (context_.textEditor->currentLineNumber() != sourceReference->lineNumber
                    || context_.textEditor->currentColumnNumber() != sourceReference->xStartColumn) {
                    context_.textEditor->goToLineColumn(sourceReference->lineNumber, sourceReference->xStartColumn);
                }
            } else if (selectedLineNumber != context_.currentLineNumber()) {
                context_.textEditor->goToLine(selectedLineNumber);
            }
        } else if (selectedLineNumber != context_.currentLineNumber()) {
            context_.textEditor->goToLine(selectedLineNumber);
        }
    }

    (*context_.selectedObjectLineNumber) = selectedSourceLineNumber;
    (*context_.selectedObjectVertexIndex) = selectedSourceVertexIndex;
    (*context_.selectedLineSegmentStartVertexIndex) = selectedLineSegmentStartVertexIndex;
    (*context_.selectedLineSegmentEndVertexIndex) = selectedLineSegmentEndVertexIndex;
    (*context_.selectedObjectCoordinate).reset();
    if (selectedPointSource.has_value()) {
        (*context_.selectedObjectKind) = QStringLiteral("point");
        (*context_.selectedObjectCoordinate) = selectedPointSource.value();
    } else if (selectedSourceVertexIndex >= 0) {
        const QString normalizedGeometryKind = selectedGeometryKind.trimmed().toLower();
        if (normalizedGeometryKind.startsWith(QStringLiteral("line"))) {
            (*context_.selectedObjectKind) = QStringLiteral("line");
        } else if (normalizedGeometryKind.startsWith(QStringLiteral("area"))) {
            (*context_.selectedObjectKind) = QStringLiteral("area");
        } else {
            (*context_.selectedObjectKind) = normalizedGeometryKind;
        }
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem)) {
            (*context_.selectedObjectCoordinate) = context_.sourcePointFromScenePosition(vertexItem->pos());
        } else if (selectedLineNumber > 0 && selectedSourceVertexIndex >= 0 && context_.scene != nullptr) {
            if (MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(context_.scene,
                                                                                   selectedLineNumber,
                                                                                   selectedSourceVertexIndex,
                                                                                   QStringLiteral("line"))) {
                const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
                ownerAnchor->setSelected(true);
                (*context_.selectedObjectCoordinate) = context_.sourcePointFromScenePosition(ownerAnchor->pos());
            }
        }
    } else if (selectedLineNumber > 0 && context_.textEditor != nullptr) {
        (*context_.selectedObjectKind) = objectKindForSourceLine(context_.parsedLinesForCurrentDocument(),
                                                                 selectedLineNumber);
        if ((*context_.selectedObjectKind).isEmpty()) {
            (*context_.selectedObjectKind) = QStringLiteral("object");
        }
    } else {
        (*context_.selectedObjectKind).clear();
    }

    if (selectedCard != nullptr) {
        if (context_.sceneRefreshSelectionLineNumber != nullptr) {
            (*context_.sceneRefreshSelectionLineNumber) = selectedSourceLineNumber;
        }
        if (context_.sceneRefreshSelectionVertexIndex != nullptr) {
            (*context_.sceneRefreshSelectionVertexIndex) = -1;
        }
        if (context_.sceneRefreshSelectionGeometryKind != nullptr) {
            (*context_.sceneRefreshSelectionGeometryKind) = QStringLiteral("card");
        }
        if (selectedSourceLineNumber > 0 && context_.syncInspectorObjectSelectionToLineExpanding) {
            context_.syncInspectorObjectSelectionToLineExpanding(selectedSourceLineNumber);
        }
        updateGeometrySelectionPresentation();
        context_.updateCommandSurfaceState();
        context_.updateHelpPanel();
        context_.refreshObjectDetailsPanel();
        return;
    }

    const bool primarySelectionIsAreaFill = primarySelectedItem != nullptr
        && primarySelectedItem->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeAreaFill;
    if (selectedLineNumber > 0
        && (*context_.selectedObjectKind) == QStringLiteral("area")
        && !primarySelectionIsAreaFill
        && context_.logicalSource.logicalCommandsForCurrentDocument) {
        const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
        selectAreaBorderLineItems(context_.scene,
                                  *context_.itemsByLine,
                                  context_.logicalSource.logicalCommandsForCurrentDocument(),
                                  selectedLineNumber);
    }

    if (context_.sceneRefreshSelectionLineNumber != nullptr) {
        (*context_.sceneRefreshSelectionLineNumber) = selectedSourceLineNumber;
    }
    if (context_.sceneRefreshSelectionVertexIndex != nullptr) {
        (*context_.sceneRefreshSelectionVertexIndex) = -1;
    }
    if (context_.sceneRefreshSelectionGeometryKind != nullptr) {
        (*context_.sceneRefreshSelectionGeometryKind) = *context_.selectedObjectKind;
    }
    if (auto *selectedVertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem);
        selectedVertexItem != nullptr) {
        if (context_.sceneRefreshSelectionVertexIndex != nullptr) {
            (*context_.sceneRefreshSelectionVertexIndex) = selectedVertexItem->vertexIndex();
        }
        if (context_.sceneRefreshSelectionGeometryKind != nullptr) {
            (*context_.sceneRefreshSelectionGeometryKind) = selectedVertexItem->geometryKind();
        }
    }
    if (selectedSourceLineNumber > 0 && context_.syncInspectorObjectSelectionToLineExpanding) {
        context_.syncInspectorObjectSelectionToLineExpanding(selectedSourceLineNumber);
    }
    updateGeometrySelectionPresentation();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
}

void MapEditorSelectionController::syncMapSelectionFromTextCursor(int lineNumber, int columnNumber)
{
    (*context_.lastCursorSyncedLine) = lineNumber;
    (*context_.lastCursorSyncedColumn) = columnNumber;

    if (context_.scene == nullptr || context_.textEditor == nullptr || lineNumber <= 0) {
        return;
    }
    if ((*context_.suppressedAutoReselectLineNumbers).contains(lineNumber)) {
        return;
    }
    if (!(*context_.suppressedAutoReselectLineNumbers).isEmpty()) {
        (*context_.suppressedAutoReselectLineNumbers).clear();
    }

    const QVector<TherionParsedLine> parsedLines = context_.parsedLinesForCurrentDocument();
    if (const std::optional<QSet<int>> scrapLines = scrapObjectLinesForCursor(parsedLines, lineNumber);
        scrapLines.has_value()) {
        setSelectedObjectStateForSourceLine(context_, lineNumber);
        if (!scrapLines->isEmpty()) {
            selectMapLines(scrapLines.value());
        } else {
            selectMapLine(lineNumber);
        }
        context_.updateCommandSurfaceState();
        context_.updateHelpPanel();
        context_.refreshObjectDetailsPanel();
        return;
    }

    const CursorGeometrySelection cursorSelection = cursorGeometrySelectionForTextCursor(parsedLines,
                                                                                         lineNumber,
                                                                                         columnNumber);
    if (cursorSelection.featureLineNumber <= 0) {
        selectMapLine(lineNumber);
        return;
    }

    selectMapLine(cursorSelection.featureLineNumber);
    if (!cursorSelection.sourceVertexReference.has_value()) {
        return;
    }

    const int sourceVertexIndex = cursorSelection.sourceVertexReference->sourceVertexIndex;
    if (sourceVertexIndex < 0 || context_.scene == nullptr) {
        return;
    }

    const QString geometryKind = cursorSelection.geometryKind.trimmed().toLower();
    const bool lineGeometry = geometryKind == QStringLiteral("line");
    const bool areaGeometry = geometryKind == QStringLiteral("area");
    if (!lineGeometry && !areaGeometry) {
        return;
    }

    (*context_.selectedObjectLineNumber) = cursorSelection.featureLineNumber;
    (*context_.selectedObjectVertexIndex) = sourceVertexIndex;
    (*context_.selectedLineSegmentStartVertexIndex) = -1;
    (*context_.selectedLineSegmentEndVertexIndex) = -1;
    (*context_.selectedObjectKind) = geometryKind;
    (*context_.selectedObjectCoordinate).reset();

    MapEditableGeometryVertexItem *targetItem = nullptr;
    if (lineGeometry) {
        int ownerSourceVertexIndex = -1;
        if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(parsedLines,
                                                                                            cursorSelection.featureLineNumber);
            lineFeature.has_value()) {
            const int ownerVertexOrder = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
            if (ownerVertexOrder >= 0 && ownerVertexOrder < lineFeature->lineVertices.size()) {
                const MapGeometryFeature::TH2LineVertex &ownerVertex = lineFeature->lineVertices.at(ownerVertexOrder);
                ownerSourceVertexIndex = ownerVertex.anchorSourceVertexIndex >= 0
                    ? ownerVertex.anchorSourceVertexIndex
                    : ownerVertexOrder;
            }
        }

        (*context_.updatingSelection) = true;
        if (ownerSourceVertexIndex >= 0) {
            if (MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(context_.scene,
                                                                                     cursorSelection.featureLineNumber,
                                                                                     ownerSourceVertexIndex,
                                                                                     QStringLiteral("line"))) {
                ownerAnchor->setSelected(true);
            }
        }
        (*context_.updatingSelection) = false;
        updateGeometrySelectionPresentation();

        targetItem = findGeometryVertexItem(context_.scene,
                                            cursorSelection.featureLineNumber,
                                            sourceVertexIndex,
                                            QStringLiteral("line"));
    } else {
        targetItem = findGeometryVertexItem(context_.scene,
                                            cursorSelection.featureLineNumber,
                                            sourceVertexIndex,
                                            QStringLiteral("area"));
    }

    if (targetItem != nullptr) {
        (*context_.updatingSelection) = true;
        targetItem->setSelected(true);
        (*context_.updatingSelection) = false;
        (*context_.selectedObjectCoordinate) = context_.sourcePointFromScenePosition(targetItem->pos());
        updateGeometrySelectionPresentation();
        context_.updateCommandSurfaceState();
        context_.updateHelpPanel();
    }
    context_.refreshObjectDetailsPanel();
}

void MapEditorSelectionController::updateGeometrySelectionPresentation()
{
    if (context_.scene == nullptr) {
        return;
    }

    QSet<int> selectedLines;
    QHash<int, QSet<int>> selectedLineControlOwnersByLine;
    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
    for (QGraphicsItem *item : selectedItems) {
        if (item == nullptr) {
            continue;
        }

        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber > 0) {
            selectedLines.insert(lineNumber);
        }

        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        if (subtype == kMapSceneSelectionSubtypeLineAnchor
            || subtype == kMapSceneSelectionSubtypeLineControl
            || subtype == kMapSceneSelectionSubtypeLineControlConnector) {
            const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
            if (lineNumber > 0 && ownerVertexIndex >= 0) {
                selectedLineControlOwnersByLine[lineNumber].insert(ownerVertexIndex);
            }
        }
    }
    if ((*context_.selectedObjectLineNumber) > 0
        && (*context_.selectedObjectKind) == QStringLiteral("line")
        && (*context_.selectedLineSegmentStartVertexIndex) >= 0
        && (*context_.selectedLineSegmentEndVertexIndex) >= 0) {
        QSet<int> segmentLineNumbers = selectedLines;
        if (segmentLineNumbers.contains(*context_.selectedObjectLineNumber)) {
            segmentLineNumbers = {*context_.selectedObjectLineNumber};
        } else if (segmentLineNumbers.isEmpty()) {
            segmentLineNumbers.insert(*context_.selectedObjectLineNumber);
        }
        for (int lineNumber : std::as_const(segmentLineNumbers)) {
            selectedLineControlOwnersByLine[lineNumber].insert(
                (*context_.selectedLineSegmentStartVertexIndex));
            selectedLineControlOwnersByLine[lineNumber].insert(
                (*context_.selectedLineSegmentEndVertexIndex));
        }
    }
    if (context_.logicalSource.logicalCommandsForCurrentDocument) {
        selectedLines.unite(selectedAreaBorderLineNumbers(context_.logicalSource.logicalCommandsForCurrentDocument(), selectedLines));
    }

    const auto sceneItems = context_.scene->items();
    for (QGraphicsItem *item : sceneItems) {
        if (item == nullptr) {
            continue;
        }
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
        const bool focusedLineAnchor = subtype == kMapSceneSelectionSubtypeLineAnchor
            && lineNumber > 0
            && ownerVertexIndex >= 0
            && lineNumber == (*context_.selectedObjectLineNumber)
            && ownerVertexIndex == (*context_.selectedObjectVertexIndex)
            && (*context_.selectedObjectKind) == QStringLiteral("line");
        if (item->data(kMapSceneFocusedVertexRole).toBool() != focusedLineAnchor) {
            item->setData(kMapSceneFocusedVertexRole, focusedLineAnchor);
            item->update();
        }

        const bool visuallySelected = lineNumber > 0
            && selectedLines.contains(lineNumber)
            && isInteractiveMapSelectionItem(item);
        if (item->data(kMapSceneInteractionSelectionRole).toBool() != visuallySelected) {
            item->setData(kMapSceneInteractionSelectionRole, visuallySelected);
            item->update();
        }

        if (!item->data(kMapSceneSelectionGatedRole).toBool()) {
            continue;
        }

        bool visible = lineNumber > 0
            && !(*context_.hiddenObjectLines).contains(lineNumber)
            && selectedLines.contains(lineNumber);
        if (visible) {
            const bool lineControlSelectionSubtype = subtype == kMapSceneSelectionSubtypeLineControl
                || subtype == kMapSceneSelectionSubtypeLineControlConnector;
            if (lineControlSelectionSubtype) {
                const QSet<int> selectedOwnerVertices = selectedLineControlOwnersByLine.value(lineNumber);
                visible = ownerVertexIndex >= 0
                    && !selectedOwnerVertices.isEmpty()
                    && selectedOwnerVertices.contains(ownerVertexIndex);
            }
        }

        item->setVisible(visible);
    }
}

void MapEditorSelectionController::selectMapLine(int lineNumber, bool centerOnSelection)
{
    if (context_.scene == nullptr) {
        return;
    }

    QGraphicsItem *selectedItem = nullptr;
    auto selectedItemIt = (*context_.itemsByLine).find(lineNumber);
    if (selectedItemIt != (*context_.itemsByLine).end()) {
        selectedItem = selectedItemIt.value();
    }
    setSelectedObjectStateForSourceLine(context_, lineNumber, selectedItem);

    (*context_.pendingClickSelection) = false;
    (*context_.pendingClickLineNumber) = 0;
    (*context_.pendingClickSourceVertexIndex) = -1;
    (*context_.pendingClickGeometryKind).clear();
    (*context_.selectedLineSegmentStartVertexIndex) = -1;
    (*context_.selectedLineSegmentEndVertexIndex) = -1;
    (*context_.updatingSelection) = true;
    context_.scene->clearSelection();

    if (selectedItem != nullptr) {
        selectedItem->setSelected(true);
        if (context_.logicalSource.logicalCommandsForCurrentDocument) {
            selectAreaBorderLineItems(context_.scene,
                                      *context_.itemsByLine,
                                      context_.logicalSource.logicalCommandsForCurrentDocument(),
                                      lineNumber);
        }
        if (centerOnSelection && !(*context_.autoFitEnabled) && context_.view != nullptr) {
            context_.view->centerOn(selectedItem);
        }
    }

    (*context_.updatingSelection) = false;
    updateGeometrySelectionPresentation();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
}

void MapEditorSelectionController::selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection)
{
    if (context_.scene == nullptr) {
        return;
    }

    (*context_.pendingClickSelection) = false;
    (*context_.pendingClickLineNumber) = 0;
    (*context_.pendingClickSourceVertexIndex) = -1;
    (*context_.pendingClickGeometryKind).clear();
    (*context_.selectedLineSegmentStartVertexIndex) = -1;
    (*context_.selectedLineSegmentEndVertexIndex) = -1;
    (*context_.updatingSelection) = true;
    context_.scene->clearSelection();

    QList<int> sortedLines = lineNumbers.values();
    std::sort(sortedLines.begin(), sortedLines.end());
    QGraphicsItem *firstSelectedItem = nullptr;
    const QVector<TherionSourceLogicalCommand> logicalCommands = context_.logicalSource.logicalCommandsForCurrentDocument
        ? context_.logicalSource.logicalCommandsForCurrentDocument()
        : QVector<TherionSourceLogicalCommand>();
    for (const int lineNumber : std::as_const(sortedLines)) {
        auto selectedItemIt = (*context_.itemsByLine).find(lineNumber);
        if (selectedItemIt == (*context_.itemsByLine).end() || selectedItemIt.value() == nullptr) {
            continue;
        }
        selectedItemIt.value()->setSelected(true);
        if (!logicalCommands.isEmpty()) {
            selectAreaBorderLineItems(context_.scene, *context_.itemsByLine, logicalCommands, lineNumber);
        }
        if (firstSelectedItem == nullptr) {
            firstSelectedItem = selectedItemIt.value();
        }
    }

    if (centerOnSelection && firstSelectedItem != nullptr && !(*context_.autoFitEnabled) && context_.view != nullptr) {
        context_.view->centerOn(firstSelectedItem);
    }

    (*context_.updatingSelection) = false;
    updateGeometrySelectionPresentation();
}

}
