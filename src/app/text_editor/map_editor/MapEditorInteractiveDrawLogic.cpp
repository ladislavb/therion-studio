#include "MapEditorInteractiveDrawLogic.h"

#include "../../../core/TherionCommandSyntax.h"

#include <QLineF>

#include <algorithm>
#include <cmath>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr qreal kFreehandSimplifyAverageSampleFactor = 0.35;
constexpr qreal kFreehandSimplifyBoundsFactor = 0.0005;
constexpr int kFreehandMaximumSafetyAnchors = 512;

qreal normalizedDraftOrientationDegrees(qreal degrees)
{
    qreal normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

QString formatSourceCoordinate(qreal value)
{
    return QString::number(value, 'f', 1);
}

std::optional<QPointF> mirroredSmoothControlPoint(const QPointF &anchor,
                                                  const QPointF &movedControlPoint,
                                                  const std::optional<QPointF> &oppositeControlPoint)
{
    constexpr qreal kEpsilon = 1e-6;

    const QPointF vector = movedControlPoint - anchor;
    const qreal vectorLength = std::hypot(vector.x(), vector.y());
    if (vectorLength <= kEpsilon) {
        return std::nullopt;
    }

    const QPointF direction(vector.x() / vectorLength, vector.y() / vectorLength);
    const qreal oppositeLength = oppositeControlPoint.has_value()
        ? std::hypot(oppositeControlPoint->x() - anchor.x(), oppositeControlPoint->y() - anchor.y())
        : vectorLength;
    return QPointF(anchor.x() - (direction.x() * oppositeLength),
                   anchor.y() - (direction.y() * oppositeLength));
}

qreal pointSegmentDistance(const QPointF &point, const QPointF &start, const QPointF &end)
{
    const qreal segmentX = end.x() - start.x();
    const qreal segmentY = end.y() - start.y();
    const qreal segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;
    if (segmentLengthSquared <= 0.000001) {
        return QLineF(point, start).length();
    }

    const qreal relativeX = point.x() - start.x();
    const qreal relativeY = point.y() - start.y();
    const qreal projection = std::clamp((relativeX * segmentX + relativeY * segmentY) / segmentLengthSquared,
                                        0.0,
                                        1.0);
    const QPointF projected(start.x() + segmentX * projection,
                            start.y() + segmentY * projection);
    return QLineF(point, projected).length();
}

void markRamerDouglasPeuckerKeepPoints(const QVector<QPointF> &points,
                                       int first,
                                       int last,
                                       qreal tolerance,
                                       QVector<bool> *keep)
{
    if (keep == nullptr || last <= first + 1) {
        return;
    }

    qreal maxDistance = 0.0;
    int maxIndex = -1;
    for (int index = first + 1; index < last; ++index) {
        const qreal distance = pointSegmentDistance(points.at(index), points.at(first), points.at(last));
        if (distance > maxDistance) {
            maxDistance = distance;
            maxIndex = index;
        }
    }

    if (maxIndex < 0 || maxDistance <= tolerance) {
        return;
    }

    (*keep)[maxIndex] = true;
    markRamerDouglasPeuckerKeepPoints(points, first, maxIndex, tolerance, keep);
    markRamerDouglasPeuckerKeepPoints(points, maxIndex, last, tolerance, keep);
}

QVector<QPointF> ramerDouglasPeuckerSimplified(const QVector<QPointF> &points, qreal tolerance)
{
    if (points.size() <= 2) {
        return points;
    }

    QVector<bool> keep(points.size(), false);
    keep[0] = true;
    keep[keep.size() - 1] = true;
    markRamerDouglasPeuckerKeepPoints(points, 0, points.size() - 1, tolerance, &keep);

    QVector<QPointF> simplified;
    simplified.reserve(points.size());
    for (int index = 0; index < points.size(); ++index) {
        if (keep.at(index)) {
            simplified.append(points.at(index));
        }
    }
    return simplified.size() >= 2 ? simplified : points;
}

QVector<QPointF> evenlyCappedAnchors(const QVector<QPointF> &points, int maxAnchors)
{
    if (points.size() <= maxAnchors || maxAnchors < 2) {
        return points;
    }

    QVector<QPointF> capped;
    capped.reserve(maxAnchors);
    for (int index = 0; index < maxAnchors; ++index) {
        const qreal sourceIndex = static_cast<qreal>(index) * static_cast<qreal>(points.size() - 1)
            / static_cast<qreal>(maxAnchors - 1);
        capped.append(points.at(std::clamp(static_cast<qsizetype>(std::round(sourceIndex)),
                                           qsizetype{0},
                                           points.size() - 1)));
    }
    return capped;
}

qreal polylineLength(const QVector<QPointF> &points)
{
    if (points.size() <= 2) {
        return points.size() == 2 ? QLineF(points.first(), points.last()).length() : 0.0;
    }

    qreal length = 0.0;
    for (int index = 1; index < points.size(); ++index) {
        length += QLineF(points.at(index - 1), points.at(index)).length();
    }
    return length;
}

QVector<QPointF> simplifiedFreehandSourceVertices(const QVector<QPointF> &sourceVertices)
{
    QVector<QPointF> deduplicated;
    deduplicated.reserve(sourceVertices.size());
    for (const QPointF &point : sourceVertices) {
        if (deduplicated.isEmpty() || QLineF(deduplicated.last(), point).length() > 0.0001) {
            deduplicated.append(point);
        }
    }

    if (deduplicated.size() <= 2) {
        return deduplicated;
    }

    qreal minX = deduplicated.first().x();
    qreal maxX = minX;
    qreal minY = deduplicated.first().y();
    qreal maxY = minY;
    for (const QPointF &point : std::as_const(deduplicated)) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }
    const qreal diagonal = std::hypot(maxX - minX, maxY - minY);
    if (diagonal <= 0.0001) {
        return evenlyCappedAnchors(deduplicated, std::min(static_cast<int>(deduplicated.size()),
                                                          kFreehandMaximumSafetyAnchors));
    }

    const qreal averageSampleLength = polylineLength(deduplicated) / static_cast<qreal>(deduplicated.size() - 1);
    const qreal tolerance = std::max(averageSampleLength * kFreehandSimplifyAverageSampleFactor,
                                    diagonal * kFreehandSimplifyBoundsFactor);
    const QVector<QPointF> simplified = ramerDouglasPeuckerSimplified(deduplicated, tolerance);
    if (simplified.size() <= kFreehandMaximumSafetyAnchors) {
        return simplified;
    }

    return evenlyCappedAnchors(simplified, kFreehandMaximumSafetyAnchors);
}

