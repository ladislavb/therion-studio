#include "MapEditorXviBackgroundItem.h"
#include "MapEditorPerformanceCounters.h"
#include "../../../core/MapAppearanceSettings.h"

#include <QBrush>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QSet>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{
namespace
{
// Upper bound for one per-view sketch raster (64 MiB ARGB32).
constexpr qint64 kMaxViewportCachePixels = 16LL * 1024 * 1024;
// At most this many views (main + detached/second view) keep a raster.
constexpr int kMaxViewportCaches = 2;

bool &viewportCacheEnabled()
{
    static bool enabled = true;
    return enabled;
}

qreal zoomOutStrokeScale(qreal lod)
{
    if (lod >= 1.0) {
        return 1.0;
    }

    return qBound<qreal>(0.30, std::pow(qMax<qreal>(0.01, lod), 0.80), 1.0);
}

constexpr qreal kXviLineTileTargetSize = 256.0;
constexpr int kXviLineTileMaxAxisCount = 160;

qreal centerlineStrokePadding(const QTransform &transform, qreal width)
{
    const qreal lod = qMax<qreal>(0.01, QStyleOptionGraphicsItem::levelOfDetailFromTransform(transform));
    bool invertible = false;
    const QTransform inverse = transform.inverted(&invertible);
    if (!invertible) {
        return width / 2.0 + 1.0;
    }
    const qreal radius = width * zoomOutStrokeScale(lod) / 2.0 + 1.0;
    const QRectF localStroke = inverse.mapRect(QRectF(-radius, -radius, 2 * radius, 2 * radius));
    return qMax(localStroke.width(), localStroke.height()) / 2.0;
}

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

QVector<int> exposedLineIndexes(const QVector<QLineF> &lines,
                                const QVector<MapEditorXviLineTile> &tiles,
                                const QRectF &clipRect,
                                const QRectF &paintBounds,
                                qreal padding)
{
    QVector<int> indexes;
    if (clipRect.contains(paintBounds)) {
        indexes.reserve(lines.size());
        for (int index = 0; index < lines.size(); ++index) {
            indexes.append(index);
        }
        return indexes;
    }

    QSet<int> seen;
    if (tiles.isEmpty()) {
        for (int index = 0; index < lines.size(); ++index) {
            if (lineBounds(lines.at(index), padding).intersects(clipRect)) {
                indexes.append(index);
            }
        }
        return indexes;
    }

    for (const MapEditorXviLineTile &tile : tiles) {
        if (!tile.bounds.adjusted(-padding, -padding, padding, padding).intersects(clipRect)) {
            continue;
        }
        for (const int index : tile.lineIndexes) {
            if (index < 0 || index >= lines.size() || seen.contains(index)) {
                continue;
            }
            seen.insert(index);
            if (lineBounds(lines.at(index), padding).intersects(clipRect)) {
                indexes.append(index);
            }
        }
    }
    return indexes;
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
            if (!tile.bounds.adjusted(-padding, -padding, padding, padding).intersects(clipRect)) {
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
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
}

void MapEditorXviBackgroundItem::setGeometryData(const MapEditorXviLayerGeometryData &geometry)
{
    prepareGeometryChange();
    invalidateViewportCache();
    geometry_ = geometry;
    centerlineAltitudeColors_.clear();
    centerlineAltitudeColorsEnabled_ = false;
    refreshPaintBounds();
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

void MapEditorXviBackgroundItem::setCenterlineAltitudeColors(
    const QVector<MapEditorXviTraverseColor> &colors, bool enabled)
{
    const bool nextEnabled = enabled && colors.size() == geometry_.traverseShotLines.size();
    if (centerlineAltitudeColorsEnabled_ == nextEnabled
        && centerlineAltitudeColors_ == colors) {
        return;
    }
    centerlineAltitudeColors_ = colors;
    centerlineAltitudeColorsEnabled_ = nextEnabled;
    invalidateViewportCache();
    update();
}

void MapEditorXviBackgroundItem::setCenterlineAppearance(const QColor &color, qreal width)
{
    if (!color.isValid() || !std::isfinite(width)
        || width < MapAppearanceSettings::minimumCenterlineWidth
        || width > MapAppearanceSettings::maximumCenterlineWidth) {
        qWarning("Invalid XVI centerline appearance; keeping previous values.");
        return;
    }
    if (color == centerlineColor_ && width == centerlineWidth_) {
        return;
    }
    centerlineColor_ = color;
    centerlineWidth_ = width;
    invalidateViewportCache();
    refreshPaintBounds();
    update();
}

void MapEditorXviBackgroundItem::refreshPaintBounds()
{
    qreal padding = 2.0;
    // Keep the original default fit footprint. Wider cosmetic strokes need
    // view-local padding so zooming out cannot cull their visible edges.
    if (centerlineWidth_ > 1.8) {
        padding = centerlineWidth_ / 2.0 + 1.0;
        if (scene()) {
            for (auto *view : scene()->views()) {
                padding = qMax(padding, centerlineStrokePadding(deviceTransform(view->viewportTransform()),
                                                                centerlineWidth_));
            }
        }
    }
    const QRectF bounds = geometry_.contentBounds.adjusted(-padding, -padding, padding, padding);
    if (bounds != paintBounds_) {
        prepareGeometryChange();
        invalidateViewportCache();
        paintBounds_ = bounds;
    }
}

QVariant MapEditorXviBackgroundItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    const QVariant result = QGraphicsPixmapItem::itemChange(change, value);
    if (change == QGraphicsItem::ItemSceneHasChanged
        || change == QGraphicsItem::ItemTransformHasChanged
        || change == QGraphicsItem::ItemScaleHasChanged
        || change == QGraphicsItem::ItemRotationHasChanged) {
        refreshPaintBounds();
    }
    if (change == QGraphicsItem::ItemSceneHasChanged
        || (change == QGraphicsItem::ItemVisibleHasChanged && !value.toBool())) {
        // Release the rasters of views the item is no longer shown in.
        viewportCaches_.clear();
    }
    return result;
}

void MapEditorXviBackgroundItem::setViewportCacheEnabledForTesting(bool enabled)
{
    viewportCacheEnabled() = enabled;
}

qint64 MapEditorXviBackgroundItem::viewportCacheBytesForTesting() const
{
    qint64 bytes = 0;
    for (const ViewportCache &cache : viewportCaches_) {
        bytes += cache.image.sizeInBytes();
    }
    return bytes;
}

void MapEditorXviBackgroundItem::invalidateViewportCache()
{
    ++contentGeneration_;
}

// Paints the cached raster for this view when the camera (world transform),
// viewport size, device pixel ratio and content generation are unchanged.
// A changed key is re-rasterised only once the camera is seen twice in a row,
// so continuous pan/zoom/auto-pan frames paint vectors directly instead of
// regenerating a full-viewport raster every frame. Off-screen painting
// (export, magnifier, scene rendering to images) never uses the cache.
bool MapEditorXviBackgroundItem::paintFromViewportCache(QPainter *painter, QWidget *widget)
{
    if (!viewportCacheEnabled() || widget == nullptr || painter->device() != widget) {
        return false;
    }
    const QTransform transform = painter->worldTransform();
    const qreal opacity = painter->opacity();
    const QSize size = widget->size();
    const qreal devicePixelRatio = widget->devicePixelRatioF();
    const QSize pixelSize(qCeil(size.width() * devicePixelRatio), qCeil(size.height() * devicePixelRatio));
    if (size.isEmpty() || !transform.isInvertible()
        || static_cast<qint64>(pixelSize.width()) * pixelSize.height() > kMaxViewportCachePixels) {
        return false;
    }

    viewportCaches_.removeIf([](const ViewportCache &cache) { return cache.widget.isNull(); });
    ViewportCache *cache = nullptr;
    for (ViewportCache &candidate : viewportCaches_) {
        if (candidate.widget == widget) {
            cache = &candidate;
            break;
        }
    }
    if (cache == nullptr) {
        if (viewportCaches_.size() >= kMaxViewportCaches) {
            viewportCaches_.removeFirst();
        }
        viewportCaches_.append(ViewportCache{});
        cache = &viewportCaches_.last();
        cache->widget = widget;
    }

    const bool hit = !cache->image.isNull() && cache->transform == transform && cache->size == size
        && cache->devicePixelRatio == devicePixelRatio && cache->opacity == opacity
        && cache->generation == contentGeneration_;
    if (!hit) {
        const bool cameraSettled = cache->pendingValid && cache->pendingTransform == transform
            && cache->pendingSize == size;
        cache->pendingValid = true;
        cache->pendingTransform = transform;
        cache->pendingSize = size;
        if (!cameraSettled) {
            cache->image = QImage();
            return false;
        }

        QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
        if (image.isNull()) {
            return false;
        }
        image.setDevicePixelRatio(devicePixelRatio);
        image.fill(Qt::transparent);
        QPainter imagePainter(&image);
        imagePainter.setRenderHints(painter->renderHints());
        imagePainter.setTransform(transform);
        // Per-primitive opacity, exactly like direct painting; source-over
        // compositing is associative, so blitting the raster opaquely below
        // gives the same pixels.
        imagePainter.setOpacity(opacity);
        paintVector(&imagePainter, true, transform.inverted().mapRect(QRectF(QPointF(0.0, 0.0), QSizeF(size))));
        imagePainter.end();
        cache->image = std::move(image);
        cache->transform = transform;
        cache->size = size;
        cache->devicePixelRatio = devicePixelRatio;
        cache->opacity = opacity;
        cache->generation = contentGeneration_;
    }

    painter->save();
    painter->resetTransform();
    painter->setOpacity(1.0);
    painter->drawImage(QPointF(0.0, 0.0), cache->image);
    painter->restore();
    return true;
}

QRectF MapEditorXviBackgroundItem::boundingRect() const
{
    return paintBounds_;
}

void MapEditorXviBackgroundItem::paint(QPainter *painter,
                                       const QStyleOptionGraphicsItem *option,
                                       QWidget *widget)
{
    if (painter == nullptr || !geometry_.hasContent()) {
        return;
    }
    if (option != nullptr && !option->exposedRect.intersects(paintBounds_)) {
        return;
    }
    if (paintFromViewportCache(painter, widget)) {
        return;
    }
    paintVector(painter, option != nullptr, option != nullptr ? option->exposedRect : QRectF());
}

void MapEditorXviBackgroundItem::paintVector(QPainter *painter, bool hasOption, const QRectF &exposedRect)
{
    ++mapEditorPerformanceCounters().xviBackgroundRenders;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qreal lod = hasOption
        ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform())
        : 1.0;
    const qreal zoomOutScale = zoomOutStrokeScale(lod);
    qreal exposedPadding = qMax<qreal>(2.0,
        qMax<qreal>(8.0, centerlineWidth_ / 2.0 + 1.0) / qMax<qreal>(0.01, lod));
    if (centerlineWidth_ > 1.8) {
        exposedPadding = qMax(exposedPadding, centerlineStrokePadding(painter->worldTransform(), centerlineWidth_));
    }
    const QRectF clipRect = (hasOption && exposedRect.isValid())
        ? exposedRect.adjusted(-exposedPadding, -exposedPadding, exposedPadding, exposedPadding)
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

    QPen traverseShotPen(centerlineColor_);
    traverseShotPen.setWidthF(centerlineWidth_ * zoomOutScale);
    traverseShotPen.setCosmetic(true);
    traverseShotPen.setCapStyle(Qt::RoundCap);
    traverseShotPen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(traverseShotPen);
    if (!geometry_.traverseShotLines.isEmpty()) {
        if (!centerlineAltitudeColorsEnabled_) {
            drawExposedLines(painter,
                             geometry_.traverseShotLines,
                             geometry_.traverseShotTiles,
                             clipRect,
                             paintBounds_,
                             exposedPadding);
        } else {
            const QVector<int> indexes = exposedLineIndexes(geometry_.traverseShotLines,
                geometry_.traverseShotTiles, clipRect, paintBounds_, exposedPadding);
            for (const int index : indexes) {
                const QLineF &line = geometry_.traverseShotLines.at(index);
                const MapEditorXviTraverseColor &colors = centerlineAltitudeColors_.at(index);
                if (colors.hasAltitude) {
                    QLinearGradient gradient(line.p1(), line.p2());
                    gradient.setColorAt(0.0, colors.from);
                    gradient.setColorAt(1.0, colors.to);
                    QPen altitudePen(QBrush(gradient), centerlineWidth_ * zoomOutScale);
                    altitudePen.setCosmetic(true);
                    altitudePen.setCapStyle(Qt::RoundCap);
                    altitudePen.setJoinStyle(Qt::RoundJoin);
                    painter->setPen(altitudePen);
                } else {
                    painter->setPen(traverseShotPen);
                }
                painter->drawLine(line);
            }
        }
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

    if (!geometry_.stations.isEmpty() && lod >= 0.25) {
        const qreal inverseLod = 1.0 / qMax<qreal>(0.01, lod);
        const qreal ringRadius = 3.6 * inverseLod;
        const qreal centerRadius = 1.15 * inverseLod;
        QPen stationPen(QColor(0, 104, 194, 224));
        stationPen.setWidthF(1.35 * zoomOutScale);
        stationPen.setCosmetic(true);
        painter->setPen(stationPen);
        painter->setBrush(QColor(226, 244, 255, 210));

        for (const MapEditorXviStationData &station : geometry_.stations) {
            const QPointF position = station.position;
            if (!clipRect.contains(position)) {
                continue;
            }
            painter->drawEllipse(position, ringRadius, ringRadius);
            painter->setBrush(QColor(0, 104, 194, 235));
            painter->drawEllipse(position, centerRadius, centerRadius);
            painter->setBrush(QColor(226, 244, 255, 210));
        }
    }

    painter->restore();
}
}
