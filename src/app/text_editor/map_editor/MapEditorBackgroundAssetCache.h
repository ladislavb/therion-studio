#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <functional>
#include <memory>

namespace TherionStudio
{
enum class MapEditorBackgroundAssetFormat
{
    Xvi,
    Raster,
    Svg
};

struct MapEditorBackgroundAssetKey
{
    QString canonicalSourcePath;
    MapEditorBackgroundAssetFormat format = MapEditorBackgroundAssetFormat::Raster;
    QString decodeOptions;
};

struct MapEditorBackgroundAssetRevision
{
    qint64 byteSize = -1;
    qint64 modifiedMilliseconds = -1;
    QByteArray contentFingerprint;
};

struct MapEditorBackgroundAssetRequest
{
    MapEditorBackgroundAssetKey key;
    MapEditorBackgroundAssetRevision revision;
};

struct MapEditorBackgroundAssetPayload
{
    QByteArray bytes;
};

struct MapEditorBackgroundAssetLoadResult
{
    std::shared_ptr<const MapEditorBackgroundAssetPayload> payload;
    QString error;
    std::size_t byteCost = 0;
};

struct MapEditorBackgroundAssetCacheResult
{
    MapEditorBackgroundAssetLoadResult loadResult;
    bool cacheHit = false;
    bool cached = false;
};

struct MapEditorBackgroundAssetCacheStats
{
    std::size_t byteLimit = 0;
    std::size_t storedBytes = 0;
    int entryCount = 0;
    quint64 hits = 0;
    quint64 misses = 0;
    quint64 evictions = 0;
};

class MapEditorBackgroundAssetCache final
{
public:
    using Loader = std::function<MapEditorBackgroundAssetLoadResult(const MapEditorBackgroundAssetRequest &)>;

    explicit MapEditorBackgroundAssetCache(std::size_t byteLimit);

    MapEditorBackgroundAssetCacheResult load(const MapEditorBackgroundAssetRequest &request,
                                             const Loader &loader);
    int invalidateSource(const QString &canonicalSourcePath);
    void clear();
    MapEditorBackgroundAssetCacheStats stats() const;

private:
    struct Entry
    {
        MapEditorBackgroundAssetKey key;
        MapEditorBackgroundAssetRevision revision;
        MapEditorBackgroundAssetLoadResult loadResult;
        std::size_t byteCost = 0;
    };

    QString entryId(const MapEditorBackgroundAssetKey &key) const;
    static bool revisionsMatch(const MapEditorBackgroundAssetRevision &left,
                               const MapEditorBackgroundAssetRevision &right);
    static std::size_t storedByteCost(const MapEditorBackgroundAssetLoadResult &loadResult);
    void touch(const QString &entryId);
    void remove(const QString &entryId, bool eviction);
    bool canStore(std::size_t byteCost) const;

    std::size_t byteLimit_ = 0;
    std::size_t storedBytes_ = 0;
    quint64 hits_ = 0;
    quint64 misses_ = 0;
    quint64 evictions_ = 0;
    QHash<QString, Entry> entries_;
    QStringList leastRecentlyUsedEntryIds_;
};
}
