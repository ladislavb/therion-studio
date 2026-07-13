#include "MapEditorSceneLifecycleController.h"

#include "MapEditorSceneSupport.h"

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScopedValueRollback>
#include <QSet>
#include <QTransform>

#include <cmath>
#include <utility>

namespace TherionStudio
{
MapEditorSceneLifecycleController::MapEditorSceneLifecycleController(MapEditorSceneLifecycleContext context)
    : context_(std::move(context))
{
}

QGraphicsScene *MapEditorSceneLifecycleController::scene() const
{
    return context_.scene != nullptr ? *context_.scene : nullptr;
}

void MapEditorSceneLifecycleController::clearMapScene()
{
    context_.itemsByLine->clear();
    if (context_.vertexItemsByKey != nullptr) {
        context_.vertexItemsByKey->clear();
    }
    *context_.interactiveDrawStrokeActive = false;
    *context_.interactiveDrawPreviewPath = nullptr;
    context_.interactiveDrawPreviewMarkers->clear();
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr) {
        return;
    }
    QScopedValueRollback<bool> selectionGuard(*context_.updatingSelection, true);

    QVector<QGraphicsRectItem *> preservedDrafts;
    QVector<QGraphicsPixmapItem *> preservedBackgrounds;
    QSet<QGraphicsPixmapItem *> preservedBackgroundSet;
    preservedBackgrounds.reserve(context_.backgroundImageItems->size());

    for (QGraphicsPixmapItem *backgroundItem : std::as_const(*context_.backgroundImageItems)) {
        if (backgroundItem == nullptr || backgroundItem->scene() != mapScene) {
            continue;
        }

        mapScene->removeItem(backgroundItem);
        preservedBackgrounds.append(backgroundItem);
        preservedBackgroundSet.insert(backgroundItem);
    }

    const QList<QGraphicsItem *> items = mapScene->items();
    for (QGraphicsItem *item : items) {
        if (auto *backgroundItem = dynamic_cast<QGraphicsPixmapItem *>(item)) {
            if (preservedBackgroundSet.contains(backgroundItem)) {
                continue;
            }
            mapScene->removeItem(backgroundItem);
            preservedBackgrounds.append(backgroundItem);
            continue;
        }
        if (auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item)) {
            mapScene->removeItem(draftItem);
            preservedDrafts.append(draftItem);
            continue;
        }

        mapScene->removeItem(item);
        delete item;
    }

    *context_.draftGeometryItems = preservedDrafts;
    *context_.backgroundImageItems = preservedBackgrounds;
}

void MapEditorSceneLifecycleController::clearDraftGeometryItems()
{
    QGraphicsScene *mapScene = scene();
    if (mapScene != nullptr) {
        for (QGraphicsRectItem *item : std::as_const(*context_.draftGeometryItems)) {
            if (item != nullptr) {
                mapScene->removeItem(item);
                delete item;
            }
        }
    }

    context_.draftGeometryItems->clear();
    *context_.nextDraftGeometryId = 1;
}

void MapEditorSceneLifecycleController::clearBackgroundImageItems()
{
    QGraphicsScene *mapScene = scene();
    if (mapScene != nullptr) {
        for (QGraphicsPixmapItem *item : std::as_const(*context_.backgroundImageItems)) {
            if (item != nullptr) {
                mapScene->removeItem(item);
                delete item;
            }
        }
    }

    context_.backgroundImageItems->clear();
    *context_.selectedBackgroundLayerIndex = -1;
    updateSceneRectForBackgroundBounds();
    context_.refreshBackgroundLayerControls();
}

void MapEditorSceneLifecycleController::restoreDraftGeometryItems()
{
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr) {
        return;
    }

    for (QGraphicsRectItem *item : std::as_const(*context_.draftGeometryItems)) {
        if (item == nullptr || mapScene->items().contains(item)) {
            continue;
        }

        mapScene->addItem(item);
        item->setZValue(20.0);
    }
}

void MapEditorSceneLifecycleController::restoreBackgroundImageItems()
{
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr) {
        return;
    }

    for (QGraphicsPixmapItem *item : std::as_const(*context_.backgroundImageItems)) {
        if (item == nullptr || mapScene->items().contains(item)) {
            continue;
        }

        mapScene->addItem(item);
    }

    context_.applyBackgroundLayerStackingOrder();
    context_.setSelectedBackgroundLayerIndexInternal(*context_.selectedBackgroundLayerIndex);
    updateSceneRectForBackgroundBounds();
    context_.refreshBackgroundLayerControls();
}

