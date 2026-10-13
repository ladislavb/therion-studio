#pragma once

#include <QColor>
#include <QGraphicsPixmapItem>
#include <QLineF>
#include <QImage>
#include <QPointer>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QVector>

namespace TherionStudio
{
struct MapEditorXviLineTile
{
    QRectF bounds;
    QVector<int> lineIndexes;
};

struct MapEditorXviSketchPathData
{
    QVector<QLineF> lines;
    QVector<MapEditorXviLineTile> tiles;
    QColor color;
    Qt::PenStyle style = Qt::SolidLine;
};

struct MapEditorXviPassagePolygonData
{
    QPolygonF polygon;
    QRectF bounds;
};

struct MapEditorXviPassageTile
{
    QRectF bounds;
    QVector<int> polygonIndexes;
};

struct MapEditorXviStationData
{
    QString name;
    QPointF position;
};

struct MapEditorXviTraverseStations
{
    QString from;
    QString to;
};

struct MapEditorXviTraverseColor
{
    QColor from;
    QColor to;
    bool hasAltitude = false;

    bool operator==(const MapEditorXviTraverseColor &) const = default;
};

struct MapEditorXviLayerGeometryData
{
    QVector<QLineF> gridLines;
    QVector<MapEditorXviLineTile> gridTiles;
    QVector<QLineF> traverseShotLines;
    QVector<MapEditorXviTraverseStations> traverseShotStations;
    QVector<MapEditorXviLineTile> traverseShotTiles;
    QVector<QLineF> splayShotLines;
    QVector<MapEditorXviLineTile> splayShotTiles;
    QVector<MapEditorXviPassagePolygonData> passagePolygons;
    QVector<MapEditorXviPassageTile> passageTiles;
    QVector<MapEditorXviSketchPathData> sketchPaths;
    QVector<MapEditorXviStationData> stations;
    QRectF contentBounds;

    bool hasContent() const
    {
        bool hasSketch = false;
        for (const MapEditorXviSketchPathData &sketch : sketchPaths) {
            if (!sketch.lines.isEmpty()) {
                hasSketch = true;
                break;
            }
        }
        return !contentBounds.isEmpty()
            && (!gridLines.isEmpty()
                || !traverseShotLines.isEmpty()
                || !splayShotLines.isEmpty()
                || !passagePolygons.isEmpty()
                || hasSketch
                || !stations.isEmpty());
    }
};

class MapEditorXviBackgroundItem final : public QGraphicsPixmapItem
{
public:
    explicit MapEditorXviBackgroundItem(QGraphicsItem *parent = nullptr);

    void setGeometryData(const MapEditorXviLayerGeometryData &geometry);
    const MapEditorXviLayerGeometryData &geometryData() const;
    void setCenterlineAppearance(const QColor &color, qreal width);
    void setCenterlineAltitudeColors(const QVector<MapEditorXviTraverseColor> &colors, bool enabled);
    const QVector<MapEditorXviTraverseColor> &centerlineAltitudeColors() const { return centerlineAltitudeColors_; }
    QColor centerlineColor() const { return centerlineColor_; }
    qreal centerlineWidth() const { return centerlineWidth_; }
    void refreshPaintBounds();

    static void setViewportCacheEnabledForTesting(bool enabled);
    qint64 viewportCacheBytesForTesting() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    // Viewport-sized raster of the sketch for one view and camera, reused
    // while only other items (hover, selection, drag) repaint.
    struct ViewportCache
    {
        QPointer<QWidget> widget;
        QImage image;
        QTransform transform;
        QSize size;
        qreal devicePixelRatio = 1.0;
        qreal opacity = 1.0;
        quint64 generation = 0;
        bool pendingValid = false;
        QTransform pendingTransform;
        QSize pendingSize;
    };

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void paintVector(QPainter *painter, bool hasOption, const QRectF &exposedRect);
    bool paintFromViewportCache(QPainter *painter, QWidget *widget);
    void invalidateViewportCache();

    MapEditorXviLayerGeometryData geometry_;
    QVector<ViewportCache> viewportCaches_;
    quint64 contentGeneration_ = 1;
    QRectF paintBounds_;
    QColor centerlineColor_ = QColor(18, 44, 88, 130);
    qreal centerlineWidth_ = 1.8;
    QVector<MapEditorXviTraverseColor> centerlineAltitudeColors_;
    bool centerlineAltitudeColorsEnabled_ = false;
};
}
