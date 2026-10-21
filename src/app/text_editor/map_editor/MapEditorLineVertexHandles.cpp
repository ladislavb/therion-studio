#include "MapEditorLineVertexHandles.h"

#include "MapEditorSceneInternals.h"

#include <cmath>

namespace TherionStudio
{
std::optional<QPointF> normalizedLineControlPoint(const QPointF &anchor, const QPointF &control)
{
    if (!mapSourcePointsDiffer(anchor, control)) {
        return std::nullopt;
    }
    return control;
}

std::optional<QPointF> defaultIncomingControlForLineVertex(const QVector<MapGeometryFeature::TH2LineVertex> &vertices,
                                                           int ownerIndex,
                                                           bool closed)
{
    if (ownerIndex < 0 || ownerIndex >= vertices.size()) {
        return std::nullopt;
    }

    const MapGeometryFeature::TH2LineVertex &target = vertices.at(ownerIndex);
    if (target.outgoingControl.has_value()) {
        return mirroredSmoothControlPoint(target.anchor, target.outgoingControl.value(), std::nullopt);
    }
    if (closed && vertices.size() >= 2) {
        const int previousIndex = ownerIndex > 0 ? ownerIndex - 1 : vertices.size() - 1;
        const QPointF previousAnchor = vertices.at(previousIndex).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor + ((previousAnchor - target.anchor) / 3.0));
    }
    if (ownerIndex > 0) {
        const QPointF previousAnchor = vertices.at(ownerIndex - 1).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor + ((previousAnchor - target.anchor) / 3.0));
    }
    if (ownerIndex + 1 < vertices.size()) {
        const QPointF nextAnchor = vertices.at(ownerIndex + 1).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor - ((nextAnchor - target.anchor) / 3.0));
    }
    return std::nullopt;
}

std::optional<QPointF> defaultOutgoingControlForLineVertex(const QVector<MapGeometryFeature::TH2LineVertex> &vertices,
                                                           int ownerIndex,
                                                           bool closed)
{
    if (ownerIndex < 0 || ownerIndex >= vertices.size()) {
        return std::nullopt;
    }

    const MapGeometryFeature::TH2LineVertex &target = vertices.at(ownerIndex);
    if (target.incomingControl.has_value()) {
        return mirroredSmoothControlPoint(target.anchor, target.incomingControl.value(), std::nullopt);
    }
    if (closed && vertices.size() >= 2) {
        const int nextIndex = ownerIndex + 1 < vertices.size() ? ownerIndex + 1 : 0;
        const QPointF nextAnchor = vertices.at(nextIndex).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor + ((nextAnchor - target.anchor) / 3.0));
    }
    if (ownerIndex + 1 < vertices.size()) {
        const QPointF nextAnchor = vertices.at(ownerIndex + 1).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor + ((nextAnchor - target.anchor) / 3.0));
    }
    if (ownerIndex > 0) {
        const QPointF previousAnchor = vertices.at(ownerIndex - 1).anchor;
        return normalizedLineControlPoint(target.anchor, target.anchor - ((previousAnchor - target.anchor) / 3.0));
    }
    return std::nullopt;
}

void ensureLineVertexControlHandles(QVector<MapGeometryFeature::TH2LineVertex> *vertices, int ownerIndex, bool closed)
{
    if (vertices == nullptr || ownerIndex < 0 || ownerIndex >= vertices->size()) {
        return;
    }

    MapGeometryFeature::TH2LineVertex &target = (*vertices)[ownerIndex];
    if (!target.incomingControl.has_value()) {
        target.incomingControl = defaultIncomingControlForLineVertex(*vertices, ownerIndex, closed);
    }
    if (!target.outgoingControl.has_value()) {
        target.outgoingControl = defaultOutgoingControlForLineVertex(*vertices, ownerIndex, closed);
    }
}

void smoothLineVertexControlHandles(MapGeometryFeature::TH2LineVertex *vertex)
{
    if (vertex == nullptr || !vertex->incomingControl.has_value() || !vertex->outgoingControl.has_value()) {
        return;
    }

    constexpr qreal kEpsilon = 1e-6;
    const QPointF anchor = vertex->anchor;
    const QPointF incomingVector = anchor - vertex->incomingControl.value();
    const QPointF outgoingVector = vertex->outgoingControl.value() - anchor;
    const qreal incomingLength = std::hypot(incomingVector.x(), incomingVector.y());
    const qreal outgoingLength = std::hypot(outgoingVector.x(), outgoingVector.y());
    if (incomingLength <= kEpsilon || outgoingLength <= kEpsilon) {
        return;
    }

    QPointF direction = vertex->outgoingControl.value() - vertex->incomingControl.value();
    qreal directionLength = std::hypot(direction.x(), direction.y());
    if (directionLength <= kEpsilon) {
        direction = QPointF(-incomingVector.y(), incomingVector.x());
        directionLength = std::hypot(direction.x(), direction.y());
    }
    if (directionLength <= kEpsilon) {
        return;
    }

    const QPointF unit(direction.x() / directionLength, direction.y() / directionLength);
    vertex->incomingControl = anchor - (unit * incomingLength);
    vertex->outgoingControl = anchor + (unit * outgoingLength);
}

void setLineVertexSmooth(QVector<MapGeometryFeature::TH2LineVertex> *vertices, int ownerIndex, bool closed, bool smooth)
{
    MapGeometryFeature::TH2LineVertex &target = (*vertices)[ownerIndex];
    if (smooth) {
        ensureLineVertexControlHandles(vertices, ownerIndex, closed);
        smoothLineVertexControlHandles(&target);
        // Lined-up handles are smooth under auto; only an explicit off row would keep a corner.
        if (target.smoothRow == Th2LinePointSmooth::Off) {
            target.smoothRow.reset();
        }
        return;
    }
    // A corner keeps its handles where they are, so the shape is unchanged and each handle can then move on its own;
    // the explicit off row stops Therion reading handles that happen to be in line as smooth (GitHub #48).
    target.smoothRow = Th2LinePointSmooth::Off;
}
}
