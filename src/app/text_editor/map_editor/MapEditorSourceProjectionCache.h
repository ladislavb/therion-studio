#pragma once

#include <QRectF>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

#include "../../../core/Th2GeometryProjection.h"
#include "../../../core/TherionSourceLogicalDocument.h"

namespace TherionStudio
{
struct MapEditorSourceProjectionSnapshot final
{
    int revision = -1;
    QVector<TherionSourceLogicalCommand> logicalCommands;
    Th2GeometryProjection geometryProjection;
    QRectF sourceBounds;
};

using MapEditorSourceProjectionSnapshotPtr = std::shared_ptr<const MapEditorSourceProjectionSnapshot>;

class MapEditorSourceProjectionCache final
{
public:
    using SnapshotBuilder = std::function<MapEditorSourceProjectionSnapshot()>;

    MapEditorSourceProjectionSnapshotPtr snapshotFor(int revision, const SnapshotBuilder &buildSnapshot);
    bool lastRequestWasCacheHit() const;

private:
    int cachedRevision_ = -1;
    MapEditorSourceProjectionSnapshotPtr cachedSnapshot_;
    bool lastRequestWasCacheHit_ = false;
};
}
