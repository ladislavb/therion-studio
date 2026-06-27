#include "MapEditorLineDecorationItem.h"

#include "MapEditorPointSymbolGeometry.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"

#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace TherionStudio
{
namespace
{
qreal zoomOutStrokeScale(qreal lod)
{
    if (lod >= 1.0) {
        return 1.0;
    }

    return qBound<qreal>(0.32, std::pow(qMax<qreal>(0.01, lod), 0.72), 1.0);
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

quint32 stableHashU32(quint32 value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

qreal stableRandomSigned(quint32 seed, int major, int minor, quint32 salt = 0U)
{
    quint32 value = seed ^ (static_cast<quint32>(major) * 0x9e3779b9U) ^ (static_cast<quint32>(minor) * 0x85ebca6bU) ^ salt;
    value = stableHashU32(value);
    const qreal normalized = static_cast<qreal>(value) / static_cast<qreal>(std::numeric_limits<quint32>::max());
    return (normalized * 2.0) - 1.0;
}

qreal lineDecorationSideFactor(MapEditorLineDecorationSide side)
{
    switch (side) {
    case MapEditorLineDecorationSide::Left:
        return 1.0;
    case MapEditorLineDecorationSide::Right:
        return -1.0;
    case MapEditorLineDecorationSide::Center:
        return 0.0;
    }
    return 0.0;
}

qreal lineDecorationEffectiveOffset(const MapEditorLineDecorationStyle &decoration,
                                    int markerIndex,
                                    int seed)
{
    qreal offset = decoration.side == MapEditorLineDecorationSide::Center
        ? decoration.offset
        : lineDecorationSideFactor(decoration.side) * decoration.offset;
    if (decoration.offsetJitter > 0.0) {
        offset += stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x91f2U) * decoration.offsetJitter;
    }
    return offset;
}

qreal lineDecorationEffectiveDistance(qreal baseDistance,
                                      qreal pathLength,
                                      qreal spacing,
                                      const MapEditorLineDecorationStyle &decoration,
                                      int markerIndex,
                                      int seed)
{
    qreal distance = baseDistance;
    if (decoration.distanceJitter > 0.0) {
        const qreal jitter = qMin(decoration.distanceJitter, spacing * 0.45);
        distance += stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x4d712U) * jitter;
    }
    return qBound<qreal>(0.0, distance, pathLength);
}

qreal lineDecorationAdjustedStep(qreal length, qreal targetSpacing)
{
    if (length <= 0.001) {
        return qMax<qreal>(0.001, length);
    }

    const qreal spacing = qMax<qreal>(1.0, targetSpacing);
    if (spacing <= length / 2.0) {
        return length / qMax<qreal>(1.0, std::floor(length / spacing));
    }
    return length / 2.0;
}

struct LineDecorationMarkerDistances
{
    QVector<qreal> distances;
    qreal jitterSpacing = 1.0;
};

LineDecorationMarkerDistances slopeTickMarkerDistances(qreal pathLength,
                                                       const MapEditorLineDecorationStyle &decoration)
{
    LineDecorationMarkerDistances result;
    const qreal spacing = qMax<qreal>(1.0, decoration.spacing);
    if (pathLength <= 0.001) {
        return result;
    }

    if (!decoration.adjustSpacing) {
        const int markerCount = qBound(1, static_cast<int>(std::floor(pathLength / spacing)), 2048);
        result.distances.reserve(markerCount);
        result.jitterSpacing = spacing;
        for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
            result.distances.append(markerCount == 1
                                        ? pathLength * 0.5
                                        : (markerIndex + 0.5) * spacing);
        }
        return result;
    }

    const qreal symbolUnit = qMax<qreal>(0.001, spacing / 1.4);
    qreal edgeOffset = 0.0;
    if (pathLength > 3.0 * symbolUnit) {
        edgeOffset = 0.3 * symbolUnit;
    } else if (pathLength > symbolUnit) {
        edgeOffset = 0.1 * symbolUnit;
    }

    const qreal drawableLength = qMax<qreal>(0.0, pathLength - (2.0 * edgeOffset));
    if (drawableLength <= 0.001) {
        result.distances.append(pathLength * 0.5);
        result.jitterSpacing = spacing;
        return result;
    }

    const qreal adjustedSpacing = lineDecorationAdjustedStep(drawableLength, spacing);
    const qreal step = qMax<qreal>(0.001, adjustedSpacing / qMax<qreal>(1.0, decoration.spacingDivisor));
    const qreal start = qBound<qreal>(0.0, edgeOffset, pathLength);
    const qreal end = qBound<qreal>(0.0, edgeOffset + drawableLength, pathLength);
    result.jitterSpacing = step;
    for (qreal distance = start; distance <= end + 0.001 && result.distances.size() < 2048; distance += step) {
        result.distances.append(qBound<qreal>(0.0, distance, pathLength));
    }
    if (result.distances.isEmpty()) {
        result.distances.append(pathLength * 0.5);
    }
    return result;
}

LineDecorationMarkerDistances repeatedMarkerDistances(qreal pathLength,
                                                      const MapEditorLineDecorationStyle &decoration)
{
    LineDecorationMarkerDistances result;
    const qreal spacing = qMax<qreal>(1.0, decoration.spacing);
    if (pathLength <= 0.001) {
        return result;
    }

    if (!decoration.adjustSpacing) {
        const int markerCount = qBound(0, static_cast<int>(std::floor(pathLength / spacing)), 2048);
        result.distances.reserve(markerCount);
        result.jitterSpacing = spacing;
        for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
            result.distances.append((markerIndex + 0.5) * spacing);
        }
        return result;
    }

    const qreal adjustedSpacing = lineDecorationAdjustedStep(pathLength, spacing);
    const qreal step = qMax<qreal>(0.001, adjustedSpacing / qMax<qreal>(1.0, decoration.spacingDivisor));
    const int markerCount = qBound(1, static_cast<int>(std::floor(pathLength / step)), 2048);
    result.distances.reserve(markerCount);
    result.jitterSpacing = step;
    for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
        result.distances.append(qBound<qreal>(0.0, (markerIndex + 0.5) * step, pathLength));
    }
    return result;
}

qreal lineDecorationPadding(const QVector<MapEditorLineDecorationStyle> &decorations)
{
    qreal padding = 1.0;
    for (const MapEditorLineDecorationStyle &decoration : decorations) {
        qreal offsetExtent = std::abs(decoration.offset);
        for (const qreal offset : decoration.offsets) {
            offsetExtent = qMax(offsetExtent, std::abs(offset));
        }
        if (decoration.fromOffset.has_value()) {
            offsetExtent = qMax(offsetExtent, std::abs(decoration.fromOffset.value()));
        }
        if (decoration.toOffset.has_value()) {
            offsetExtent = qMax(offsetExtent, std::abs(decoration.toOffset.value()));
        }
        padding = qMax(padding,
                       offsetExtent
                           + decoration.size
                           + decoration.length
                           + decoration.sizeJitter
                           + decoration.offsetJitter
                           + decoration.strokeWidth
                           + 4.0);
    }
    return padding;
}

QPen lineDecorationPen(const MapEditorLineDecorationStyle &decoration,
                       const QColor &fallbackColor,
                       qreal zoomOutScale,
                       const std::optional<QColor> &interactionColorOverride)
{
    QColor color = interactionColorOverride.value_or(decoration.strokeColor.value_or(fallbackColor));
    const qreal interactionWidthExtra = interactionColorOverride.has_value() ? 1.8 : 0.0;
    QPen pen(color,
             qMax<qreal>(0.15, (decoration.strokeWidth * zoomOutScale) + interactionWidthExtra),
             decoration.strokeStyle,
             Qt::RoundCap,
             Qt::RoundJoin);
    pen.setCosmetic(true);
    if (!decoration.dashPattern.isEmpty()) {
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern(decoration.dashPattern);
    }
    return pen;
}

struct LineDecorationSample
{
    QPointF point;
    QPointF tangent;
    QPointF normal;
};

struct LineDecorationVertexMetric
{
    qreal distance = 0.0;
    std::optional<qreal> orientationDegrees;
    std::optional<qreal> leftSize;
};

qreal normalizeDegrees(qreal value)
{
    qreal normalized = std::fmod(value, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    if (normalized >= 360.0) {
        normalized -= 360.0;
    }
    return normalized;
}

QPointF directionForOrientationDegrees(qreal orientationDegrees)
{
    constexpr qreal pi = 3.14159265358979323846;
    const qreal radians = normalizeDegrees(orientationDegrees) * pi / 180.0;
    return QPointF(std::sin(radians), -std::cos(radians));
}

qreal nearestPathDistanceToPoint(const QPainterPath &path, const QPointF &point)
{
    const qreal pathLength = path.length();
    if (pathLength <= 0.001) {
        return 0.0;
    }

    const int sampleCount = qBound(16, static_cast<int>(std::ceil(pathLength / 8.0)) + 1, 512);
    qreal bestDistance = 0.0;
    qreal bestDistanceSquared = std::numeric_limits<qreal>::max();
    for (int index = 0; index < sampleCount; ++index) {
        const qreal distance = index == sampleCount - 1
            ? pathLength
            : pathLength * static_cast<qreal>(index) / static_cast<qreal>(sampleCount - 1);
        const QPointF samplePoint = path.pointAtPercent(path.percentAtLength(distance));
        const QPointF delta = samplePoint - point;
        const qreal distanceSquared = delta.x() * delta.x() + delta.y() * delta.y();
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestDistance = distance;
        }
    }
    return bestDistance;
}

QVector<LineDecorationVertexMetric> lineDecorationVertexMetrics(const QPainterPath &path,
                                                                const QVector<MapEditorLineDecorationVertex> &vertices)
{
    QVector<LineDecorationVertexMetric> metrics;
    const qreal pathLength = path.length();
    if (pathLength <= 0.001) {
        return metrics;
    }

    metrics.reserve(vertices.size());
    for (const MapEditorLineDecorationVertex &vertex : vertices) {
        if (!vertex.orientationDegrees.has_value() && !vertex.leftSize.has_value()) {
            continue;
        }

        LineDecorationVertexMetric metric;
        metric.distance = qBound<qreal>(0.0,
                                        vertex.pathDistance.value_or(nearestPathDistanceToPoint(path, vertex.anchor)),
                                        pathLength);
        metric.orientationDegrees = vertex.orientationDegrees;
        metric.leftSize = vertex.leftSize;
        metrics.append(metric);
    }

    std::sort(metrics.begin(), metrics.end(), [](const LineDecorationVertexMetric &a, const LineDecorationVertexMetric &b) {
        return a.distance < b.distance;
    });
    return metrics;
}

qreal interpolationFactor(qreal beforeDistance, qreal afterDistance, qreal distance)
{
    const qreal span = afterDistance - beforeDistance;
    if (std::abs(span) <= 0.001) {
        return 0.0;
    }
    return qBound<qreal>(0.0, (distance - beforeDistance) / span, 1.0);
}

qreal slopeTickLengthAt(const QVector<LineDecorationVertexMetric> &metrics, qreal distance, qreal fallbackLength)
{
    int beforeIndex = -1;
    int afterIndex = -1;
    for (int index = 0; index < metrics.size(); ++index) {
        if (!metrics.at(index).leftSize.has_value()) {
            continue;
        }
        if (metrics.at(index).distance <= distance) {
            beforeIndex = index;
        }
        if (metrics.at(index).distance >= distance) {
            afterIndex = index;
            break;
        }
    }

    if (beforeIndex >= 0 && afterIndex >= 0) {
        const LineDecorationVertexMetric &before = metrics.at(beforeIndex);
        const LineDecorationVertexMetric &after = metrics.at(afterIndex);
        const qreal factor = interpolationFactor(before.distance, after.distance, distance);
        return before.leftSize.value() + ((after.leftSize.value() - before.leftSize.value()) * factor);
    }
    if (beforeIndex >= 0) {
        return metrics.at(beforeIndex).leftSize.value();
    }
    if (afterIndex >= 0) {
        return metrics.at(afterIndex).leftSize.value();
    }
    return fallbackLength;
}

std::optional<QPointF> slopeTickOrientationAt(const QVector<LineDecorationVertexMetric> &metrics, qreal distance)
{
    int beforeIndex = -1;
    int afterIndex = -1;
    for (int index = 0; index < metrics.size(); ++index) {
        if (!metrics.at(index).orientationDegrees.has_value()) {
            continue;
        }
        if (metrics.at(index).distance <= distance) {
            beforeIndex = index;
        }
        if (metrics.at(index).distance >= distance) {
            afterIndex = index;
            break;
        }
    }

    if (beforeIndex < 0 && afterIndex < 0) {
        return std::nullopt;
    }
    if (beforeIndex < 0) {
        return directionForOrientationDegrees(metrics.at(afterIndex).orientationDegrees.value());
    }
    if (afterIndex < 0 || beforeIndex == afterIndex) {
        return directionForOrientationDegrees(metrics.at(beforeIndex).orientationDegrees.value());
    }

    const LineDecorationVertexMetric &before = metrics.at(beforeIndex);
    const LineDecorationVertexMetric &after = metrics.at(afterIndex);
    const qreal factor = interpolationFactor(before.distance, after.distance, distance);
    const QPointF beforeDirection = directionForOrientationDegrees(before.orientationDegrees.value());
    const QPointF afterDirection = directionForOrientationDegrees(after.orientationDegrees.value());
    QPointF direction = beforeDirection * (1.0 - factor) + afterDirection * factor;
    const qreal directionLength = std::hypot(direction.x(), direction.y());
    if (directionLength <= 0.001) {
        return std::nullopt;
    }
    direction /= directionLength;
    return direction;
}

