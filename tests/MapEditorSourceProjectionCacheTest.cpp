#include "../src/app/text_editor/map_editor/MapEditorSourceProjectionCache.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorSourceProjectionCacheTest final : public QObject
{
    Q_OBJECT

private slots:
    void reusesImmutableSnapshotForOneRevision();
    void createsNewSnapshotForNewRevision();
};

void MapEditorSourceProjectionCacheTest::reusesImmutableSnapshotForOneRevision()
{
    MapEditorSourceProjectionCache cache;
    int buildCount = 0;
    const auto buildSnapshot = [&buildCount]() {
        ++buildCount;
        MapEditorSourceProjectionSnapshot snapshot;
        snapshot.sourceBounds = QRectF(1.0, 2.0, 3.0, 4.0);
        return snapshot;
    };

    const MapEditorSourceProjectionSnapshotPtr first = cache.snapshotFor(41, buildSnapshot);
    QVERIFY(first != nullptr);
    QCOMPARE(buildCount, 1);
    QVERIFY(!cache.lastRequestWasCacheHit());

    const MapEditorSourceProjectionSnapshotPtr repeated = cache.snapshotFor(41, buildSnapshot);
    QVERIFY(repeated == first);
    QCOMPARE(buildCount, 1);
    QVERIFY(cache.lastRequestWasCacheHit());
    QCOMPARE(repeated->revision, 41);
    QCOMPARE(repeated->sourceBounds, QRectF(1.0, 2.0, 3.0, 4.0));
}

void MapEditorSourceProjectionCacheTest::createsNewSnapshotForNewRevision()
{
    MapEditorSourceProjectionCache cache;
    const auto first = cache.snapshotFor(41, []() {
        MapEditorSourceProjectionSnapshot snapshot;
        snapshot.sourceBounds = QRectF(1.0, 1.0, 1.0, 1.0);
        return snapshot;
    });
    const auto second = cache.snapshotFor(42, []() {
        MapEditorSourceProjectionSnapshot snapshot;
        snapshot.sourceBounds = QRectF(2.0, 2.0, 2.0, 2.0);
        return snapshot;
    });

    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY(first != second);
    QCOMPARE(first->revision, 41);
    QCOMPARE(second->revision, 42);
    QCOMPARE(second->sourceBounds, QRectF(2.0, 2.0, 2.0, 2.0));
    QVERIFY(!cache.lastRequestWasCacheHit());
}
}

int runMapEditorSourceProjectionCacheTest(int argc, char **argv)
{
    MapEditorSourceProjectionCacheTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorSourceProjectionCacheTest.moc"
