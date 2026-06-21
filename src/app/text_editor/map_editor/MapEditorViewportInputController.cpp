#include "MapEditorViewportInputController.h"

#include <QCoreApplication>

#include "MapEditorInputPolicy.h"
#include "MapEditorLineDecorationItem.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainterPathStroker>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QTransform>
#include <QWheelEvent>
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
constexpr qreal kFilledPathInteriorHitDistancePixels = 6.0;
constexpr qreal kDirectVertexCenterHitDistance = 1.0;
constexpr qreal kDirectVertexAffordanceHitDistancePixels = 12.0;
constexpr qreal kMaximumPathPrimaryHitDistancePixels = 10.0;

bool wheelEventHasPreciseScrollingDeltas(const QWheelEvent *event)
{
    if (event == nullptr) {
        return false;
    }

    if (!event->pixelDelta().isNull()) {
        return true;
    }

    return event->phase() != Qt::NoScrollPhase;
}

bool isSecondaryClickPress(const QMouseEvent *event)
{
    if (event == nullptr) {
        return false;
    }
    return event->button() == Qt::LeftButton
        && event->modifiers().testFlag(Qt::ControlModifier);
}

void applyDefaultMapViewportCursor(const MapEditorViewportInputContext &context, QWidget *viewport)
{
    if (viewport == nullptr) {
        return;
    }

    const MapEditorInteractiveDrawMode drawMode = context.drawMode ? context.drawMode() : MapEditorInteractiveDrawMode::None;
    const bool selectMode = context.selectModeActive != nullptr && (*context.selectModeActive);
    if (selectMode && drawMode == MapEditorInteractiveDrawMode::None) {
        viewport->setCursor(Qt::CrossCursor);
    } else {
        viewport->unsetCursor();
    }
}

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

QPointF viewportHitCenterScenePoint(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return QPointF();
    }
    if (dynamic_cast<const MapEditableGeometryVertexItem *>(item) != nullptr) {
        return item->scenePos();
    }
    return item->mapToScene(item->boundingRect().center());
}

QGraphicsItem *preferredMapHitItem(const QList<QGraphicsItem *> &hitItems,
                                   bool requireSelected,
                                   std::optional<QPointF> scenePosition,
                                   const QTransform &viewTransform);
int mapSelectionHitPriority(const QGraphicsItem *item);
qreal genericPathItemHitDistancePixels(const QGraphicsItem *item,
                                       const QPointF &scenePosition,
                                       const QTransform &viewTransform);

bool isDistanceRankedPathSubtype(int subtype);
bool isVertexLikeSelectionSubtype(int subtype);

void clearPendingPrimarySelectionItem(MapEditorViewportInputContext &context)
{
    if (context.scene == nullptr) {
        return;
    }

    for (QGraphicsItem *item : context.scene->items()) {
        if (item != nullptr && item->data(kMapScenePendingPrimarySelectionRole).toBool()) {
            item->setData(kMapScenePendingPrimarySelectionRole, false);
        }
    }
}

void resetPendingClickSelection(MapEditorViewportInputContext &context, const QPointF &scenePosition)
{
    (*context.pendingClickSelection) = true;
    (*context.pendingClickScenePosition) = scenePosition;
    (*context.pendingClickElapsed).start();
    (*context.pendingClickLineNumber) = 0;
    (*context.pendingClickSourceVertexIndex) = -1;
    (*context.pendingClickGeometryKind).clear();
    clearPendingPrimarySelectionItem(context);
}

void describePendingClickSelection(MapEditorViewportInputContext &context, QGraphicsItem *item)
{
    if (item == nullptr) {
        return;
    }

    (*context.pendingClickLineNumber) = item->data(kMapSceneLineNumberRole).toInt();
    item->setData(kMapScenePendingPrimarySelectionRole, true);
    const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
    if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item)) {
        (*context.pendingClickSourceVertexIndex) = vertexItem->vertexIndex();
        (*context.pendingClickGeometryKind) = vertexItem->geometryKind();
    } else if (subtype == kMapSceneSelectionSubtypeLineAnchor || subtype == kMapSceneSelectionSubtypeLineControl) {
        const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
        if (ownerVertexIndex >= 0) {
            (*context.pendingClickSourceVertexIndex) = ownerVertexIndex;
            (*context.pendingClickGeometryKind) = QStringLiteral("line");
        }
    } else if (subtype == kMapSceneSelectionSubtypeAreaVertex) {
        const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
        if (ownerVertexIndex >= 0) {
            (*context.pendingClickSourceVertexIndex) = ownerVertexIndex;
            (*context.pendingClickGeometryKind) = QStringLiteral("area");
        }
    } else if (subtype == kMapSceneSelectionSubtypeLineControlConnector) {
        const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
        if (ownerVertexIndex >= 0) {
            (*context.pendingClickSourceVertexIndex) = ownerVertexIndex;
            (*context.pendingClickGeometryKind) = QStringLiteral("line");
        }
    }
}

void selectSingleMapHitItem(MapEditorViewportInputContext &context, QGraphicsItem *item)
{
    if (context.scene == nullptr || item == nullptr) {
        return;
    }

    const QSignalBlocker sceneSelectionBlocker(context.scene);
    item->setSelected(true);
    const QList<QGraphicsItem *> selectedItems = context.scene->selectedItems();
    for (QGraphicsItem *selectedItem : selectedItems) {
        if (selectedItem != nullptr && selectedItem != item) {
            selectedItem->setSelected(false);
        }
    }

    if (context.syncMapSelectionFromScene != nullptr) {
        context.syncMapSelectionFromScene();
    }
}

QGraphicsItem *nearestDirectVertexLikeItemForViewportPosition(MapEditorViewportInputContext &context,
                                                              const QPoint &viewportPosition,
                                                              bool requireSelected,
                                                              bool requireVisible)
{
    if (context.scene == nullptr || context.view == nullptr) {
        return nullptr;
    }

    QGraphicsItem *nearestDirectVertexItem = nullptr;
    qreal nearestDistance = std::numeric_limits<qreal>::max();
    const QList<QGraphicsItem *> allItems = context.scene->items();
    for (QGraphicsItem *candidate : allItems) {
        if (candidate == nullptr) {
            continue;
        }

        const int subtype = candidate->data(kMapSceneSelectionSubtypeRole).toInt();
        const bool vertexLikeItem = dynamic_cast<MapEditableGeometryVertexItem *>(candidate) != nullptr
            || subtype == kMapSceneSelectionSubtypeLineAnchor
            || subtype == kMapSceneSelectionSubtypeLineControl
            || subtype == kMapSceneSelectionSubtypeLineControlConnector
            || subtype == kMapSceneSelectionSubtypeAreaVertex
            || subtype == kMapSceneSelectionSubtypePointOrientationHandle;
        if (!vertexLikeItem) {
            continue;
        }
        if (!isInteractiveMapSelectionItem(candidate)) {
            continue;
        }
        if (requireVisible && !candidate->isVisible()) {
            continue;
        }
        if (requireSelected && !candidate->isSelected()) {
            continue;
        }
        if (candidate->data(kMapSceneLineNumberRole).toInt() <= 0) {
            continue;
        }

        const QPoint candidateCenter = context.view->mapFromScene(viewportHitCenterScenePoint(candidate));
        const QPoint delta = viewportPosition - candidateCenter;
        const qreal distance = std::hypot(delta.x(), delta.y());
        if (distance > kDirectVertexAffordanceHitDistancePixels) {
            continue;
        }

        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestDirectVertexItem = candidate;
        }
    }

    return nearestDirectVertexItem;
}

QGraphicsItem *preferredMapHitItemForViewportPosition(MapEditorViewportInputContext &context,
                                                      const QPoint &viewportPosition,
                                                      bool requireSelected,
                                                      bool updatePendingClickSelection = true)
{
    if (context.scene == nullptr || context.view == nullptr) {
        return nullptr;
    }

    const QPointF scenePosition = context.view->mapToScene(viewportPosition);
    if (updatePendingClickSelection) {
        resetPendingClickSelection(context, scenePosition);
    }
    QList<QGraphicsItem *> hitItems = context.scene->items(scenePosition,
                                                           Qt::IntersectsItemShape,
                                                           Qt::DescendingOrder,
                                                           context.view->transform());
    QGraphicsItem *nearestDirectVertexItem =
        nearestDirectVertexLikeItemForViewportPosition(context,
                                                       viewportPosition,
                                                       requireSelected,
                                                       false);
    qreal nearestDistance = std::numeric_limits<qreal>::max();
    const QList<QGraphicsItem *> allItems = context.scene->items();
    for (QGraphicsItem *candidate : allItems) {
        if (candidate == nullptr) {
            continue;
        }
        if (!hitItems.contains(candidate)) {
            const int subtype = candidate->data(kMapSceneSelectionSubtypeRole).toInt();
            const bool pathSelectionCandidate = dynamic_cast<QGraphicsPathItem *>(candidate) != nullptr
                && isDistanceRankedPathSubtype(subtype);
            if (pathSelectionCandidate
                && isInteractiveMapSelectionItem(candidate)
                && (!requireSelected || candidate->isSelected())
                && candidate->data(kMapSceneLineNumberRole).toInt() > 0) {
                const qreal distancePixels =
                    genericPathItemHitDistancePixels(candidate, scenePosition, context.view->transform());
                if (std::isfinite(distancePixels)
                    && distancePixels != std::numeric_limits<qreal>::max()
                    && distancePixels <= kMaximumPathPrimaryHitDistancePixels) {
                    hitItems.append(candidate);
                }
            }
        }
        if (candidate == nearestDirectVertexItem) {
            const QPoint candidateCenter = context.view->mapFromScene(viewportHitCenterScenePoint(candidate));
            const QPoint delta = viewportPosition - candidateCenter;
            nearestDistance = std::hypot(delta.x(), delta.y());
        }
    }

    QGraphicsItem *item = preferredMapHitItem(hitItems, requireSelected, scenePosition, context.view->transform());
    if (nearestDirectVertexItem != nullptr) {
        const int nearestPriority = mapSelectionHitPriority(nearestDirectVertexItem);
        const int nearestLineNumber = nearestDirectVertexItem->data(kMapSceneLineNumberRole).toInt();
        const int itemLineNumber = item != nullptr ? item->data(kMapSceneLineNumberRole).toInt() : 0;
        if (item == nullptr
            || nearestPriority == 0
            || nearestLineNumber == itemLineNumber
            || nearestDistance <= kDirectVertexCenterHitDistance) {
            item = nearestDirectVertexItem;
        }
    }
    if (updatePendingClickSelection) {
        describePendingClickSelection(context, item);
    }
    return item;
}

QGraphicsItem *preferredHoveredPathHitItemForViewportPosition(MapEditorViewportInputContext &context,
                                                              const QPoint &viewportPosition)
{
    if (context.scene == nullptr || context.view == nullptr) {
        return nullptr;
    }

    const QPointF scenePosition = context.view->mapToScene(viewportPosition);
    QList<QGraphicsItem *> hoverHitItems;
    const QList<QGraphicsItem *> allItems = context.scene->items();
    for (QGraphicsItem *candidate : allItems) {
        if (candidate == nullptr || !candidate->data(kMapSceneInteractionHoverRole).toBool()) {
            continue;
        }
        if (dynamic_cast<QGraphicsPathItem *>(candidate) == nullptr) {
            continue;
        }
        const int subtype = candidate->data(kMapSceneSelectionSubtypeRole).toInt();
        if (!isDistanceRankedPathSubtype(subtype)) {
            continue;
        }
        if (!isInteractiveMapSelectionItem(candidate) || candidate->data(kMapSceneLineNumberRole).toInt() <= 0) {
            continue;
        }
        const qreal distancePixels =
            genericPathItemHitDistancePixels(candidate, scenePosition, context.view->transform());
        if (!std::isfinite(distancePixels)
            || distancePixels == std::numeric_limits<qreal>::max()
            || distancePixels > kMaximumPathPrimaryHitDistancePixels) {
            continue;
        }
        hoverHitItems.append(candidate);
    }

    return preferredMapHitItem(hoverHitItems, false, scenePosition, context.view->transform());
}

QGraphicsItem *preferredMapClickHitItemForViewportPosition(MapEditorViewportInputContext &context,
                                                           const QPoint &viewportPosition)
{
    if (context.scene == nullptr || context.view == nullptr) {
        return nullptr;
    }

    if (QGraphicsItem *directVertexItem =
            nearestDirectVertexLikeItemForViewportPosition(context,
                                                           viewportPosition,
                                                           false,
                                                           true)) {
        return directVertexItem;
    }

    if (QGraphicsItem *hoverItem = preferredHoveredPathHitItemForViewportPosition(context, viewportPosition)) {
        const QPointF scenePosition = context.view->mapToScene(viewportPosition);
        resetPendingClickSelection(context, scenePosition);
        describePendingClickSelection(context, hoverItem);
        return hoverItem;
    }

    return preferredMapHitItemForViewportPosition(context, viewportPosition, false);
}

bool isSameMapObjectInteractionGroup(const QGraphicsItem *item, const QGraphicsItem *referenceItem)
{
    if (item == nullptr || referenceItem == nullptr) {
        return false;
    }

    const int referenceLineNumber = referenceItem->data(kMapSceneLineNumberRole).toInt();
    return referenceLineNumber > 0
        && item->data(kMapSceneLineNumberRole).toInt() == referenceLineNumber
        && isInteractiveMapSelectionItem(item);
}

void setMapInteractionHoverItem(MapEditorViewportInputContext &context, QGraphicsItem *hoverItem)
{
    if (context.scene == nullptr) {
        return;
    }

    for (QGraphicsItem *item : context.scene->items()) {
        if (item == nullptr) {
            continue;
        }
        const bool shouldHover = isSameMapObjectInteractionGroup(item, hoverItem);
        if (item->data(kMapSceneInteractionHoverRole).toBool() == shouldHover) {
            continue;
        }
        item->setData(kMapSceneInteractionHoverRole, shouldHover);
        item->update();
    }
}

