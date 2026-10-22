#include "../src/app/text_editor/map_editor/MapEditorMoveToScrapPlanner.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
// Lines: 2 scrap a, 3 point, 4-7 line wall-1, 8-10 area, 11 point -id p1, 12 endscrap,
//        13 scrap b, 14 point, 15 endscrap.
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
    "point 3 3 label -id p1\n"
    "endscrap\n"
    "scrap b -projection plan\n"
    "point 5 5 label\n"
    "endscrap\n");

class MapEditorMoveToScrapPlannerTest : public QObject
{
    Q_OBJECT

private slots:
    void movesPointToEndOfLaterScrap();
    void movesPointToEarlierScrap();
    void movesAreaWithItsBorderLines();
    void movesSoleBorderLineWithItsArea();
    void refusesOneOfSeveralBorderLines();
    void refusesSharedBorderLine();
    void listsOtherScrapsWithReasons();
    void preservesCrLfLineEndings();
};

void MapEditorMoveToScrapPlannerTest::movesPointToEndOfLaterScrap()
{
    const MapEditorObjectMovePlan plan = planMapEditorMoveToScrap(kTwoScraps, 3, 13);
    QVERIFY2(plan.resolved, qPrintable(plan.errorMessage));
    QVERIFY(plan.changed);
    QCOMPARE(plan.movedText,
             QStringLiteral("encoding utf-8\n"
                            "scrap a -projection plan\n"
                            "line wall -id wall-1 -close on\n"
                            "  0 0\n"
                            "  10 0\n"
                            "endline\n"
                            "area water\n"
                            "  wall-1\n"
                            "endarea\n"
                            "point 3 3 label -id p1\n"
                            "endscrap\n"
                            "scrap b -projection plan\n"
                            "point 5 5 label\n"
                            "point 0 0 station -name A1\n"
                            "endscrap\n"));
    QCOMPARE(plan.insertBeforeLineAfterRemoval, 14);
}

void MapEditorMoveToScrapPlannerTest::movesPointToEarlierScrap()
{
    const QString text = QStringLiteral(
        "scrap a\n"
        "point 0 0 station\n"
        "endscrap\n"
        "scrap b\n"
        "point 5 5 station\n"
        "endscrap\n");
    const MapEditorObjectMovePlan earlier = planMapEditorMoveToScrap(text, 5, 1);
    QVERIFY2(earlier.resolved, qPrintable(earlier.errorMessage));
    QCOMPARE(earlier.movedText,
             QStringLiteral("scrap a\n"
                            "point 0 0 station\n"
                            "point 5 5 station\n"
                            "endscrap\n"
                            "scrap b\n"
                            "endscrap\n"));
    QCOMPARE(earlier.insertBeforeLineAfterRemoval, 3);
}

void MapEditorMoveToScrapPlannerTest::movesAreaWithItsBorderLines()
{
    const MapEditorObjectMovePlan plan = planMapEditorMoveToScrap(kTwoScraps, 8, 13);
    QVERIFY2(plan.resolved, qPrintable(plan.errorMessage));
    QCOMPARE(plan.movedText,
             QStringLiteral("encoding utf-8\n"
                            "scrap a -projection plan\n"
                            "point 0 0 station -name A1\n"
                            "point 3 3 label -id p1\n"
                            "endscrap\n"
                            "scrap b -projection plan\n"
                            "point 5 5 label\n"
                            "line wall -id wall-1 -close on\n"
                            "  0 0\n"
                            "  10 0\n"
                            "endline\n"
                            "area water\n"
                            "  wall-1\n"
                            "endarea\n"
                            "endscrap\n"));
    // The selection follows the area, which now starts after its border line.
    QCOMPARE(plan.insertBeforeLineAfterRemoval, 12);
}

void MapEditorMoveToScrapPlannerTest::movesSoleBorderLineWithItsArea()
{
    const MapEditorObjectMovePlan linePlan = planMapEditorMoveToScrap(kTwoScraps, 4, 13);
    QVERIFY2(linePlan.resolved, qPrintable(linePlan.errorMessage));
    QCOMPARE(linePlan.movedText, planMapEditorMoveToScrap(kTwoScraps, 8, 13).movedText);
    // The selection follows the line, which now starts the moved group.
    QCOMPARE(linePlan.insertBeforeLineAfterRemoval, 8);
}

void MapEditorMoveToScrapPlannerTest::refusesOneOfSeveralBorderLines()
{
    const QString text = QStringLiteral(
        "scrap a\n"
        "line wall -id wall-1\n"
        "  0 0\n"
        "  10 0\n"
        "endline\n"
        "line wall -id wall-2\n"
        "  10 0\n"
        "  0 0\n"
        "endline\n"
        "area water\n"
        "  wall-1\n"
        "  wall-2\n"
        "endarea\n"
        "endscrap\n"
        "scrap b\n"
        "endscrap\n");
    const MapEditorObjectMovePlan plan = planMapEditorMoveToScrap(text, 2, 15);
    QVERIFY(!plan.resolved);
    QVERIFY(!plan.errorMessage.isEmpty());

    const MapEditorObjectMovePlan areaPlan = planMapEditorMoveToScrap(text, 10, 15);
    QVERIFY2(areaPlan.resolved, qPrintable(areaPlan.errorMessage));
    QCOMPARE(areaPlan.movedText,
             QStringLiteral("scrap a\n"
                            "endscrap\n"
                            "scrap b\n"
                            "line wall -id wall-1\n"
                            "  0 0\n"
                            "  10 0\n"
                            "endline\n"
                            "line wall -id wall-2\n"
                            "  10 0\n"
                            "  0 0\n"
                            "endline\n"
                            "area water\n"
                            "  wall-1\n"
                            "  wall-2\n"
                            "endarea\n"
                            "endscrap\n"));
}

void MapEditorMoveToScrapPlannerTest::refusesSharedBorderLine()
{
    const QString text = QStringLiteral(
        "scrap a\n"
        "line wall -id wall-1 -close on\n"
        "  0 0\n"
        "  10 0\n"
        "endline\n"
        "area water\n"
        "  wall-1\n"
        "endarea\n"
        "area sand\n"
        "  wall-1\n"
        "endarea\n"
        "endscrap\n"
        "scrap b\n"
        "endscrap\n");
    const MapEditorObjectMovePlan plan = planMapEditorMoveToScrap(text, 6, 13);
    QVERIFY(!plan.resolved);
    QVERIFY(!plan.errorMessage.isEmpty());
}

void MapEditorMoveToScrapPlannerTest::listsOtherScrapsWithReasons()
{
    const QVector<MapEditorMoveToScrapTarget> pointTargets = mapEditorMoveToScrapTargets(kTwoScraps, 3);
    QCOMPARE(pointTargets.size(), 1);
    QCOMPARE(pointTargets.constFirst().scrapLineNumber, 13);
    QCOMPARE(pointTargets.constFirst().scrapId, QStringLiteral("b"));
    QVERIFY(pointTargets.constFirst().blockedReason.isEmpty());

    QVERIFY(mapEditorMoveToScrapTargets(kTwoScraps, 2).isEmpty());
    QVERIFY(mapEditorMoveToScrapTargets(kTwoScraps, 1).isEmpty());
}

void MapEditorMoveToScrapPlannerTest::preservesCrLfLineEndings()
{
    const QString text = QStringLiteral("scrap a\r\npoint 0 0 station\r\nendscrap\r\nscrap b\r\nendscrap\r\n");
    const MapEditorObjectMovePlan plan = planMapEditorMoveToScrap(text, 2, 4);
    QVERIFY2(plan.resolved, qPrintable(plan.errorMessage));
    QCOMPARE(plan.movedText, QStringLiteral("scrap a\r\nendscrap\r\nscrap b\r\npoint 0 0 station\r\nendscrap\r\n"));
}
}

int runMapEditorMoveToScrapPlannerTest(int argc, char **argv)
{
    MapEditorMoveToScrapPlannerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorMoveToScrapPlannerTest.moc"
