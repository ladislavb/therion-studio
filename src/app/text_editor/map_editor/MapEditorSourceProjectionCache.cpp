#include "MapEditorSourceProjectionCache.h"

#include <utility>

namespace TherionStudio
{
MapEditorSourceProjectionSnapshotPtr MapEditorSourceProjectionCache::snapshotFor(
    int revision,
    const SnapshotBuilder &buildSnapshot)
{
    if (cachedSnapshot_ != nullptr && cachedRevision_ == revision) {
        lastRequestWasCacheHit_ = true;
        return cachedSnapshot_;
    }

    if (!buildSnapshot) {
        lastRequestWasCacheHit_ = false;
        return {};
    }

    MapEditorSourceProjectionSnapshot snapshot = buildSnapshot();
    snapshot.revision = revision;
    cachedSnapshot_ = std::make_shared<const MapEditorSourceProjectionSnapshot>(std::move(snapshot));
    cachedRevision_ = revision;
    lastRequestWasCacheHit_ = false;
    return cachedSnapshot_;
}

bool MapEditorSourceProjectionCache::lastRequestWasCacheHit() const
{
    return lastRequestWasCacheHit_;
}
}
