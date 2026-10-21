#pragma once

#include "MapEditorSceneSupport.h"

#include <QPointF>
#include <QVector>

#include <optional>

namespace TherionStudio
{
// The control point, or nothing when it coincides with the anchor.
std::optional<QPointF> normalizedLineControlPoint(const QPointF &anchor, const QPointF &control);
// Default handles for a line vertex: opposite its other handle, otherwise a third of the way to a neighbour.
std::optional<QPointF> defaultIncomingControlForLineVertex(const QVector<MapGeometryFeature::TH2LineVertex> &vertices,
                                                           int ownerIndex,
                                                           bool closed);
std::optional<QPointF> defaultOutgoingControlForLineVertex(const QVector<MapGeometryFeature::TH2LineVertex> &vertices,
                                                           int ownerIndex,
                                                           bool closed);
// Adds default handles where the vertex has none.
void ensureLineVertexControlHandles(QVector<MapGeometryFeature::TH2LineVertex> *vertices, int ownerIndex, bool closed);
// Lines the two handles up through the anchor, keeping each handle's length.
void smoothLineVertexControlHandles(MapGeometryFeature::TH2LineVertex *vertex);
// Makes the point smooth (handles lined up, each keeping its length) or a corner (handles kept, smooth off).
void setLineVertexSmooth(QVector<MapGeometryFeature::TH2LineVertex> *vertices, int ownerIndex, bool closed, bool smooth);
}
