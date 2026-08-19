#include "../../src/core/TherionXviParser.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class TherionXviPassageTest final : public QObject
{
    Q_OBJECT

private slots:
    void retainsLrudPassageOutlineFromShotRecord();
    void doesNotCreatePassageOutlineForCentrelineOnlyShot();
};

void TherionXviPassageTest::retainsLrudPassageOutlineFromShotRecord()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {0 0 1 0 0 1 10 10}\n"
        "set XVIstations {\n"
        "  {0 0 from}\n"
        "  {10 0 to}\n"
        "}\n"
        "set XVIshots {\n"
        "  {0 0 10 0 0 3 10 4 10 -5 0 -2}\n"
        "}\n");

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentText(xviText, &document));
    QCOMPARE(document.shots.size(), 1);

    const TherionXviShot &shot = document.shots.first();
    QCOMPARE(shot.centerLine.p1(), QPointF(0.0, 0.0));
    QCOMPARE(shot.centerLine.p2(), QPointF(10.0, 0.0));
    QCOMPARE(shot.passageOutline,
             QVector<QPointF>({QPointF(0.0, 3.0),
                               QPointF(10.0, 4.0),
                               QPointF(10.0, -5.0),
                               QPointF(0.0, -2.0)}));
}

void TherionXviPassageTest::doesNotCreatePassageOutlineForCentrelineOnlyShot()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {0 0 1 0 0 1 10 10}\n"
        "set XVIshots {\n"
        "  {0 0 10 0}\n"
        "}\n");

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentText(xviText, &document));
    QCOMPARE(document.shots.size(), 1);
    QVERIFY(document.shots.first().passageOutline.isEmpty());
}

int runTherionXviPassageTest(int argc, char **argv)
{
    TherionXviPassageTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionXviPassageTest.moc"