void refreshMapHoverFromCurrentCursor(MapEditorViewportInputContext &context, QWidget *viewport)
{
    if (viewport == nullptr || context.view == nullptr || context.scene == nullptr) {
        return;
    }

    const QPoint viewportPosition = viewport->mapFromGlobal(QCursor::pos());
    if (!viewport->rect().contains(viewportPosition)) {
        setMapInteractionHoverItem(context, nullptr);
        return;
    }

    QGraphicsItem *hoverItem = preferredMapHitItemForViewportPosition(context,
                                                                      viewportPosition,
                                                                      false,
                                                                      false);
    setMapInteractionHoverItem(context, hoverItem);
}

bool selectMapHitItemForContextMenu(MapEditorViewportInputContext &context, const QPoint &viewportPosition)
{
    QGraphicsItem *item = preferredMapHitItemForViewportPosition(context, viewportPosition, false);
    if (item == nullptr || context.scene == nullptr) {
        return false;
    }

    if (!item->isVisible() && item->data(kMapSceneSelectionGatedRole).toBool()) {
        item->setVisible(true);
    }
    selectSingleMapHitItem(context, item);
    if (context.prepareSelectionContextMenuState != nullptr) {
        context.prepareSelectionContextMenuState((*context.pendingClickLineNumber),
                                                 (*context.pendingClickSourceVertexIndex),
                                                 (*context.pendingClickGeometryKind),
                                                 item->scenePos());
    }
    return true;
}

void showSelectionContextMenuAtViewportPosition(MapEditorViewportInputContext &context,
                                                const QPoint &viewportPosition,
                                                const QPoint &globalPosition)
{
    if (context.showSelectionContextMenu == nullptr) {
        return;
    }

    if (!selectMapHitItemForContextMenu(context, viewportPosition)) {
        return;
    }
    context.showSelectionContextMenu(globalPosition);
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
        || subtype == kMapSceneSelectionSubtypeAreaFill;
}

bool isVertexLikeSelectionSubtype(int subtype)
{
    return subtype == kMapSceneSelectionSubtypeLineAnchor
        || subtype == kMapSceneSelectionSubtypeLineControl
        || subtype == kMapSceneSelectionSubtypeLineControlConnector
        || subtype == kMapSceneSelectionSubtypeAreaVertex
        || subtype == kMapSceneSelectionSubtypePointOrientationHandle;
}

bool hasSelectedVertexLikeItem(const MapEditorViewportInputContext &context)
{
    if (context.scene == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = context.scene->selectedItems();
    for (const QGraphicsItem *item : selectedItems) {
        if (item == nullptr) {
            continue;
        }
        if (dynamic_cast<const MapEditableGeometryVertexItem *>(item) != nullptr) {
            return true;
        }
        if (isVertexLikeSelectionSubtype(item->data(kMapSceneSelectionSubtypeRole).toInt())) {
            return true;
        }
    }
    return false;
}

qreal sceneRadiusForViewportPixels(const QGraphicsView *view, const QPoint &viewportPoint, int pixelRadius)
{
    if (view == nullptr || pixelRadius <= 0) {
        return 0.0;
    }

    const QPointF scenePoint = view->mapToScene(viewportPoint);
    const QPointF sceneOffsetPoint = view->mapToScene(viewportPoint + QPoint(pixelRadius, 0));
    return QLineF(scenePoint, sceneOffsetPoint).length();
}

std::optional<QPointF> nearestGeometryAnchorSnapPoint(const QGraphicsScene *scene,
                                                      const QPointF &scenePoint,
                                                      qreal sceneRadius)
{
    if (scene == nullptr || sceneRadius <= 0.0) {
        return std::nullopt;
    }

    const qreal maxDistanceSquared = sceneRadius * sceneRadius;
    qreal bestDistanceSquared = std::numeric_limits<qreal>::max();
    std::optional<QPointF> bestPoint;
    const QList<QGraphicsItem *> items = scene->items();
    for (QGraphicsItem *item : items) {
        QPointF candidatePoint;
        int candidateLineNumber = 0;
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item)) {
            if (vertexItem->geometryKind() != QStringLiteral("line")
                && vertexItem->geometryKind() != QStringLiteral("area")) {
                continue;
            }
            candidateLineNumber = vertexItem->lineNumber();
            candidatePoint = vertexItem->pos();
        } else if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(item)) {
            candidateLineNumber = 1;
            candidatePoint = pointItem->pos();
        } else {
            continue;
        }
        if (candidateLineNumber <= 0) {
            continue;
        }
        const qreal deltaX = candidatePoint.x() - scenePoint.x();
        const qreal deltaY = candidatePoint.y() - scenePoint.y();
        const qreal distanceSquared = (deltaX * deltaX) + (deltaY * deltaY);
        if (distanceSquared > maxDistanceSquared || distanceSquared >= bestDistanceSquared) {
            continue;
        }

        bestDistanceSquared = distanceSquared;
        bestPoint = candidatePoint;
    }

    return bestPoint;
}

QPointF snapInteractiveDrawAnchorIfAvailable(const MapEditorViewportInputContext &context,
                                             const QPoint &viewportPoint,
                                             const QPointF &scenePoint)
{
    const MapEditorInteractiveDrawMode mode = context.drawMode ? context.drawMode() : MapEditorInteractiveDrawMode::None;
    if (mode != MapEditorInteractiveDrawMode::Point
        && mode != MapEditorInteractiveDrawMode::Line
        && mode != MapEditorInteractiveDrawMode::Area) {
        return scenePoint;
    }

    const qreal snapRadius = sceneRadiusForViewportPixels(context.view, viewportPoint, 10);
    if (snapRadius <= 0.0) {
        return scenePoint;
    }

    const std::optional<QPointF> snappedPoint = nearestGeometryAnchorSnapPoint(context.scene, scenePoint, snapRadius);
    return snappedPoint.value_or(scenePoint);
}

int nearestNeighborGeometryLineNumberForSnapGuides(const MapEditorViewportInputContext &context,
                                                   const QPoint &viewportPoint,
                                                   const QPointF &scenePoint)
{
    if (context.scene == nullptr || context.view == nullptr) {
        return 0;
    }

    const qreal guideRadius = sceneRadiusForViewportPixels(context.view, viewportPoint, 28);
    if (guideRadius <= 0.0) {
        return 0;
    }

    qreal bestDistanceSquared = std::numeric_limits<qreal>::max();
    int bestLineNumber = 0;
    const QList<QGraphicsItem *> items = context.scene->items();
    for (QGraphicsItem *item : items) {
        auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (pathItem == nullptr) {
            continue;
        }
        const int candidateLineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (candidateLineNumber <= 0) {
            continue;
        }

        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        if (subtype != kMapSceneSelectionSubtypeGeneric
            && subtype != kMapSceneSelectionSubtypeLineDetail
            && subtype != kMapSceneSelectionSubtypeAreaFill) {
            continue;
        }

        const QPointF localCandidate = pathItem->mapFromScene(scenePoint);
        QPainterPathStroker stroker;
        stroker.setWidth(guideRadius * 2.0);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        const QPainterPath guideHitPath = stroker.createStroke(pathItem->path()).united(pathItem->path());
        if (!guideHitPath.contains(localCandidate)) {
            continue;
        }

        const QPointF center = pathItem->sceneBoundingRect().center();
        const qreal deltaX = center.x() - scenePoint.x();
        const qreal deltaY = center.y() - scenePoint.y();
        const qreal distanceSquared = (deltaX * deltaX) + (deltaY * deltaY);
        if (distanceSquared >= bestDistanceSquared) {
            continue;
        }

        bestDistanceSquared = distanceSquared;
        bestLineNumber = candidateLineNumber;
    }

    return bestLineNumber;
}