void MapEditorSceneLifecycleController::updateSceneRectForBackgroundBounds()
{
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr) {
        return;
    }

    const QRectF backgroundBounds = context_.mapBackgroundFitBounds
        ? context_.mapBackgroundFitBounds()
        : QRectF();
    mapScene->setSceneRect(mapEditorScrollableSceneRect(backgroundBounds));
}

void MapEditorSceneLifecycleController::fitMapToView(bool includeBackgroundImages, bool updateCommandSurface)
{
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr || context_.view == nullptr) {
        return;
    }

    QRectF fitBounds = mapGeometryFitBounds();
    const QRectF backgroundBounds = context_.mapBackgroundFitBounds();
    if (backgroundBounds.isValid() && includeBackgroundImages) {
        fitBounds = fitBounds.isValid() ? fitBounds.united(backgroundBounds) : backgroundBounds;
    }
    if (!fitBounds.isValid()) {
        fitBounds = mapEditorCanvasSceneFrame();
    }

    context_.view->resetTransform();
    context_.view->fitInView(fitBounds.adjusted(-24, -24, 24, 24), Qt::KeepAspectRatio);
    *context_.autoFitEnabled = true;
    syncZoomFactorFromView();
    if (updateCommandSurface) {
        context_.updateCommandSurfaceState();
    }
}

void MapEditorSceneLifecycleController::syncZoomFactorFromView()
{
    if (context_.view == nullptr) {
        *context_.zoomFactor = 1.0;
        context_.emitZoomStatusChanged(context_.zoomPercent());
        return;
    }

    const QTransform viewTransform = context_.view->transform();
    const qreal scaleX = std::hypot(viewTransform.m11(), viewTransform.m21());
    *context_.zoomFactor = scaleX > 0.0 ? scaleX : 1.0;
    context_.emitZoomStatusChanged(context_.zoomPercent());
}

void MapEditorSceneLifecycleController::applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition)
{
    if (context_.view == nullptr || factor <= 0.0) {
        return;
    }

    syncZoomFactorFromView();
    const qreal targetZoomFactor = qBound(0.1, *context_.zoomFactor * factor, 50.0);
    if (qFuzzyCompare(targetZoomFactor, *context_.zoomFactor)) {
        return;
    }

    const qreal appliedFactor = targetZoomFactor / *context_.zoomFactor;
    const QPoint viewportPoint = viewportPosition.toPoint();
    const QPointF scenePointBefore = context_.view->mapToScene(viewportPoint);
    context_.view->scale(appliedFactor, appliedFactor);
    const QPointF scenePointAfter = context_.view->mapToScene(viewportPoint);
    const QPointF sceneDelta = scenePointAfter - scenePointBefore;
    context_.view->translate(sceneDelta.x(), sceneDelta.y());

    *context_.autoFitEnabled = false;
    syncZoomFactorFromView();
    context_.updateCommandSurfaceState();
}

QRectF MapEditorSceneLifecycleController::mapGeometryFitBounds() const
{
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr) {
        return QRectF();
    }

    QRectF bounds;
    bool hasBounds = false;
    const QList<QGraphicsItem *> items = mapScene->items();
    for (QGraphicsItem *item : items) {
        if (item == nullptr) {
            continue;
        }
        if (item->data(kMapItemRole).toInt() != kMapItemGeometryValue) {
            continue;
        }

        const QRectF itemBounds = item->sceneBoundingRect();
        if (!itemBounds.isValid()) {
            continue;
        }

        if (!hasBounds) {
            bounds = itemBounds;
            hasBounds = true;
        } else {
            bounds = bounds.united(itemBounds);
        }
    }

    return hasBounds ? bounds : QRectF();
}

QRectF MapEditorSceneLifecycleController::mapPreviewBounds() const
{
    if (scene() == nullptr) {
        return QRectF();
    }
    return mapEditorCanvasPreviewBounds();
}

void MapEditorSceneLifecycleController::adjustMapZoom(qreal factor)
{
    if (context_.view == nullptr) {
        return;
    }

    applyZoomAtViewportPosition(factor, context_.view->viewport()->rect().center());
}

}
