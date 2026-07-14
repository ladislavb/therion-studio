#include "../src/app/text_editor/map_editor/MapEditorBackgroundAssetCache.h"

#include <QFile>
#include <QTest>
#include <QTemporaryDir>

using namespace TherionStudio;

namespace
{
MapEditorBackgroundAssetRequest request(const QString &path,
                                        qint64 byteSize = 10,
                                        qint64 modifiedMilliseconds = 100,
                                        const QByteArray &contentFingerprint = QByteArray(),
                                        MapEditorBackgroundAssetFormat format = MapEditorBackgroundAssetFormat::Raster,
                                        const QString &decodeOptions = QString())
{
    return MapEditorBackgroundAssetRequest{
        .key = MapEditorBackgroundAssetKey{
            .canonicalSourcePath = path,
            .format = format,
            .decodeOptions = decodeOptions},
        .revision = MapEditorBackgroundAssetRevision{
            .byteSize = byteSize,
            .modifiedMilliseconds = modifiedMilliseconds,
            .contentFingerprint = contentFingerprint}};
}

MapEditorBackgroundAssetCache::Loader countingLoader(int *calls, std::size_t byteCost = 4)
{
    return [calls, byteCost](const MapEditorBackgroundAssetRequest &) {
        ++(*calls);
        return MapEditorBackgroundAssetLoadResult{
            .payload = std::make_shared<const QByteArray>(QByteArray("asset")),
            .byteCost = byteCost};
    };
}
}

class MapEditorBackgroundAssetCacheTest final : public QObject
{
    Q_OBJECT

private slots:
    void hitsForSameIdentityRevisionAndOptions();
    void storesWorkerResultForLaterLookup();
    void reloadsWhenMetadataRevisionChanges();
    void reloadsWhenContentFingerprintChangesAtSameTimestamp();
    void evictsLeastRecentlyUsedEntryAtByteLimit();
    void doesNotStoreOversizedEntries();
    void invalidatesAllFormatsForSource();
    void invalidatesRemovedSourceByStableIdentity();
    void clearsEntriesAndReleasesPayloads();
    void retainsLoadErrorsAsCacheEntries();
};

void MapEditorBackgroundAssetCacheTest::hitsForSameIdentityRevisionAndOptions()
{
    MapEditorBackgroundAssetCache cache(16);
    int calls = 0;
    const MapEditorBackgroundAssetRequest first = request(QStringLiteral("/project/background.png"));

    QVERIFY(!cache.load(first, countingLoader(&calls)).cacheHit);
    QVERIFY(cache.load(first, countingLoader(&calls)).cacheHit);
    QVERIFY(!cache.load(request(QStringLiteral("/project/background.png"), 10, 100, QByteArray(), MapEditorBackgroundAssetFormat::Raster, QStringLiteral("gamma=1.2")), countingLoader(&calls)).cacheHit);

    QCOMPARE(calls, 2);
    QCOMPARE(cache.stats().entryCount, 2);
    QCOMPARE(cache.stats().hits, 1ULL);
}

void MapEditorBackgroundAssetCacheTest::storesWorkerResultForLaterLookup()
{
    MapEditorBackgroundAssetCache cache(16);
    const MapEditorBackgroundAssetRequest assetRequest = request(QStringLiteral("/project/background.png"));
    QVERIFY(!cache.find(assetRequest).cacheHit);

    QVERIFY(cache.store(assetRequest, MapEditorBackgroundAssetLoadResult{
                                          .payload = std::make_shared<const QByteArray>(QByteArray("asset")),
                                          .byteCost = 4}));

    const MapEditorBackgroundAssetCacheResult result = cache.find(assetRequest);
    QVERIFY(result.cacheHit);
    QVERIFY(result.cached);
    QCOMPARE(cache.stats().hits, 1ULL);
    QCOMPARE(cache.stats().misses, 1ULL);
}

void MapEditorBackgroundAssetCacheTest::reloadsWhenMetadataRevisionChanges()
{
    MapEditorBackgroundAssetCache cache(16);
    int calls = 0;
    QVERIFY(!cache.load(request(QStringLiteral("/project/background.png"), 10, 100), countingLoader(&calls)).cacheHit);
    QVERIFY(!cache.load(request(QStringLiteral("/project/background.png"), 11, 101), countingLoader(&calls)).cacheHit);
    QCOMPARE(calls, 2);
    QCOMPARE(cache.stats().entryCount, 1);
}

void MapEditorBackgroundAssetCacheTest::reloadsWhenContentFingerprintChangesAtSameTimestamp()
{
    MapEditorBackgroundAssetCache cache(16);
    int calls = 0;
    QVERIFY(!cache.load(request(QStringLiteral("/project/background.png"), 10, 100, QByteArray("first")), countingLoader(&calls)).cacheHit);
    QVERIFY(!cache.load(request(QStringLiteral("/project/background.png"), 10, 100, QByteArray("second")), countingLoader(&calls)).cacheHit);
    QCOMPARE(calls, 2);
}

