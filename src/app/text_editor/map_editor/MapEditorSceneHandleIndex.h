#pragma once

#include <QGraphicsItem>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QtGlobal>

class QGraphicsScene;

namespace TherionStudio
{
enum class MapEditorSceneHandleKind : quint8
{
    Point,
    GeometryVertex,
    LinePointSizeHandle,
    PointOrientationHandle
};

struct MapEditorSceneHandleIndexStats
{
    int trackedItems = 0;
    qreal cellSize = 0.0;
};

// Scene-coordinate uniform grid of vertex-like editing items (point handles,
// line/area vertices and controls, slope l-size and point orientation
// handles). Unlike QGraphicsScene's BSP index it includes hidden
// (selection-gated) items and ItemIgnoresTransformations items, so hit-test
// and snap code can replace whole-scene scans with local queries.
//
// Items register themselves from itemChange(ItemSceneChange /
// ItemSceneHasChanged / ItemPositionHasChanged) and their destructors, so a
// raw pointer never outlives its item. Only immutable item facts (kind, line
// number) and the key point are stored; visibility, selection, data roles and
// stacking are read live by callers. Results are returned in
// Qt::DescendingOrder stacking order (zValue, then most recently added first)
// so tie-breaking matches the former QGraphicsScene::items() walks.
namespace MapEditorSceneHandleIndex
{
void track(QGraphicsItem *item, MapEditorSceneHandleKind kind, int lineNumber, const QPointF &sceneKey);
void untrack(QGraphicsItem *item, const QGraphicsScene *scene);
void moveKey(QGraphicsItem *item, const QPointF &sceneKey);

QVector<QGraphicsItem *> itemsInRect(const QGraphicsScene *scene, const QRectF &sceneRect);
QVector<QGraphicsItem *> itemsForLine(const QGraphicsScene *scene, int lineNumber);
MapEditorSceneHandleIndexStats stats(const QGraphicsScene *scene);

// Called from the tracked item's itemChange(); sceneKey is evaluated only when
// the index needs the current key.
template <typename SceneKeyFunction>
void noteItemChange(QGraphicsItem *item,
                    QGraphicsItem::GraphicsItemChange change,
                    MapEditorSceneHandleKind kind,
                    int lineNumber,
                    SceneKeyFunction sceneKey)
{
    switch (change) {
    case QGraphicsItem::ItemSceneChange:
        if (item->scene() != nullptr) {
            untrack(item, item->scene());
        }
        break;
    case QGraphicsItem::ItemSceneHasChanged:
        if (item->scene() != nullptr) {
            track(item, kind, lineNumber, sceneKey());
        }
        break;
    case QGraphicsItem::ItemPositionHasChanged:
        moveKey(item, sceneKey());
        break;
    default:
        break;
    }
}
}
}
