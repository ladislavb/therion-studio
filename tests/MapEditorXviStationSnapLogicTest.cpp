#include "../src/app/text_editor/map_editor/MapEditorXviStationSnapLogic.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorXviStationSnapLogicTest final : public QObject
{
    Q_OBJECT

private slots:
    void selectsSingleNearbyStation();
    void ignoresStationsOutsideRadius();
    void rejectsAmbiguousNearbyStations();
    void acceptsDuplicateLayerRepresentations();
    void listsEquatedAliasesNearestFirst();
};

void MapEditorXviStationSnapLogicTest::selectsSingleNearbyStation()
{
    const QVector<MapEditorXviStationSnapCandidate> candidates{
        {QStringLiteral("12@survey"), QPointF(100.0, 200.0), QPointF(40.0, 60.0)}};

    const auto snap = mapEditorUniqueXviStationSnap(QPointF(48.0, 66.0), candidates, 12.0);

    QVERIFY(snap.has_value());
    QCOMPARE(snap->name, QStringLiteral("12@survey"));
    QCOMPARE(snap->scenePosition, QPointF(100.0, 200.0));
}

void MapEditorXviStationSnapLogicTest::ignoresStationsOutsideRadius()
{
    const QVector<MapEditorXviStationSnapCandidate> candidates{
        {QStringLiteral("12"), QPointF(100.0, 200.0), QPointF(40.0, 60.0)}};

    QVERIFY(!mapEditorUniqueXviStationSnap(QPointF(53.0, 60.0), candidates, 12.0).has_value());
}

void MapEditorXviStationSnapLogicTest::rejectsAmbiguousNearbyStations()
{
    const QVector<MapEditorXviStationSnapCandidate> candidates{
        {QStringLiteral("12"), QPointF(100.0, 200.0), QPointF(40.0, 60.0)},
        {QStringLiteral("13"), QPointF(120.0, 200.0), QPointF(46.0, 60.0)}};

    QVERIFY(!mapEditorUniqueXviStationSnap(QPointF(43.0, 60.0), candidates, 12.0).has_value());
}

void MapEditorXviStationSnapLogicTest::acceptsDuplicateLayerRepresentations()
{
    const QVector<MapEditorXviStationSnapCandidate> candidates{
        {QStringLiteral("12"), QPointF(100.0, 200.0), QPointF(40.0, 60.0)},
        {QStringLiteral("12"), QPointF(100.0, 200.0), QPointF(40.0, 60.0)}};

    const auto snap = mapEditorUniqueXviStationSnap(QPointF(40.0, 60.0), candidates, 12.0);

    QVERIFY(snap.has_value());
    QCOMPARE(snap->name, QStringLiteral("12"));
}

void MapEditorXviStationSnapLogicTest::listsEquatedAliasesNearestFirst()
{
    // Jasovska v041: 5109@Jaskyna_Okno and 5109@pripojenie_vchodov share one XVI position.
    const QVector<MapEditorXviStationSnapCandidate> candidates{
        {QStringLiteral("149@a"), QPointF(352.0, 296.0), QPointF(50.0, 60.0)},
        {QStringLiteral("5109@a"), QPointF(358.0, 307.0), QPointF(44.0, 60.0)},
        {QStringLiteral("5109@b"), QPointF(358.0, 307.0), QPointF(44.0, 60.0)}};

    const QVector<MapEditorXviStationSnapCandidate> nearby =
        mapEditorNearbyXviStationSnaps(QPointF(43.0, 60.0), candidates, 12.0);

    QCOMPARE(nearby.size(), 3);
    QCOMPARE(nearby.at(0).name, QStringLiteral("5109@a"));
    QCOMPARE(nearby.at(1).name, QStringLiteral("5109@b"));
    QCOMPARE(nearby.at(2).name, QStringLiteral("149@a"));
    QVERIFY(!mapEditorUniqueXviStationSnap(QPointF(43.0, 60.0), candidates, 12.0).has_value());
}
} // namespace

int runMapEditorXviStationSnapLogicTest(int argc, char **argv)
{
    MapEditorXviStationSnapLogicTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorXviStationSnapLogicTest.moc"
