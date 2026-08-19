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

[[nodiscard]] std::optional<MapEditorXviStationSnapCandidate> mapEditorUniqueXviStationSnap(
    const QPointF &clickViewportPosition,
    const QVector<MapEditorXviStationSnapCandidate> &candidates,
    qreal radiusPixels);
}