std::optional<LineDecorationSample> lineDecorationSampleAt(const QPainterPath &path,
                                                           qreal distance,
                                                           bool reversed)
{
    const qreal pathLength = path.length();
    if (pathLength <= 0.001) {
        return std::nullopt;
    }

    const qreal clampedDistance = qBound<qreal>(0.0, distance, pathLength);
    const qreal percent = path.percentAtLength(clampedDistance);
    const QPointF point = path.pointAtPercent(percent);
    const qreal tangentStep = qBound<qreal>(0.5, pathLength * 0.002, 4.0);
    const QPointF before = path.pointAtPercent(path.percentAtLength(qMax<qreal>(0.0, clampedDistance - tangentStep)));
    const QPointF after = path.pointAtPercent(path.percentAtLength(qMin<qreal>(pathLength, clampedDistance + tangentStep)));
    QPointF tangent = after - before;
    const qreal tangentLength = std::hypot(tangent.x(), tangent.y());
    if (tangentLength <= 0.001) {
        return std::nullopt;
    }

    tangent /= tangentLength;
    if (reversed) {
        tangent = -tangent;
    }
    QPointF normal(tangent.y(), -tangent.x());
    const qreal normalLength = std::hypot(normal.x(), normal.y());
    if (normalLength <= 0.001) {
        return std::nullopt;
    }
    normal /= normalLength;
    return LineDecorationSample{point, tangent, normal};
}
}

MapEditorLineDecorationItem::MapEditorLineDecorationItem(const QPainterPath &path,
                                                         const QVector<MapEditorLineDecorationStyle> &decorations,
                                                         const QColor &fallbackColor,
                                                         bool reversed,
                                                         int fallbackSeed,
                                                         const QVector<MapEditorLineDecorationVertex> &lineVertices,
                                                         qreal linePointLengthScale,
                                                         QGraphicsItem *parent)
    : QGraphicsPathItem(path, parent)
    , decorations_(decorations)
    , lineVertices_(lineVertices)
    , fallbackColor_(fallbackColor)
    , reversed_(reversed)
    , fallbackSeed_(fallbackSeed)
    , linePointLengthScale_(qMax<qreal>(1e-6, linePointLengthScale))
{
    setPen(Qt::NoPen);
    setBrush(Qt::NoBrush);
    updatePaintBounds();
}

void MapEditorLineDecorationItem::setDecorationPath(const QPainterPath &path)
{
    prepareGeometryChange();
    setPath(path);
    updatePaintBounds();
}

QRectF MapEditorLineDecorationItem::boundingRect() const
{
    return paintBounds_;
}

qreal MapEditorLineDecorationItem::hitDistancePixels(const QPointF &scenePosition,
                                                     const QTransform &viewTransform) const
{
    if (decorations_.isEmpty() || path().isEmpty()) {
        return std::numeric_limits<qreal>::max();
    }

    const QPointF localPosition = mapFromScene(scenePosition);
    const qreal viewScale = itemUnitToViewPixels(this, viewTransform);
    const qreal localTolerance = 5.0 / viewScale;
    if (!paintBounds_.adjusted(-localTolerance, -localTolerance, localTolerance, localTolerance).contains(localPosition)) {
        return std::numeric_limits<qreal>::max();
    }

    const qreal pathLength = path().length();
    if (pathLength <= 0.001) {
        return std::numeric_limits<qreal>::max();
    }

    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (const MapEditorLineDecorationStyle &decoration : decorations_) {
        QVector<qreal> offsets;
        switch (decoration.kind) {
        case MapEditorLineDecorationKind::OffsetStroke:
            offsets.append(decoration.offset);
            break;
        case MapEditorLineDecorationKind::Parallel:
            offsets = decoration.offsets.isEmpty()
                ? QVector<qreal>{decoration.offset}
                : decoration.offsets;
            break;
        default:
            continue;
        }

        const qreal sampleStep = qBound<qreal>(3.0, decoration.spacing / 3.0, 12.0);
        const int sampleCount = qBound(2, static_cast<int>(std::ceil(pathLength / sampleStep)) + 1, 4096);
        for (const qreal offset : offsets) {
            QPointF previousPoint;
            bool hasPreviousPoint = false;
            for (int index = 0; index < sampleCount; ++index) {
                const qreal distance = index == sampleCount - 1
                    ? pathLength
                    : (pathLength * static_cast<qreal>(index) / static_cast<qreal>(sampleCount - 1));
                const std::optional<LineDecorationSample> sample = lineDecorationSampleAt(path(), distance, reversed_);
                if (!sample.has_value()) {
                    continue;
                }
                const QPointF point = sample->point + (sample->normal * offset);
                if (hasPreviousPoint) {
                    bestDistance = qMin(bestDistance, distanceToSegment(localPosition, previousPoint, point));
                }
                previousPoint = point;
                hasPreviousPoint = true;
            }
        }
    }

    if (bestDistance == std::numeric_limits<qreal>::max()) {
        return std::numeric_limits<qreal>::max();
    }

    const qreal bestDistancePixels = bestDistance * viewScale;
    return bestDistancePixels <= 5.0 ? bestDistancePixels : std::numeric_limits<qreal>::max();
}

void MapEditorLineDecorationItem::paint(QPainter *painter,
                                        const QStyleOptionGraphicsItem *option,
                                        QWidget *widget)
{
    Q_UNUSED(widget);
    if (painter == nullptr || decorations_.isEmpty() || path().isEmpty()) {
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
    drawDecorations(painter, zoomOutScale);

    const bool selected = (option != nullptr && (option->state & QStyle::State_Selected))
        || data(kMapSceneInteractionSelectionRole).toBool();
    const bool hovered = data(kMapSceneInteractionHoverRole).toBool();
    if (selected || hovered) {
        interactionColorOverride_ = selected
            ? mapEditorInteractionSelectionColor()
            : mapEditorInteractionHoverColor();
        interactionColorOverride_->setAlpha(selected ? 235 : 220);
        drawDecorations(painter, zoomOutScale);
        interactionColorOverride_.reset();
    }
    painter->restore();
}

void MapEditorLineDecorationItem::drawDecorations(QPainter *painter, qreal zoomOutScale)
{
    if (painter == nullptr) {
        return;
    }

    for (const MapEditorLineDecorationStyle &decoration : decorations_) {
        switch (decoration.kind) {
        case MapEditorLineDecorationKind::OffsetStroke:
            drawOffsetStroke(painter, decoration, decoration.offset, zoomOutScale);
            break;
        case MapEditorLineDecorationKind::Parallel: {
            const QVector<qreal> offsets = decoration.offsets.isEmpty()
                ? QVector<qreal>{decoration.offset}
                : decoration.offsets;
            for (const qreal offset : offsets) {
                drawOffsetStroke(painter, decoration, offset, zoomOutScale);
            }
            break;
        }
        case MapEditorLineDecorationKind::Ticks:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Ticks);
            break;
        case MapEditorLineDecorationKind::Rungs:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Rungs);
            break;
        case MapEditorLineDecorationKind::Teeth:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Teeth);
            break;
        case MapEditorLineDecorationKind::Symbols:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Symbols);
            break;
        case MapEditorLineDecorationKind::Waves:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Waves);
            break;
        case MapEditorLineDecorationKind::SlopeTicks:
            drawSlopeTicks(painter, decoration, zoomOutScale);
            break;
        }
    }
}

void MapEditorLineDecorationItem::updatePaintBounds()
{
    qreal padding = lineDecorationPadding(decorations_);
    for (const MapEditorLineDecorationStyle &decoration : decorations_) {
        if (decoration.kind != MapEditorLineDecorationKind::SlopeTicks) {
            continue;
        }

        qreal lengthExtent = decoration.length * linePointLengthScale_;
        for (const MapEditorLineDecorationVertex &vertex : lineVertices_) {
            if (vertex.leftSize.has_value()) {
                lengthExtent = qMax(lengthExtent, vertex.leftSize.value() * linePointLengthScale_);
            }
        }
        padding = qMax(padding,
                       std::abs(decoration.offset)
                           + lengthExtent
                           + decoration.offsetJitter
                           + decoration.strokeWidth
                           + 4.0);
    }
    paintBounds_ = path().controlPointRect().adjusted(-padding, -padding, padding, padding);
}