QVector<MapEditorInteractiveLineDraftVertex> bezierDraftVerticesFromAnchors(const QVector<QPointF> &anchors)
{
    QVector<MapEditorInteractiveLineDraftVertex> vertices;
    vertices.reserve(anchors.size());
    for (const QPointF &anchor : anchors) {
        MapEditorInteractiveLineDraftVertex vertex;
        vertex.anchorSource = anchor;
        vertex.anchorScene = anchor;
        vertices.append(vertex);
    }

    if (vertices.size() < 2) {
        return vertices;
    }

    for (int index = 0; index < vertices.size() - 1; ++index) {
        const QPointF previousAnchor = index > 0
            ? vertices.at(index - 1).anchorSource
            : vertices.at(index).anchorSource;
        const QPointF currentAnchor = vertices.at(index).anchorSource;
        const QPointF nextAnchor = vertices.at(index + 1).anchorSource;
        const QPointF afterNextAnchor = index + 2 < vertices.size()
            ? vertices.at(index + 2).anchorSource
            : nextAnchor;

        vertices[index].outgoingControlSource = currentAnchor + (nextAnchor - previousAnchor) / 6.0;
        vertices[index + 1].incomingControlSource = nextAnchor - (afterNextAnchor - currentAnchor) / 6.0;
    }

    return vertices;
}

