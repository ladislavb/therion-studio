#include "MapEditorViewportInputController.h"

#include <QCoreApplication>

#include "MapEditorInputPolicy.h"
#include "MapEditorLineDecorationItem.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
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
constexpr qreal kFilledPathInteriorHitDistancePixels = 0.0;
constexpr qreal kDirectVertexCenterHitDistance = 1.0;
constexpr qreal kDirectVertexAffordanceHitDistancePixels = 12.0;
constexpr qreal kMaximumPathPrimaryHitDistancePixels = 5.0;
constexpr qint64 kHandledTabletMouseSuppressionMs = 250;
constexpr qint64 kSlowMapInputEventMs = 8;
constexpr qint64 kFreehandPreviewRefreshIntervalMs = 16;
constexpr int kInteractiveDrawControlHitRadiusPixels = 8;
constexpr int kInteractiveDrawDragThresholdPixels = 4;
constexpr int kInteractiveDrawCloseHitRadiusPixels = 6;

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

const char *mapInputEventTypeName(QEvent::Type type)
{
    switch (type) {
    case QEvent::MouseButtonPress:
        return "MouseButtonPress";
    case QEvent::MouseMove:
        return "MouseMove";
    case QEvent::MouseButtonRelease:
        return "MouseButtonRelease";
    case QEvent::MouseButtonDblClick:
        return "MouseButtonDblClick";
    case QEvent::TabletPress:
        return "TabletPress";
    case QEvent::TabletMove:
        return "TabletMove";
    case QEvent::TabletRelease:
        return "TabletRelease";
    case QEvent::TouchBegin:
        return "TouchBegin";
    case QEvent::TouchUpdate:
        return "TouchUpdate";
    case QEvent::TouchEnd:
        return "TouchEnd";
    case QEvent::TouchCancel:
        return "TouchCancel";
    case QEvent::Wheel:
        return "Wheel";
    case QEvent::NativeGesture:
        return "NativeGesture";
    default:
        break;
    }
    return "Other";
}

const char *mapInputDrawModeName(MapEditorInteractiveDrawMode mode)
{
    switch (mode) {
    case MapEditorInteractiveDrawMode::None:
        return "none";
    case MapEditorInteractiveDrawMode::Point:
        return "point";
    case MapEditorInteractiveDrawMode::Line:
        return "line";
    case MapEditorInteractiveDrawMode::Area:
        return "area";
    case MapEditorInteractiveDrawMode::SmartArea:
        return "smart-area";
    case MapEditorInteractiveDrawMode::Freehand:
        return "freehand";
    }
    return "unknown";
}

const char *mapInputItemTypeName(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return "none";
    }
    if (dynamic_cast<const MapEditableGeometryVertexItem *>(item) != nullptr) {
        return "vertex";
    }
    if (dynamic_cast<const MapLinePointSizeHandleItem *>(item) != nullptr) {
        return "line-point-size-handle";
    }
    if (dynamic_cast<const MapEditorLineDecorationItem *>(item) != nullptr) {
        return "line-decoration";
    }
    if (dynamic_cast<const QGraphicsLineItem *>(item) != nullptr) {
        return "graphics-line";
    }
    if (dynamic_cast<const QGraphicsPathItem *>(item) != nullptr) {
        return "graphics-path";
    }
    return "graphics-item";
}

const char *mapInputSelectionSubtypeName(int subtype)
{
    switch (subtype) {
    case kMapSceneSelectionSubtypeGeneric:
        return "generic";
    case kMapSceneSelectionSubtypeLineDetail:
        return "line-detail";
    case kMapSceneSelectionSubtypeLineAnchor:
        return "line-anchor";
    case kMapSceneSelectionSubtypeLineControl:
        return "line-control";
    case kMapSceneSelectionSubtypeLineControlConnector:
        return "line-control-connector";
    case kMapSceneSelectionSubtypeAreaVertex:
        return "area-vertex";
    case kMapSceneSelectionSubtypePointOrientationHandle:
        return "point-orientation-handle";
    case kMapSceneSelectionSubtypeAreaFill:
        return "area-fill";
    default:
        break;
    }
    return "unknown";
}

bool isMapInputPointerEvent(QEvent::Type type)
{
    switch (type) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
    case QEvent::Wheel:
    case QEvent::NativeGesture:
        return true;
    default:
        break;
    }
    return false;
}

bool hasRecentHandledTabletEvent(const MapEditorViewportInputContext &context);

QString mapInputPointerSummary(const QEvent *event)
{
    if (event == nullptr) {
        return QStringLiteral("pointer=none");
    }
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick: {
        const auto *mouseEvent = static_cast<const QMouseEvent *>(event);
        return QStringLiteral("button=%1 buttons=%2 pos=%3,%4")
            .arg(static_cast<int>(mouseEvent->button()))
            .arg(static_cast<int>(mouseEvent->buttons()))
            .arg(mouseEvent->pos().x())
            .arg(mouseEvent->pos().y());
    }
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease: {
        const auto *tabletEvent = static_cast<const QTabletEvent *>(event);
        return QStringLiteral("button=%1 buttons=%2 pressure=%3 pos=%4,%5")
            .arg(static_cast<int>(tabletEvent->button()))
            .arg(static_cast<int>(tabletEvent->buttons()))
            .arg(tabletEvent->pressure(), 0, 'f', 3)
            .arg(qRound(tabletEvent->position().x()))
            .arg(qRound(tabletEvent->position().y()));
    }
    default:
        break;
    }
    return {};
}

class MapInputEventTrace
{
public:
    MapInputEventTrace(const MapEditorViewportInputContext &context,
                       const QEvent *event,
                       bool active,
                       bool forwardingTabletEventAsMouse)
        : context_(context)
        , event_(event)
        , forwardingTabletEventAsMouse_(forwardingTabletEventAsMouse)
        , enabled_(active && diagnosticMapInputLoggingEnabled() && event != nullptr && isMapInputPointerEvent(event->type()))
    {
        if (enabled_) {
            timer_.start();
        }
    }