QVector<QPointF> snapGuidePointsForNearbyGeometry(const MapEditorViewportInputContext &context,
                                                  const QPoint &viewportPoint,
                                                  const QPointF &scenePoint)
{
    QVector<QPointF> guidePoints;
    if (context.scene == nullptr) {
        return guidePoints;
    }

    const int guideLineNumber = nearestNeighborGeometryLineNumberForSnapGuides(context, viewportPoint, scenePoint);
    const qreal guideRadius = sceneRadiusForViewportPixels(context.view, viewportPoint, 28);
    const qreal maxPointDistanceSquared = guideRadius * guideRadius;

    const QList<QGraphicsItem *> items = context.scene->items();
    for (QGraphicsItem *item : items) {
        if (item == nullptr) {
            continue;
        }
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item)) {
            if (guideLineNumber <= 0 || vertexItem->lineNumber() != guideLineNumber) {
                continue;
            }
            if (vertexItem->geometryKind() == QStringLiteral("line")
                || vertexItem->geometryKind() == QStringLiteral("area")) {
                guidePoints.append(vertexItem->pos());
            }
            continue;
        }
        if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(item)) {
            if (guideRadius <= 0.0) {
                continue;
            }
            const QPointF candidatePoint = pointItem->pos();
            const qreal deltaX = candidatePoint.x() - scenePoint.x();
            const qreal deltaY = candidatePoint.y() - scenePoint.y();
            const qreal distanceSquared = (deltaX * deltaX) + (deltaY * deltaY);
            if (distanceSquared <= maxPointDistanceSquared) {
                guidePoints.append(candidatePoint);
            }
        }
    }
    return guidePoints;
}

bool isLineAnchorSnapActive(const MapEditorViewportInputContext &context,
                            const QPointF &scenePoint,
                            const QPointF &snappedPoint)
{
    if (context.view == nullptr) {
        return false;
    }

    const QPoint viewportAnchor = context.view->mapFromScene(scenePoint);
    const qreal snapRadius = sceneRadiusForViewportPixels(context.view, viewportAnchor, 10);
    if (snapRadius <= 0.0) {
        return false;
    }

    const qreal distance = QLineF(scenePoint, snappedPoint).length();
    return distance > 0.0 && distance <= snapRadius;
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
        const bool usesDistanceRanking = scenePosition.has_value() && isDistanceRankedPathSubtype(subtype);
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
                       || subtype == kMapSceneSelectionSubtypeLineControlConnector
                       || subtype == kMapSceneSelectionSubtypeAreaVertex
                       || subtype == kMapSceneSelectionSubtypePointOrientationHandle) {
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

}

MapEditorInteractiveDrawMode MapEditorViewportInputController::drawMode() const
{
    return context_.drawMode ? context_.drawMode() : MapEditorInteractiveDrawMode::None;
}

QString MapEditorViewportInputController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorViewportInputController", text);
}

MapEditorViewportInputController::MapEditorViewportInputController(MapEditorViewportInputContext context)
    : context_(std::move(context))
{
}

void MapEditorViewportInputController::showContextMenuAtViewportPosition(const QPoint &viewportPosition,
                                                                         const QPoint &globalPosition)
{
    showSelectionContextMenuAtViewportPosition(context_, viewportPosition, globalPosition);
}

