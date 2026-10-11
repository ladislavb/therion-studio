#pragma once

#include "MapEditorObjectStyleCatalog.h"

#include <QColor>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QVector>

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
    void updatePaintBounds();
    void drawDecorations(QPainter *painter, qreal zoomOutScale);
    void drawScreenDecoration(QPainter *painter,
                         const MapEditorLineDecorationStyle &decoration,
                         qreal zoomOutScale);
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
};

void updateMapEditorScreenSpaceMetrics(QGraphicsScene *scene, const QTransform &viewTransform);
}