void MapEditorLineDecorationItem::drawOffsetStroke(QPainter *painter,
                                                   const MapEditorLineDecorationStyle &decoration,
                                                   qreal offset,
                                                   qreal zoomOutScale)
{
    const qreal pathLength = path().length();
    if (painter == nullptr || pathLength <= 0.001) {
        return;
    }

    QVector<QPointF> points;
    const qreal sampleStep = qBound<qreal>(3.0, decoration.spacing / 3.0, 12.0);
    const int sampleCount = qBound(2, static_cast<int>(std::ceil(pathLength / sampleStep)) + 1, 4096);
    points.reserve(sampleCount);
    for (int index = 0; index < sampleCount; ++index) {
        const qreal distance = index == sampleCount - 1
            ? pathLength
            : (pathLength * static_cast<qreal>(index) / static_cast<qreal>(sampleCount - 1));
        if (const std::optional<LineDecorationSample> sample = lineDecorationSampleAt(path(), distance, reversed_)) {
            points.append(sample->point + (sample->normal * offset));
        }
    }
    if (points.size() < 2) {
        return;
    }

    painter->setPen(lineDecorationPen(decoration, fallbackColor_, zoomOutScale, interactionColorOverride_));
    painter->setBrush(Qt::NoBrush);
    painter->drawPolyline(QPolygonF(points));
}

void MapEditorLineDecorationItem::drawRepeated(QPainter *painter,
                                               const MapEditorLineDecorationStyle &decoration,
                                               qreal zoomOutScale,
                                               DrawMode mode)
{
    const qreal pathLength = path().length();
    if (painter == nullptr || pathLength <= 0.001) {
        return;
    }

    const LineDecorationMarkerDistances markers = repeatedMarkerDistances(pathLength, decoration);
    if (markers.distances.isEmpty()) {
        return;
    }

    painter->setPen(lineDecorationPen(decoration, fallbackColor_, zoomOutScale, interactionColorOverride_));
    const int seed = decoration.seed.value_or(fallbackSeed_);
    for (int markerIndex = 0; markerIndex < markers.distances.size(); ++markerIndex) {
        const qreal distance = lineDecorationEffectiveDistance(markers.distances.at(markerIndex),
                                                               pathLength,
                                                               markers.jitterSpacing,
                                                               decoration,
                                                               markerIndex,
                                                               seed);
        const std::optional<LineDecorationSample> sample = lineDecorationSampleAt(path(), distance, reversed_);
        if (!sample.has_value()) {
            continue;
        }

        switch (mode) {
        case DrawMode::Ticks:
            drawTick(painter, decoration, sample->point, sample->normal, markerIndex);
            break;
        case DrawMode::Rungs:
            drawRung(painter, decoration, sample->point, sample->normal);
            break;
        case DrawMode::Teeth:
            drawTooth(painter, decoration, distance, markers.jitterSpacing, markerIndex);
            break;
        case DrawMode::Symbols:
            drawSymbol(painter, decoration, sample->point, sample->tangent, sample->normal, markerIndex, zoomOutScale);
            break;
        case DrawMode::Waves:
            drawWave(painter, decoration, sample->point, sample->tangent, sample->normal, markerIndex);
            break;
        }
    }
}

void MapEditorLineDecorationItem::drawTick(QPainter *painter,
                                           const MapEditorLineDecorationStyle &decoration,
                                           const QPointF &point,
                                           const QPointF &normal,
                                           int markerIndex)
{
    const int seed = decoration.seed.value_or(fallbackSeed_);
    const QPointF base = point + (normal * lineDecorationEffectiveOffset(decoration, markerIndex, seed));
    const qreal sideFactor = lineDecorationSideFactor(decoration.side);
    if (decoration.side == MapEditorLineDecorationSide::Center) {
        const QPointF half = normal * (decoration.length / 2.0);
        painter->drawLine(QLineF(base - half, base + half));
    } else {
        painter->drawLine(QLineF(base, base + (normal * sideFactor * decoration.length)));
    }
}

void MapEditorLineDecorationItem::drawRung(QPainter *painter,
                                           const MapEditorLineDecorationStyle &decoration,
                                           const QPointF &point,
                                           const QPointF &normal)
{
    const qreal fromOffset = decoration.fromOffset.value_or(-decoration.length / 2.0);
    const qreal toOffset = decoration.toOffset.value_or(decoration.length / 2.0);
    painter->drawLine(QLineF(point + (normal * fromOffset),
                             point + (normal * toOffset)));
}

