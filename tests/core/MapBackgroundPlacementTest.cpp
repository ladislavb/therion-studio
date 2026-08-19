#include "../../src/core/MapBackgroundPlacement.h"
#include "../../src/core/TherionXviParser.h"

#include <QtMath>
#include <QtTest/QtTest>

using namespace TherionStudio;

class MapBackgroundPlacementTest final : public QObject
{
    Q_OBJECT

private slots:
    void anchorsClopyFallbackAtTopEdge();
    void usesMatchingAreaAdjustBounds();
    void appliesOffsetTopEdgeAnchor();
    void doesNotOverrideMovedPlacementWithAreaAdjust();
    void resolvesXviRootStationPlacement();
    void fallsBackWhenXviRootStationIsMissing();
    void resolvesXviRootStationCanonicalVariant();
    void resolvesUniqueUnqualifiedXviRootStation();
    void rejectsAmbiguousUnqualifiedXviRootStation();
    void usesFirstDuplicateXviStationFromParser();
    void preservesPocketTopoPlacedCoordinatesForXviRootStation();
    void usesFirstMatchingStationForPocketTopoInsertion();
    void fallsBackToFirstStationForPocketTopoInsertion();
};

namespace
{
bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.0001)
{
    return qAbs(a - b) <= epsilon;
}
}

void MapBackgroundPlacementTest::anchorsClopyFallbackAtTopEdge()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(0.0, 0.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = true;
    areaAdjust.modelRect = QRectF(QPointF(-128.0, -1152.0), QPointF(851.0, 128.0));

    const QRectF rect = resolveRasterModelRect(QSizeF(723.0, 1024.0), metadata, areaAdjust);
    QVERIFY(rect.isValid());
    QVERIFY(nearlyEqual(rect.left(), 0.0));
    QVERIFY(nearlyEqual(rect.top(), -1024.0));
    QVERIFY(nearlyEqual(rect.right(), 723.0));
    QVERIFY(nearlyEqual(rect.bottom(), 0.0));
}

void MapBackgroundPlacementTest::usesMatchingAreaAdjustBounds()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(0.0, 0.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = true;
    areaAdjust.modelRect = QRectF(QPointF(-128.0, -860.0), QPointF(736.0, 132.0));

    const QRectF rect = resolveRasterModelRect(QSizeF(860.0, 988.0), metadata, areaAdjust);
    QVERIFY(rect.isValid());
    QVERIFY(nearlyEqual(rect.left(), -128.0));
    QVERIFY(nearlyEqual(rect.top(), -860.0));
    QVERIFY(nearlyEqual(rect.right(), 736.0));
    QVERIFY(nearlyEqual(rect.bottom(), 132.0));
}

void MapBackgroundPlacementTest::appliesOffsetTopEdgeAnchor()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(232.0, -145.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    const QRectF rect = resolveRasterModelRect(QSizeF(772.0, 337.0), metadata, areaAdjust);
    QVERIFY(rect.isValid());
    QVERIFY(nearlyEqual(rect.left(), 232.0));
    QVERIFY(nearlyEqual(rect.top(), -482.0));
    QVERIFY(nearlyEqual(rect.right(), 1004.0));
    QVERIFY(nearlyEqual(rect.bottom(), -145.0));
}

void MapBackgroundPlacementTest::doesNotOverrideMovedPlacementWithAreaAdjust()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(120.0, 75.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = true;
    areaAdjust.modelRect = QRectF(QPointF(0.0, -40.0), QPointF(80.0, 0.0));

    const QRectF rect = resolveRasterModelRect(QSizeF(80.0, 40.0), metadata, areaAdjust);
    QVERIFY(rect.isValid());
    QVERIFY(nearlyEqual(rect.left(), 120.0));
    QVERIFY(nearlyEqual(rect.top(), 35.0));
    QVERIFY(nearlyEqual(rect.right(), 200.0));
    QVERIFY(nearlyEqual(rect.bottom(), 75.0));
}

void MapBackgroundPlacementTest::resolvesXviRootStationPlacement()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("ignore.0"), QPointF(-112.40, -198.33));
    stations.insert(QStringLiteral("ignore.1"), QPointF(-60.04, -27.07));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(59.057, 59.052);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("ignore.0");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(-171.457, -257.382), stations, metadata);
    QVERIFY(result.rootRequested);
    QVERIFY(result.rootResolved);
    QVERIFY(nearlyEqual(result.modelOffset.x(), 171.457));
    QVERIFY(nearlyEqual(result.modelOffset.y(), 257.382));
}

void MapBackgroundPlacementTest::fallsBackWhenXviRootStationIsMissing()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("other"), QPointF(10.0, 20.0));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(59.057, 59.052);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("missing");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(-171.457, -257.382), stations, metadata);
    QVERIFY(result.rootRequested);
    QVERIFY(!result.rootResolved);
    QVERIFY(nearlyEqual(result.modelOffset.x(), 230.514));
    QVERIFY(nearlyEqual(result.modelOffset.y(), 316.434));
}

void MapBackgroundPlacementTest::resolvesXviRootStationCanonicalVariant()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("0@create."), QPointF(197.83, -179.72));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(456.253, 60.18);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("0@create");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(0.0, 0.0), stations, metadata);
    QVERIFY(result.rootResolved);
    QCOMPARE(result.matchedRootStationName, QStringLiteral("0@create."));
    QVERIFY(nearlyEqual(result.modelOffset.x(), 258.423));
    QVERIFY(nearlyEqual(result.modelOffset.y(), 239.9));
}

void MapBackgroundPlacementTest::resolvesUniqueUnqualifiedXviRootStation()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("1.4@jablanica"), QPointF(1950.03480315, 1497.5492126));
    stations.insert(QStringLiteral("1.5@jablanica"), QPointF(1455.72480315, 1316.4492126));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(1950.03480315, 1497.5492126);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("1.4.");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(0.0, 0.0), stations, metadata);
    QVERIFY(result.rootResolved);
    QCOMPARE(result.matchedRootStationName, QStringLiteral("1.4@jablanica"));
    QVERIFY(nearlyEqual(result.modelOffset.x(), 0.0));
    QVERIFY(nearlyEqual(result.modelOffset.y(), 0.0));
}

void MapBackgroundPlacementTest::rejectsAmbiguousUnqualifiedXviRootStation()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("1.4@lower"), QPointF(10.0, 20.0));
    stations.insert(QStringLiteral("1.4@upper"), QPointF(30.0, 40.0));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(1950.03480315, 1497.5492126);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("1.4");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(100.0, 200.0), stations, metadata);
    QVERIFY(!result.rootResolved);
    QVERIFY(nearlyEqual(result.modelOffset.x(), 1850.03480315));
    QVERIFY(nearlyEqual(result.modelOffset.y(), 1297.5492126));
}

void MapBackgroundPlacementTest::usesFirstDuplicateXviStationFromParser()
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

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(1950.0348031500002, 1497.5492126);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("1.4");

    QVector<XviStationPlacementEntry> stationEntries;
    stationEntries.reserve(document.stationEntries.size());
    for (const TherionXviStation &station : document.stationEntries) {
        stationEntries.append(XviStationPlacementEntry{station.name, station.position});
    }

    const XviPlacementResult result = resolveXviModelOffset(document.gridOrigin, stationEntries, metadata);
    QVERIFY(result.rootResolved);
    QVERIFY(nearlyEqual(result.modelOffset.x(), 2651.57480315));
    QVERIFY(nearlyEqual(result.modelOffset.y(), 4006.2992126));
}

void MapBackgroundPlacementTest::preservesPocketTopoPlacedCoordinatesForXviRootStation()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("5.1"), QPointF(862.52, 3307.56));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(643.14992126, 1083.15055118);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("5.1");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(219.37007874, 2224.40944882), stations, metadata);
    QVERIFY(result.rootResolved);
    QVERIFY(nearlyEqual(result.modelOffset.x(), -219.37007874));
    QVERIFY(nearlyEqual(result.modelOffset.y(), -2224.40944882));

    const QPointF sampleSketchPoint = QPointF(1247.24, 3460.31) + result.modelOffset;
    QVERIFY(nearlyEqual(sampleSketchPoint.x(), 1027.86992126));
    QVERIFY(nearlyEqual(sampleSketchPoint.y(), 1235.90055118));
}

void MapBackgroundPlacementTest::usesFirstMatchingStationForPocketTopoInsertion()
{
    const QString documentText = QStringLiteral(
        "point 1247.24 3460.31 station 5.1\n"
        "point 999.0 888.0 station 9.9\n");
    const QVector<XviStationPlacementEntry> stationEntries{
        XviStationPlacementEntry{QStringLiteral("5.1"), QPointF(862.52, 3307.56)},
        XviStationPlacementEntry{QStringLiteral("9.9"), QPointF(120.0, 200.0)},
    };

    const XviBackgroundInsertionPlacement placement =
        resolvePocketTopoXviInsertionPlacement(stationEntries, documentText);
    QVERIFY(nearlyEqual(placement.basePosition.x(), 1247.24));
    QVERIFY(nearlyEqual(placement.basePosition.y(), 3460.31));
    QCOMPARE(placement.rootStationName, QStringLiteral("5.1"));
}

void MapBackgroundPlacementTest::fallsBackToFirstStationForPocketTopoInsertion()
{
    const QVector<XviStationPlacementEntry> stationEntries{
        XviStationPlacementEntry{QStringLiteral("5.1"), QPointF(862.52, 3307.56)},
        XviStationPlacementEntry{QStringLiteral("9.9"), QPointF(120.0, 200.0)},
    };

    const XviBackgroundInsertionPlacement placement =
        resolvePocketTopoXviInsertionPlacement(stationEntries, QStringLiteral("survey demo\nendsurvey\n"));
    QVERIFY(nearlyEqual(placement.basePosition.x(), 0.0));
    QVERIFY(nearlyEqual(placement.basePosition.y(), 0.0));
    QCOMPARE(placement.rootStationName, QStringLiteral("5.1"));
}

int runMapBackgroundPlacementTest(int argc, char **argv)
{
    MapBackgroundPlacementTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapBackgroundPlacementTest.moc"
