#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

#include <optional>

namespace TherionStudio
{
struct MapEditorXviStationSnapCandidate
{
    QString name;
    QPointF scenePosition;
    QPointF viewportPosition;
};

// Distinct named stations within the radius, nearest first (ties keep XVI order).
// Equated aliases sharing one position are returned as separate entries.
[[nodiscard]] QVector<MapEditorXviStationSnapCandidate> mapEditorNearbyXviStationSnaps(
    const QPointF &clickViewportPosition,
    const QVector<MapEditorXviStationSnapCandidate> &candidates,
    qreal radiusPixels);

[[nodiscard]] std::optional<MapEditorXviStationSnapCandidate> mapEditorUniqueXviStationSnap(
    const QPointF &clickViewportPosition,
    const QVector<MapEditorXviStationSnapCandidate> &candidates,
    qreal radiusPixels);
}
