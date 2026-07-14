#include "MapEditorBackgroundAssetCache.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace TherionStudio
{
MapEditorBackgroundAssetCache::MapEditorBackgroundAssetCache(std::size_t byteLimit)
    : byteLimit_(byteLimit)
{
}

QString MapEditorBackgroundAssetCache::canonicalSourceIdentity(const QString &sourcePath)
{
    if (sourcePath.trimmed().isEmpty()) {
        return {};
    }

    const QFileInfo fileInfo(sourcePath);
    QString resolvedPath = fileInfo.absoluteFilePath();
    if (resolvedPath.isEmpty()) {
        resolvedPath = sourcePath;
    }

    return QDir::cleanPath(resolvedPath)
        .normalized(QString::NormalizationForm_C)
        .toCaseFolded();
}

MapEditorBackgroundAssetCacheResult MapEditorBackgroundAssetCache::find(
    const MapEditorBackgroundAssetRequest &request)
{
    const QString id = entryId(request.key);
    if (id.isEmpty()) {
        ++misses_;
        return MapEditorBackgroundAssetCacheResult{
            .loadResult = MapEditorBackgroundAssetLoadResult{.error = QStringLiteral("Invalid background asset request.")}};
    }

    const auto existing = entries_.constFind(id);
    if (existing != entries_.constEnd() && revisionsMatch(existing->revision, request.revision)) {
        ++hits_;
        touch(id);
        return MapEditorBackgroundAssetCacheResult{
            .loadResult = existing->loadResult,
            .cacheHit = true,
            .cached = true};
    }

    if (existing != entries_.constEnd()) {
        remove(id, false);
    }

    ++misses_;
    return MapEditorBackgroundAssetCacheResult{};
}

bool MapEditorBackgroundAssetCache::store(const MapEditorBackgroundAssetRequest &request,
                                          MapEditorBackgroundAssetLoadResult loadResult)
{
    const QString id = entryId(request.key);
    if (id.isEmpty()) {
        return false;
    }

    if (entries_.contains(id)) {
        remove(id, false);
    }

    const std::size_t byteCost = storedByteCost(loadResult);
    if (!canStore(byteCost)) {
        return false;
    }

    while (!leastRecentlyUsedEntryIds_.isEmpty() && storedBytes_ + byteCost > byteLimit_) {
        remove(leastRecentlyUsedEntryIds_.front(), true);
    }

    entries_.insert(id, Entry{
                            .key = request.key,
                            .revision = request.revision,
                            .loadResult = loadResult,
                            .byteCost = byteCost});
    storedBytes_ += byteCost;
    leastRecentlyUsedEntryIds_.append(id);
    return true;
}

MapEditorBackgroundAssetCacheResult MapEditorBackgroundAssetCache::load(
    const MapEditorBackgroundAssetRequest &request,
    const Loader &loader)
{
    if (!loader) {
        ++misses_;
        return MapEditorBackgroundAssetCacheResult{
            .loadResult = MapEditorBackgroundAssetLoadResult{.error = QStringLiteral("Invalid background asset loader.")}};
    }

    MapEditorBackgroundAssetCacheResult cachedResult = find(request);
    if (cachedResult.cacheHit) {
        return cachedResult;
    }

    MapEditorBackgroundAssetLoadResult loadResult = loader(request);
    const bool cached = store(request, loadResult);
    return MapEditorBackgroundAssetCacheResult{
        .loadResult = std::move(loadResult),
        .cacheHit = false,
        .cached = cached};
}

int MapEditorBackgroundAssetCache::invalidateSource(const QString &canonicalSourcePath)
{
    const QString normalizedPath = canonicalSourcePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return 0;
    }

    QStringList matchingEntryIds;
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        if (it->key.canonicalSourcePath == normalizedPath) {
            matchingEntryIds.append(it.key());
        }
    }
    for (const QString &id : matchingEntryIds) {
        remove(id, false);
    }
    return matchingEntryIds.size();
}

void MapEditorBackgroundAssetCache::clear()
{
    entries_.clear();
    leastRecentlyUsedEntryIds_.clear();
    storedBytes_ = 0;
}

MapEditorBackgroundAssetCacheStats MapEditorBackgroundAssetCache::stats() const
{
    return MapEditorBackgroundAssetCacheStats{
        .byteLimit = byteLimit_,
        .storedBytes = storedBytes_,
        .entryCount = static_cast<int>(entries_.size()),
        .hits = hits_,
        .misses = misses_,
        .evictions = evictions_};
}

QString MapEditorBackgroundAssetCache::entryId(const MapEditorBackgroundAssetKey &key) const
{
    const QString path = key.canonicalSourcePath.trimmed();
    if (path.isEmpty()) {
        return QString();
    }
    return QStringLiteral("%1\n%2\n%3")
        .arg(path, QString::number(static_cast<int>(key.format)), key.decodeOptions);
}

bool MapEditorBackgroundAssetCache::revisionsMatch(const MapEditorBackgroundAssetRevision &left,
                                                    const MapEditorBackgroundAssetRevision &right)
{
    if (left.byteSize != right.byteSize || left.modifiedMilliseconds != right.modifiedMilliseconds) {
        return false;
    }
    if (!left.contentFingerprint.isEmpty() || !right.contentFingerprint.isEmpty()) {
        return left.contentFingerprint == right.contentFingerprint;
    }
    return true;
}

std::size_t MapEditorBackgroundAssetCache::storedByteCost(const MapEditorBackgroundAssetLoadResult &loadResult)
{
    return std::max<std::size_t>(1, loadResult.byteCost);
}

void MapEditorBackgroundAssetCache::touch(const QString &id)
{
    leastRecentlyUsedEntryIds_.removeAll(id);
    leastRecentlyUsedEntryIds_.append(id);
}

void MapEditorBackgroundAssetCache::remove(const QString &id, bool eviction)
{
    const auto entry = entries_.find(id);
    if (entry == entries_.end()) {
        return;
    }

    storedBytes_ -= entry->byteCost;
    entries_.erase(entry);
    leastRecentlyUsedEntryIds_.removeAll(id);
    if (eviction) {
        ++evictions_;
    }
}

bool MapEditorBackgroundAssetCache::canStore(std::size_t byteCost) const
{
    return byteLimit_ > 0 && byteCost <= byteLimit_;
}
}
