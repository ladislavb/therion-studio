#include "MapEditorLineDecorationItem.h"
#include "MapEditorSceneItems.h"

#include "MapEditorDirectionAidItem.h"
#include "MapEditorPointSymbolGeometry.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorPerformanceCounters.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QPen>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <set>

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

std::optional<QPair<qreal, qreal>> clippedSegmentFractions(const QPointF &start,
                                                          const QPointF &end,
                                                          const QRectF &clip)
{
    qreal first = 0.0;
    qreal last = 1.0;
    const QPointF delta = end - start;
    const auto intersect = [&](qreal origin, qreal direction, qreal minimum, qreal maximum) {
        if (std::abs(direction) < 1e-9) {
            return origin >= minimum && origin <= maximum;
        }
        const qreal a = (minimum - origin) / direction;
        const qreal b = (maximum - origin) / direction;
        first = qMax(first, qMin(a, b));
        last = qMin(last, qMax(a, b));
        return first <= last;
    };
    if (!intersect(start.x(), delta.x(), clip.left(), clip.right())
        || !intersect(start.y(), delta.y(), clip.top(), clip.bottom())) {
        return std::nullopt;
    }
    return QPair<qreal, qreal>{first, last};
}

QPointF polylinePointAt(const QPolygonF &points, const QVector<qreal> &distances, qreal distance)
{
    const qreal clamped = qBound<qreal>(0.0, distance, distances.last());
    const auto end = std::lower_bound(distances.cbegin() + 1, distances.cend(), clamped);
    const int index = qMin<int>(std::distance(distances.cbegin(), end), points.size() - 1);
    const qreal segmentLength = distances[index] - distances[index - 1];
    const qreal fraction = (clamped - distances[index - 1]) / qMax<qreal>(1e-9, segmentLength);
    return points[index - 1] + (points[index] - points[index - 1]) * fraction;
}

QList<QPolygonF> projectedLinePolygons(const QPainterPath &path, const QTransform &projection)
{
    const QPainterPath projected = projection.map(path);
    QPainterPath flattenedInput;
    const auto appendCubic = [&](auto &&self, const QPointF &a, const QPointF &b,
                                 const QPointF &c, const QPointF &d, int depth) -> void {
        if (depth == 0) {
            flattenedInput.cubicTo(b, c, d);
            return;
        }
        const QPointF ab = (a + b) * 0.5;
        const QPointF bc = (b + c) * 0.5;
        const QPointF cd = (c + d) * 0.5;
        const QPointF abc = (ab + bc) * 0.5;
        const QPointF bcd = (bc + cd) * 0.5;
        const QPointF midpoint = (abc + bcd) * 0.5;
        self(self, a, ab, abc, midpoint, depth - 1);
        self(self, midpoint, bcd, cd, d, depth - 1);
    };
    for (int index = 0; index < projected.elementCount(); ++index) {
        const auto element = projected.elementAt(index);
        const QPointF point(element.x, element.y);
        if (element.isMoveTo()) {
            flattenedInput.moveTo(point);
        } else if (element.isLineTo()) {
            flattenedInput.lineTo(point);
        } else if (element.type == QPainterPath::CurveToElement && index + 2 < projected.elementCount()) {
            const auto control = projected.elementAt(index + 1);
            const auto end = projected.elementAt(index + 2);
            // Keep Qt's flattening recursion ceiling from reducing far-zoomed curve accuracy.
            appendCubic(appendCubic, flattenedInput.currentPosition(), point,
                        QPointF(control.x, control.y), QPointF(end.x, end.y), 2);
            index += 2;
        }
    }
    return flattenedInput.toSubpathPolygons();
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
        qreal symbolExtent = decoration.size + decoration.sizeJitter;
        qreal strokeWidth = decoration.strokeWidth;
        if (decoration.kind == MapEditorLineDecorationKind::Symbols) {
            const qreal size = qMax<qreal>(0.6, symbolExtent);
            const QRectF rect(-size * 0.5, -size * 0.5, size, size);
            for (const auto &part : decoration.symbolParts) {
                const QRectF bounds = mapEditorPointSymbolPartPath(part, rect,
                    qMax<qreal>(0.001, decoration.size)).controlPointRect();
                symbolExtent = qMax(symbolExtent, std::hypot(
                    qMax(std::abs(bounds.left()), std::abs(bounds.right())),
                    qMax(std::abs(bounds.top()), std::abs(bounds.bottom()))));
                strokeWidth = qMax(strokeWidth, part.strokeWidth.value_or(decoration.strokeWidth));
            }
        }
        padding = qMax(padding,
                       offsetExtent
                           + symbolExtent
                           + decoration.length
                           + decoration.offsetJitter
                           + strokeWidth
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

QVector<MapEditorLineDecorationVertexMetric> lineDecorationVertexMetrics(const QPainterPath &path,
                                                                const QVector<MapEditorLineDecorationVertex> &vertices)
{
    QVector<MapEditorLineDecorationVertexMetric> metrics;
    const qreal pathLength = path.length();
    if (pathLength <= 0.001) {
        return metrics;
    }

    metrics.reserve(vertices.size());
    for (const MapEditorLineDecorationVertex &vertex : vertices) {
        if (!vertex.orientationDegrees.has_value() && !vertex.leftSize.has_value()) {
            continue;
        }

        MapEditorLineDecorationVertexMetric metric;
        metric.distance = qBound<qreal>(0.0,
                                        vertex.pathDistance.value_or(nearestPathDistanceToPoint(path, vertex.anchor)),
                                        pathLength);
        metric.orientationDegrees = vertex.orientationDegrees;
        metric.leftSize = vertex.leftSize;
        metrics.append(metric);
    }

    std::sort(metrics.begin(), metrics.end(), [](const MapEditorLineDecorationVertexMetric &a, const MapEditorLineDecorationVertexMetric &b) {
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

qreal slopeTickLengthAt(const QVector<MapEditorLineDecorationVertexMetric> &metrics, qreal distance, qreal fallbackLength)
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
        const MapEditorLineDecorationVertexMetric &before = metrics.at(beforeIndex);
        const MapEditorLineDecorationVertexMetric &after = metrics.at(afterIndex);
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

std::optional<QPointF> slopeTickOrientationAt(const QVector<MapEditorLineDecorationVertexMetric> &metrics, qreal distance)
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

    const MapEditorLineDecorationVertexMetric &before = metrics.at(beforeIndex);
    const MapEditorLineDecorationVertexMetric &after = metrics.at(afterIndex);
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
    cachedPathLength_ = -1.0;
    cachedSourceMetricsValid_ = false;
    for (auto &entry : projectedPathCache_) {
        entry.valid = false;
    }
    updatePaintBounds();
}

void MapEditorLineDecorationItem::setDecorationPath(
    const QPainterPath &path, const QVector<MapEditorLineDecorationVertex> &lineVertices)
{
    lineVertices_ = lineVertices;
    setDecorationPath(path);
}

bool MapEditorLineDecorationItem::usesLineVertexMetrics() const
{
    return std::any_of(decorations_.cbegin(), decorations_.cend(), [](const auto &decoration) {
        return decoration.kind == MapEditorLineDecorationKind::SlopeTicks;
    });
}

QRectF MapEditorLineDecorationItem::boundingRect() const
{
    return paintBounds_;
}

QRectF MapEditorLineDecorationItem::sourceGeometryBounds() const
{
    qreal extent = 0.0;
    if (usesLineVertexMetrics()) {
        for (const auto &vertex : lineVertices_) {
            extent = qMax(extent, std::abs(vertex.leftSize.value_or(0)) * linePointLengthScale_);
        }
    }
    return path().controlPointRect().adjusted(-extent, -extent, extent, extent);
}

void MapEditorLineDecorationItem::updateViewTransform(const QTransform &viewTransform)
{
    bool invertible = false;
    const QTransform inverse = deviceTransform(viewTransform).inverted(&invertible);
    if (!invertible) {
        return;
    }
    const qreal paddingScale = qMax(std::hypot(inverse.m11(), inverse.m21()),
                                    std::hypot(inverse.m12(), inverse.m22()));
    if (!qFuzzyCompare(paddingScale, screenPaddingScale_)) {
        prepareGeometryChange();
        screenPaddingScale_ = paddingScale;
        updatePaintBounds();
    }
}

void updateMapEditorScreenSpaceMetrics(QGraphicsScene *scene, const QTransform &viewTransform)
{
    if (scene == nullptr) {
        return;
    }
    ++mapEditorPerformanceCounters().screenSpaceMetricPasses;
    for (QGraphicsItem *item : unsortedSceneItems(scene)) {
        if (auto *decoration = dynamic_cast<MapEditorLineDecorationItem *>(item)) {
            decoration->updateViewTransform(viewTransform);
        } else if (auto *direction = dynamic_cast<MapEditorDirectionAidItem *>(item)) {
            direction->updateViewTransform(viewTransform);
        } else if (auto *lineHandle = dynamic_cast<MapLinePointSizeHandleItem *>(item)) {
            lineHandle->updateViewTransform(viewTransform);
        } else if (auto *pointHandle = dynamic_cast<MapPointOrientationHandleItem *>(item)) {
            pointHandle->updateViewTransform(viewTransform);
        }
    }
}

qreal MapEditorLineDecorationItem::cachedPathLength() const
{
    if (cachedPathLength_ < 0.0) {
        cachedPathLength_ = path().length();
    }
    return cachedPathLength_;
}

const QVector<MapEditorLineDecorationVertexMetric> &MapEditorLineDecorationItem::cachedSourceMetrics() const
{
    if (!cachedSourceMetricsValid_) {
        ++mapEditorPerformanceCounters().lineDecorationSourceMetricComputations;
        cachedSourceMetrics_ = lineDecorationVertexMetrics(path(), lineVertices_);
        cachedSourceMetricsValid_ = true;
    }
    return cachedSourceMetrics_;
}

const MapEditorLineDecorationItem::ProjectedPathCacheEntry &MapEditorLineDecorationItem::projectedPolygonsForProjection(
    const QTransform &projection) const
{
    const QPainterPath currentPath = path();
    if (projectedPathSource_ != currentPath) {
        for (auto &entry : projectedPathCache_) {
            entry.valid = false;
        }
        projectedPathSource_ = currentPath;
    }
    for (std::size_t index = 0; index < projectedPathCache_.size(); ++index) {
        const auto &entry = projectedPathCache_[index];
        const bool sameLinearTransform = entry.transform.isAffine() && projection.isAffine()
            && entry.transform.m11() == projection.m11() && entry.transform.m12() == projection.m12()
            && entry.transform.m21() == projection.m21() && entry.transform.m22() == projection.m22();
        if (entry.valid && (sameLinearTransform || entry.transform == projection)) {
            mostRecentProjection_ = index;
            return entry;
        }
    }

    // Keep the main view and magnifier independently warm. Translation is
    // applied at paint/hit-test time, without copying or re-flattening curves.
    mostRecentProjection_ = 1 - mostRecentProjection_;
    auto &entry = projectedPathCache_[mostRecentProjection_];
    ++mapEditorPerformanceCounters().lineDecorationProjectionComputations;
    entry.polygons = projectedLinePolygons(currentPath, projection);
    entry.cumulativeDistances.clear();
    entry.cumulativeDistances.reserve(entry.polygons.size());
    for (const QPolygonF &points : entry.polygons) {
        QVector<qreal> distances;
        if (!points.isEmpty()) {
            distances.reserve(points.size());
            distances.append(0.0);
            for (int index = 1; index < points.size(); ++index) {
                const QPointF delta = points[index] - points[index - 1];
                distances.append(distances.last() + std::hypot(delta.x(), delta.y()));
            }
        }
        entry.cumulativeDistances.append(std::move(distances));
    }
    entry.transform = projection;
    entry.valid = true;
    return entry;
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

    const QTransform projection = deviceTransform(viewTransform);
    const auto &cached = projectedPolygonsForProjection(projection);
    const QPointF projectionOffset(projection.dx() - cached.transform.dx(),
                                    projection.dy() - cached.transform.dy());
    const QPointF screenPosition = viewTransform.map(scenePosition) - projectionOffset;
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

        for (const qreal offset : offsets) {
            for (const auto &points : cached.polygons) {
                for (int index = 1; index < points.size(); ++index) {
                    const QPointF tangent = points[index] - points[index - 1];
                    const qreal length = std::hypot(tangent.x(), tangent.y());
                    const QPointF normal = QPointF(tangent.y(), -tangent.x())
                        * (reversed_ ? -1 : 1) / qMax<qreal>(1e-9, length);
                    bestDistance = qMin(bestDistance, distanceToSegment(screenPosition,
                        points[index - 1] + normal * offset, points[index] + normal * offset));
                }
            }
        }
    }

    if (bestDistance == std::numeric_limits<qreal>::max()) {
        return std::numeric_limits<qreal>::max();
    }

    return bestDistance <= 5.0 ? bestDistance : std::numeric_limits<qreal>::max();
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
    const QTransform projection = painter->worldTransform();
    const auto &cached = projectedPolygonsForProjection(projection);
    const QPointF projectionOffset(projection.dx() - cached.transform.dx(),
                                    projection.dy() - cached.transform.dy());
    drawDecorations(painter, zoomOutScale, projection, projectionOffset, cached.polygons, cached.cumulativeDistances);

    const bool selected = (option != nullptr && (option->state & QStyle::State_Selected))
        || data(kMapSceneInteractionSelectionRole).toBool();
    const bool hovered = data(kMapSceneInteractionHoverRole).toBool();
    if (selected || hovered) {
        interactionColorOverride_ = selected
            ? mapEditorInteractionSelectionColor()
            : mapEditorInteractionHoverColor();
        interactionColorOverride_->setAlpha(selected ? 235 : 220);
        drawDecorations(painter, zoomOutScale, projection, projectionOffset, cached.polygons, cached.cumulativeDistances);
        interactionColorOverride_.reset();
    }
    painter->restore();
}

void MapEditorLineDecorationItem::drawDecorations(
    QPainter *painter, qreal zoomOutScale, const QTransform &projection, const QPointF &projectionOffset,
    const QList<QPolygonF> &polygons, const QVector<QVector<qreal>> &cumulativeDistances)
{
    if (painter == nullptr) {
        return;
    }

    for (const MapEditorLineDecorationStyle &decoration : decorations_) {
        drawScreenDecoration(painter, decoration, zoomOutScale, projection, projectionOffset, polygons, cumulativeDistances);
    }
}

void MapEditorLineDecorationItem::drawScreenDecoration(
    QPainter *painter, const MapEditorLineDecorationStyle &decoration, qreal zoomOutScale,
    const QTransform &projection, const QPointF &projectionOffset, const QList<QPolygonF> &polygons,
    const QVector<QVector<qreal>> &cumulativeDistances)
{
    const QVector<MapEditorLineDecorationVertexMetric> emptySourceMetrics;
    const auto &sourceMetrics = decoration.kind == MapEditorLineDecorationKind::SlopeTicks
        ? cachedSourceMetrics()
        : emptySourceMetrics;
    const bool sourceLengths = std::any_of(sourceMetrics.cbegin(), sourceMetrics.cend(),
        [](const auto &metric) { return metric.leftSize.has_value(); });
    const QRectF deviceBounds(0.0, 0.0,
                             painter->device()->width() / painter->device()->devicePixelRatioF(),
                             painter->device()->height() / painter->device()->devicePixelRatioF());
    const QRectF deviceExposedBounds = painter->hasClipping()
        ? deviceBounds.intersected(painter->worldTransform().mapRect(painter->clipBoundingRect()))
        : deviceBounds;
    const QRectF exposedBounds = deviceExposedBounds.translated(-projectionOffset);
    const qreal side = lineDecorationSideFactor(decoration.side);
    if (decoration.kind == MapEditorLineDecorationKind::Teeth && std::abs(side) <= 0.001) {
        return;
    }
    painter->save();
    painter->resetTransform();
    painter->translate(projectionOffset);
    painter->setPen(lineDecorationPen(decoration, fallbackColor_, zoomOutScale, interactionColorOverride_));
    painter->setBrush(Qt::NoBrush);
    const int seed = decoration.seed.value_or(fallbackSeed_);
    const qreal totalPathLength = qMax<qreal>(1e-9, cachedPathLength());
    for (qsizetype polylineIndex = 0; polylineIndex < polygons.size(); ++polylineIndex) {
        const QPolygonF &points = polygons.at(polylineIndex);
        const QVector<qreal> &distances = cumulativeDistances.at(polylineIndex);
        if (points.size() < 2) {
            continue;
        }
        const qreal length = distances.last();
        if (length <= 0.001) {
            continue;
        }
        const qreal spacing = qMax<qreal>(1.0, decoration.spacing);
        const bool slope = decoration.kind == MapEditorLineDecorationKind::SlopeTicks;
        qreal edge = 0.0;
        if (slope && decoration.adjustSpacing) {
            const qreal unit = spacing / 1.4;
            edge = length > 3 * unit ? 0.3 * unit : length > unit ? 0.1 * unit : 0.0;
        }
        const qreal drawableLength = length - 2 * edge;
        const qreal step = decoration.adjustSpacing
            ? lineDecorationAdjustedStep(drawableLength, spacing) / qMax<qreal>(1.0, decoration.spacingDivisor)
            : spacing;
        const qreal startDistance = slope && decoration.adjustSpacing ? edge : step * 0.5;
        const qreal count = slope && decoration.adjustSpacing
            ? std::floor(drawableLength / step + 0.001) + 1
            : qMax<qreal>(slope ? 1 : 0, std::floor(length / step));
        const qreal lengthScale = length / totalPathLength;
        qreal sourceExtent = 0.0;
        if (sourceLengths) {
            for (const auto &metric : sourceMetrics) {
                sourceExtent = qMax(sourceExtent, std::abs(metric.leftSize.value_or(0)) * linePointLengthScale_ * lengthScale);
            }
        }
        qreal margin = lineDecorationPadding({decoration}) + step
            + decoration.distanceJitter + sourceExtent;
        margin = qMax(margin, std::abs(decoration.fromOffset.value_or(0))
                            + std::abs(decoration.toOffset.value_or(0)) + decoration.strokeWidth);
        const QRectF clip = exposedBounds.adjusted(-margin, -margin, margin, margin);
        const auto normalAt = [&](qreal distance) {
            const QPointF before = polylinePointAt(points, distances, distance - 0.5);
            const QPointF after = polylinePointAt(points, distances, distance + 0.5);
            const QPointF tangent = after - before;
            const qreal tangentLength = std::hypot(tangent.x(), tangent.y());
            const qreal direction = reversed_ ? -1.0 : 1.0;
            return QPointF(tangent.y(), -tangent.x()) * direction / qMax<qreal>(1e-9, tangentLength);
        };
        const bool stroke = decoration.kind == MapEditorLineDecorationKind::OffsetStroke
            || decoration.kind == MapEditorLineDecorationKind::Parallel;
        const QVector<qreal> offsets = decoration.kind == MapEditorLineDecorationKind::Parallel
            && !decoration.offsets.isEmpty() ? decoration.offsets : QVector<qreal>{decoration.offset};
        if (stroke) {
            const QPen originalPen = painter->pen();
            QVector<qreal> dashLengths;
            qreal dashPeriod = 0.0;
            if (originalPen.style() != Qt::SolidLine && originalPen.style() != Qt::NoPen) {
                for (qreal dash : originalPen.dashPattern()) {
                    const qreal dashLength = dash * qMax<qreal>(0.15, decoration.strokeWidth);
                    dashLengths.append(dashLength);
                    dashPeriod += dashLength;
                }
                QPen solidPen = originalPen;
                solidPen.setStyle(Qt::SolidLine);
                painter->setPen(solidPen);
            }
            for (qreal offset : offsets) {
                qreal cumulativeLength = 0.0;
                for (int index = 1; index < points.size(); ++index) {
                    const QPointF a = points[index - 1] + normalAt(distances[index - 1]) * offset;
                    const QPointF b = points[index] + normalAt(distances[index]) * offset;
                    const qreal segmentLength = QLineF(a, b).length();
                    const auto fractions = clippedSegmentFractions(a, b, clip);
                    if (fractions) {
                        const qreal start = cumulativeLength + segmentLength * fractions->first;
                        const qreal end = cumulativeLength + segmentLength * fractions->second;
                        const auto pointAt = [&](qreal distance) {
                            return a + (b - a) * ((distance - cumulativeLength)
                                / qMax<qreal>(1e-9, segmentLength));
                        };
                        if (dashLengths.isEmpty()) {
                            painter->drawLine(QLineF(pointAt(start), pointAt(end)));
                        } else {
                            // Sample only visible dashes; native cosmetic dash clipping can reset
                            // phase and spend time stroking millions of offscreen device pixels.
                            qreal phase = std::fmod(start, dashPeriod);
                            int dashIndex = 0;
                            while (phase >= dashLengths[dashIndex]) {
                                phase -= dashLengths[dashIndex];
                                dashIndex = (dashIndex + 1) % dashLengths.size();
                            }
                            qreal cursor = 0.0;
                            const qreal span = end - start;
                            qreal remaining = dashLengths[dashIndex] - phase;
                            while (cursor < span) {
                                const qreal next = qMin(span, cursor + remaining);
                                if (dashIndex % 2 == 0) {
                                    painter->drawLine(QLineF(pointAt(start + cursor), pointAt(start + next)));
                                }
                                cursor = next;
                                dashIndex = (dashIndex + 1) % dashLengths.size();
                                remaining = dashLengths[dashIndex];
                            }
                        }
                    }
                    cumulativeLength += segmentLength;
                }
            }
            painter->setPen(originalPen);
            continue;
        }
        std::set<qint64> markers;
        for (int index = 1; index < points.size(); ++index) {
            const auto fractions = clippedSegmentFractions(points[index - 1], points[index], clip);
            if (!fractions) {
                continue;
            }
            const qreal segmentLength = distances[index] - distances[index - 1];
            const qreal start = distances[index - 1] + fractions->first * segmentLength;
            const qreal end = distances[index - 1] + fractions->second * segmentLength;
            const qint64 first = qMax<qint64>(0, static_cast<qint64>(std::floor((start - startDistance) / step)) - 1);
            const qint64 last = static_cast<qint64>(qMin(count - 1, std::ceil((end - startDistance) / step) + 1));
            for (qint64 marker = first; marker <= last; ++marker) {
                markers.insert(marker);
            }
        }
        for (const qint64 index : markers) {
            const int markerIndex = static_cast<int>(index);
            const qreal center = lineDecorationEffectiveDistance(
                slope && !decoration.adjustSpacing && count == 1 ? length * 0.5
                    : startDistance + index * step, length, step, decoration, markerIndex, seed);
            const qreal offset = lineDecorationEffectiveOffset(decoration, markerIndex, seed);
            const QPointF point = polylinePointAt(points, distances, center);
            const QPointF normal = normalAt(center);
            const QPointF tangent = QPointF(-normal.y(), normal.x()) * (reversed_ ? -1 : 1);
            switch (decoration.kind) {
            case MapEditorLineDecorationKind::Ticks:
                drawTick(painter, decoration, point, normal, markerIndex);
                continue;
            case MapEditorLineDecorationKind::Rungs:
                drawRung(painter, decoration, point, normal);
                continue;
            case MapEditorLineDecorationKind::Symbols:
                drawSymbol(painter, decoration, point, tangent, normal, markerIndex, zoomOutScale);
                continue;
            case MapEditorLineDecorationKind::Waves:
                drawWave(painter, decoration, point, tangent, normal, markerIndex);
                continue;
            case MapEditorLineDecorationKind::SlopeTicks: {
                const qreal sourceDistance = center / lengthScale;
                const auto orientation = slopeTickOrientationAt(sourceMetrics, sourceDistance);
                const auto sourceSample = sourceLengths
                    ? lineDecorationSampleAt(path(), sourceDistance, reversed_) : std::optional<LineDecorationSample>();
                const QPointF sceneDirection = orientation.value_or(sourceSample ? sourceSample->normal : QPointF());
                QPointF direction = orientation || sourceLengths
                    ? projection.map(sceneDirection) - projection.map(QPointF()) : normal;
                const qreal directionScale = std::hypot(direction.x(), direction.y());
                direction /= qMax<qreal>(1e-9, directionScale);
                const qreal tickLength = sourceLengths
                    ? slopeTickLengthAt(sourceMetrics, sourceDistance, decoration.length) * linePointLengthScale_
                        * directionScale
                    : decoration.length * zoomOutScale;
                const qreal alternating = index % 2 == 0 ? 1.0 : decoration.alternateLengthScale;
                const QPointF base = point + normal * (decoration.offset
                    + stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x91f2U) * decoration.offsetJitter);
                painter->drawLine(QLineF(base, base + direction * tickLength * alternating));
                continue;
            }
            case MapEditorLineDecorationKind::Teeth:
                break;
            default:
                continue;
            }
            const qreal start = qMax<qreal>(0.0, center - step * 0.5);
            const qreal end = qMin(length, center + step * 0.5);
            QPainterPath tooth;
            const auto baseAt = [&](qreal distance) {
                return polylinePointAt(points, distances, distance) + normalAt(distance) * offset;
            };
            tooth.moveTo(baseAt(start));
            const auto firstVertex = std::upper_bound(distances.cbegin(), distances.cend(), start);
            for (auto vertex = firstVertex; vertex != distances.cend() && *vertex < end; ++vertex) {
                tooth.lineTo(baseAt(*vertex));
            }
            tooth.lineTo(baseAt(end));
            tooth.lineTo(baseAt(center) + normalAt(center) * side * decoration.size);
            tooth.closeSubpath();
            painter->setBrush(interactionColorOverride_.value_or(
                decoration.fillColor.value_or(decoration.strokeColor.value_or(fallbackColor_))));
            painter->drawPath(tooth);
            painter->setBrush(Qt::NoBrush);
        }
    }
    painter->restore();
}

void MapEditorLineDecorationItem::updatePaintBounds()
{
    qreal padding = lineDecorationPadding(decorations_) * screenPaddingScale_;
    for (const MapEditorLineDecorationStyle &decoration : decorations_) {
        if (decoration.kind != MapEditorLineDecorationKind::SlopeTicks) {
            continue;
        }

        qreal lengthExtent = 0.0;
        for (const MapEditorLineDecorationVertex &vertex : lineVertices_) {
            if (vertex.leftSize.has_value()) {
                lengthExtent = qMax(lengthExtent, std::abs(vertex.leftSize.value()) * linePointLengthScale_);
            }
        }
        padding = qMax(padding,
                       std::abs(decoration.offset) * screenPaddingScale_
                           + lengthExtent
                           + (decoration.offsetJitter + decoration.strokeWidth + 4.0) * screenPaddingScale_);
    }
    paintBounds_ = path().controlPointRect().adjusted(-padding, -padding, padding, padding);
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
    const qreal symbolSize = qMax<qreal>(0.6, decoration.size + sizeJitter);
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
        QPen pen(interactionColorOverride_.value_or(partStrokeColor.value_or(strokeColor)),
                 qMax<qreal>(0.15, partStrokeWidth.value_or(decoration.strokeWidth) * zoomOutScale)
                    + (interactionColorOverride_ ? 1.8 : 0.0),
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
            painter->setBrush(QBrush(interactionColorOverride_.value_or(fillColor.value())));
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

}