std::optional<bool> MapEditorViewportInputController::handleEvent(QObject *watched, QEvent *event)
{
    if (context_.view == nullptr) {
        return std::nullopt;
    }

    QWidget *viewport = context_.view->viewport();
    const auto handleDeleteKeyPress = [&](QKeyEvent *keyEvent) -> bool {
        if (keyEvent == nullptr) {
            return false;
        }
        const Qt::KeyboardModifiers disallowedModifiers =
            keyEvent->modifiers() & ~(Qt::KeyboardModifier::KeypadModifier);
        const bool deleteKeyNoModifier = disallowedModifiers == Qt::NoModifier;

        if (drawMode() == MapEditorInteractiveDrawMode::Line
            || drawMode() == MapEditorInteractiveDrawMode::Area
            || drawMode() == MapEditorInteractiveDrawMode::Freehand) {
            if ((keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Delete)
                && deleteKeyNoModifier
                && drawMode() != MapEditorInteractiveDrawMode::Freehand) {
                if (drawMode() == MapEditorInteractiveDrawMode::Line
                    && !(*context_.interactiveDrawLineVertices).isEmpty()) {
                    (*context_.interactiveDrawLineVertices).removeLast();
                    if (!(*context_.interactiveDrawLineVertices).isEmpty()) {
                        MapEditorInteractiveLineDraftVertex &tail = (*context_.interactiveDrawLineVertices).last();
                        tail.outgoingControlScene.reset();
                        tail.outgoingControlSource.reset();
                    }
                    context_.updateInteractiveDrawPreview();
                    (*context_.toolbarStatusNote) = tr("Vertex removed from current draft (%1 remaining).")
                                             .arg((*context_.interactiveDrawLineVertices).size());
                    context_.refreshToolbarSummary();
                    context_.updateCommandSurfaceState();
                    return true;
                }
                if (drawMode() == MapEditorInteractiveDrawMode::Area
                    && !(*context_.interactiveDrawLineVertices).isEmpty()) {
                    (*context_.interactiveDrawLineVertices).removeLast();
                    if (!(*context_.interactiveDrawLineVertices).isEmpty()) {
                        MapEditorInteractiveLineDraftVertex &tail = (*context_.interactiveDrawLineVertices).last();
                        tail.outgoingControlScene.reset();
                        tail.outgoingControlSource.reset();
                    }
                    context_.updateInteractiveDrawPreview();
                    (*context_.toolbarStatusNote) = tr("Vertex removed from current draft (%1 remaining).")
                                             .arg((*context_.interactiveDrawLineVertices).size());
                    context_.refreshToolbarSummary();
                    context_.updateCommandSurfaceState();
                    return true;
                }
            }
        }

        if ((keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
            && deleteKeyNoModifier) {
            if (hasSelectedVertexLikeItem(context_)) {
                if (context_.removeLineVertexFromSelection()) {
                    return true;
                }
            }
            if (context_.deleteSelectedObjectFromSelection
                && context_.deleteSelectedObjectFromSelection()) {
                return true;
            }
        }
        return false;
    };

    if (watched == viewport || watched == context_.view) {
        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease:
            (*context_.lastTabletInteractionUtc) = QDateTime::currentDateTimeUtc();
            break;
        case QEvent::Enter:
        case QEvent::Show:
        case QEvent::FocusIn:
            applyDefaultMapViewportCursor(context_, viewport);
            refreshMapHoverFromCurrentCursor(context_, viewport);
            break;
        case QEvent::ContextMenu: {
            auto *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
            QPoint viewportPosition = contextMenuEvent->pos();
            if (watched == context_.view) {
                viewportPosition = viewport->mapFrom(context_.view, contextMenuEvent->pos());
            }
            if (context_.mapPanMoved != nullptr && (*context_.mapPanMoved)) {
                (*context_.mapPanMoved) = false;
                event->accept();
                return true;
            }
            (*context_.mapPanActive) = false;
            if (context_.mapPanMoved != nullptr) {
                (*context_.mapPanMoved) = false;
            }
            applyDefaultMapViewportCursor(context_, viewport);
            showContextMenuAtViewportPosition(viewportPosition, contextMenuEvent->globalPos());
            event->accept();
            return true;
        }
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton) {
                context_.view->setFocus(Qt::MouseFocusReason);
                viewport->setFocus(Qt::MouseFocusReason);
                (*context_.mapPanActive) = true;
                if (context_.mapPanMoved != nullptr) {
                    (*context_.mapPanMoved) = false;
                }
                if (context_.mapPanStartPosition != nullptr) {
                    (*context_.mapPanStartPosition) = mouseEvent->pos();
                }
                if (context_.mapPanLastPosition != nullptr) {
                    (*context_.mapPanLastPosition) = mouseEvent->pos();
                }
                viewport->setCursor(Qt::OpenHandCursor);
                event->accept();
                return true;
            }
            if (isSecondaryClickPress(mouseEvent)) {
                (*context_.mapPanActive) = false;
                if (context_.mapPanMoved != nullptr) {
                    (*context_.mapPanMoved) = false;
                }
                applyDefaultMapViewportCursor(context_, viewport);
                showContextMenuAtViewportPosition(mouseEvent->pos(), viewport->mapToGlobal(mouseEvent->pos()));
                event->accept();
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton) {
                context_.view->setFocus(Qt::MouseFocusReason);
                viewport->setFocus(Qt::MouseFocusReason);
                if (drawMode() == MapEditorInteractiveDrawMode::Freehand) {
                    if (context_.textEditor == nullptr) {
                        (*context_.toolbarStatusNote) = tr("Drawing failed: no active TH2 text editor.");
                        context_.refreshToolbarSummary();
                        event->accept();
                        return true;
                    }

                    context_.clearInteractiveDrawSession(false);
                    const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                    (*context_.interactiveDrawStrokeActive) = true;
                    (*context_.interactiveDrawSourceVertices).append(context_.sourcePointFromScenePosition(scenePoint));
                    (*context_.interactiveDrawSceneVertices).append(scenePoint);
                    context_.updateInteractiveDrawPreview();
                    (*context_.toolbarStatusNote) = tr("Freehand mode: drawing stroke...");
                    context_.refreshToolbarSummary();
                    context_.updateCommandSurfaceState();
                    (*context_.primaryPointerInteractionActive) = false;
                    event->accept();
                    return true;
                }
                if (drawMode() == MapEditorInteractiveDrawMode::Line
                    || drawMode() == MapEditorInteractiveDrawMode::Area) {
                    const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                    const QPointF anchorScenePoint = snapInteractiveDrawAnchorIfAvailable(context_, mouseEvent->pos(), scenePoint);
                    const QPointF sceneOffset = context_.view->mapToScene(mouseEvent->pos() + QPoint(8, 0));
                    const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                    if (const auto handle = context_.interactiveLineControlAt(scenePoint, controlHitRadius)) {
                        (*context_.interactiveDrawControlDragActive) = true;
                        (*context_.interactiveDrawControlDragHandle) = handle.value();
                        (*context_.interactiveDrawAnchorPressActive) = false;
                        (*context_.interactiveDrawAnchorDragActive) = false;
                        (*context_.interactiveDrawHoverActive) = false;
                        if (context_.interactiveDrawHoverSnapActive != nullptr) {
                            (*context_.interactiveDrawHoverSnapActive) = false;
                        }
                        if (context_.interactiveDrawHoverSnapGuideScenePoints != nullptr) {
                            context_.interactiveDrawHoverSnapGuideScenePoints->clear();
                        }
                        viewport->setCursor(Qt::ClosedHandCursor);
                        if (handle->kind == MapEditorInteractiveLineControlHandleRef::Kind::Anchor) {
                            (*context_.toolbarStatusNote) = drawMode() == MapEditorInteractiveDrawMode::Line
                                ? tr("Line mode: dragging draft point.")
                                : tr("Area mode: dragging draft point.");
                        } else {
                            (*context_.toolbarStatusNote) = drawMode() == MapEditorInteractiveDrawMode::Line
                                ? tr("Line mode: dragging bezier control point.")
                                : tr("Area mode: dragging bezier control point.");
                        }
                        context_.refreshToolbarSummary();
                        context_.updateCommandSurfaceState();
                        (*context_.primaryPointerInteractionActive) = false;
                        event->accept();
                        return true;
                    }

                    (*context_.interactiveDrawAnchorPressActive) = true;
                    (*context_.interactiveDrawAnchorPressScenePoint) = anchorScenePoint;
                    (*context_.interactiveDrawAnchorDragActive) = false;
                    (*context_.interactiveDrawAnchorDragScenePoint) = (*context_.interactiveDrawAnchorPressScenePoint);
                    (*context_.interactiveDrawControlDragActive) = false;
                    (*context_.interactiveDrawHoverActive) = false;
                    if (context_.interactiveDrawHoverSnapActive != nullptr) {
                        (*context_.interactiveDrawHoverSnapActive) = false;
                    }
                    if (context_.interactiveDrawHoverSnapGuideScenePoints != nullptr) {
                        context_.interactiveDrawHoverSnapGuideScenePoints->clear();
                    }
                    context_.updateInteractiveDrawPreview();
                    (*context_.primaryPointerInteractionActive) = false;
                    event->accept();
                    return true;
                }
                if (drawMode() == MapEditorInteractiveDrawMode::Point) {
                    const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                    const QPointF anchorScenePoint = snapInteractiveDrawAnchorIfAvailable(context_, mouseEvent->pos(), scenePoint);
                    if (context_.handleInteractiveDrawClick(anchorScenePoint)) {
                        (*context_.primaryPointerInteractionActive) = false;
                        event->accept();
                        return true;
                    }
                }
                if (context_.handleInteractiveDrawClick(context_.view->mapToScene(mouseEvent->pos()))) {
                    (*context_.primaryPointerInteractionActive) = false;
                    event->accept();
                    return true;
                }
                (*context_.primaryPointerInteractionActive) = true;
                if (context_.view != nullptr) {
                    QGraphicsItem *item = preferredMapClickHitItemForViewportPosition(context_, mouseEvent->pos());
                    if (item != nullptr) {
                        if (dynamic_cast<QGraphicsPathItem *>(item) != nullptr) {
                            const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
                            if (subtype == kMapSceneSelectionSubtypeGeneric
                                || subtype == kMapSceneSelectionSubtypeLineDetail
                                || subtype == kMapSceneSelectionSubtypeAreaFill) {
                                setMapInteractionHoverItem(context_, item);
                                selectSingleMapHitItem(context_, item);
                                event->accept();
                                return true;
                            }
                        }
                    }
                }
            }
            break;
        }
        case QEvent::MouseMove: {
            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawControlDragActive)) {
                const QPointF scenePoint = context_.view->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                if (context_.setInteractiveLineControlScenePoint((*context_.interactiveDrawControlDragHandle), scenePoint)) {
                    context_.updateInteractiveDrawPreview();
                }
                event->accept();
                return true;
            }

            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawAnchorPressActive)) {
                const QPointF scenePoint = context_.view->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal dragThreshold = 4.0;
                if (!(*context_.interactiveDrawAnchorDragActive)
                    && QLineF((*context_.interactiveDrawAnchorPressScenePoint), scenePoint).length() >= dragThreshold) {
                    (*context_.interactiveDrawAnchorDragActive) = true;
                }
                (*context_.interactiveDrawAnchorDragScenePoint) = scenePoint;
                context_.updateInteractiveDrawPreview();
                event->accept();
                return true;
            }

            if (drawMode() == MapEditorInteractiveDrawMode::Freehand && (*context_.interactiveDrawStrokeActive)) {
                const QPointF scenePoint = context_.view->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal minimumSceneSampleDistance = 4.0;
                if ((*context_.interactiveDrawSceneVertices).isEmpty()
                    || QLineF((*context_.interactiveDrawSceneVertices).last(), scenePoint).length() >= minimumSceneSampleDistance) {
                    (*context_.interactiveDrawSceneVertices).append(scenePoint);
                    (*context_.interactiveDrawSourceVertices).append(context_.sourcePointFromScenePosition(scenePoint));
                    context_.updateInteractiveDrawPreview();
                    context_.updateCommandSurfaceState();
                }
                event->accept();
                return true;
            }

            if (drawMode() == MapEditorInteractiveDrawMode::Point
                || drawMode() == MapEditorInteractiveDrawMode::Line
                || drawMode() == MapEditorInteractiveDrawMode::Area) {
                setMapInteractionHoverItem(context_, nullptr);
                const QPoint mousePosition = static_cast<QMouseEvent *>(event)->pos();
                const QPointF scenePoint = context_.view->mapToScene(mousePosition);
                const QPointF hoverScenePoint = snapInteractiveDrawAnchorIfAvailable(context_, mousePosition, scenePoint);
                const bool snapActive = isLineAnchorSnapActive(context_, scenePoint, hoverScenePoint);
                if (drawMode() == MapEditorInteractiveDrawMode::Point) {
                    applyDefaultMapViewportCursor(context_, viewport);
                } else if (!(*context_.interactiveDrawLineVertices).isEmpty()) {
                    const QPointF sceneOffset = context_.view->mapToScene(mousePosition + QPoint(8, 0));
                    const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                    if (context_.interactiveLineControlAt(scenePoint, controlHitRadius).has_value()) {
                        viewport->setCursor(Qt::OpenHandCursor);
                    } else {
                        applyDefaultMapViewportCursor(context_, viewport);
                    }
                } else {
                    applyDefaultMapViewportCursor(context_, viewport);
                }
                (*context_.interactiveDrawHoverActive) = true;
                (*context_.interactiveDrawHoverScenePoint) = hoverScenePoint;
                if (context_.interactiveDrawHoverSnapActive != nullptr) {
                    (*context_.interactiveDrawHoverSnapActive) = snapActive;
                }
                if (context_.interactiveDrawHoverSnapScenePoint != nullptr && snapActive) {
                    (*context_.interactiveDrawHoverSnapScenePoint) = hoverScenePoint;
                }
                if (context_.interactiveDrawHoverSnapGuideScenePoints != nullptr) {
                    (*context_.interactiveDrawHoverSnapGuideScenePoints) =
                        snapGuidePointsForNearbyGeometry(context_, mousePosition, scenePoint);
                }
                context_.updateInteractiveDrawPreview();
                event->accept();
                return true;
            }

            if (!(*context_.mapPanActive)) {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                QGraphicsItem *hoverItem = preferredMapHitItemForViewportPosition(context_,
                                                                                  mouseEvent->pos(),
                                                                                  false,
                                                                                  false);
                setMapInteractionHoverItem(context_, hoverItem);
                applyDefaultMapViewportCursor(context_, viewport);
            }

            if (!(*context_.mapPanActive)) {
                break;
            }

            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (context_.mapPanMoved != nullptr
                && context_.mapPanStartPosition != nullptr
                && !(*context_.mapPanMoved)) {
                constexpr int dragThreshold = 4;
                const QPoint dragDelta = mouseEvent->pos() - (*context_.mapPanStartPosition);
                if (std::abs(dragDelta.x()) < dragThreshold && std::abs(dragDelta.y()) < dragThreshold) {
                    event->accept();
                    return true;
                }
                (*context_.mapPanMoved) = true;
                viewport->setCursor(Qt::ClosedHandCursor);
            }
            const QPoint delta = mouseEvent->pos() - (*context_.mapPanLastPosition);
            (*context_.mapPanLastPosition) = mouseEvent->pos();
            if (context_.view->horizontalScrollBar() != nullptr) {
                context_.view->horizontalScrollBar()->setValue(context_.view->horizontalScrollBar()->value() - delta.x());
            }
            if (context_.view->verticalScrollBar() != nullptr) {
                context_.view->verticalScrollBar()->setValue(context_.view->verticalScrollBar()->value() - delta.y());
            }

            (*context_.autoFitEnabled) = false;
            context_.syncZoomFactorFromView();
            context_.updateCommandSurfaceState();
            event->accept();
            return true;
        }
        case QEvent::MouseButtonDblClick: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (drawMode() == MapEditorInteractiveDrawMode::Line
                && mouseEvent->button() == Qt::LeftButton
                && context_.cancelInteractiveDrawingToSelectMode != nullptr) {
                const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                const QPointF anchorScenePoint = snapInteractiveDrawAnchorIfAvailable(context_, mouseEvent->pos(), scenePoint);
                bool anchorAlreadyCaptured = false;
                if (context_.interactiveDrawLineVertices != nullptr
                    && !context_.interactiveDrawLineVertices->isEmpty()) {
                    const QPointF probe = context_.view->mapToScene(mouseEvent->pos() + QPoint(10, 0));
                    const qreal hitRadius = std::max<qreal>(5.0, QLineF(scenePoint, probe).length());
                    anchorAlreadyCaptured =
                        QLineF(context_.interactiveDrawLineVertices->last().anchorScene, anchorScenePoint).length() <= hitRadius;
                }
                if (!anchorAlreadyCaptured && context_.captureInteractiveLineAnchor) {
                    context_.captureInteractiveLineAnchor(anchorScenePoint, std::nullopt);
                }
                (*context_.interactiveDrawAnchorPressActive) = false;
                (*context_.interactiveDrawAnchorDragActive) = false;
                (*context_.interactiveDrawControlDragActive) = false;
                (*context_.interactiveDrawHoverActive) = false;
                if (context_.interactiveDrawHoverSnapActive != nullptr) {
                    (*context_.interactiveDrawHoverSnapActive) = false;
                }
                context_.cancelInteractiveDrawingToSelectMode();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawControlDragActive)
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                const MapEditorInteractiveLineControlHandleRef dragHandle = (*context_.interactiveDrawControlDragHandle);
                if (dragHandle.kind == MapEditorInteractiveLineControlHandleRef::Kind::Anchor
                    && context_.interactiveDrawLineVertices != nullptr
                    && dragHandle.vertexIndex == 0
                    && context_.interactiveDrawLineVertices->size() >= (drawMode() == MapEditorInteractiveDrawMode::Line ? 2 : 3)) {
                    const QPointF closeHitProbe = context_.view->mapToScene(mouseEvent->pos() + QPoint(10, 0));
                    const qreal closeHitRadius = std::max<qreal>(5.0, QLineF(scenePoint, closeHitProbe).length());
                    const QPointF firstAnchorScenePoint = context_.interactiveDrawLineVertices->first().anchorScene;
                    if (QLineF(scenePoint, firstAnchorScenePoint).length() <= closeHitRadius
                        && context_.commitInteractiveDrawSession != nullptr) {
                        (*context_.interactiveDrawControlDragActive) = false;
                        context_.commitInteractiveDrawSession(drawMode() == MapEditorInteractiveDrawMode::Line);
                        event->accept();
                        return true;
                    }
                }
                context_.setInteractiveLineControlScenePoint((*context_.interactiveDrawControlDragHandle), scenePoint);
                (*context_.interactiveDrawControlDragActive) = false;
                const QPointF sceneOffset = context_.view->mapToScene(mouseEvent->pos() + QPoint(8, 0));
                const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                if (context_.interactiveLineControlAt(scenePoint, controlHitRadius).has_value()) {
                    viewport->setCursor(Qt::OpenHandCursor);
                } else {
                    applyDefaultMapViewportCursor(context_, viewport);
                }
                const bool draggedAnchor =
                    context_.interactiveDrawControlDragHandle->kind == MapEditorInteractiveLineControlHandleRef::Kind::Anchor;
                if (draggedAnchor) {
                    (*context_.toolbarStatusNote) = drawMode() == MapEditorInteractiveDrawMode::Line
                        ? tr("Line mode: draft point moved.")
                        : tr("Area mode: draft point moved.");
                } else {
                    (*context_.toolbarStatusNote) = drawMode() == MapEditorInteractiveDrawMode::Line
                        ? tr("Line mode: bezier control adjusted.")
                        : tr("Area mode: bezier control adjusted.");
                }
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                event->accept();
                return true;
            }

            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawAnchorPressActive)
                && mouseEvent->button() == Qt::LeftButton) {
                const MapEditorInteractiveDrawMode currentDrawMode = drawMode();
                const QPointF anchorScenePoint = (*context_.interactiveDrawAnchorPressScenePoint);
                const QPointF releaseScenePoint = context_.view->mapToScene(mouseEvent->pos());
                std::optional<QPointF> dragScenePoint;
                if ((*context_.interactiveDrawAnchorDragActive)) {
                    constexpr qreal dragThreshold = 4.0;
                    if (QLineF((*context_.interactiveDrawAnchorPressScenePoint), releaseScenePoint).length() >= dragThreshold) {
                        dragScenePoint = releaseScenePoint;
                    }
                }

                bool completeClosedDraftByClick = false;
                if ((currentDrawMode == MapEditorInteractiveDrawMode::Line
                     || currentDrawMode == MapEditorInteractiveDrawMode::Area)
                    && !(*context_.interactiveDrawAnchorDragActive)
                    && context_.interactiveDrawLineVertices != nullptr
                    && context_.interactiveDrawLineVertices->size() >= (currentDrawMode == MapEditorInteractiveDrawMode::Line ? 2 : 3)) {
                    const QPointF closeHitProbe = context_.view->mapToScene(mouseEvent->pos() + QPoint(10, 0));
                    const qreal closeHitRadius = std::max<qreal>(5.0, QLineF(releaseScenePoint, closeHitProbe).length());
                    const QPointF firstAnchorScenePoint = context_.interactiveDrawLineVertices->first().anchorScene;
                    completeClosedDraftByClick = QLineF(releaseScenePoint, firstAnchorScenePoint).length() <= closeHitRadius;
                }

                (*context_.interactiveDrawAnchorPressActive) = false;
                (*context_.interactiveDrawAnchorDragActive) = false;
                (*context_.interactiveDrawHoverActive) = false;
                if (context_.interactiveDrawHoverSnapActive != nullptr) {
                    (*context_.interactiveDrawHoverSnapActive) = false;
                }

                if (completeClosedDraftByClick && context_.commitInteractiveDrawSession != nullptr) {
                    context_.commitInteractiveDrawSession(currentDrawMode == MapEditorInteractiveDrawMode::Line);
                    event->accept();
                    return true;
                }

                context_.captureInteractiveLineAnchor(anchorScenePoint, dragScenePoint);
                (*context_.toolbarStatusNote) = currentDrawMode == MapEditorInteractiveDrawMode::Line
                    ? tr("Line mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                          .arg((*context_.interactiveDrawLineVertices).size())
                    : tr("Area mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                          .arg((*context_.interactiveDrawLineVertices).size());
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                event->accept();
                return true;
            }
            if (drawMode() == MapEditorInteractiveDrawMode::Freehand
                && (*context_.interactiveDrawStrokeActive)
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF releasePoint = context_.view->mapToScene(mouseEvent->pos());
                if ((*context_.interactiveDrawSceneVertices).isEmpty()
                    || QLineF((*context_.interactiveDrawSceneVertices).last(), releasePoint).length() >= 1.0) {
                    (*context_.interactiveDrawSceneVertices).append(releasePoint);
                    (*context_.interactiveDrawSourceVertices).append(context_.sourcePointFromScenePosition(releasePoint));
                }
                (*context_.interactiveDrawStrokeActive) = false;

                if ((*context_.interactiveDrawSourceVertices).size() < 2) {
                    context_.clearInteractiveDrawSession(false);
                    (*context_.toolbarStatusNote) = tr("Freehand mode needs a drag stroke to create a line.");
                    context_.refreshToolbarSummary();
                    context_.updateCommandSurfaceState();
                    event->accept();
                    return true;
                }

                const bool committed = context_.commitInteractiveDrawVertices(QStringLiteral("line"),
                                                                     (*context_.interactiveDrawSourceVertices),
                                                                     tr("freehand line"));
                context_.clearInteractiveDrawSession(false);
                if (committed) {
                    context_.updateHelpPanel();
                }
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                event->accept();
                return true;
            }

            if (mouseEvent->button() == Qt::LeftButton && mouseEvent->buttons() == Qt::NoButton) {
                (*context_.primaryPointerInteractionActive) = false;
            }

            if ((*context_.mapPanActive) && mouseEvent->button() == Qt::RightButton) {
                (*context_.mapPanActive) = false;
                applyDefaultMapViewportCursor(context_, viewport);
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::Wheel: {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            if ((*context_.nativeZoomGestureActive)
                && (*context_.lastNativeZoomGestureUtc).isValid()
                && (*context_.lastNativeZoomGestureUtc).msecsTo(QDateTime::currentDateTimeUtc()) > 1500) {
                (*context_.nativeZoomGestureActive) = false;
            }

            const bool recentNativeZoom = (*context_.lastNativeZoomGestureUtc).isValid()
                && (*context_.lastNativeZoomGestureUtc).msecsTo(QDateTime::currentDateTimeUtc()) <= 150;
            if ((*context_.nativeZoomGestureActive) || recentNativeZoom) {
                event->accept();
                return true;
            }

            if ((*context_.primaryPointerInteractionActive)) {
                event->accept();
                return true;
            }

            const Qt::KeyboardModifiers modifiers = wheelEvent->modifiers();
            const bool cmdModifier = modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier);
            const bool preciseScroll = wheelEventHasPreciseScrollingDeltas(wheelEvent);
            const MapEditorWheelAction wheelAction = resolveMapEditorWheelAction((*context_.touchFriendlyControlsEnabled),
                                                                                 preciseScroll,
                                                                                 cmdModifier);
            if (wheelAction == MapEditorWheelAction::Zoom) {
                const QPoint pixelDelta = wheelEvent->pixelDelta();
                const QPoint angleDelta = wheelEvent->angleDelta();
                qreal delta = !pixelDelta.isNull()
                    ? static_cast<qreal>(pixelDelta.y())
                    : static_cast<qreal>(angleDelta.y());
                if (qFuzzyIsNull(delta) && !angleDelta.isNull()) {
                    delta = static_cast<qreal>(angleDelta.x());
                }

                if (!qFuzzyIsNull(delta)) {
                    const qreal factor = std::pow(1.0015, delta);
                    context_.applyZoomAtViewportPosition(factor, wheelEvent->position());
                }

                event->accept();
                return true;
            }

            QPoint panDelta = wheelEvent->pixelDelta();
            if (panDelta.isNull()) {
                const QPoint angleDelta = wheelEvent->angleDelta();
                panDelta = QPoint(qRound(angleDelta.x() / 4.0), qRound(angleDelta.y() / 4.0));
            }

            if (!panDelta.isNull()) {
                if (context_.view->horizontalScrollBar() != nullptr) {
                    context_.view->horizontalScrollBar()->setValue(context_.view->horizontalScrollBar()->value() - panDelta.x());
                }
                if (context_.view->verticalScrollBar() != nullptr) {
                    context_.view->verticalScrollBar()->setValue(context_.view->verticalScrollBar()->value() - panDelta.y());
                }

                (*context_.autoFitEnabled) = false;
                context_.syncZoomFactorFromView();
                context_.updateCommandSurfaceState();
            }

            event->accept();
            return true;
        }
        case QEvent::NativeGesture: {
            auto *gestureEvent = static_cast<QNativeGestureEvent *>(event);
            if ((*context_.primaryPointerInteractionActive)) {
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::BeginNativeGesture) {
                (*context_.nativeZoomGestureActive) = true;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::EndNativeGesture) {
                (*context_.nativeZoomGestureActive) = false;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
                (*context_.nativeZoomGestureActive) = true;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                const qreal rawValue = gestureEvent->value();
                if (!std::isfinite(rawValue)) {
                    event->accept();
                    return true;
                }

                // Clamp one pinch update so trackpad spikes cannot jump straight to extreme scales.
                const qreal clampedDelta = qBound(-0.35, rawValue, 0.35);
                const qreal factor = std::exp(clampedDelta);
                if (factor > 0.0) {
                    context_.applyZoomAtViewportPosition(factor, gestureEvent->position());
                }
                event->accept();
                return true;
            }

            // On some platforms/devices, pinch can interleave non-zoom native gestures.
            // While a zoom sequence is active, suppress them so pinch never pans the viewport.
            if ((*context_.nativeZoomGestureActive)
                && (gestureEvent->gestureType() == Qt::PanNativeGesture
                    || gestureEvent->gestureType() == Qt::RotateNativeGesture)) {
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::TouchBegin: {
            if (!shouldEnableTouchPanCandidate((*context_.touchFriendlyControlsEnabled),
                                               (*context_.selectModeActive),
                                               (*context_.primaryPointerInteractionActive))) {
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() == 2) {
                const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
                (*context_.touchPanCandidate) = true;
                (*context_.touchPanActive) = false;
                (*context_.touchPanStartPosition) = centroid;
                (*context_.touchPanLastPosition) = centroid;
            }
            break;
        }
        case QEvent::TouchUpdate: {
            if (!(*context_.touchPanCandidate) || (*context_.primaryPointerInteractionActive)) {
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() != 2) {
                (*context_.touchPanCandidate) = false;
                (*context_.touchPanActive) = false;
                break;
            }

            const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
            if (!(*context_.touchPanActive)) {
                const qreal threshold = 8.0;
                if (QLineF((*context_.touchPanStartPosition), centroid).length() < threshold) {
                    event->accept();
                    return true;
                }
                (*context_.touchPanActive) = true;
            }

            const QPointF delta = centroid - (*context_.touchPanLastPosition);
            (*context_.touchPanLastPosition) = centroid;
            if (context_.view->horizontalScrollBar() != nullptr) {
                context_.view->horizontalScrollBar()->setValue(context_.view->horizontalScrollBar()->value() - qRound(delta.x()));
            }
            if (context_.view->verticalScrollBar() != nullptr) {
                context_.view->verticalScrollBar()->setValue(context_.view->verticalScrollBar()->value() - qRound(delta.y()));
            }

            (*context_.autoFitEnabled) = false;
            context_.syncZoomFactorFromView();
            context_.updateCommandSurfaceState();
            event->accept();
            return true;
        }
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            (*context_.touchPanCandidate) = false;
            (*context_.touchPanActive) = false;
            break;
        case QEvent::Leave:
            if ((*context_.mapPanActive)) {
                (*context_.mapPanActive) = false;
                if (context_.mapPanMoved != nullptr) {
                    (*context_.mapPanMoved) = false;
                }
                viewport->unsetCursor();
            }
            (*context_.primaryPointerInteractionActive) = false;
            (*context_.touchPanCandidate) = false;
            (*context_.touchPanActive) = false;
            (*context_.nativeZoomGestureActive) = false;
            (*context_.interactiveDrawStrokeActive) = false;
            (*context_.interactiveDrawAnchorPressActive) = false;
            (*context_.interactiveDrawAnchorDragActive) = false;
            (*context_.interactiveDrawControlDragActive) = false;
            if (context_.interactiveDrawHoverSnapActive != nullptr) {
                (*context_.interactiveDrawHoverSnapActive) = false;
            }
            viewport->unsetCursor();
            if ((*context_.interactiveDrawHoverActive)) {
                (*context_.interactiveDrawHoverActive) = false;
                context_.updateInteractiveDrawPreview();
            }
            setMapInteractionHoverItem(context_, nullptr);
            break;
        case QEvent::Resize:
            if ((*context_.autoFitEnabled) && context_.view->isVisible()) {
                if (context_.fitMapToViewAfterViewportResize) {
                    context_.fitMapToViewAfterViewportResize((*context_.fitBackgroundRequested));
                } else {
                    context_.fitMapToView((*context_.fitBackgroundRequested));
                }
            }
            break;
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (handleDeleteKeyPress(keyEvent)) {
                event->accept();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return std::nullopt;
}
}
