#include "../src/core/MapBackgroundPlacement.h"
#include "../src/core/TherionXviParser.h"

#include <iostream>
#include <QtMath>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.0001)
{
    return qAbs(a - b) <= epsilon;
}

int runClopyFallbackAnchorTest()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(0.0, 0.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = true;
    areaAdjust.modelRect = QRectF(QPointF(-128.0, -1152.0), QPointF(851.0, 128.0));

    const QRectF rect = resolveRasterModelRect(QSizeF(723.0, 1024.0), metadata, areaAdjust);
    if (!expect(rect.isValid(), "Expected a valid model rect for clopy fallback test.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.left(), 0.0), "Expected clopy fallback left edge to be anchored at x=0.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.top(), -1024.0), "Expected clopy fallback top edge to be y=-1024.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.right(), 723.0), "Expected clopy fallback right edge to be x=723.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.bottom(), 0.0), "Expected clopy fallback bottom edge to be y=0.")) {
        return 1;
    }

    return 0;
}

int runAreaAdjustMatchTest()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(0.0, 0.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = true;
    areaAdjust.modelRect = QRectF(QPointF(-128.0, -860.0), QPointF(736.0, 132.0));

    const QSizeF paddedImageSize(860.0, 988.0);
    const QRectF rect = resolveRasterModelRect(paddedImageSize, metadata, areaAdjust);
    if (!expect(rect.isValid(), "Expected a valid model rect when area-adjust matches image dimensions.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.left(), -128.0), "Expected area-adjust match to preserve left edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.top(), -860.0), "Expected area-adjust match to preserve top edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.right(), 736.0), "Expected area-adjust match to preserve right edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.bottom(), 132.0), "Expected area-adjust match to preserve bottom edge.")) {
        return 1;
    }

    return 0;
}

int runOffsetAnchorTest()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(232.0, -145.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = false;

    const QRectF rect = resolveRasterModelRect(QSizeF(772.0, 337.0), metadata, areaAdjust);
    if (!expect(rect.isValid(), "Expected a valid model rect for offset-anchor test.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.left(), 232.0), "Expected offset-anchor left edge at x=232.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.top(), -482.0), "Expected offset-anchor top edge at y=-482.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.right(), 1004.0), "Expected offset-anchor right edge at x=1004.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.bottom(), -145.0), "Expected offset-anchor bottom edge at y=-145.")) {
        return 1;
    }

    return 0;
}

int runAreaAdjustMatchDoesNotOverrideMovedPlacementTest()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(120.0, 75.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = true;
    areaAdjust.modelRect = QRectF(QPointF(0.0, -40.0), QPointF(80.0, 0.0));

    const QRectF rect = resolveRasterModelRect(QSizeF(80.0, 40.0), metadata, areaAdjust);
    if (!expect(rect.isValid(), "Expected valid model rect for moved placement with matching area-adjust size.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.left(), 120.0), "Expected moved placement to preserve left edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.top(), 35.0), "Expected moved placement to preserve top edge from base position.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.right(), 200.0), "Expected moved placement to preserve right edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.bottom(), 75.0), "Expected moved placement to preserve bottom edge.")) {
        return 1;
    }

    return 0;
}

int runXviRootStationPlacementTest()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("ignore.0"), QPointF(-112.40, -198.33));
    stations.insert(QStringLiteral("ignore.1"), QPointF(-60.04, -27.07));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(59.057, 59.052);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("ignore.0");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(-171.457, -257.382), stations, metadata);
    if (!expect(result.rootRequested, "Expected XVI root-station placement to mark root as requested.")) {
        return 1;
    }
    if (!expect(result.rootResolved, "Expected exact XVI root station to resolve.")) {
        return 1;
    }
    if (!expect(nearlyEqual(result.modelOffset.x(), 171.457)
                    && nearlyEqual(result.modelOffset.y(), 257.382),
                "Expected XVI root-station offset to align the root station to metadata base position.")) {
        return 1;
    }

    return 0;
}

int runXviFallbackPlacementTest()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("other"), QPointF(10.0, 20.0));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(59.057, 59.052);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("missing");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(-171.457, -257.382), stations, metadata);
    if (!expect(result.rootRequested, "Expected missing XVI root to still mark root as requested.")) {
        return 1;
    }
    if (!expect(!result.rootResolved, "Expected missing XVI root station to remain unresolved.")) {
        return 1;
    }
    if (!expect(nearlyEqual(result.modelOffset.x(), 230.514)
                    && nearlyEqual(result.modelOffset.y(), 316.434),
                "Expected unresolved XVI root fallback to anchor metadata base position against grid origin.")) {
        return 1;
    }

    return 0;
}

int runXviRootStationCanonicalMatchTest()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("0@create."), QPointF(197.83, -179.72));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(456.253, 60.18);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("0@create");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(0.0, 0.0), stations, metadata);
    if (!expect(result.rootResolved, "Expected XVI root station to resolve across trailing namespace dot variant.")) {
        return 1;
    }
    if (!expect(result.matchedRootStationName == QStringLiteral("0@create."),
                "Expected XVI root station resolver to report the matched station key.")) {
        return 1;
    }
    if (!expect(nearlyEqual(result.modelOffset.x(), 258.423)
                    && nearlyEqual(result.modelOffset.y(), 239.9),
                "Expected canonical XVI root-station offset to use the matched station coordinate.")) {
        return 1;
    }

    return 0;
}

int runXviRootStationUnqualifiedMatchTest()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("1.4@jablanica"), QPointF(1950.03480315, 1497.5492126));
    stations.insert(QStringLiteral("1.5@jablanica"), QPointF(1455.72480315, 1316.4492126));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(1950.03480315, 1497.5492126);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("1.4.");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(0.0, 0.0), stations, metadata);
    if (!expect(result.rootResolved, "Expected unique unqualified XVI root station to resolve.")) {
        return 1;
    }
    if (!expect(result.matchedRootStationName == QStringLiteral("1.4@jablanica"),
                "Expected unique unqualified XVI root station resolver to report the matched station key.")) {
        return 1;
    }
    if (!expect(nearlyEqual(result.modelOffset.x(), 0.0)
                    && nearlyEqual(result.modelOffset.y(), 0.0),
                "Expected unique unqualified XVI root station to align station coordinate to metadata base position.")) {
        return 1;
    }

    return 0;
}

int runXviRootStationAmbiguousUnqualifiedMatchTest()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("1.4@lower"), QPointF(10.0, 20.0));
    stations.insert(QStringLiteral("1.4@upper"), QPointF(30.0, 40.0));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(1950.03480315, 1497.5492126);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("1.4");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(100.0, 200.0), stations, metadata);
    if (!expect(!result.rootResolved, "Expected ambiguous unqualified XVI root station to remain unresolved.")) {
        return 1;
    }
    if (!expect(nearlyEqual(result.modelOffset.x(), 1850.03480315)
                    && nearlyEqual(result.modelOffset.y(), 1297.5492126),
                "Expected ambiguous unqualified XVI root station to fall back to grid-origin anchoring.")) {
        return 1;
    }

    return 0;
}

int runXviParserPlacementUsesFirstDuplicateStationTest()
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
    if (!expect(parseTherionXviDocumentText(xviText, &document),
                "Expected sample-like XVI text to parse before placement resolution.")) {
        return 1;
    }

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
    if (!expect(result.rootResolved, "Expected duplicate XVI root station to resolve.")) {
        return 1;
    }
    if (!expect(nearlyEqual(result.modelOffset.x(), 2651.57480315)
                    && nearlyEqual(result.modelOffset.y(), 4006.2992126),
                "Expected duplicate XVI root placement to use the first station table match, as XTherion does.")) {
        return 1;
    }

    return 0;
}

int runXviRootStationPlacementKeepsPocketTopoPlacedCoordinatesTest()
{
    QHash<QString, QPointF> stations;
    stations.insert(QStringLiteral("5.1"), QPointF(862.52, 3307.56));

    XviPlacementMetadata metadata{};
    metadata.basePosition = QPointF(643.14992126, 1083.15055118);
    metadata.hasBasePosition = true;
    metadata.rootStationName = QStringLiteral("5.1");

    const XviPlacementResult result = resolveXviModelOffset(QPointF(219.37007874, 2224.40944882), stations, metadata);
    if (!expect(result.rootResolved, "Expected Babice XVI root station to resolve exactly.")) {
        return 1;
    }
    if (!expect(nearlyEqual(result.modelOffset.x(), -219.37007874)
                    && nearlyEqual(result.modelOffset.y(), -2224.40944882),
                "Expected Babice XVI placement to use XTherion root-station offset.")) {
        return 1;
    }

    const QPointF sampleSketchPoint = QPointF(1247.24, 3460.31) + result.modelOffset;
    if (!expect(nearlyEqual(sampleSketchPoint.x(), 1027.86992126)
                    && nearlyEqual(sampleSketchPoint.y(), 1235.90055118),
                "Expected Babice XVI placement offset to match XTherion placed coordinates before scene projection.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int rc = runClopyFallbackAnchorTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaAdjustMatchTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runOffsetAnchorTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaAdjustMatchDoesNotOverrideMovedPlacementTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviRootStationPlacementTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviFallbackPlacementTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviRootStationCanonicalMatchTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviRootStationUnqualifiedMatchTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviRootStationAmbiguousUnqualifiedMatchTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviParserPlacementUsesFirstDuplicateStationTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviRootStationPlacementKeepsPocketTopoPlacedCoordinatesTest(); rc != 0) {
        return rc;
    }

    return 0;
}