void MapEditorBackgroundAssetCacheTest::evictsLeastRecentlyUsedEntryAtByteLimit()
{
    MapEditorBackgroundAssetCache cache(8);
    int calls = 0;
    const auto loader = countingLoader(&calls, 4);
    cache.load(request(QStringLiteral("/project/a.png")), loader);
    cache.load(request(QStringLiteral("/project/b.png")), loader);
    QVERIFY(cache.load(request(QStringLiteral("/project/a.png")), loader).cacheHit);
    cache.load(request(QStringLiteral("/project/c.png")), loader);

    QVERIFY(cache.load(request(QStringLiteral("/project/a.png")), loader).cacheHit);
    QVERIFY(!cache.load(request(QStringLiteral("/project/b.png")), loader).cacheHit);
    QCOMPARE(cache.stats().evictions, 2ULL);
}

void MapEditorBackgroundAssetCacheTest::doesNotStoreOversizedEntries()
{
    MapEditorBackgroundAssetCache cache(4);
    int calls = 0;
    const auto loader = countingLoader(&calls, 5);
    QVERIFY(!cache.load(request(QStringLiteral("/project/large.png")), loader).cached);
    QVERIFY(!cache.load(request(QStringLiteral("/project/large.png")), loader).cached);
    QCOMPARE(calls, 2);
    QCOMPARE(cache.stats().entryCount, 0);
}

void MapEditorBackgroundAssetCacheTest::invalidatesAllFormatsForSource()
{
    MapEditorBackgroundAssetCache cache(16);
    int calls = 0;
    const auto loader = countingLoader(&calls, 4);
    cache.load(request(QStringLiteral("/project/background"), 10, 100, QByteArray(), MapEditorBackgroundAssetFormat::Raster), loader);
    cache.load(request(QStringLiteral("/project/background"), 10, 100, QByteArray(), MapEditorBackgroundAssetFormat::Svg), loader);
    cache.load(request(QStringLiteral("/project/other")), loader);

    QCOMPARE(cache.invalidateSource(QStringLiteral("/project/background")), 2);
    QCOMPARE(cache.stats().entryCount, 1);
    QVERIFY(!cache.load(request(QStringLiteral("/project/background")), loader).cacheHit);
    QCOMPARE(calls, 4);
}

void MapEditorBackgroundAssetCacheTest::invalidatesRemovedSourceByStableIdentity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("background.svg"));
    QFile sourceFile(sourcePath);
    QVERIFY(sourceFile.open(QIODevice::WriteOnly));
    QVERIFY(sourceFile.write("svg") > 0);
    sourceFile.close();

    const QString sourceIdentity = MapEditorBackgroundAssetCache::canonicalSourceIdentity(sourcePath);
    QVERIFY(!sourceIdentity.isEmpty());
    MapEditorBackgroundAssetCache cache(16);
    int calls = 0;
    cache.load(request(sourceIdentity, 3, 100, QByteArray(), MapEditorBackgroundAssetFormat::Svg), countingLoader(&calls));
    QVERIFY(QFile::remove(sourcePath));

    QCOMPARE(MapEditorBackgroundAssetCache::canonicalSourceIdentity(sourcePath), sourceIdentity);
    QCOMPARE(cache.invalidateSource(MapEditorBackgroundAssetCache::canonicalSourceIdentity(sourcePath)), 1);
    QCOMPARE(cache.stats().entryCount, 0);
}

void MapEditorBackgroundAssetCacheTest::clearsEntriesAndReleasesPayloads()
{
    MapEditorBackgroundAssetCache cache(16);
    std::weak_ptr<const void> weakPayload;
    {
        const auto response = cache.load(request(QStringLiteral("/project/background.png")), [](const MapEditorBackgroundAssetRequest &) {
            return MapEditorBackgroundAssetLoadResult{
                .payload = std::make_shared<const QByteArray>(QByteArray("asset")),
                .byteCost = 4};
        });
        weakPayload = response.loadResult.payload;
    }

    QVERIFY(!weakPayload.expired());
    cache.clear();
    QVERIFY(weakPayload.expired());
    QCOMPARE(cache.stats().entryCount, 0);
    QCOMPARE(cache.stats().storedBytes, std::size_t(0));
}

void MapEditorBackgroundAssetCacheTest::retainsLoadErrorsAsCacheEntries()
{
    MapEditorBackgroundAssetCache cache(16);
    int calls = 0;
    const auto loader = [&calls](const MapEditorBackgroundAssetRequest &) {
        ++calls;
        return MapEditorBackgroundAssetLoadResult{.error = QStringLiteral("Unable to decode background.")};
    };

    const auto first = cache.load(request(QStringLiteral("/project/broken.svg"), 40, 200), loader);
    const auto second = cache.load(request(QStringLiteral("/project/broken.svg"), 40, 200), loader);
    QVERIFY(!first.cacheHit);
    QVERIFY(second.cacheHit);
    QCOMPARE(second.loadResult.error, QStringLiteral("Unable to decode background."));
    QCOMPARE(calls, 1);
}

int runMapEditorBackgroundAssetCacheTest(int argc, char **argv)
{
    MapEditorBackgroundAssetCacheTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorBackgroundAssetCacheTest.moc"
