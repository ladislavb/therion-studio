#include "../../src/core/TherionXviParser.h"

#include <QTemporaryFile>
#include <QtMath>
#include <QtTest/QtTest>

using namespace TherionStudio;

class TherionXviParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesText();
    void parsesMixedLineEndings();
    void parsesFile();
    void parsesStationNameFromThirdField();
    void preservesDuplicateStationOrder();
    void doesNotTreatXviGridsHeaderAsGridDefinition();
    void rejectsInvalidContent();
    void preservesSketchLineTokens();
};

namespace
{
bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.0001)
{
    return qAbs(a - b) <= epsilon;
}
}

void TherionXviParserTest::parsesText()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {-10 20 5 0 0 10 4 3}\n"
        "set XVIstations {\n"
        "  {1 2 station.alpha}\n"
        "  {3 4 station.beta}\n"
        "}\n"
        "set XVIshots {\n"
        "  {0 0 10 10}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {line 0 0 10 0 10 10}\n"
        "}\n");

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentText(xviText, &document));
    QVERIFY(document.hasGridOrigin);
    QVERIFY(nearlyEqual(document.gridOrigin.x(), -10.0));
    QVERIFY(nearlyEqual(document.gridOrigin.y(), 20.0));
    QVERIFY(document.hasGridDefinition);
    QCOMPARE(document.gridCountX, 4);
    QCOMPARE(document.gridCountY, 3);
    QCOMPARE(document.stations.size(), 2);
    QCOMPARE(document.stationEntries.size(), 2);
    QCOMPARE(document.shots.size(), 1);
    QCOMPARE(document.sketchLines.size(), 1);
    QCOMPARE(document.sketchLines.first().points.size(), 3);
    QCOMPARE(document.sketchLines.first().colorToken, QStringLiteral("line"));
}

void TherionXviParserTest::parsesMixedLineEndings()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {-10 20 5 0 0 10 4 3}\r"
        "set XVIstations {\r\n"
        "  {1 2 station.alpha}\n"
        "}\r"
        "set XVIshots {\r\n"
        "  {0 0 10 10}\n"
        "}\n");

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentText(xviText, &document));
    QVERIFY(document.hasGridDefinition);
    QCOMPARE(document.stationEntries.size(), 1);
    QCOMPARE(document.shots.size(), 1);
    QVERIFY(document.stations.contains(QStringLiteral("station.alpha")));
}

void TherionXviParserTest::parsesFile()
{
    QTemporaryFile temporaryFile;
    temporaryFile.setAutoRemove(true);
    QVERIFY(temporaryFile.open());

    const QByteArray xviBytes =
        "set XVIgrid {0 0 1 0 0 1 2 2}\n"
        "set XVIstations {\n"
        "  {0 0 root}\n"
        "}\n"
        "set XVIshots {\n"
        "  {0 0 1 1}\n"
        "}\n";
    QCOMPARE(temporaryFile.write(xviBytes), xviBytes.size());
    QVERIFY(temporaryFile.flush());

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentFile(temporaryFile.fileName(), &document));
    QVERIFY(document.hasGridOrigin);
    QVERIFY(document.hasGridDefinition);
    QVERIFY(document.stations.contains(QStringLiteral("root")));
    QCOMPARE(document.shots.size(), 1);
}

void TherionXviParserTest::parsesStationNameFromThirdField()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {0 0 1 0 0 1 2 2}\n"
        "set XVIstations {\n"
        "  {1950.0348031500002 1497.5492126 1.4 trailing-token}\n"
        "}\n");

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentText(xviText, &document));
    QVERIFY(document.stations.contains(QStringLiteral("1.4")));
    QCOMPARE(document.stationEntries.size(), 1);
    QVERIFY(!document.stations.contains(QStringLiteral("trailing-token")));
    const QPointF station = document.stations.value(QStringLiteral("1.4"));
    QVERIFY(nearlyEqual(station.x(), 1950.0348031500002));
    QVERIFY(nearlyEqual(station.y(), 1497.5492126));
}

void TherionXviParserTest::preservesDuplicateStationOrder()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {-2651.57480315 -4006.2992126 78.7401574803 0.0 0.0 78.7401574803 76.0 98.0}\n"
        "set XVIstations {\n"
        "  {-701.54 -2508.75 1.4}\n"
        "  {253.15 -2651.97 1.4}\n"
        "}\n"
        "set XVIshots {\n"
        "  {-701.54 -2508.75 253.15 -2651.97}\n"
        "}\n");

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentText(xviText, &document));
    QCOMPARE(document.stationEntries.size(), 2);
    QCOMPARE(document.stationEntries.first().name, QStringLiteral("1.4"));
    const QPointF firstStation = document.stationEntries.first().position;
    const QPointF secondStation = document.stationEntries.at(1).position;
    QVERIFY(nearlyEqual(firstStation.x(), -701.54));
    QVERIFY(nearlyEqual(firstStation.y(), -2508.75));
    QVERIFY(nearlyEqual(secondStation.x(), 253.15));
    QVERIFY(nearlyEqual(secondStation.y(), -2651.97));
    const QPointF lookupStation = document.stations.value(QStringLiteral("1.4"));
    QVERIFY(nearlyEqual(lookupStation.x(), -701.54));
    QVERIFY(nearlyEqual(lookupStation.y(), -2508.75));
}

void TherionXviParserTest::doesNotTreatXviGridsHeaderAsGridDefinition()
{
    const QString xviText = QStringLiteral(
        "set XVIgrids {1.0 m}\n"
        "set XVIstations {\n"
        "  {862.52 3307.56 5.1}\n"
        "}\n"
        "set XVIgrid {219.37007874 2224.40944882 157.480314961 0.0 0.0 157.480314961 11 9}\n");

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentText(xviText, &document));
    QVERIFY(document.hasGridOrigin);
    QVERIFY(document.hasGridDefinition);
    QVERIFY(nearlyEqual(document.gridOrigin.x(), 219.37007874));
    QVERIFY(nearlyEqual(document.gridOrigin.y(), 2224.40944882));
    QCOMPARE(document.gridCountX, 11);
    QCOMPARE(document.gridCountY, 9);
}

void TherionXviParserTest::rejectsInvalidContent()
{
    const QString invalidText = QStringLiteral("set XVIgrid {0 0 1 0}\n");
    TherionXviDocument document;
    QVERIFY(!parseTherionXviDocumentText(invalidText, &document));
}

void TherionXviParserTest::preservesSketchLineTokens()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {0 0 1 0 0 1 2 2}\n"
        "set XVIsketchlines {\n"
        "  {connect 0 0 1 1 2 2}\n"
        "}\n");

    TherionXviDocument document;
    QVERIFY(parseTherionXviDocumentText(xviText, &document));
    QCOMPARE(document.sketchLines.size(), 1);
    QCOMPARE(document.sketchLines.first().colorToken, QStringLiteral("connect"));
    QCOMPARE(document.sketchLines.first().points.size(), 3);
}

int runTherionXviParserTest(int argc, char **argv)
{
    TherionXviParserTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionXviParserTest.moc"
