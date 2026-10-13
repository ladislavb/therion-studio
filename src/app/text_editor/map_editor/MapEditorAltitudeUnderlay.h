#pragma once

#include "MapEditorSourceProjectionCache.h"
#include "../../../core/ProjectStructureIndex.h"
#include "../../../core/ScrapAltitudeModel.h"

#include <QColor>
#include <QHash>

class QGraphicsScene;

namespace TherionStudio
{
constexpr int kMapAltitudeUnderlayRole = Qt::UserRole + 170;
constexpr int kMapAltitudeUnderlayApproximateRole = Qt::UserRole + 171;

struct MapEditorAltitudeScrap
{
    QColor color;
    QString description;
};

[[nodiscard]] QHash<int, MapEditorAltitudeScrap> mapEditorAltitudeScraps(
    const MapEditorSourceProjectionSnapshot &snapshot, const QString &filePath,
    const ProjectIndexSnapshot &index, const ScrapAltitudeStations &stations,
    int strengthPercent = 19);

int renderMapEditorAltitudeUnderlay(
    QGraphicsScene *scene, const MapEditorSourceProjectionSnapshot &snapshot,
    const QHash<int, MapEditorAltitudeScrap> &scraps, int *approximateCount = nullptr);
}
