#pragma once

#include "MapEditorObjectStyleCatalog.h"

#include <QColor>
#include <QGraphicsPathItem>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QTransform>
#include <QVector>

#include <array>
#include <optional>

namespace TherionStudio
{
struct MapEditorLineDecorationVertex
{
    QPointF anchor;
    std::optional<qreal> pathDistance;
    std::optional<qreal> orientationDegrees;
    std::optional<qreal> leftSize;
};

struct MapEditorLineDecorationVertexMetric
{
    qreal distance = 0.0;
    std::optional<qreal> orientationDegrees;
    std::optional<qreal> leftSize;
};

class MapEditorLineDecorationItem final : public QGraphicsPathItem
{
public:
    MapEditorLineDecorationItem(const QPainterPath &path,
                                const QVector<MapEditorLineDecorationStyle> &decorations,
                                const QColor &fallbackColor,
                                bool reversed,
                                int fallbackSeed,
                                const QVector<MapEditorLineDecorationVertex> &lineVertices = {},
                                qreal linePointLengthScale = 1.0,
                                QGraphicsItem *parent = nullptr);

    void setDecorationPath(const QPainterPath &path);
    void setDecorationPath(const QPainterPath &path,
                           const QVector<MapEditorLineDecorationVertex> &lineVertices);
    bool usesLineVertexMetrics() const;
    void updateViewTransform(const QTransform &viewTransform);
    QRectF boundingRect() const override;
    QRectF sourceGeometryBounds() const;
    qreal hitDistancePixels(const QPointF &scenePosition, const QTransform &viewTransform) const;

protected:
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    struct ProjectedPathCacheEntry
    {
        QTransform transform;
        QList<QPolygonF> polygons;
        QVector<QVector<qreal>> cumulativeDistances;
        bool valid = false;
    };

    void updatePaintBounds();
    qreal cachedPathLength() const;
    const QVector<MapEditorLineDecorationVertexMetric> &cachedSourceMetrics() const;
    const ProjectedPathCacheEntry &projectedPolygonsForProjection(const QTransform &projection) const;
    void drawDecorations(QPainter *painter,
                         qreal zoomOutScale,
                         const QTransform &projection,
                         const QPointF &projectionOffset,
                         const QList<QPolygonF> &polygons,
                         const QVector<QVector<qreal>> &cumulativeDistances);
    void drawScreenDecoration(QPainter *painter,
                              const MapEditorLineDecorationStyle &decoration,
                              qreal zoomOutScale,
                              const QTransform &projection,
                              const QPointF &projectionOffset,
                              const QList<QPolygonF> &polygons,
                              const QVector<QVector<qreal>> &cumulativeDistances);
    void drawTick(QPainter *painter,
                  const MapEditorLineDecorationStyle &decoration,
                  const QPointF &point,
                  const QPointF &normal,
                  int markerIndex);
    void drawRung(QPainter *painter,
                  const MapEditorLineDecorationStyle &decoration,
                  const QPointF &point,
                  const QPointF &normal);
    void drawSymbol(QPainter *painter,
                    const MapEditorLineDecorationStyle &decoration,
                    const QPointF &point,
                    const QPointF &tangent,
                    const QPointF &normal,
                    int markerIndex,
                    qreal zoomOutScale);
    void drawWave(QPainter *painter,
                  const MapEditorLineDecorationStyle &decoration,
                  const QPointF &point,
                  const QPointF &tangent,
                  const QPointF &normal,
                  int markerIndex);

    QVector<MapEditorLineDecorationStyle> decorations_;
    QVector<MapEditorLineDecorationVertex> lineVertices_;
    QColor fallbackColor_;
    bool reversed_ = false;
    int fallbackSeed_ = 0;
    qreal linePointLengthScale_ = 1.0;
    QRectF paintBounds_;
    std::optional<QColor> interactionColorOverride_;
    qreal screenPaddingScale_ = 2.0;
    mutable QPainterPath projectedPathSource_;
    mutable std::array<ProjectedPathCacheEntry, 2> projectedPathCache_;
    mutable std::size_t mostRecentProjection_ = 0;
    mutable qreal cachedPathLength_ = -1.0;
    mutable QVector<MapEditorLineDecorationVertexMetric> cachedSourceMetrics_;
    mutable bool cachedSourceMetricsValid_ = false;
};

void updateMapEditorScreenSpaceMetrics(QGraphicsScene *scene, const QTransform &viewTransform);
}