    ~MapInputEventTrace()
    {
        if (!enabled_) {
            return;
        }

        const qint64 elapsedMs = timer_.elapsed();
        if (elapsedMs < kSlowMapInputEventMs && !forceLog_) {
            return;
        }

        const bool includeSceneItemCount = forceLog_
            || (event_->type() != QEvent::MouseMove && event_->type() != QEvent::TabletMove);
        const int sceneItemCount = includeSceneItemCount && context_.scene != nullptr ? context_.scene->items().size() : -1;
        const int sourceVertexCount = context_.interactiveDrawSourceVertices != nullptr
            ? context_.interactiveDrawSourceVertices->size()
            : -1;
        const int sceneVertexCount = context_.interactiveDrawSceneVertices != nullptr
            ? context_.interactiveDrawSceneVertices->size()
            : -1;
        const int draftVertexCount = context_.interactiveDrawLineVertices != nullptr
            ? context_.interactiveDrawLineVertices->size()
            : -1;
        const int snapGuideCount = context_.interactiveDrawHoverSnapGuideScenePoints != nullptr
            ? context_.interactiveDrawHoverSnapGuideScenePoints->size()
            : -1;
        const bool recentTablet = hasRecentHandledTabletEvent(context_);
        const QString pointerSummary = mapInputPointerSummary(event_);

        QString message = QStringLiteral(
            "map-input event=%1 action=%2 elapsed_ms=%3 accepted=%4 forwarded_tablet=%5 draw_mode=%6 scene_items=%7 "
            "source_vertices=%8 scene_vertices=%9 draft_vertices=%10 snap_guides=%11 primary_active=%12 pan_active=%13 "
            "stroke_active=%14 anchor_press=%15 anchor_drag=%16 control_drag=%17 recent_tablet=%18")
                              .arg(QString::fromLatin1(mapInputEventTypeName(event_->type())))
                              .arg(QString::fromLatin1(action_))
                              .arg(elapsedMs)
                              .arg(event_->isAccepted() ? 1 : 0)
                              .arg(forwardingTabletEventAsMouse_ ? 1 : 0)
                              .arg(QString::fromLatin1(mapInputDrawModeName(
                                  context_.drawMode ? context_.drawMode() : MapEditorInteractiveDrawMode::None)))
                              .arg(sceneItemCount)
                              .arg(sourceVertexCount)
                              .arg(sceneVertexCount)
                              .arg(draftVertexCount)
                              .arg(snapGuideCount)
                              .arg(context_.primaryPointerInteractionActive != nullptr
                                       && (*context_.primaryPointerInteractionActive) ? 1 : 0)
                              .arg(context_.mapPanActive != nullptr && (*context_.mapPanActive) ? 1 : 0)
                              .arg(context_.interactiveDrawStrokeActive != nullptr
                                       && (*context_.interactiveDrawStrokeActive) ? 1 : 0)
                              .arg(context_.interactiveDrawAnchorPressActive != nullptr
                                       && (*context_.interactiveDrawAnchorPressActive) ? 1 : 0)
                              .arg(context_.interactiveDrawAnchorDragActive != nullptr
                                       && (*context_.interactiveDrawAnchorDragActive) ? 1 : 0)
                              .arg(context_.interactiveDrawControlDragActive != nullptr
                                       && (*context_.interactiveDrawControlDragActive) ? 1 : 0)
                              .arg(recentTablet ? 1 : 0);
        if (!pointerSummary.isEmpty()) {
            message += QLatin1Char(' ');
            message += pointerSummary;
        }
        if (sampleCount_ >= 0) {
            message += QStringLiteral(" samples=%1").arg(sampleCount_);
        }
        if (!detail_.isEmpty()) {
            message += QLatin1Char(' ');
            message += detail_;
        }
        qInfo().noquote() << message;
    }

    void setAction(const char *action)
    {
        action_ = action;
    }

    void setSampleCount(int sampleCount)
    {
        sampleCount_ = sampleCount;
    }

    void setDetail(QString detail)
    {
        detail_ = std::move(detail);
    }

    void forceLog()
    {
        forceLog_ = true;
    }

private:
    const MapEditorViewportInputContext &context_;
    const QEvent *event_ = nullptr;
    const bool forwardingTabletEventAsMouse_ = false;
    const bool enabled_ = false;
    bool forceLog_ = false;
    int sampleCount_ = -1;
    const char *action_ = "dispatch";
    QString detail_;
    QElapsedTimer timer_;
};

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

bool isSpacePanPress(const MapEditorViewportInputContext &context, const QMouseEvent *event)
{
    return event != nullptr
        && event->button() == Qt::LeftButton
        && context.mapSpacePanKeyDown != nullptr
        && (*context.mapSpacePanKeyDown);
}

bool isControlPanPress(const QMouseEvent *event)
{
    return event != nullptr
        && event->button() == Qt::LeftButton
        && event->modifiers().testFlag(Qt::ControlModifier);
}

QString scrollBarInputDetail(const QScrollBar *scrollBar,
                             QStringView prefix,
                             std::optional<int> valueBefore = std::nullopt)
{
    if (scrollBar == nullptr) {
        return QStringLiteral("%1bar=null").arg(prefix);
    }

    QString detail;
    if (valueBefore.has_value()) {
        detail += QStringLiteral("%1bar_before=%2 ").arg(prefix).arg(*valueBefore);
    }
    detail += QStringLiteral("%1bar_value=%2 %1bar_min=%3 %1bar_max=%4 %1bar_page=%5")
                  .arg(prefix)
                  .arg(scrollBar->value())
                  .arg(scrollBar->minimum())
                  .arg(scrollBar->maximum())
                  .arg(scrollBar->pageStep());
    return detail;
}

QString viewportScrollBarInputDetail(const MapEditorViewportInputContext &context,
                                     std::optional<int> horizontalBefore = std::nullopt,
                                     std::optional<int> verticalBefore = std::nullopt)
{
    if (context.view == nullptr) {
        return QStringLiteral("view=null");
    }

    return QStringLiteral("%1 %2")
        .arg(scrollBarInputDetail(context.view->horizontalScrollBar(), QStringLiteral("h"), horizontalBefore),
             scrollBarInputDetail(context.view->verticalScrollBar(), QStringLiteral("v"), verticalBefore));
}

void beginMapPanDrag(MapEditorViewportInputContext &context,
                     QWidget *viewport,
                     const QPoint &position,
                     bool controlPan)
{
    if (context.view != nullptr) {
        context.view->setFocus(Qt::MouseFocusReason);
    }
    if (viewport != nullptr) {
        viewport->setFocus(Qt::MouseFocusReason);
        viewport->setCursor(Qt::OpenHandCursor);
    }
    (*context.mapPanActive) = true;
    if (context.mapPanMoved != nullptr) {
        (*context.mapPanMoved) = false;
    }
    if (context.mapPanStartPosition != nullptr) {
        (*context.mapPanStartPosition) = position;
    }
    if (context.mapPanLastPosition != nullptr) {
        (*context.mapPanLastPosition) = position;
    }
    if (context.mapControlPanActive != nullptr) {
        (*context.mapControlPanActive) = controlPan;
    }
}

bool hasRecentHandledTabletEvent(const MapEditorViewportInputContext &context)
{
    if (context.lastTabletInteractionUtc == nullptr || !context.lastTabletInteractionUtc->isValid()) {
        return false;
    }
    return context.lastTabletInteractionUtc->msecsTo(QDateTime::currentDateTimeUtc()) <= kHandledTabletMouseSuppressionMs;
}