void MapEditorLineDecorationItem::drawTooth(QPainter *painter,
                                            const MapEditorLineDecorationStyle &decoration,
                                            qreal centerDistance,
                                            qreal markerStep,
                                            int markerIndex)
{
    const qreal pathLength = path().length();
    const qreal sideFactor = lineDecorationSideFactor(decoration.side);
    if (painter == nullptr || pathLength <= 0.001 || std::abs(sideFactor) <= 0.001) {
        return;
    }

    const int seed = decoration.seed.value_or(fallbackSeed_);
    const qreal effectiveOffset = lineDecorationEffectiveOffset(decoration, markerIndex, seed);
    const qreal halfStep = qMax<qreal>(0.001, markerStep * 0.5);
    const qreal startDistance = qBound<qreal>(0.0, centerDistance - halfStep, pathLength);
    const qreal endDistance = qBound<qreal>(0.0, centerDistance + halfStep, pathLength);
    if (endDistance - startDistance <= 0.001) {
        return;
    }

    QPainterPath toothPath;
    bool hasBase = false;
    const int baseSampleCount = qBound(2,
                                       static_cast<int>(std::ceil((endDistance - startDistance) / 4.0)) + 1,
                                       16);
    for (int sampleIndex = 0; sampleIndex < baseSampleCount; ++sampleIndex) {
        const qreal distance = sampleIndex == baseSampleCount - 1
            ? endDistance
            : startDistance + ((endDistance - startDistance)
                               * static_cast<qreal>(sampleIndex)
                               / static_cast<qreal>(baseSampleCount - 1));
        const std::optional<LineDecorationSample> sample = lineDecorationSampleAt(path(), distance, reversed_);
        if (!sample.has_value()) {
            continue;
        }

        const QPointF basePoint = sample->point + (sample->normal * effectiveOffset);
        if (!hasBase) {
            toothPath.moveTo(basePoint);
            hasBase = true;
        } else {
            toothPath.lineTo(basePoint);
        }
    }

    const std::optional<LineDecorationSample> centerSample = lineDecorationSampleAt(path(), centerDistance, reversed_);
    if (!hasBase || !centerSample.has_value()) {
        return;
    }

    const QPointF baseCenter = centerSample->point + (centerSample->normal * effectiveOffset);
    const QPointF tip = baseCenter + (centerSample->normal * sideFactor * decoration.size);
    toothPath.lineTo(tip);
    toothPath.closeSubpath();
    painter->setBrush(QBrush(decoration.fillColor.value_or(decoration.strokeColor.value_or(fallbackColor_))));
    painter->drawPath(toothPath);
    painter->setBrush(Qt::NoBrush);
}

void MapEditorLineDecorationItem::drawSymbol(QPainter *painter,
                                             const MapEditorLineDecorationStyle &decoration,
                                             const QPointF &point,
                                             const QPointF &tangent,
                                             const QPointF &normal,
                                             int markerIndex,
                                             qreal zoomOutScale)
{
    const int seed = decoration.seed.value_or(fallbackSeed_);
    const qreal sizeJitter = decoration.sizeJitter <= 0.0
        ? 0.0
        : stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x53acU) * decoration.sizeJitter;
    const qreal symbolSize = qBound<qreal>(0.6, decoration.size + sizeJitter, 128.0);
    const qreal offset = lineDecorationEffectiveOffset(decoration, markerIndex, seed);
    const QPointF center = point + (normal * offset);
    const qreal tangentAngle = std::atan2(tangent.y(), tangent.x()) * 180.0 / 3.14159265358979323846;
    const qreal jitterAngle = decoration.angleJitter <= 0.0
        ? 0.0
        : stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0xa174U) * decoration.angleJitter;
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(tangentAngle + decoration.angle + jitterAngle);
    transform.translate(-center.x(), -center.y());

    const QColor strokeColor = decoration.strokeColor.value_or(fallbackColor_);
    const auto drawPath = [&](const QPainterPath &path,
                              bool usesFill,
                              const std::optional<QColor> &partStrokeColor,
                              const std::optional<QColor> &partFillColor,
                              const std::optional<qreal> &partStrokeWidth) {
        QPen pen(partStrokeColor.value_or(strokeColor),
                 qMax<qreal>(0.15, partStrokeWidth.value_or(decoration.strokeWidth) * zoomOutScale),
                 decoration.strokeStyle,
                 Qt::RoundCap,
                 Qt::RoundJoin);
        pen.setCosmetic(true);
        if (!decoration.dashPattern.isEmpty()) {
            pen.setStyle(Qt::CustomDashLine);
            pen.setDashPattern(decoration.dashPattern);
        }
        painter->setPen(pen);
        const std::optional<QColor> fillColor = partFillColor.has_value()
            ? partFillColor
            : decoration.fillColor;
        if (usesFill && fillColor.has_value()) {
            painter->setBrush(QBrush(fillColor.value()));
        } else {
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawPath(transform.map(path));
    };

    const QRectF symbolRect(center.x() - (symbolSize / 2.0),
                            center.y() - (symbolSize / 2.0),
                            symbolSize,
                            symbolSize);
    if (decoration.symbolParts.isEmpty()) {
        drawPath(mapEditorPointSymbolPath(decoration.symbol, symbolRect),
                 mapEditorPointSymbolUsesFill(decoration.symbol),
                 std::nullopt,
                 std::nullopt,
                 std::nullopt);
    } else {
        for (const MapEditorPointSymbolPart &part : decoration.symbolParts) {
            drawPath(mapEditorPointSymbolPartPath(part, symbolRect, qMax<qreal>(0.001, decoration.size)),
                     mapEditorSymbolPartUsesFill(part),
                     part.strokeColor,
                     part.fillColor,
                     part.strokeWidth);
        }
    }
    painter->setBrush(Qt::NoBrush);
}

void MapEditorLineDecorationItem::drawWave(QPainter *painter,
                                           const MapEditorLineDecorationStyle &decoration,
                                           const QPointF &point,
                                           const QPointF &tangent,
                                           const QPointF &normal,
                                           int markerIndex)
{
    const int seed = decoration.seed.value_or(fallbackSeed_);
    const QPointF center = point + (normal * lineDecorationEffectiveOffset(decoration, markerIndex, seed));
    const qreal halfLength = qMax<qreal>(0.5, decoration.length * 0.5);
    const qreal amplitude = qMax<qreal>(0.2, decoration.size);
    const QPointF start = center - (tangent * halfLength);
    const QPointF middle = center;
    const QPointF end = center + (tangent * halfLength);
    const QPointF quarter = tangent * (halfLength * 0.5);
    const QPointF crest = normal * amplitude;
    const QPointF trough = normal * -amplitude;

    QPainterPath wave;
    wave.moveTo(start);
    wave.cubicTo(start + quarter * 0.65,
                 middle - quarter + crest,
                 middle);
    wave.cubicTo(middle + quarter + trough,
                 end - quarter * 0.65,
                 end);
    painter->drawPath(wave);
}

void MapEditorLineDecorationItem::drawSlopeTicks(QPainter *painter,
                                                 const MapEditorLineDecorationStyle &decoration,
                                                 qreal zoomOutScale)
{
    const qreal pathLength = path().length();
    if (painter == nullptr || pathLength <= 0.001) {
        return;
    }

    const LineDecorationMarkerDistances markers = slopeTickMarkerDistances(pathLength, decoration);
    if (markers.distances.isEmpty()) {
        return;
    }
    const int seed = decoration.seed.value_or(fallbackSeed_);
    const QVector<LineDecorationVertexMetric> metrics = lineDecorationVertexMetrics(path(), lineVertices_);

    painter->setPen(lineDecorationPen(decoration, fallbackColor_, zoomOutScale, interactionColorOverride_));
    painter->setBrush(Qt::NoBrush);
    for (int markerIndex = 0; markerIndex < markers.distances.size(); ++markerIndex) {
        const qreal baseDistance = markers.distances.at(markerIndex);
        const qreal distance = lineDecorationEffectiveDistance(baseDistance,
                                                               pathLength,
                                                               markers.jitterSpacing,
                                                               decoration,
                                                               markerIndex,
                                                               seed);
        const std::optional<LineDecorationSample> sample = lineDecorationSampleAt(path(), distance, reversed_);
        if (!sample.has_value()) {
            continue;
        }

        QPointF direction = slopeTickOrientationAt(metrics, distance).value_or(sample->normal);
        const qreal directionLength = std::hypot(direction.x(), direction.y());
        if (directionLength <= 0.001) {
            continue;
        }
        direction /= directionLength;

        qreal offset = decoration.offset;
        if (decoration.offsetJitter > 0.0) {
            offset += stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x91f2U) * decoration.offsetJitter;
        }
        const QPointF base = sample->point + (sample->normal * offset);
        const qreal sourceLength = qMax<qreal>(0.1, slopeTickLengthAt(metrics, distance, decoration.length));
        const qreal alternatingScale = markerIndex % 2 == 0 ? 1.0 : decoration.alternateLengthScale;
        const qreal displayLength = sourceLength * alternatingScale * linePointLengthScale_;
        painter->drawLine(QLineF(base, base + direction * displayLength));
    }
}
}
