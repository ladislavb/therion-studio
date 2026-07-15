#pragma once

#include <functional>

#include "MapEditorSourceProjectionCache.h"

namespace TherionStudio
{
struct TherionSourceLogicalCommand;

struct MapEditorLogicalSourceContext
{
    std::function<MapEditorSourceProjectionSnapshotPtr()> projectionSnapshotForCurrentDocument;
    std::function<bool()> projectionSnapshotWasReused;

    // Compatibility callbacks remain while non-refresh consumers migrate to the
    // immutable snapshot handle. New refresh code shall use the snapshot.
    std::function<QVector<TherionSourceLogicalCommand>()> logicalCommandsForCurrentDocument;
    std::function<Th2GeometryProjection()> geometryProjectionForCurrentDocument;
};
}