MapLinePointSizeHandleItem *slopeOrientationHandleAtViewportPosition(MapEditorViewportInputContext &context,
                                                                     const QPoint &viewportPosition)
{
    if (context.scene == nullptr || context.view == nullptr) {
        return nullptr;
    }

    const QPointF scenePosition = context.view->mapToScene(viewportPosition);
    const QPointF sceneDx = context.view->mapToScene(viewportPosition + QPoint(10, 0));
    const QPointF sceneDy = context.view->mapToScene(viewportPosition + QPoint(0, 10));
    const qreal radiusX = std::max<qreal>(1.0, std::abs(sceneDx.x() - scenePosition.x()));
    const qreal radiusY = std::max<qreal>(1.0, std::abs(sceneDy.y() - scenePosition.y()));
    const QRectF probeRect(scenePosition.x() - radiusX,
                           scenePosition.y() - radiusY,
                           radiusX * 2.0,
                           radiusY * 2.0);
    const QList<QGraphicsItem *> hitItems =
        context.scene->items(probeRect, Qt::IntersectsItemShape, Qt::DescendingOrder, context.view->transform());
    for (QGraphicsItem *item : hitItems) {
        auto *handle = dynamic_cast<MapLinePointSizeHandleItem *>(item);
        if (handle == nullptr || !handle->isVisible()) {
            continue;
        }
        return handle;
    }

    return nullptr;
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
bool isDistanceRankedPathItem(const QGraphicsItem *item, int subtype);
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
        if (subtype == kMapSceneSelectionSubtypeLineControl
            || subtype == kMapSceneSelectionSubtypeLineControlConnector) {
            const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
            (*context.pendingClickSourceVertexIndex) = ownerVertexIndex >= 0
                ? ownerVertexIndex
                : vertexItem->vertexIndex();
            (*context.pendingClickGeometryKind) = QStringLiteral("line");
        } else {
            (*context.pendingClickSourceVertexIndex) = vertexItem->vertexIndex();
            (*context.pendingClickGeometryKind) = vertexItem->geometryKind();
        }
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
        const bool lineControlConnectorHandle = subtype == kMapSceneSelectionSubtypeLineControlConnector
            && dynamic_cast<QGraphicsLineItem *>(candidate) == nullptr
            && dynamic_cast<QGraphicsPathItem *>(candidate) == nullptr;
        const bool vertexLikeItem = dynamic_cast<MapEditableGeometryVertexItem *>(candidate) != nullptr
            || subtype == kMapSceneSelectionSubtypeLineAnchor
            || subtype == kMapSceneSelectionSubtypeLineControl
            || lineControlConnectorHandle
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

QRectF sceneProbeRectForViewportRadius(const QGraphicsView *view,
                                       const QPoint &viewportPosition,
                                       qreal radiusPixels)
{
    if (view == nullptr) {
        return {};
    }

    const int radius = static_cast<int>(std::ceil(radiusPixels));
    const QRect viewportRect(viewportPosition - QPoint(radius, radius),
                             QSize((radius * 2) + 1, (radius * 2) + 1));
    return view->mapToScene(viewportRect).boundingRect();
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
    const QRectF pathProbeRect =
        sceneProbeRectForViewportRadius(context.view, viewportPosition, kMaximumPathPrimaryHitDistancePixels + 1.0);
    const QList<QGraphicsItem *> nearbyPathCandidates =
        context.scene->items(pathProbeRect,
                             Qt::IntersectsItemBoundingRect,
                             Qt::DescendingOrder,
                             context.view->transform());
    QGraphicsItem *nearestDirectVertexItem =
        nearestDirectVertexLikeItemForViewportPosition(context,
                                                       viewportPosition,
                                                       requireSelected,
                                                       false);
    qreal nearestDistance = std::numeric_limits<qreal>::max();
    for (QGraphicsItem *candidate : nearbyPathCandidates) {
        if (candidate == nullptr) {
            continue;
        }
        if (!hitItems.contains(candidate)) {
            const int subtype = candidate->data(kMapSceneSelectionSubtypeRole).toInt();
            const bool pathSelectionCandidate = isDistanceRankedPathItem(candidate, subtype);
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
    }
    if (nearestDirectVertexItem != nullptr) {
        const QPoint candidateCenter = context.view->mapFromScene(viewportHitCenterScenePoint(nearestDirectVertexItem));
        const QPoint delta = viewportPosition - candidateCenter;
        nearestDistance = std::hypot(delta.x(), delta.y());
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
    const QRectF pathProbeRect =
        sceneProbeRectForViewportRadius(context.view, viewportPosition, kMaximumPathPrimaryHitDistancePixels + 1.0);
    const QList<QGraphicsItem *> nearbyPathCandidates =
        context.scene->items(pathProbeRect,
                             Qt::IntersectsItemBoundingRect,
                             Qt::DescendingOrder,
                             context.view->transform());
    for (QGraphicsItem *candidate : nearbyPathCandidates) {
        if (candidate == nullptr || !candidate->data(kMapSceneInteractionHoverRole).toBool()) {
            continue;
        }
        const int subtype = candidate->data(kMapSceneSelectionSubtypeRole).toInt();
        if (!isDistanceRankedPathItem(candidate, subtype)) {
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

    if (MapLinePointSizeHandleItem *orientationHandle =
            slopeOrientationHandleAtViewportPosition(context, viewportPosition)) {
        const QPointF scenePosition = context.view->mapToScene(viewportPosition);
        resetPendingClickSelection(context, scenePosition);
        describePendingClickSelection(context, orientationHandle);
        return orientationHandle;
    }

    if (QGraphicsItem *directVertexItem =
            nearestDirectVertexLikeItemForViewportPosition(context,
                                                           viewportPosition,
                                                           false,
                                                           true)) {
        const QPointF scenePosition = context.view->mapToScene(viewportPosition);
        resetPendingClickSelection(context, scenePosition);
        describePendingClickSelection(context, directVertexItem);
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

QString mapInputHitItemSummary(const MapEditorViewportInputContext &context,
                               const QGraphicsItem *item,
                               const QPoint &viewportPosition)
{
    if (context.view == nullptr || item == nullptr) {
        return QStringLiteral("hit_item=none");
    }

    const QPointF scenePosition = context.view->mapToScene(viewportPosition);
    const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
    const qreal distancePixels = isDistanceRankedPathItem(item, subtype)
        ? genericPathItemHitDistancePixels(item, scenePosition, context.view->transform())
        : std::numeric_limits<qreal>::quiet_NaN();
    const QPointF itemScenePosition = item->scenePos();

    return QStringLiteral("hit_item_type=%1 hit_subtype=%2 hit_line=%3 hit_distance_px=%4 hit_item_scene=%5,%6")
        .arg(QString::fromLatin1(mapInputItemTypeName(item)))
        .arg(QString::fromLatin1(mapInputSelectionSubtypeName(subtype)))
        .arg(item->data(kMapSceneLineNumberRole).toInt())
        .arg(std::isfinite(distancePixels) ? QString::number(distancePixels, 'f', 2) : QStringLiteral("n/a"))
        .arg(itemScenePosition.x(), 0, 'f', 2)
        .arg(itemScenePosition.y(), 0, 'f', 2);
}

QString mapInputRawSceneHitSummary(const MapEditorViewportInputContext &context, const QPoint &viewportPosition)
{
    if (context.view == nullptr || context.scene == nullptr) {
        return QStringLiteral("raw_hit=none");
    }

    const QPointF scenePosition = context.view->mapToScene(viewportPosition);
    const QList<QGraphicsItem *> rawHits = context.scene->items(scenePosition,
                                                                Qt::IntersectsItemShape,
                                                                Qt::DescendingOrder,
                                                                context.view->transform());
    for (const QGraphicsItem *rawHit : rawHits) {
        if (!isInteractiveMapSelectionItem(rawHit) || rawHit->data(kMapSceneLineNumberRole).toInt() <= 0) {
            continue;
        }
        const int subtype = rawHit->data(kMapSceneSelectionSubtypeRole).toInt();
        return QStringLiteral("raw_hit_type=%1 raw_hit_subtype=%2 raw_hit_line=%3")
            .arg(QString::fromLatin1(mapInputItemTypeName(rawHit)))
            .arg(QString::fromLatin1(mapInputSelectionSubtypeName(subtype)))
            .arg(rawHit->data(kMapSceneLineNumberRole).toInt());
    }
    return QStringLiteral("raw_hit=none");
}

bool hasRejectedDistanceRankedRawHit(const MapEditorViewportInputContext &context, const QPoint &viewportPosition)
{
    if (context.view == nullptr || context.scene == nullptr) {
        return false;
    }

    const QPointF scenePosition = context.view->mapToScene(viewportPosition);
    const QList<QGraphicsItem *> rawHits = context.scene->items(scenePosition,
                                                                Qt::IntersectsItemShape,
                                                                Qt::DescendingOrder,
                                                                context.view->transform());
    for (const QGraphicsItem *rawHit : rawHits) {
        if (!isInteractiveMapSelectionItem(rawHit) || rawHit->data(kMapSceneLineNumberRole).toInt() <= 0) {
            continue;
        }
        const int subtype = rawHit->data(kMapSceneSelectionSubtypeRole).toInt();
        if (!isDistanceRankedPathItem(rawHit, subtype)) {
            return false;
        }
        const qreal distancePixels =
            genericPathItemHitDistancePixels(rawHit, scenePosition, context.view->transform());
        return !std::isfinite(distancePixels)
            || distancePixels == std::numeric_limits<qreal>::max()
            || distancePixels > kMaximumPathPrimaryHitDistancePixels;
    }
    return false;
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

qreal interactiveDrawControlHitSceneRadius(const MapEditorViewportInputContext &context,
                                           const QPoint &viewportPoint)
{
    return sceneRadiusForViewportPixels(context.view,
                                        viewportPoint,
                                        kInteractiveDrawControlHitRadiusPixels);
}

qreal interactiveDrawDragThresholdSceneRadius(const MapEditorViewportInputContext &context,
                                              const QPoint &viewportPoint)
{
    return sceneRadiusForViewportPixels(context.view,
                                        viewportPoint,
                                        kInteractiveDrawDragThresholdPixels);
}

qreal interactiveDrawCloseHitSceneRadius(const MapEditorViewportInputContext &context,
                                         const QPoint &viewportPoint)
{
    return sceneRadiusForViewportPixels(context.view,
                                        viewportPoint,
                                        kInteractiveDrawCloseHitRadiusPixels);
}

QRectF sceneSearchRectAround(const QPointF &scenePoint, qreal sceneRadius)
{
    return QRectF(scenePoint.x() - sceneRadius,
                  scenePoint.y() - sceneRadius,
                  sceneRadius * 2.0,
                  sceneRadius * 2.0);
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
    const QList<QGraphicsItem *> items = scene->items(sceneSearchRectAround(scenePoint, sceneRadius),
                                                      Qt::IntersectsItemBoundingRect,
                                                      Qt::DescendingOrder);
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

    const qreal snapRadius = sceneRadiusForViewportPixels(context.view,
                                                          viewportPoint,
                                                          kMapEditorSnapTargetRadiusPixels);
    if (snapRadius <= 0.0) {
        return scenePoint;
    }

    const std::optional<QPointF> snappedPoint = nearestGeometryAnchorSnapPoint(context.scene, scenePoint, snapRadius);
    return snappedPoint.value_or(scenePoint);
}

struct SnapGuideLineCandidate
{
    int lineNumber = 0;
    QRectF sceneBounds;
};

SnapGuideLineCandidate nearestNeighborGeometryLineForSnapGuides(const MapEditorViewportInputContext &context,
                                                                const QPoint &viewportPoint,
                                                                const QPointF &scenePoint)
{
    if (context.scene == nullptr || context.view == nullptr) {
        return {};
    }

    const qreal guideRadius = sceneRadiusForViewportPixels(context.view,
                                                           viewportPoint,
                                                           kMapEditorSnapGuideRadiusPixels);
    if (guideRadius <= 0.0) {
        return {};
    }

    qreal bestDistanceSquared = std::numeric_limits<qreal>::max();
    SnapGuideLineCandidate bestCandidate;
    const QList<QGraphicsItem *> items = context.scene->items(sceneSearchRectAround(scenePoint, guideRadius),
                                                              Qt::IntersectsItemShape,
                                                              Qt::DescendingOrder);
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
        bestCandidate.lineNumber = candidateLineNumber;
        bestCandidate.sceneBounds = pathItem->sceneBoundingRect();
    }

    return bestCandidate;
}

QVector<QPointF> snapGuidePointsForNearbyGeometry(const MapEditorViewportInputContext &context,
                                                  const QPoint &viewportPoint,
                                                  const QPointF &scenePoint)
{
    QVector<QPointF> guidePoints;
    if (context.scene == nullptr) {
        return guidePoints;
    }

    const SnapGuideLineCandidate guideLine = nearestNeighborGeometryLineForSnapGuides(context, viewportPoint, scenePoint);
    if (guideLine.lineNumber <= 0 || guideLine.sceneBounds.isNull()) {
        return guidePoints;
    }

    const QList<QGraphicsItem *> items = context.scene->items(guideLine.sceneBounds.adjusted(-1.0, -1.0, 1.0, 1.0),
                                                              Qt::IntersectsItemBoundingRect,
                                                              Qt::DescendingOrder);
    for (QGraphicsItem *item : items) {
        if (item == nullptr) {
            continue;
        }
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item)) {
            if (vertexItem->lineNumber() != guideLine.lineNumber) {
                continue;
            }
            if (vertexItem->geometryKind() == QStringLiteral("line")
                || vertexItem->geometryKind() == QStringLiteral("area")) {
                guidePoints.append(vertexItem->pos());
            }
            continue;
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
    const qreal snapRadius = sceneRadiusForViewportPixels(context.view,
                                                          viewportAnchor,
                                                          kMapEditorSnapTargetRadiusPixels);
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
    const bool mapViewportEvent = watched == viewport || watched == context_.view;
    MapInputEventTrace inputTrace(context_, event, mapViewportEvent, forwardingTabletEventAsMouse_);
    const auto traceAction = [&inputTrace](const char *action) {
        inputTrace.setAction(action);
    };
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
        case QEvent::TabletRelease: {
            traceAction("tablet-forward");
            auto *tabletEvent = static_cast<QTabletEvent *>(event);
            QEvent::Type mouseEventType = QEvent::MouseMove;
            Qt::MouseButton mouseButton = Qt::NoButton;
            Qt::MouseButtons mouseButtons = tabletEvent->buttons();
            switch (event->type()) {
            case QEvent::TabletPress:
                mouseEventType = QEvent::MouseButtonPress;
                mouseButton = tabletEvent->button() == Qt::NoButton ? Qt::LeftButton : tabletEvent->button();
                if (mouseButtons == Qt::NoButton) {
                    mouseButtons = mouseButton;
                }
                break;
            case QEvent::TabletMove:
                mouseEventType = QEvent::MouseMove;
                if (mouseButtons == Qt::NoButton && tabletEvent->pressure() > 0.0) {
                    mouseButtons = Qt::LeftButton;
                }
                break;
            case QEvent::TabletRelease:
                mouseEventType = QEvent::MouseButtonRelease;
                mouseButton = tabletEvent->button() == Qt::NoButton ? Qt::LeftButton : tabletEvent->button();
                mouseButtons = Qt::NoButton;
                break;
            default:
                break;
            }

            QMouseEvent mouseEvent(mouseEventType,
                                   tabletEvent->position(),
                                   tabletEvent->globalPosition(),
                                   mouseButton,
                                   mouseButtons,
                                   tabletEvent->modifiers());
            forwardingTabletEventAsMouse_ = true;
            const std::optional<bool> handled = handleEvent(watched, &mouseEvent);
            forwardingTabletEventAsMouse_ = false;
            if (handled.has_value() && handled.value()) {
                if (context_.lastTabletInteractionUtc != nullptr) {
                    (*context_.lastTabletInteractionUtc) = QDateTime::currentDateTimeUtc();
                }
                event->accept();
                return true;
            }
            break;
        }
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
            if (context_.mapControlPanActive != nullptr) {
                (*context_.mapControlPanActive) = false;
            }
            if (context_.mapPanMoved != nullptr) {
                (*context_.mapPanMoved) = false;
            }
            applyDefaultMapViewportCursor(context_, viewport);
            showContextMenuAtViewportPosition(viewportPosition, contextMenuEvent->globalPos());
            event->accept();
            return true;
        }
        case QEvent::MouseButtonPress: {
            if (!forwardingTabletEventAsMouse_ && hasRecentHandledTabletEvent(context_)) {
                traceAction("suppress-synthetic-mouse-press");
                inputTrace.forceLog();
                event->accept();
                return true;
            }
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton
                || isSpacePanPress(context_, mouseEvent)
                || isControlPanPress(mouseEvent)) {
                traceAction("begin-pan");
                inputTrace.setDetail(QStringLiteral("space_down=%1 control_pan=%2 %3")
                                         .arg(context_.mapSpacePanKeyDown != nullptr
                                                  && (*context_.mapSpacePanKeyDown) ? 1 : 0)
                                         .arg(isControlPanPress(mouseEvent) ? 1 : 0)
                                         .arg(viewportScrollBarInputDetail(context_)));
                inputTrace.forceLog();
                beginMapPanDrag(context_, viewport, mouseEvent->pos(), isControlPanPress(mouseEvent));
                event->accept();
                return true;
            }
            if (isSecondaryClickPress(mouseEvent)) {
                traceAction("context-menu");
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
                    traceAction("freehand-begin");
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
                    freehandPreviewThrottleTimer_.restart();
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
                    traceAction("line-area-press");
                    const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                    const QPointF anchorScenePoint = snapInteractiveDrawAnchorIfAvailable(context_, mouseEvent->pos(), scenePoint);
                    const qreal controlHitRadius = interactiveDrawControlHitSceneRadius(context_, mouseEvent->pos());
                    if (const auto handle = context_.interactiveLineControlAt(scenePoint, controlHitRadius)) {
                        traceAction("line-area-control-drag-begin");
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
                    traceAction("point-insert");
                    const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                    const QPointF anchorScenePoint = snapInteractiveDrawAnchorIfAvailable(context_, mouseEvent->pos(), scenePoint);
                    if (context_.handleInteractiveDrawClick(anchorScenePoint)) {
                        (*context_.primaryPointerInteractionActive) = false;
                        event->accept();
                        return true;
                    }
                }
                if (context_.handleInteractiveDrawClick(context_.view->mapToScene(mouseEvent->pos()))) {
                    traceAction("generic-insert");
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
                                traceAction("select-path-hit");
                                inputTrace.setDetail(mapInputHitItemSummary(context_, item, mouseEvent->pos())
                                                     + QLatin1Char(' ')
                                                     + mapInputRawSceneHitSummary(context_, mouseEvent->pos()));
                                inputTrace.forceLog();
                                setMapInteractionHoverItem(context_, item);
                                selectSingleMapHitItem(context_, item);
                                event->accept();
                                return true;
                            }
                        }
                        traceAction("primary-press-fallthrough-hit");
                        inputTrace.setDetail(mapInputHitItemSummary(context_, item, mouseEvent->pos())
                                             + QLatin1Char(' ')
                                             + mapInputRawSceneHitSummary(context_, mouseEvent->pos()));
                        inputTrace.forceLog();
                        event->ignore();
                    } else {
                        traceAction("primary-press-fallthrough-empty");
                        inputTrace.setDetail(mapInputHitItemSummary(context_, nullptr, mouseEvent->pos())
                                             + QLatin1Char(' ')
                                             + mapInputRawSceneHitSummary(context_, mouseEvent->pos()));
                        inputTrace.forceLog();
                        if (hasRejectedDistanceRankedRawHit(context_, mouseEvent->pos())) {
                            traceAction("primary-press-reject-raw-hit");
                            (*context_.primaryPointerInteractionActive) = false;
                            event->accept();
                            return true;
                        }
                        event->ignore();
                    }
                }
            }
            break;
        }
        case QEvent::MouseMove: {
            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawControlDragActive)) {
                traceAction("line-area-control-drag-move");
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
                traceAction("line-area-anchor-drag-preview");
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                const qreal dragThreshold = interactiveDrawDragThresholdSceneRadius(context_, mouseEvent->pos());
                if (!(*context_.interactiveDrawAnchorDragActive)
                    && dragThreshold > 0.0
                    && QLineF((*context_.interactiveDrawAnchorPressScenePoint), scenePoint).length() >= dragThreshold) {
                    (*context_.interactiveDrawAnchorDragActive) = true;
                }
                (*context_.interactiveDrawAnchorDragScenePoint) = scenePoint;
                context_.updateInteractiveDrawPreview();
                event->accept();
                return true;
            }

            if (drawMode() == MapEditorInteractiveDrawMode::Freehand && (*context_.interactiveDrawStrokeActive)) {
                traceAction("freehand-sample");
                const QPointF scenePoint = context_.view->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal minimumSceneSampleDistance = 0.5;
                if ((*context_.interactiveDrawSceneVertices).isEmpty()
                    || QLineF((*context_.interactiveDrawSceneVertices).last(), scenePoint).length() >= minimumSceneSampleDistance) {
                    (*context_.interactiveDrawSceneVertices).append(scenePoint);
                    (*context_.interactiveDrawSourceVertices).append(context_.sourcePointFromScenePosition(scenePoint));
                    const int sampleCount = context_.interactiveDrawSceneVertices->size();
                    inputTrace.setSampleCount(sampleCount);
                    const bool refreshPreview = sampleCount <= 2
                        || !freehandPreviewThrottleTimer_.isValid()
                        || freehandPreviewThrottleTimer_.elapsed() >= kFreehandPreviewRefreshIntervalMs;
                    if (refreshPreview) {
                        freehandPreviewThrottleTimer_.restart();
                        context_.updateInteractiveDrawPreview();
                        context_.updateCommandSurfaceState();
                    }
                }
                event->accept();
                return true;
            }

            if (drawMode() == MapEditorInteractiveDrawMode::Point
                || drawMode() == MapEditorInteractiveDrawMode::Line
                || drawMode() == MapEditorInteractiveDrawMode::Area) {
                traceAction("draw-hover-preview");
                setMapInteractionHoverItem(context_, nullptr);
                const QPoint mousePosition = static_cast<QMouseEvent *>(event)->pos();
                const QPointF scenePoint = context_.view->mapToScene(mousePosition);
                const QPointF hoverScenePoint = snapInteractiveDrawAnchorIfAvailable(context_, mousePosition, scenePoint);
                const bool snapActive = isLineAnchorSnapActive(context_, scenePoint, hoverScenePoint);
                if (drawMode() == MapEditorInteractiveDrawMode::Point) {
                    applyDefaultMapViewportCursor(context_, viewport);
                } else if (!(*context_.interactiveDrawLineVertices).isEmpty()) {
                    const qreal controlHitRadius = interactiveDrawControlHitSceneRadius(context_, mousePosition);
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
                traceAction("select-hover-hit-test");
                QGraphicsItem *hoverItem = preferredMapHitItemForViewportPosition(context_,
                                                                                  mouseEvent->pos(),
                                                                                  false,
                                                                                  false);
                setMapInteractionHoverItem(context_, hoverItem);
                if (slopeOrientationHandleAtViewportPosition(context_, mouseEvent->pos()) != nullptr) {
                    viewport->setCursor(Qt::OpenHandCursor);
                } else {
                    applyDefaultMapViewportCursor(context_, viewport);
                }
            }

            if (!(*context_.mapPanActive)) {
                break;
            }

            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            traceAction("pan-move");
            if (context_.mapPanMoved != nullptr
                && context_.mapPanStartPosition != nullptr
                && !(*context_.mapPanMoved)) {
                constexpr int dragThreshold = 4;
                const QPoint dragDelta = mouseEvent->pos() - (*context_.mapPanStartPosition);
                if (std::abs(dragDelta.x()) < dragThreshold && std::abs(dragDelta.y()) < dragThreshold) {
                    inputTrace.setDetail(QStringLiteral("threshold_passed=0 drag_delta=%1,%2 %3")
                                             .arg(dragDelta.x())
                                             .arg(dragDelta.y())
                                             .arg(viewportScrollBarInputDetail(context_)));
                    inputTrace.forceLog();
                    event->accept();
                    return true;
                }
                (*context_.mapPanMoved) = true;
                viewport->setCursor(Qt::ClosedHandCursor);
            }
            const QPoint delta = mouseEvent->pos() - (*context_.mapPanLastPosition);
            (*context_.mapPanLastPosition) = mouseEvent->pos();
            const int horizontalBefore = context_.view->horizontalScrollBar() != nullptr
                ? context_.view->horizontalScrollBar()->value()
                : 0;
            const int verticalBefore = context_.view->verticalScrollBar() != nullptr
                ? context_.view->verticalScrollBar()->value()
                : 0;
            QElapsedTimer panStageTimer;
            const bool logPanStageTiming = diagnosticMapInputLoggingEnabled();
            if (logPanStageTiming) {
                panStageTimer.start();
            }
            if (context_.view->horizontalScrollBar() != nullptr) {
                context_.view->horizontalScrollBar()->setValue(context_.view->horizontalScrollBar()->value() - delta.x());
            }
            if (context_.view->verticalScrollBar() != nullptr) {
                context_.view->verticalScrollBar()->setValue(context_.view->verticalScrollBar()->value() - delta.y());
            }
            const qint64 scrollMs = logPanStageTiming ? panStageTimer.restart() : 0;

            const bool autoFitWasEnabled = context_.autoFitEnabled != nullptr && (*context_.autoFitEnabled);
            (*context_.autoFitEnabled) = false;
            if (autoFitWasEnabled) {
                context_.syncZoomFactorFromView();
                context_.updateCommandSurfaceState();
            }
            const qint64 stateMs = logPanStageTiming ? panStageTimer.elapsed() : 0;
            inputTrace.setDetail(QStringLiteral("threshold_passed=1 delta=%1,%2 auto_fit_update=%3 scroll_ms=%4 state_ms=%5 %6")
                                     .arg(delta.x())
                                     .arg(delta.y())
                                     .arg(autoFitWasEnabled ? 1 : 0)
                                     .arg(scrollMs)
                                     .arg(stateMs)
                                     .arg(viewportScrollBarInputDetail(context_, horizontalBefore, verticalBefore)));
            inputTrace.forceLog();
            event->accept();
            return true;
        }
        case QEvent::MouseButtonDblClick: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (drawMode() == MapEditorInteractiveDrawMode::Line
                && mouseEvent->button() == Qt::LeftButton
                && context_.cancelInteractiveDrawingToSelectMode != nullptr) {
                traceAction("line-double-click-complete");
                const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                const QPointF anchorScenePoint = snapInteractiveDrawAnchorIfAvailable(context_, mouseEvent->pos(), scenePoint);
                bool anchorAlreadyCaptured = false;
                if (context_.interactiveDrawLineVertices != nullptr
                    && !context_.interactiveDrawLineVertices->isEmpty()) {
                    const qreal hitRadius = interactiveDrawCloseHitSceneRadius(context_, mouseEvent->pos());
                    anchorAlreadyCaptured =
                        hitRadius > 0.0
                        && QLineF(context_.interactiveDrawLineVertices->last().anchorScene, anchorScenePoint).length() <= hitRadius;
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
                traceAction("line-area-control-drag-release");
                const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                const MapEditorInteractiveLineControlHandleRef dragHandle = (*context_.interactiveDrawControlDragHandle);
                if (dragHandle.kind == MapEditorInteractiveLineControlHandleRef::Kind::Anchor
                    && context_.interactiveDrawLineVertices != nullptr
                    && dragHandle.vertexIndex == 0
                    && context_.interactiveDrawLineVertices->size() >= (drawMode() == MapEditorInteractiveDrawMode::Line ? 2 : 3)) {
                    const qreal closeHitRadius = interactiveDrawCloseHitSceneRadius(context_, mouseEvent->pos());
                    const QPointF firstAnchorScenePoint = context_.interactiveDrawLineVertices->first().anchorScene;
                    if (closeHitRadius > 0.0
                        && QLineF(scenePoint, firstAnchorScenePoint).length() <= closeHitRadius
                        && context_.commitInteractiveDrawSession != nullptr) {
                        (*context_.interactiveDrawControlDragActive) = false;
                        traceAction("line-area-control-drag-close");
                        context_.commitInteractiveDrawSession(drawMode() == MapEditorInteractiveDrawMode::Line);
                        event->accept();
                        return true;
                    }
                }
                context_.setInteractiveLineControlScenePoint((*context_.interactiveDrawControlDragHandle), scenePoint);
                (*context_.interactiveDrawControlDragActive) = false;
                const qreal controlHitRadius = interactiveDrawControlHitSceneRadius(context_, mouseEvent->pos());
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
                traceAction("line-area-anchor-release");
                const MapEditorInteractiveDrawMode currentDrawMode = drawMode();
                const QPointF anchorScenePoint = (*context_.interactiveDrawAnchorPressScenePoint);
                const QPointF releaseScenePoint = context_.view->mapToScene(mouseEvent->pos());
                std::optional<QPointF> dragScenePoint;
                if ((*context_.interactiveDrawAnchorDragActive)) {
                    const qreal dragThreshold = interactiveDrawDragThresholdSceneRadius(context_, mouseEvent->pos());
                    if (dragThreshold > 0.0
                        && QLineF((*context_.interactiveDrawAnchorPressScenePoint), releaseScenePoint).length() >= dragThreshold) {
                        dragScenePoint = releaseScenePoint;
                    }
                }

                bool completeClosedDraftByClick = false;
                if ((currentDrawMode == MapEditorInteractiveDrawMode::Line
                     || currentDrawMode == MapEditorInteractiveDrawMode::Area)
                    && !(*context_.interactiveDrawAnchorDragActive)
                    && context_.interactiveDrawLineVertices != nullptr
                    && context_.interactiveDrawLineVertices->size() >= (currentDrawMode == MapEditorInteractiveDrawMode::Line ? 2 : 3)) {
                    const qreal closeHitRadius = interactiveDrawCloseHitSceneRadius(context_, mouseEvent->pos());
                    const QPointF firstAnchorScenePoint = context_.interactiveDrawLineVertices->first().anchorScene;
                    completeClosedDraftByClick = closeHitRadius > 0.0
                        && QLineF(releaseScenePoint, firstAnchorScenePoint).length() <= closeHitRadius;
                }

                (*context_.interactiveDrawAnchorPressActive) = false;
                (*context_.interactiveDrawAnchorDragActive) = false;
                (*context_.interactiveDrawHoverActive) = false;
                if (context_.interactiveDrawHoverSnapActive != nullptr) {
                    (*context_.interactiveDrawHoverSnapActive) = false;
                }

                if (completeClosedDraftByClick && context_.commitInteractiveDrawSession != nullptr) {
                    traceAction("line-area-anchor-release-close");
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
                traceAction("freehand-release");
                const int sceneItemCountBeforeRelease = context_.scene != nullptr ? context_.scene->items().size() : -1;
                const QPointF releasePoint = context_.view->mapToScene(mouseEvent->pos());
                if ((*context_.interactiveDrawSceneVertices).isEmpty()
                    || QLineF((*context_.interactiveDrawSceneVertices).last(), releasePoint).length() >= 1.0) {
                    (*context_.interactiveDrawSceneVertices).append(releasePoint);
                    (*context_.interactiveDrawSourceVertices).append(context_.sourcePointFromScenePosition(releasePoint));
                }
                (*context_.interactiveDrawStrokeActive) = false;
                const int verticesBeforeCommit = context_.interactiveDrawSourceVertices->size();

                if ((*context_.interactiveDrawSourceVertices).size() < 2) {
                    inputTrace.setDetail(QStringLiteral("vertices_before_commit=%1 scene_items_before_release=%2")
                                             .arg(verticesBeforeCommit)
                                             .arg(sceneItemCountBeforeRelease));
                    inputTrace.forceLog();
                    context_.clearInteractiveDrawSession(false);
                    (*context_.toolbarStatusNote) = tr("Freehand mode needs a drag stroke to create a line.");
                    context_.refreshToolbarSummary();
                    context_.updateCommandSurfaceState();
                    event->accept();
                    return true;
                }

                QElapsedTimer releaseStageTimer;
                releaseStageTimer.start();
                const bool committed = context_.commitInteractiveDrawVertices(QStringLiteral("line"),
                                                                     (*context_.interactiveDrawSourceVertices),
                                                                     tr("freehand line"));
                const qint64 commitMs = releaseStageTimer.elapsed();
                releaseStageTimer.restart();
                inputTrace.setSampleCount(context_.interactiveDrawSourceVertices->size());
                context_.clearInteractiveDrawSession(false);
                const qint64 clearMs = releaseStageTimer.elapsed();
                releaseStageTimer.restart();
                if (committed) {
                    context_.updateHelpPanel();
                }
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                const qint64 postCommitUiMs = releaseStageTimer.elapsed();
                inputTrace.setDetail(QStringLiteral(
                                         "vertices_before_commit=%1 committed=%2 commit_ms=%3 clear_ms=%4 post_commit_ui_ms=%5 "
                                         "scene_items_before_release=%6")
                                         .arg(verticesBeforeCommit)
                                         .arg(committed ? 1 : 0)
                                         .arg(commitMs)
                                         .arg(clearMs)
                                         .arg(postCommitUiMs)
                                         .arg(sceneItemCountBeforeRelease));
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            if (mouseEvent->button() == Qt::LeftButton && mouseEvent->buttons() == Qt::NoButton) {
                (*context_.primaryPointerInteractionActive) = false;
            }

            if ((*context_.mapPanActive)
                && (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::LeftButton)) {
                traceAction("pan-release");
                const bool controlPan = context_.mapControlPanActive != nullptr
                    && (*context_.mapControlPanActive);
                const bool moved = context_.mapPanMoved != nullptr && (*context_.mapPanMoved);
                inputTrace.setDetail(QStringLiteral("moved=%1 control_pan=%2 %3")
                                         .arg(moved ? 1 : 0)
                                         .arg(controlPan ? 1 : 0)
                                         .arg(viewportScrollBarInputDetail(context_)));
                inputTrace.forceLog();
                (*context_.mapPanActive) = false;
                if (context_.mapControlPanActive != nullptr) {
                    (*context_.mapControlPanActive) = false;
                }
                applyDefaultMapViewportCursor(context_, viewport);
                if (mouseEvent->button() == Qt::LeftButton && controlPan && !moved) {
                    showContextMenuAtViewportPosition(mouseEvent->pos(), mouseEvent->globalPosition().toPoint());
                }
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
                traceAction("wheel-suppress-native-zoom");
                inputTrace.setDetail(QStringLiteral("native_zoom_active=%1 recent_native_zoom=%2")
                                         .arg((*context_.nativeZoomGestureActive) ? 1 : 0)
                                         .arg(recentNativeZoom ? 1 : 0));
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            if ((*context_.primaryPointerInteractionActive)) {
                traceAction("wheel-suppress-primary-pointer");
                inputTrace.forceLog();
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
                traceAction("wheel-zoom");
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

                inputTrace.setDetail(QStringLiteral(
                                         "resolved_action=Zoom precise_scroll=%1 modifier_zoom=%2 touch_friendly=%3 "
                                         "pixel_delta=%4,%5 angle_delta=%6,%7 zoom_delta=%8")
                                         .arg(preciseScroll ? 1 : 0)
                                         .arg(cmdModifier ? 1 : 0)
                                         .arg((*context_.touchFriendlyControlsEnabled) ? 1 : 0)
                                         .arg(pixelDelta.x())
                                         .arg(pixelDelta.y())
                                         .arg(angleDelta.x())
                                         .arg(angleDelta.y())
                                         .arg(delta, 0, 'f', 3));
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            traceAction("wheel-pan");
            QPoint panDelta = wheelEvent->pixelDelta();
            if (panDelta.isNull()) {
                const QPoint angleDelta = wheelEvent->angleDelta();
                panDelta = QPoint(qRound(angleDelta.x() / 4.0), qRound(angleDelta.y() / 4.0));
            }

            const int horizontalBefore = context_.view->horizontalScrollBar() != nullptr
                ? context_.view->horizontalScrollBar()->value()
                : 0;
            const int verticalBefore = context_.view->verticalScrollBar() != nullptr
                ? context_.view->verticalScrollBar()->value()
                : 0;
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

            inputTrace.setDetail(QStringLiteral(
                                     "resolved_action=Pan precise_scroll=%1 modifier_zoom=%2 touch_friendly=%3 "
                                     "pixel_delta=%4,%5 angle_delta=%6,%7 pan_delta=%8,%9 %10")
                                     .arg(preciseScroll ? 1 : 0)
                                     .arg(cmdModifier ? 1 : 0)
                                     .arg((*context_.touchFriendlyControlsEnabled) ? 1 : 0)
                                     .arg(wheelEvent->pixelDelta().x())
                                     .arg(wheelEvent->pixelDelta().y())
                                     .arg(wheelEvent->angleDelta().x())
                                     .arg(wheelEvent->angleDelta().y())
                                     .arg(panDelta.x())
                                     .arg(panDelta.y())
                                     .arg(viewportScrollBarInputDetail(context_, horizontalBefore, verticalBefore)));
            inputTrace.forceLog();
            event->accept();
            return true;
        }
        case QEvent::NativeGesture: {
            auto *gestureEvent = static_cast<QNativeGestureEvent *>(event);
            if ((*context_.primaryPointerInteractionActive)) {
                traceAction("native-gesture-suppress-primary-pointer");
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::BeginNativeGesture) {
                traceAction("native-gesture-begin");
                (*context_.nativeZoomGestureActive) = true;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                inputTrace.setDetail(QStringLiteral("gesture_type=%1 value=%2")
                                         .arg(static_cast<int>(gestureEvent->gestureType()))
                                         .arg(gestureEvent->value(), 0, 'f', 3));
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::EndNativeGesture) {
                traceAction("native-gesture-end");
                (*context_.nativeZoomGestureActive) = false;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                inputTrace.setDetail(QStringLiteral("gesture_type=%1 value=%2")
                                         .arg(static_cast<int>(gestureEvent->gestureType()))
                                         .arg(gestureEvent->value(), 0, 'f', 3));
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
                traceAction("native-gesture-zoom");
                (*context_.nativeZoomGestureActive) = true;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                const qreal rawValue = gestureEvent->value();
                if (!std::isfinite(rawValue)) {
                    inputTrace.setDetail(QStringLiteral("gesture_type=%1 value=nonfinite")
                                             .arg(static_cast<int>(gestureEvent->gestureType())));
                    inputTrace.forceLog();
                    event->accept();
                    return true;
                }

                // Clamp one pinch update so trackpad spikes cannot jump straight to extreme scales.
                const qreal clampedDelta = qBound(-0.35, rawValue, 0.35);
                const qreal factor = std::exp(clampedDelta);
                if (factor > 0.0) {
                    context_.applyZoomAtViewportPosition(factor, gestureEvent->position());
                }
                inputTrace.setDetail(QStringLiteral("gesture_type=%1 raw_value=%2 clamped_delta=%3 factor=%4")
                                         .arg(static_cast<int>(gestureEvent->gestureType()))
                                         .arg(rawValue, 0, 'f', 3)
                                         .arg(clampedDelta, 0, 'f', 3)
                                         .arg(factor, 0, 'f', 3));
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            // On some platforms/devices, pinch can interleave non-zoom native gestures.
            // While a zoom sequence is active, suppress them so pinch never pans the viewport.
            if ((*context_.nativeZoomGestureActive)
                && (gestureEvent->gestureType() == Qt::PanNativeGesture
                    || gestureEvent->gestureType() == Qt::RotateNativeGesture)) {
                traceAction("native-gesture-suppress-during-zoom");
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                inputTrace.setDetail(QStringLiteral("gesture_type=%1 value=%2")
                                         .arg(static_cast<int>(gestureEvent->gestureType()))
                                         .arg(gestureEvent->value(), 0, 'f', 3));
                inputTrace.forceLog();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::TouchBegin: {
            if (!shouldEnableTouchPanCandidate((*context_.touchFriendlyControlsEnabled),
                                               (*context_.selectModeActive),
                                               (*context_.primaryPointerInteractionActive))) {
                traceAction("touch-begin-suppress-pan-candidate");
                inputTrace.setDetail(QStringLiteral("touch_friendly=%1 select_mode=%2 primary_active=%3")
                                         .arg((*context_.touchFriendlyControlsEnabled) ? 1 : 0)
                                         .arg((*context_.selectModeActive) ? 1 : 0)
                                         .arg((*context_.primaryPointerInteractionActive) ? 1 : 0));
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() == 2) {
                traceAction("touch-begin-pan-candidate");
                const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
                (*context_.touchPanCandidate) = true;
                (*context_.touchPanActive) = false;
                (*context_.touchPanStartPosition) = centroid;
                (*context_.touchPanLastPosition) = centroid;
                inputTrace.setDetail(QStringLiteral("points=2 centroid=%1,%2 %3")
                                         .arg(centroid.x(), 0, 'f', 1)
                                         .arg(centroid.y(), 0, 'f', 1)
                                         .arg(viewportScrollBarInputDetail(context_)));
                inputTrace.forceLog();
            } else {
                traceAction("touch-begin-ignore");
                inputTrace.setDetail(QStringLiteral("points=%1").arg(touchEvent->points().size()));
                inputTrace.forceLog();
            }
            break;
        }
        case QEvent::TouchUpdate: {
            if (!(*context_.touchPanCandidate) || (*context_.primaryPointerInteractionActive)) {
                traceAction("touch-update-suppress-pan");
                inputTrace.setDetail(QStringLiteral("candidate=%1 primary_active=%2")
                                         .arg((*context_.touchPanCandidate) ? 1 : 0)
                                         .arg((*context_.primaryPointerInteractionActive) ? 1 : 0));
                inputTrace.forceLog();
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() != 2) {
                traceAction("touch-update-cancel-pan-candidate");
                inputTrace.setDetail(QStringLiteral("points=%1").arg(touchEvent->points().size()));
                inputTrace.forceLog();
                (*context_.touchPanCandidate) = false;
                (*context_.touchPanActive) = false;
                break;
            }

            const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
            if (!(*context_.touchPanActive)) {
                const qreal threshold = 8.0;
                if (QLineF((*context_.touchPanStartPosition), centroid).length() < threshold) {
                    traceAction("touch-pan-threshold-wait");
                    inputTrace.setDetail(QStringLiteral("centroid=%1,%2 start=%3,%4 threshold=%5")
                                             .arg(centroid.x(), 0, 'f', 1)
                                             .arg(centroid.y(), 0, 'f', 1)
                                             .arg(context_.touchPanStartPosition->x(), 0, 'f', 1)
                                             .arg(context_.touchPanStartPosition->y(), 0, 'f', 1)
                                             .arg(threshold, 0, 'f', 1));
                    inputTrace.forceLog();
                    event->accept();
                    return true;
                }
                (*context_.touchPanActive) = true;
            }

            traceAction("touch-pan-move");
            const QPointF delta = centroid - (*context_.touchPanLastPosition);
            (*context_.touchPanLastPosition) = centroid;
            const int horizontalBefore = context_.view->horizontalScrollBar() != nullptr
                ? context_.view->horizontalScrollBar()->value()
                : 0;
            const int verticalBefore = context_.view->verticalScrollBar() != nullptr
                ? context_.view->verticalScrollBar()->value()
                : 0;
            if (context_.view->horizontalScrollBar() != nullptr) {
                context_.view->horizontalScrollBar()->setValue(context_.view->horizontalScrollBar()->value() - qRound(delta.x()));
            }
            if (context_.view->verticalScrollBar() != nullptr) {
                context_.view->verticalScrollBar()->setValue(context_.view->verticalScrollBar()->value() - qRound(delta.y()));
            }

            (*context_.autoFitEnabled) = false;
            context_.syncZoomFactorFromView();
            context_.updateCommandSurfaceState();
            inputTrace.setDetail(QStringLiteral("centroid=%1,%2 delta=%3,%4 %5")
                                     .arg(centroid.x(), 0, 'f', 1)
                                     .arg(centroid.y(), 0, 'f', 1)
                                     .arg(delta.x(), 0, 'f', 1)
                                     .arg(delta.y(), 0, 'f', 1)
                                     .arg(viewportScrollBarInputDetail(context_, horizontalBefore, verticalBefore)));
            inputTrace.forceLog();
            event->accept();
            return true;
        }
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            traceAction(event->type() == QEvent::TouchEnd ? "touch-end" : "touch-cancel");
            inputTrace.setDetail(QStringLiteral("candidate=%1 active=%2 %3")
                                     .arg((*context_.touchPanCandidate) ? 1 : 0)
                                     .arg((*context_.touchPanActive) ? 1 : 0)
                                     .arg(viewportScrollBarInputDetail(context_)));
            inputTrace.forceLog();
            (*context_.touchPanCandidate) = false;
            (*context_.touchPanActive) = false;
            break;
        case QEvent::Leave:
            if ((*context_.mapPanActive)) {
                (*context_.mapPanActive) = false;
                if (context_.mapControlPanActive != nullptr) {
                    (*context_.mapControlPanActive) = false;
                }
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
