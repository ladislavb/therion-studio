#pragma once

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include <functional>

class QGraphicsItem;
class QGraphicsPathItem;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QGraphicsView;

namespace TherionStudio
{
struct MapEditorSceneLifecycleContext
{
    QGraphicsScene **scene = nullptr;
    QGraphicsView *view = nullptr;
    QHash<int, QGraphicsItem *> *itemsByLine = nullptr;
    QHash<QString, QGraphicsItem *> *vertexItemsByKey = nullptr;
    QVector<QGraphicsRectItem *> *draftGeometryItems = nullptr;
    QVector<QGraphicsPixmapItem *> *backgroundImageItems = nullptr;
    bool *interactiveDrawStrokeActive = nullptr;
    QGraphicsPathItem **interactiveDrawPreviewPath = nullptr;
    QVector<QGraphicsItem *> *interactiveDrawPreviewMarkers = nullptr;
    bool *updatingSelection = nullptr;
    int *nextDraftGeometryId = nullptr;
    int *selectedBackgroundLayerIndex = nullptr;
    bool *autoFitEnabled = nullptr;
    qreal *zoomFactor = nullptr;

    std::function<void()> refreshBackgroundLayerControls;
    std::function<void()> applyBackgroundLayerStackingOrder;
    std::function<void(int)> setSelectedBackgroundLayerIndexInternal;
    std::function<void()> reprojectMetadataBackgroundLayersForCurrentDocument;
    std::function<QRectF()> mapBackgroundFitBounds;
    std::function<void()> updateCommandSurfaceState;
    std::function<int()> zoomPercent;
    std::function<void(int)> emitZoomStatusChanged;
};

class MapEditorSceneLifecycleController final
{
public:
    explicit MapEditorSceneLifecycleController(MapEditorSceneLifecycleContext context);

    void clearMapScene();
    void clearDraftGeometryItems();
    void clearBackgroundImageItems();
    void restoreDraftGeometryItems();
    void restoreBackgroundImageItems();
    void fitMapToView(bool includeBackgroundImages, bool updateCommandSurface = true);
    void syncZoomFactorFromView();
    void applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition);
    void adjustMapZoom(qreal factor);
    QRectF mapGeometryFitBounds() const;
    QRectF mapPreviewBounds() const;

private:
    QGraphicsScene *scene() const;

    MapEditorSceneLifecycleContext context_;
};
}
