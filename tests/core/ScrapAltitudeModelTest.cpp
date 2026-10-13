#include "../../src/core/ScrapAltitudeModel.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class ScrapAltitudeModelTest final : public QObject
{
    Q_OBJECT
private slots:
    void normalizesLoxNamesToProjectStationKeys()
    {
        ThreeDViewerSceneModel scene;
        scene.surveys = {{1, 0, QStringLiteral("Cave"), {}}, {2, 1, QStringLiteral("Branch"), {}}};
        scene.stations = {
            {1, 2, QStringLiteral("A"), {}, {0, 0, 300}, false, true},
            {2, 2, QStringLiteral("B"), {}, {0, 0, 280}},
            {3, 2, QStringLiteral("DuP"), {}, {0, 0, 250}},
            {4, 2, QStringLiteral("dup"), {}, {0, 0, 260}}};
        const auto stations = scrapAltitudeStations(scene);
        QVERIFY(stations.heights.contains(QStringLiteral("a@branch.cave")));
        QVERIFY(!stations.heights.contains(QStringLiteral("dup@branch.cave")));
        const auto height = scrapAltitudeForReferences(
            QStringLiteral("scrap"), {QStringLiteral("a@branch.cave"), QStringLiteral("b@branch.cave")}, stations);
        QVERIFY(height.relativeHeightMetres);
        QCOMPARE(*height.relativeHeightMetres, -10.0);
    }
    void rejectsDuplicateStationsAndChoosesCanonicalEntrance()
    {
        ThreeDViewerSceneModel scene;
        scene.surveys = {{1, 0, QStringLiteral("cave"), {}}};
        scene.stations = {
            {1, 1, QStringLiteral("z"), {}, {0, 0, 500}, false, true},
            {2, 1, QStringLiteral("a"), {}, {0, 0, 300}, false, true},
            {3, 1, QStringLiteral("dup"), {}, {0, 0, 280}},
            {4, 1, QStringLiteral("dup"), {}, {0, 0, 280}}};
        const auto stations = scrapAltitudeStations(scene);
        QCOMPARE(stations.entranceReference, QStringLiteral("a@cave"));
        QVERIFY(!stations.heights.contains(QStringLiteral("dup@cave")));
        const auto equal = scrapAltitudeForReferences(QStringLiteral("one"), {QStringLiteral("a@cave")}, stations);
        QCOMPARE(equal.palettePosition,
            scrapAltitudeForReferences(QStringLiteral("two"), {QStringLiteral("a@cave")}, stations).palettePosition);
        QVERIFY(!scrapAltitudeForReferences(QStringLiteral("bad"), {QStringLiteral("dup@cave")}, stations).relativeHeightMetres);
    }
    void neverClaimsAnAbsoluteDatumAndRejectsPartialMappings()
    {
        ScrapAltitudeStations stations;
        stations.heights = {{QStringLiteral("a"), 350}};
        const auto noEntrance = scrapAltitudeForReferences(QStringLiteral("s"), {QStringLiteral("a")}, stations);
        QVERIFY(!noEntrance.relativeHeightMetres.has_value());
        stations.entranceHeightMetres = 350;
        const auto partial = scrapAltitudeForReferences(QStringLiteral("s"), {QStringLiteral("a"), QStringLiteral("missing")}, stations);
        QVERIFY(!partial.relativeHeightMetres.has_value());
        QCOMPARE(partial.palettePosition, noEntrance.palettePosition);
        QCOMPARE(partial.mappedStations, 1);
        QVERIFY(partial.palettePosition != scrapAltitudeForReferences(QStringLiteral("other"), {}, stations).palettePosition);
    }
    void averagesUniqueMappedStationsRelativeToEntrance()
    {
        ThreeDViewerSceneModel scene;
        scene.surveys = {{1, 0, QStringLiteral("cave"), {}}};
        scene.stations = {
            {1, 1, QStringLiteral("a"), {}, {0, 0, 300}, false, true},
            {2, 1, QStringLiteral("b"), {}, {0, 0, 280}}};
        const auto stations = scrapAltitudeStations(scene);
        const auto result = scrapAltitudeForReferences(
            QStringLiteral("scrap@cave"),
            {QStringLiteral("a@cave"), QStringLiteral("b@cave"), QStringLiteral("b@cave")},
            stations);
        QVERIFY(result.relativeHeightMetres.has_value());
        QCOMPARE(*result.relativeHeightMetres, -10.0);
        QCOMPARE(result.mappedStations, 2);
        QCOMPARE(stations.entranceReference, QStringLiteral("a@cave"));
    }
};

int runScrapAltitudeModelTest(int argc, char **argv)
{
    ScrapAltitudeModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ScrapAltitudeModelTest.moc"
