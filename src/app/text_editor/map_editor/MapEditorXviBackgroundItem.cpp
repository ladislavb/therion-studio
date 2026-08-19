#include "MapEditorXviBackgroundItem.h"

#include <QBrush>
#include <QPainter>
#include <QPen>
#include <QSet>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{
namespace
{
qreal zoomOutStrokeScale(qreal lod)
{
    if (lod >= 1.0) {
        return 1.0;
    }

    return qBound<qreal>(0.30, std::pow(qMax<qreal>(0.01, lod), 0.80), 1.0);
}

constexpr qreal kXviLineTileTargetSize = 256.0;
constexpr int kXviLineTileMaxAxisCount = 160;

QRectF lineBounds(const QLineF &line, qreal padding)
{
    const qreal left = qMin(line.p1().x(), line.p2().x());
    const qreal top = qMin(line.p1().y(), line.p2().y());
    const qreal right = qMax(line.p1().x(), line.p2().x());
    const qreal bottom = qMax(line.p1().y(), line.p2().y());
    return QRectF(QPointF(left, top), QPointF(right, bottom)).normalized().adjusted(-padding, -padding, padding, padding);
}

QVector<MapEditorXviLineTile> buildLineTiles(const QVector<QLineF> &lines, const QRectF &bounds)
{
    if (lines.isEmpty() || !bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return {};
    }

    const qreal tileSize = qMax(kXviLineTileTargetSize,
                                qMax(bounds.width(), bounds.height()) / kXviLineTileMaxAxisCount);
    const int columnCount = qMax(1, static_cast<int>(std::ceil(bounds.width() / tileSize)));
    const int rowCount = qMax(1, static_cast<int>(std::ceil(bounds.height() / tileSize)));

    QVector<MapEditorXviLineTile> tiles;
    tiles.resize(rowCount * columnCount);
    for (int row = 0; row < rowCount; ++row) {
        for (int column = 0; column < columnCount; ++column) {
            const QRectF tileBounds(bounds.left() + (column * tileSize),
                                    bounds.top() + (row * tileSize),
                                    tileSize,
                                    tileSize);
            tiles[row * columnCount + column].bounds = tileBounds.adjusted(-2.0, -2.0, 2.0, 2.0);
        }
    }

    auto tileColumn = [&](qreal x) {
        return qBound(0, static_cast<int>(std::floor((x - bounds.left()) / tileSize)), columnCount - 1);
    };
    auto tileRow = [&](qreal y) {
        return qBound(0, static_cast<int>(std::floor((y - bounds.top()) / tileSize)), rowCount - 1);
    };

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QRectF lineRect = lineBounds(lines.at(lineIndex), 2.0);
        if (!lineRect.isValid() || !lineRect.intersects(bounds)) {
            continue;
        }

        const int firstColumn = tileColumn(lineRect.left());
        const int lastColumn = tileColumn(lineRect.right());
        const int firstRow = tileRow(lineRect.top());
        const int lastRow = tileRow(lineRect.bottom());
        for (int row = firstRow; row <= lastRow; ++row) {
            for (int column = firstColumn; column <= lastColumn; ++column) {
                tiles[row * columnCount + column].lineIndexes.append(lineIndex);
            }
        }
    }

    tiles.erase(std::remove_if(tiles.begin(),
                               tiles.end(),
                               [](const MapEditorXviLineTile &tile) {
                                   return tile.lineIndexes.isEmpty();
                               }),
                tiles.end());
    return tiles;
}

void drawExposedLines(QPainter *painter,
                      const QVector<QLineF> &lines,
                      const QVector<MapEditorXviLineTile> &tiles,
                      const QRectF &clipRect,
                      const QRectF &paintBounds,
                      qreal padding)
{
    if (painter == nullptr || lines.isEmpty()) {
        return;
    }

    if (clipRect.contains(paintBounds)) {
        painter->drawLines(lines.constData(), static_cast<int>(lines.size()));
        return;
    }

    QVector<QLineF> exposedLines;
    exposedLines.reserve(qMin<qsizetype>(lines.size(), 4096));
    if (tiles.isEmpty()) {
        for (const QLineF &line : lines) {
            if (lineBounds(line, padding).intersects(clipRect)) {
                exposedLines.append(line);
            }
        }
    } else {
        QSet<int> seenLineIndexes;
        seenLineIndexes.reserve(qMin<qsizetype>(lines.size(), 4096));
        for (const MapEditorXviLineTile &tile : tiles) {
            if (!tile.bounds.intersects(clipRect)) {
                continue;
            }
            for (const int lineIndex : tile.lineIndexes) {
                if (lineIndex < 0 || lineIndex >= lines.size() || seenLineIndexes.contains(lineIndex)) {
                    continue;
                }
                seenLineIndexes.insert(lineIndex);
                const QLineF &line = lines.at(lineIndex);
                if (lineBounds(line, padding).intersects(clipRect)) {
                    exposedLines.append(line);
                }
            }
        }
    }
    if (!exposedLines.isEmpty()) {
        painter->drawLines(exposedLines.constData(), static_cast<int>(exposedLines.size()));
    }
}

QVector<MapEditorXviPassageTile> buildPassageTiles(const QVector<MapEditorXviPassagePolygonData> &polygons,
                                                    const QRectF &bounds)
{
    if (polygons.isEmpty() || !bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return {};
    }

    const qreal tileSize = qMax(kXviLineTileTargetSize,
                                qMax(bounds.width(), bounds.height()) / kXviLineTileMaxAxisCount);
    const int columnCount = qMax(1, static_cast<int>(std::ceil(bounds.width() / tileSize)));
    const int rowCount = qMax(1, static_cast<int>(std::ceil(bounds.height() / tileSize)));

    QVector<MapEditorXviPassageTile> tiles;
    tiles.resize(rowCount * columnCount);
    for (int row = 0; row < rowCount; ++row) {
        for (int column = 0; column < columnCount; ++column) {
            const QRectF tileBounds(bounds.left() + (column * tileSize),
                                    bounds.top() + (row * tileSize),
                                    tileSize,
                                    tileSize);
            tiles[row * columnCount + column].bounds = tileBounds.adjusted(-2.0, -2.0, 2.0, 2.0);
        }
    }

    auto tileColumn = [&](qreal x) {
        return qBound(0, static_cast<int>(std::floor((x - bounds.left()) / tileSize)), columnCount - 1);
    };
    auto tileRow = [&](qreal y) {
        return qBound(0, static_cast<int>(std::floor((y - bounds.top()) / tileSize)), rowCount - 1);
    };

    for (int polygonIndex = 0; polygonIndex < polygons.size(); ++polygonIndex) {
        const QRectF polygonRect = polygons.at(polygonIndex).bounds.adjusted(-2.0, -2.0, 2.0, 2.0);
        if (!polygonRect.isValid() || !polygonRect.intersects(bounds)) {
            continue;
        }

        const int firstColumn = tileColumn(polygonRect.left());
        const int lastColumn = tileColumn(polygonRect.right());
        const int firstRow = tileRow(polygonRect.top());
        const int lastRow = tileRow(polygonRect.bottom());
        for (int row = firstRow; row <= lastRow; ++row) {
            for (int column = firstColumn; column <= lastColumn; ++column) {
                tiles[row * columnCount + column].polygonIndexes.append(polygonIndex);
            }
        }
    }

    tiles.erase(std::remove_if(tiles.begin(),
                               tiles.end(),
                               [](const MapEditorXviPassageTile &tile) {
                                   return tile.polygonIndexes.isEmpty();
                               }),
                tiles.end());
    return tiles;
}

void drawExposedPassagePolygons(QPainter *painter,
                                const QVector<MapEditorXviPassagePolygonData> &polygons,
                                const QVector<MapEditorXviPassageTile> &tiles,
                                const QRectF &clipRect,
                                const QRectF &paintBounds)
{
    if (painter == nullptr || polygons.isEmpty()) {
        return;
    }

    if (clipRect.contains(paintBounds)) {
        for (const MapEditorXviPassagePolygonData &passage : polygons) {
            painter->drawPolygon(passage.polygon);
        }
        return;
    }

    QSet<int> polygonIndexes;
    polygonIndexes.reserve(qMin<qsizetype>(polygons.size(), 4096));
    if (tiles.isEmpty()) {
        for (int polygonIndex = 0; polygonIndex < polygons.size(); ++polygonIndex) {
            if (polygons.at(polygonIndex).bounds.intersects(clipRect)) {
                polygonIndexes.insert(polygonIndex);
            }
        }
    } else {
        for (const MapEditorXviPassageTile &tile : tiles) {
            if (!tile.bounds.intersects(clipRect)) {
                continue;
            }
            for (const int polygonIndex : tile.polygonIndexes) {
                if (polygonIndex >= 0 && polygonIndex < polygons.size()
                    && polygons.at(polygonIndex).bounds.intersects(clipRect)) {
                    polygonIndexes.insert(polygonIndex);
                }
            }
        }
    }

    for (const int polygonIndex : polygonIndexes) {
        painter->drawPolygon(polygons.at(polygonIndex).polygon);
    }
}
}

MapEditorXviBackgroundItem::MapEditorXviBackgroundItem(QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent)
{
    setCacheMode(QGraphicsItem::NoCache);
}

void MapEditorXviBackgroundItem::setGeometryData(const MapEditorXviLayerGeometryData &geometry)
{
    prepareGeometryChange();
    geometry_ = geometry;
    paintBounds_ = geometry_.contentBounds.adjusted(-2.0, -2.0, 2.0, 2.0);
    geometry_.gridTiles = buildLineTiles(geometry_.gridLines, paintBounds_);
    geometry_.traverseShotTiles = buildLineTiles(geometry_.traverseShotLines, paintBounds_);
    geometry_.splayShotTiles = buildLineTiles(geometry_.splayShotLines, paintBounds_);
    geometry_.passageTiles = buildPassageTiles(geometry_.passagePolygons, paintBounds_);
    for (MapEditorXviSketchPathData &sketchPath : geometry_.sketchPaths) {
        sketchPath.tiles = buildLineTiles(sketchPath.lines, paintBounds_);
    }
}

const MapEditorXviLayerGeometryData &MapEditorXviBackgroundItem::geometryData() const
{
    return geometry_;
}

QRectF MapEditorXviBackgroundItem::boundingRect() const
{
    return paintBounds_;
}

void MapEditorXviBackgroundItem::paint(QPainter *painter,
                                       const QStyleOptionGraphicsItem *option,
                                       QWidget *widget)
{
    Q_UNUSED(widget);

    if (painter == nullptr || !geometry_.hasContent()) {
        return;
    }
    if (option != nullptr && !option->exposedRect.intersects(paintBounds_)) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qreal lod = option != nullptr
        ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform())
        : 1.0;
    const qreal zoomOutScale = zoomOutStrokeScale(lod);
    const qreal exposedPadding = qMax<qreal>(2.0, 8.0 / qMax<qreal>(0.01, lod));
    const QRectF clipRect = (option != nullptr && option->exposedRect.isValid())
        ? option->exposedRect.adjusted(-exposedPadding, -exposedPadding, exposedPadding, exposedPadding)
        : paintBounds_;

    if (!geometry_.passagePolygons.isEmpty()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(89, 128, 164, 45));
        drawExposedPassagePolygons(painter,
                                   geometry_.passagePolygons,
                                   geometry_.passageTiles,
                                   clipRect,
                                   paintBounds_);
    }

    QPen gridPen(QColor(88, 114, 143, 92));
    gridPen.setWidthF(1.0 * zoomOutScale);
    gridPen.setCosmetic(true);
    gridPen.setCapStyle(Qt::SquareCap);
    gridPen.setJoinStyle(Qt::MiterJoin);
    painter->setPen(gridPen);
    if (!geometry_.gridLines.isEmpty()) {
        drawExposedLines(painter, geometry_.gridLines, geometry_.gridTiles, clipRect, paintBounds_, exposedPadding);
    }

    if (!geometry_.passagePolygons.isEmpty()) {
        QPen passageOutlinePen(QColor(41, 72, 108, 138));
        passageOutlinePen.setWidthF(1.0 * zoomOutScale);
        passageOutlinePen.setCosmetic(true);
        passageOutlinePen.setCapStyle(Qt::RoundCap);
        passageOutlinePen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(passageOutlinePen);
        painter->setBrush(Qt::NoBrush);
        drawExposedPassagePolygons(painter,
                                   geometry_.passagePolygons,
                                   geometry_.passageTiles,
                                   clipRect,
                                   paintBounds_);
    }

    QPen traverseShotPen(QColor(18, 44, 88, 130));
    traverseShotPen.setWidthF(1.8 * zoomOutScale);
    traverseShotPen.setCosmetic(true);
    traverseShotPen.setCapStyle(Qt::RoundCap);
    traverseShotPen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(traverseShotPen);
    if (!geometry_.traverseShotLines.isEmpty()) {
        drawExposedLines(painter,
                         geometry_.traverseShotLines,
                         geometry_.traverseShotTiles,
                         clipRect,
                         paintBounds_,
                         exposedPadding);
    }

    QPen splayPen(QColor(28, 92, 64, 122));
    splayPen.setWidthF(1.4 * zoomOutScale);
    splayPen.setCosmetic(true);
    splayPen.setCapStyle(Qt::RoundCap);
    splayPen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(splayPen);
    if (!geometry_.splayShotLines.isEmpty() && lod >= 0.42) {
        drawExposedLines(painter, geometry_.splayShotLines, geometry_.splayShotTiles, clipRect, paintBounds_, exposedPadding);
    }

    if (!geometry_.sketchPaths.isEmpty() && lod >= 0.60) {
        for (const MapEditorXviSketchPathData &sketch : geometry_.sketchPaths) {
            if (sketch.lines.isEmpty()) {
                continue;
            }

            QColor strokeColor = sketch.color.isValid() ? sketch.color : QColor(0, 0, 0);
            if (strokeColor.alpha() > 220) {
                strokeColor.setAlpha(220);
            } else if (strokeColor.alpha() <= 0) {
                strokeColor.setAlpha(200);
            }

            QPen sketchPen(strokeColor);
            sketchPen.setStyle(sketch.style);
            sketchPen.setWidthF(1.3 * zoomOutScale);
            sketchPen.setCosmetic(true);
            sketchPen.setCapStyle(Qt::RoundCap);
            sketchPen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(sketchPen);
            drawExposedLines(painter, sketch.lines, sketch.tiles, clipRect, paintBounds_, exposedPadding);
        }
    }

    painter->restore();
}
}
