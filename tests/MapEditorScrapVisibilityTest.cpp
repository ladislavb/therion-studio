#include "../src/app/text_editor/map_editor/MapEditorScrapVisibility.h"
#include "../src/core/Th2GeometryProjection.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
// Lines: 2 scrap a, 3 point, 4 line, 8 area, 11 endscrap; 12 scrap b, 13 point, 14 endscrap.
const QString kTwoScraps = QStringLiteral(
    "encoding utf-8\n"
    "scrap a -projection plan\n"
    "point 0 0 station -name A1\n"
    "line wall -id wall-1 -close on\n"
    "  0 0\n"
    "  10 0\n"
    "endline\n"
    "area water\n"
    "  wall-1\n"
    "endarea\n"
    "endscrap\n"
    "scrap b -projection plan\n"
    "point 5 5 station -name B1\n"
    "endscrap\n");

class MapEditorScrapVisibilityTest : public QObject
{
    Q_OBJECT

private slots:
    void cyclesShownFadedHidden();
    void collectsObjectLinesPerScrap();
    void setsThisAndOtherScraps();
    void reportsMixedVisibilityAsNone();
    void removeClearAndRetainCoverBothSets();
};

void MapEditorScrapVisibilityTest::cyclesShownFadedHidden()
{
    QCOMPARE(nextMapEditorObjectVisibility(MapEditorObjectVisibility::Shown), MapEditorObjectVisibility::Faded);
    QCOMPARE(nextMapEditorObjectVisibility(MapEditorObjectVisibility::Faded), MapEditorObjectVisibility::Hidden);
    QCOMPARE(nextMapEditorObjectVisibility(MapEditorObjectVisibility::Hidden), MapEditorObjectVisibility::Shown);
}

void MapEditorScrapVisibilityTest::collectsObjectLinesPerScrap()
{
    const QVector<MapEditorScrapObjectLines> scraps =
        mapEditorScrapObjectLines(Th2GeometryProjection::fromText(kTwoScraps));
    QCOMPARE(scraps.size(), 2);
    QCOMPARE(scraps.at(0).scrapLineNumber, 2);
    QCOMPARE(scraps.at(0).lineNumbers, QVector<int>({2, 3, 4, 8}));
    QCOMPARE(scraps.at(1).scrapLineNumber, 12);
    QCOMPARE(scraps.at(1).lineNumbers, QVector<int>({12, 13}));

    QCOMPARE(mapEditorScrapLineForLine(scraps, 8), 2);
    QCOMPARE(mapEditorScrapLineForLine(scraps, 12), 12);
    QCOMPARE(mapEditorScrapLineForLine(scraps, 1), 0);
    QVERIFY(mapEditorHasOtherScraps(scraps, 2));
}

void MapEditorScrapVisibilityTest::setsThisAndOtherScraps()
{
    const QVector<MapEditorScrapObjectLines> scraps =
        mapEditorScrapObjectLines(Th2GeometryProjection::fromText(kTwoScraps));
    QSet<int> hidden;
    QSet<int> faded;
    MapEditorObjectVisibilityLines visibility(&hidden, &faded);

    setMapEditorScrapsVisibility(visibility,
                                 scraps,
                                 2,
                                 MapEditorScrapVisibilityTarget::OtherScraps,
                                 MapEditorObjectVisibility::Faded);
    QCOMPARE(faded, QSet<int>({12, 13}));
    QVERIFY(hidden.isEmpty());
    QCOMPARE(mapEditorScrapsVisibility(visibility, scraps, 2, MapEditorScrapVisibilityTarget::ThisScrap),
             std::optional<MapEditorObjectVisibility>(MapEditorObjectVisibility::Shown));
    QCOMPARE(mapEditorScrapsVisibility(visibility, scraps, 2, MapEditorScrapVisibilityTarget::OtherScraps),
             std::optional<MapEditorObjectVisibility>(MapEditorObjectVisibility::Faded));

    // Hiding a faded scrap moves its lines from the faded set to the hidden set.
    setMapEditorScrapsVisibility(visibility,
                                 scraps,
                                 12,
                                 MapEditorScrapVisibilityTarget::ThisScrap,
                                 MapEditorObjectVisibility::Hidden);
    QVERIFY(faded.isEmpty());
    QCOMPARE(hidden, QSet<int>({12, 13}));
}

void MapEditorScrapVisibilityTest::reportsMixedVisibilityAsNone()
{
    const QVector<MapEditorScrapObjectLines> scraps =
        mapEditorScrapObjectLines(Th2GeometryProjection::fromText(kTwoScraps));
    QSet<int> hidden{4};
    QSet<int> faded;
    const MapEditorObjectVisibilityLines visibility(&hidden, &faded);

    QCOMPARE(mapEditorScrapsVisibility(visibility, scraps, 2, MapEditorScrapVisibilityTarget::ThisScrap),
             std::nullopt);
}

void MapEditorScrapVisibilityTest::removeClearAndRetainCoverBothSets()
{
    QSet<int> hidden{3, 4};
    QSet<int> faded{12, 13};
    MapEditorObjectVisibilityLines visibility(&hidden, &faded);
    QVERIFY(!visibility.allShown());

    visibility.remove(4);
    visibility.remove(13);
    QCOMPARE(hidden, QSet<int>({3}));
    QCOMPARE(faded, QSet<int>({12}));

    visibility.retainOnly({12});
    QVERIFY(hidden.isEmpty());
    QCOMPARE(faded, QSet<int>({12}));

    visibility.clear();
    QVERIFY(visibility.allShown());
}
}

int runMapEditorScrapVisibilityTest(int argc, char **argv)
{
    MapEditorScrapVisibilityTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorScrapVisibilityTest.moc"