void appendInteractiveLinePointStandaloneRows(QStringList *rows,
                                              const MapEditorInteractiveLineDraftVertex &vertex,
                                              QString *activeSegmentSubtype)
{
    if (rows == nullptr) {
        return;
    }
    if (!vertex.isSmooth) {
        rows->append(QStringLiteral("smooth off"));
    }
    const QString segmentSubtype = vertex.segmentSubtype.trimmed();
    if (!segmentSubtype.isEmpty() && (activeSegmentSubtype == nullptr || *activeSegmentSubtype != segmentSubtype)) {
        rows->append(QStringLiteral("subtype %1").arg(serializeTherionArgumentToken(segmentSubtype)));
        if (activeSegmentSubtype != nullptr) {
            *activeSegmentSubtype = segmentSubtype;
        }
    }
    if (vertex.orientationDegrees.has_value()) {
        rows->append(QStringLiteral("orientation %1")
                         .arg(formatSourceCoordinate(normalizedDraftOrientationDegrees(vertex.orientationDegrees.value()))));
    }
    if (vertex.leftSize.has_value()) {
        rows->append(QStringLiteral("l-size %1").arg(formatSourceCoordinate(std::max<qreal>(0.1, vertex.leftSize.value()))));
    }
}

bool pointsNearlyEqual(const QPointF &a, const QPointF &b)
{
    constexpr qreal kDuplicateAnchorEpsilon = 0.001;
    return QLineF(a, b).length() <= kDuplicateAnchorEpsilon;
}

void mergeDuplicateDraftVertex(MapEditorInteractiveLineDraftVertex *target,
                               const MapEditorInteractiveLineDraftVertex &duplicate)
{
    if (target == nullptr) {
        return;
    }
    if (!target->incomingControlSource.has_value() && duplicate.incomingControlSource.has_value()) {
        target->incomingControlSource = duplicate.incomingControlSource;
    }
    if (!target->incomingControlScene.has_value() && duplicate.incomingControlScene.has_value()) {
        target->incomingControlScene = duplicate.incomingControlScene;
    }
    if (!target->outgoingControlSource.has_value() && duplicate.outgoingControlSource.has_value()) {
        target->outgoingControlSource = duplicate.outgoingControlSource;
    }
    if (!target->outgoingControlScene.has_value() && duplicate.outgoingControlScene.has_value()) {
        target->outgoingControlScene = duplicate.outgoingControlScene;
    }
}

QVector<MapEditorInteractiveLineDraftVertex> compactDuplicateDraftAnchors(
    const QVector<MapEditorInteractiveLineDraftVertex> &vertices)
{
    QVector<MapEditorInteractiveLineDraftVertex> compacted;
    compacted.reserve(vertices.size());
    for (const MapEditorInteractiveLineDraftVertex &vertex : vertices) {
        if (!compacted.isEmpty()
            && pointsNearlyEqual(compacted.last().anchorSource, vertex.anchorSource)) {
            mergeDuplicateDraftVertex(&compacted.last(), vertex);
            continue;
        }
        compacted.append(vertex);
    }
    return compacted;
}

void mirrorMissingSmoothDraftControls(QVector<MapEditorInteractiveLineDraftVertex> *vertices)
{
    if (vertices == nullptr) {
        return;
    }
    for (int index = 0; index < vertices->size(); ++index) {
        MapEditorInteractiveLineDraftVertex &vertex = (*vertices)[index];
        if (!vertex.isSmooth) {
            continue;
        }
        if (index + 1 < vertices->size()
            && vertex.incomingControlSource.has_value()
            && !vertex.outgoingControlSource.has_value()) {
            vertex.outgoingControlSource =
                vertex.anchorSource - (vertex.incomingControlSource.value() - vertex.anchorSource);
        }
        if (index + 1 < vertices->size()
            && vertex.incomingControlScene.has_value()
            && !vertex.outgoingControlScene.has_value()) {
            vertex.outgoingControlScene =
                vertex.anchorScene - (vertex.incomingControlScene.value() - vertex.anchorScene);
        }
        if (index > 0
            && vertex.outgoingControlSource.has_value()
            && !vertex.incomingControlSource.has_value()) {
            vertex.incomingControlSource =
                vertex.anchorSource - (vertex.outgoingControlSource.value() - vertex.anchorSource);
        }
        if (index > 0
            && vertex.outgoingControlScene.has_value()
            && !vertex.incomingControlScene.has_value()) {
            vertex.incomingControlScene =
                vertex.anchorScene - (vertex.outgoingControlScene.value() - vertex.anchorScene);
        }
    }
}

QVector<MapEditorInteractiveLineDraftVertex> normalizedInteractiveLineDraftVertices(
    const QVector<MapEditorInteractiveLineDraftVertex> &vertices)
{
    QVector<MapEditorInteractiveLineDraftVertex> normalized = compactDuplicateDraftAnchors(vertices);
    mirrorMissingSmoothDraftControls(&normalized);
    return normalized;
}
}

QStringList lineCoordinateRowsForInteractiveDraft(const QVector<MapEditorInteractiveLineDraftVertex> &vertices)
{
    QStringList rows;
    const QVector<MapEditorInteractiveLineDraftVertex> normalizedVertices =
        normalizedInteractiveLineDraftVertices(vertices);
    if (normalizedVertices.isEmpty()) {
        return rows;
    }

    rows.reserve(normalizedVertices.size());
    const auto formatPoint = [](const QPointF &point) {
        return QStringLiteral("%1 %2")
            .arg(formatSourceCoordinate(point.x()), formatSourceCoordinate(point.y()));
    };

    rows.append(formatPoint(normalizedVertices.first().anchorSource));
    QString activeSegmentSubtype;
    appendInteractiveLinePointStandaloneRows(&rows, normalizedVertices.first(), &activeSegmentSubtype);
    for (int index = 1; index < normalizedVertices.size(); ++index) {
        const MapEditorInteractiveLineDraftVertex &previous = normalizedVertices.at(index - 1);
        const MapEditorInteractiveLineDraftVertex &current = normalizedVertices.at(index);
        QStringList tokens;
        if (previous.outgoingControlSource.has_value() || current.incomingControlSource.has_value()) {
            const QPointF control1 = previous.outgoingControlSource.value_or(previous.anchorSource);
            const QPointF control2 = current.incomingControlSource.value_or(current.anchorSource);
            tokens << formatSourceCoordinate(control1.x())
                   << formatSourceCoordinate(control1.y())
                   << formatSourceCoordinate(control2.x())
                   << formatSourceCoordinate(control2.y());
        }
        tokens << formatSourceCoordinate(current.anchorSource.x())
               << formatSourceCoordinate(current.anchorSource.y());
        rows.append(tokens.join(QLatin1Char(' ')));
        appendInteractiveLinePointStandaloneRows(&rows, current, &activeSegmentSubtype);
    }

    return rows;
}

QStringList bezierLineCoordinateRowsForFreehandStroke(const QVector<QPointF> &sourceVertices)
{
    const QVector<QPointF> simplifiedAnchors = simplifiedFreehandSourceVertices(sourceVertices);
    if (simplifiedAnchors.size() < 2) {
        return {};
    }

    return lineCoordinateRowsForInteractiveDraft(bezierDraftVerticesFromAnchors(simplifiedAnchors));
}

QStringList closedLineCoordinateRowsForInteractiveDraft(const QVector<MapEditorInteractiveLineDraftVertex> &vertices)
{
    QStringList rows = lineCoordinateRowsForInteractiveDraft(vertices);
    if (vertices.size() < 2) {
        return rows;
    }

    const MapEditorInteractiveLineDraftVertex &first = vertices.first();
    const MapEditorInteractiveLineDraftVertex &last = vertices.last();

    std::optional<QPointF> closingOutgoing = last.outgoingControlSource;
    if (!closingOutgoing.has_value() && last.incomingControlSource.has_value()) {
        closingOutgoing = mirroredSmoothControlPoint(last.anchorSource,
                                                     last.incomingControlSource.value(),
                                                     std::nullopt);
    }

    std::optional<QPointF> closingIncoming = first.incomingControlSource;
    if (!closingIncoming.has_value() && first.outgoingControlSource.has_value()) {
        closingIncoming = mirroredSmoothControlPoint(first.anchorSource,
                                                     first.outgoingControlSource.value(),
                                                     std::nullopt);
    }

    if (!closingOutgoing.has_value() && !closingIncoming.has_value()) {
        return rows;
    }

    QStringList tokens;
    const QPointF control1 = closingOutgoing.value_or(last.anchorSource);
    const QPointF control2 = closingIncoming.value_or(first.anchorSource);
    tokens << formatSourceCoordinate(control1.x())
           << formatSourceCoordinate(control1.y())
           << formatSourceCoordinate(control2.x())
           << formatSourceCoordinate(control2.y())
           << formatSourceCoordinate(first.anchorSource.x())
           << formatSourceCoordinate(first.anchorSource.y());
    rows.append(tokens.join(QLatin1Char(' ')));
    return rows;
}

QStringList areaCoordinateRowsForInteractiveDraft(const QVector<MapEditorInteractiveLineDraftVertex> &vertices)
{
    if (vertices.size() < 3) {
        return lineCoordinateRowsForInteractiveDraft(vertices);
    }

    return closedLineCoordinateRowsForInteractiveDraft(vertices);
}

void captureInteractiveLineAnchor(QVector<MapEditorInteractiveLineDraftVertex> *vertices,
                                  const QPointF &anchorScenePoint,
                                  const QPointF &anchorSourcePoint,
                                  const std::optional<QPointF> &dragScenePoint,
                                  bool smooth,
                                  bool incomingControl,
                                  bool outgoingControl,
                                  const QString &segmentSubtype,
                                  std::optional<qreal> orientationDegrees,
                                  std::optional<qreal> leftSize,
                                  const std::function<QPointF(const QPointF &)> &sceneToSource)
{
    if (vertices == nullptr || sceneToSource == nullptr) {
        return;
    }

    MapEditorInteractiveLineDraftVertex vertex;
    vertex.anchorScene = anchorScenePoint;
    vertex.anchorSource = anchorSourcePoint;
    vertex.isSmooth = smooth;
    vertex.segmentSubtype = segmentSubtype.trimmed();
    if (orientationDegrees.has_value()) {
        vertex.orientationDegrees = normalizedDraftOrientationDegrees(orientationDegrees.value());
    }
    if (leftSize.has_value()) {
        vertex.leftSize = std::max<qreal>(0.1, leftSize.value());
    }
    vertices->append(vertex);

    if (smooth && dragScenePoint.has_value()) {
        MapEditorInteractiveLineDraftVertex &current = vertices->last();
        if (outgoingControl) {
            current.outgoingControlScene = dragScenePoint.value();
            current.outgoingControlSource = sceneToSource(dragScenePoint.value());
        }
        if (incomingControl) {
            current.incomingControlScene = current.anchorScene - (dragScenePoint.value() - current.anchorScene);
            current.incomingControlSource = sceneToSource(current.incomingControlScene.value());
        }
    }
}

std::optional<MapEditorInteractiveLineControlHandleRef> interactiveLineControlAt(
    const QVector<MapEditorInteractiveLineDraftVertex> &vertices,
    const QPointF &scenePosition,
    qreal sceneRadius)
{
    if (sceneRadius <= 0.0) {
        return std::nullopt;
    }

    qreal bestDistance = sceneRadius;
    std::optional<MapEditorInteractiveLineControlHandleRef> bestHandle;
    for (int index = 0; index < vertices.size(); ++index) {
        const MapEditorInteractiveLineDraftVertex &vertex = vertices.at(index);
        const qreal anchorDistance = QLineF(scenePosition, vertex.anchorScene).length();
        if (anchorDistance <= bestDistance) {
            bestDistance = anchorDistance;
            bestHandle = MapEditorInteractiveLineControlHandleRef{index,
                                                                  MapEditorInteractiveLineControlHandleRef::Kind::Anchor};
        }
        if (vertex.incomingControlScene.has_value()) {
            const qreal distance = QLineF(scenePosition, vertex.incomingControlScene.value()).length();
            if (distance <= bestDistance) {
                bestDistance = distance;
                bestHandle = MapEditorInteractiveLineControlHandleRef{index,
                                                                      MapEditorInteractiveLineControlHandleRef::Kind::Incoming};
            }
        }
        if (vertex.outgoingControlScene.has_value()) {
            const qreal distance = QLineF(scenePosition, vertex.outgoingControlScene.value()).length();
            if (distance <= bestDistance) {
                bestDistance = distance;
                bestHandle = MapEditorInteractiveLineControlHandleRef{index,
                                                                      MapEditorInteractiveLineControlHandleRef::Kind::Outgoing};
            }
        }
    }

    return bestHandle;
}

bool setInteractiveLineControlScenePoint(QVector<MapEditorInteractiveLineDraftVertex> *vertices,
                                         const MapEditorInteractiveLineControlHandleRef &handle,
                                         const QPointF &scenePoint,
                                         const std::function<QPointF(const QPointF &)> &sceneToSource)
{
    if (vertices == nullptr
        || sceneToSource == nullptr
        || handle.vertexIndex < 0
        || handle.vertexIndex >= vertices->size()) {
        return false;
    }

    MapEditorInteractiveLineDraftVertex &vertex = (*vertices)[handle.vertexIndex];
    if (handle.kind == MapEditorInteractiveLineControlHandleRef::Kind::Anchor) {
        const QPointF delta = scenePoint - vertex.anchorScene;
        vertex.anchorScene = scenePoint;
        vertex.anchorSource = sceneToSource(scenePoint);
        if (vertex.incomingControlScene.has_value()) {
            const QPointF movedControl = vertex.incomingControlScene.value() + delta;
            vertex.incomingControlScene = movedControl;
            vertex.incomingControlSource = sceneToSource(movedControl);
        }
        if (vertex.outgoingControlScene.has_value()) {
            const QPointF movedControl = vertex.outgoingControlScene.value() + delta;
            vertex.outgoingControlScene = movedControl;
            vertex.outgoingControlSource = sceneToSource(movedControl);
        }
        return true;
    }

    if (handle.kind == MapEditorInteractiveLineControlHandleRef::Kind::Incoming) {
        vertex.incomingControlScene = scenePoint;
        vertex.incomingControlSource = sceneToSource(scenePoint);
        if (vertex.outgoingControlScene.has_value()) {
            const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(vertex.anchorScene,
                                                                               scenePoint,
                                                                               vertex.outgoingControlScene);
            if (mirrored.has_value()) {
                vertex.outgoingControlScene = mirrored.value();
                vertex.outgoingControlSource = sceneToSource(mirrored.value());
            }
        }
        return true;
    }

    vertex.outgoingControlScene = scenePoint;
    vertex.outgoingControlSource = sceneToSource(scenePoint);
    if (vertex.incomingControlScene.has_value()) {
        const std::optional<QPointF> mirrored = mirroredSmoothControlPoint(vertex.anchorScene,
                                                                           scenePoint,
                                                                           vertex.incomingControlScene);
        if (mirrored.has_value()) {
            vertex.incomingControlScene = mirrored.value();
            vertex.incomingControlSource = sceneToSource(mirrored.value());
        }
    }
    return true;
}
}
