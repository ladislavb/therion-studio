#include "../../src/core/TherionScrap3D.h"
#include "../../src/core/TherionCentreline.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cmath>

using namespace TherionStudio;

class TherionScrap3DTest : public QObject
{
    Q_OBJECT

private slots:
    void parseScaleOption();
    void fitTransformWithTwoAnchors();
    void fitTransformSingleAnchorFallback();
    void fitTransformWithoutAnchorsIsInvalid();
    void resolveStationKey();
    void projectScrapRealCaveFixture();
    void projectsLinesAreasAndPointsForATh2Scrap();
    void projectsFoldedSheetAndPreservesTh2Linework();
    void keepsGeometrySeparatedBetweenScraps();
    void filtersHiddenScrapsAndWallsBeforeProjection();
    void projectsVisibleObjectsWithoutAnySurface();
    void objectsFollowTriangulatedWallSurface();
    void joinedWallsAcrossScrapsFormOneSurface();
    void integratedComputeWithTh2();
};

void TherionScrap3DTest::projectsVisibleObjectsWithoutAnySurface()
{
    const QString text = QStringLiteral(
        "scrap passage -projection plan\n"
        "point 0 0 station -name A\npoint 10 0 station -name B\n"
        "line wall -close on\n0 0\n10 0\n10 10\n0 10\nendline\n"
        "line border\n0 2\n10 2\nendline\n"
        "line slope\n0 3\n10 3\nendline\n"
        "point 5 5 stalactite\npoint 6 6 stalagmite\n"
        "line border:invisible -id water-border -close on\n1000 1000\n1010 1000\n1010 1010\nendline\n"
        "area water\nwater-border\nendarea\n"
        "line wall:invisible\n2000 2000\n2010 2000\nendline\n"
        "line wall -subtype invisible\n3000 3000\n3010 3000\nendline\nendscrap\n");
    const auto projection = Th2GeometryProjection::fromText(text);
    const QHash<QString, ThreeDViewerVec3> stations{{QStringLiteral("A"), {0, 0, 100}},
                                                   {QStringLiteral("B"), {10, 0, 110}}};
    const auto model = TherionScrap3D::projectScrap(
        projection, QStringLiteral("passage"), {}, stations, 0, 0, {{}, false, true});
    QVERIFY(model.meshGroup.vertices.isEmpty());
    QVERIFY(model.meshGroup.triangles.isEmpty());
    QCOMPARE(model.linework.size(), 2);
    QCOMPARE(model.wallPolylines.size(), 1);
    QCOMPARE(model.linework.at(0).type, QStringLiteral("wall"));
    QCOMPARE(model.linework.at(1).type, QStringLiteral("slope"));
    QCOMPARE(model.pointFeatures.size(), 2);
    for (const auto &point : model.pointFeatures) {
        QVERIFY(point.position.z >= 100 && point.position.z <= 110);
    }
    const auto filtered = TherionScrap3D::projectScrap(
        projection, QStringLiteral("passage"), {}, stations, 0, 0, {{10, 18, 2}, false, true});
    QCOMPARE(filtered.linework.size(), 2);
    QCOMPARE(filtered.pointFeatures.size(), 1);
    QCOMPARE(filtered.transform.matchedAnchorCount, 2);
    const auto hidden = TherionScrap3D::projectTh2File(
        {}, text, {}, stations, 0, 0, {{1}, false, true});
    QVERIFY(hidden.isEmpty());
}

void TherionScrap3DTest::parseScaleOption()
{
    // Real scale option from clopy01.th2:
    // -scale [-128 -1152 851 -1152 0.0 0.0 24.8666 0.0 m]
    const QStringList tokens = {
        QStringLiteral("[-128"),
        QStringLiteral("-1152"),
        QStringLiteral("851"),
        QStringLiteral("-1152"),
        QStringLiteral("0.0"),
        QStringLiteral("0.0"),
        QStringLiteral("24.8666"),
        QStringLiteral("0.0"),
        QStringLiteral("m]")
    };

    const TherionScrapScale scale = TherionScrap3D::parseScaleOption(tokens);
    QVERIFY(scale.valid);
    QCOMPARE(scale.p1, QPointF(-128, -1152));
    QCOMPARE(scale.p2, QPointF(851, -1152));
    QVERIFY(std::abs(scale.worldLengthMeters - 24.8666) < 1e-4);
    // 24.8666 / (851 - (-128)) = 24.8666 / 979 = 0.0254 m/pt
    QVERIFY(std::abs(scale.metersPerPoint - 0.0254) < 1e-4);

    // Test feet conversion: 100 ft = 30.48 m
    const QStringList feetTokens = {
        QStringLiteral("[0"),
        QStringLiteral("0"),
        QStringLiteral("100"),
        QStringLiteral("0"),
        QStringLiteral("0.0"),
        QStringLiteral("0.0"),
        QStringLiteral("100.0"),
        QStringLiteral("0.0"),
        QStringLiteral("ft]")
    };
    const TherionScrapScale feetScale = TherionScrap3D::parseScaleOption(feetTokens);
    QVERIFY(feetScale.valid);
    QVERIFY(std::abs(feetScale.metersPerPoint - 0.3048) < 1e-4);
}

void TherionScrap3DTest::fitTransformWithTwoAnchors()
{
    // Anchor 1: scrap (0, 0) -> centreline (10, 20, 5)
    // Anchor 2: scrap (100, 0) -> centreline (10, 22.54, 15)
    // Scale: 2.54 / 100 = 0.0254 m/pt
    // Rotation: from +X (East in scrap) to +Y (North in world): +90 degrees = pi/2
    TherionScrapAnchor a1;
    a1.stationName = QStringLiteral("A");
    a1.scrapPoint = QPointF(0, 0);
    a1.centrelinePosition = ThreeDViewerVec3{10.0, 20.0, 5.0};
    a1.matched = true;

    TherionScrapAnchor a2;
    a2.stationName = QStringLiteral("B");
    a2.scrapPoint = QPointF(100, 0);
    a2.centrelinePosition = ThreeDViewerVec3{10.0, 22.54, 15.0};
    a2.matched = true;

    const TherionScrapTransform transform = TherionScrap3D::fitTransform({a1, a2});
    QVERIFY(transform.valid);
    QVERIFY(!transform.approximate);
    QCOMPARE(transform.matchedAnchorCount, 2);
    QVERIFY(std::abs(transform.scale - 0.0254) < 1e-4);
    QVERIFY(std::abs(transform.rotation - (M_PI / 2.0)) < 1e-4);
    QVERIFY(transform.meanResidual < 1e-4);
    QVERIFY(transform.maxResidual < 1e-4);

    // Test transformed points
    const QPointF p1_fit = transform.transformPoint(a1.scrapPoint);
    QVERIFY(std::abs(p1_fit.x() - 10.0) < 1e-4);
    QVERIFY(std::abs(p1_fit.y() - 20.0) < 1e-4);

    const QPointF p2_fit = transform.transformPoint(a2.scrapPoint);
    QVERIFY(std::abs(p2_fit.x() - 10.0) < 1e-4);
    QVERIFY(std::abs(p2_fit.y() - 22.54) < 1e-4);

    // Test Z-interpolation at midpoint (50, 0): equidistant from both anchors -> average (5 + 15)/2 = 10.0
    const double zMid = transform.interpolateZ(QPointF(50, 0), {a1, a2});
    QVERIFY(std::abs(zMid - 10.0) < 1e-3);
}

void TherionScrap3DTest::fitTransformSingleAnchorFallback()
{
    TherionScrapAnchor a1;
    a1.stationName = QStringLiteral("A");
    a1.scrapPoint = QPointF(100, 200);
    a1.centrelinePosition = ThreeDViewerVec3{50.0, 60.0, 10.0};
    a1.matched = true;

    TherionScrapScale scale;
    scale.valid = true;
    scale.metersPerPoint = 0.05;

    const TherionScrapTransform transform = TherionScrap3D::fitTransform({a1}, scale);
    QVERIFY(transform.valid);
    QVERIFY(transform.approximate);
    QCOMPARE(transform.matchedAnchorCount, 1);
    QCOMPARE(transform.scale, 0.05);
    QCOMPARE(transform.rotation, 0.0);

    const QPointF p1_fit = transform.transformPoint(a1.scrapPoint);
    QCOMPARE(p1_fit.x(), 50.0);
    QCOMPARE(p1_fit.y(), 60.0);
}

void TherionScrap3DTest::fitTransformWithoutAnchorsIsInvalid()
{
    TherionScrapScale scale;
    scale.valid = true;
    scale.metersPerPoint = 0.0254;

    const TherionScrapTransform transform = TherionScrap3D::fitTransform({}, scale);

    QVERIFY(!transform.valid);
    QCOMPARE(transform.matchedAnchorCount, 0);
    QVERIFY(!transform.approximate);
}

void TherionScrap3DTest::resolveStationKey()
{
    QHash<QString, ThreeDViewerVec3> stationPositions;
    stationPositions.insert(QStringLiteral("0@hp.1302"), {0.0, 0.0, 0.0});
    stationPositions.insert(QStringLiteral("1@hp.1302"), {1.0, 2.0, 3.0});
    stationPositions.insert(QStringLiteral("entrance"), {100.0, 200.0, 50.0});

    // Exact match
    QCOMPARE(TherionScrap3D::resolveStationKey(QStringLiteral("entrance"), QString(), stationPositions),
             QStringLiteral("entrance"));

    // Scoped resolution: "0@hp" in survey "1302" resolves to "0@hp.1302"
    QCOMPARE(TherionScrap3D::resolveStationKey(QStringLiteral("0@hp"), QStringLiteral("1302"), stationPositions),
             QStringLiteral("0@hp.1302"));

    // Prefix/suffix match: "0@hp" matches "0@hp.1302" even without scrap survey path
    QCOMPARE(TherionScrap3D::resolveStationKey(QStringLiteral("0@hp"), QString(), stationPositions),
             QStringLiteral("0@hp.1302"));

    // Nonexistent station
    QVERIFY(TherionScrap3D::resolveStationKey(QStringLiteral("nonexistent"), QString(), stationPositions).isEmpty());
}

void TherionScrap3DTest::projectScrapRealCaveFixture()
{
    // Real cave fixture data: hp.th and clopy01.th2
    const QString hpPath = QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("projects/map_editor_regression/real_cave_raster/data/hp.th"));
    const QString th2Path = QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("projects/map_editor_regression/real_cave_raster/data/clopy01.th2"));

    QVERIFY(QFile::exists(hpPath));
    QVERIFY(QFile::exists(th2Path));

    // 1. Solve centreline
    const TherionCentrelineResult centrelineResult = computeTherionCentreline({hpPath}, defaultTherionCentrelineFileReader());
    QVERIFY(centrelineResult.diagnostics.isEmpty());
    QCOMPARE(centrelineResult.statistics.stationCount, 10);

    // 2. Read and project clopy01.th2
    QFile th2File(th2Path);
    QVERIFY(th2File.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString th2Contents = QString::fromUtf8(th2File.readAll());

    const QVector<TherionScrap3DModel> scrapModels = TherionScrap3D::projectTh2File(
        th2Path, th2Contents, QStringLiteral("hp"), centrelineResult.stationPositions);

    QCOMPARE(scrapModels.size(), 1);
    const TherionScrap3DModel &scrap = scrapModels.first();

    QCOMPARE(scrap.scrapId, QStringLiteral("clopy01"));
    QVERIFY(scrap.transform.valid);
    QCOMPARE(scrap.transform.matchedAnchorCount, 10);

    // Verify mean and max residual against real cave hand-drawn survey (1985 paper survey):
    // As calculated earlier, mean residual is ~0.034m (3.4 cm) and max is ~0.065m (6.5 cm).
    QVERIFY(scrap.transform.meanResidual < 0.05);
    QVERIFY(scrap.transform.maxResidual < 0.07);

    // Verify wall polylines were extracted
    QVERIFY(!scrap.wallPolylines.isEmpty());

    QVERIFY(scrap.meshGroup.vertices.isEmpty());
    QVERIFY(scrap.meshGroup.triangles.isEmpty());
    QVERIFY(!scrap.linework.isEmpty());
    QVERIFY(scrap.diagnostics.contains(QStringLiteral("Wall lines do not form a closed boundary; the sheet surface was omitted.")));
}

void TherionScrap3DTest::projectsLinesAreasAndPointsForATh2Scrap()
{
    const QString th2 = QStringLiteral(
        "scrap s1 -projection plan -scale [0 0 100 0 0 0 10 0 m]\n"
        "  point 0 0 station -name A\n"
        "  line border -id b1 -close on\n"
        "    0 0\n"
        "    100 0\n"
        "    100 100\n"
        "    0 100\n"
        "  endline\n"
        "  area water\n"
        "    b1\n"
        "  endarea\n"
        "  line contour\n"
        "    0 0\n"
        "    50 50\n"
        "    100 100\n"
        "  endline\n"
        "  point 50 50 label -text marker\n"
        "endscrap\n");
    QHash<QString, ThreeDViewerVec3> stationPositions;
    stationPositions.insert(QStringLiteral("A"), {0.0, 0.0, 5.0});

    const QVector<TherionScrap3DModel> scraps =
        TherionScrap3D::projectTh2File(QStringLiteral("scrap.th2"), th2, QString(), stationPositions);

    QCOMPARE(scraps.size(), 1);
    const TherionScrap3DModel &scrap = scraps.first();
    QVERIFY(scrap.transform.valid);
    QVERIFY(scrap.transform.approximate);
    QVERIFY(scrap.diagnostics.isEmpty());
    QCOMPARE(scrap.meshGroup.vertices.size(), 4);
    QCOMPARE(scrap.meshGroup.triangles.size(), 2);
    QCOMPARE(scrap.linework.size(), 2);
    QCOMPARE(scrap.pointFeatures.size(), 1);
}

void TherionScrap3DTest::projectsFoldedSheetAndPreservesTh2Linework()
{
    const QString th2 = QStringLiteral(
        "scrap s1 -scale [0 0 10 0 0 0 10 0 m]\n"
        "  point 0 0 station -name A\n"
        "  point 10 0 station -name B\n"
        "  line wall\n"
        "    0 0\n"
        "    10 0\n"
        "  endline\n"
        "  line wall\n"
        "    10 0\n"
        "    10 10\n"
        "  endline\n"
        "  line wall\n"
        "    10 10\n"
        "    0 10\n"
        "  endline\n"
        "  line wall\n"
        "    0 10\n"
        "    0 0\n"
        "  endline\n"
        "  line contour\n"
        "    0 0\n"
        "    5 5\n"
        "    10 10\n"
        "  endline\n"
        "endscrap\n");
    QHash<QString, ThreeDViewerVec3> stationPositions;
    stationPositions.insert(QStringLiteral("A"), {0.0, 0.0, 0.0});
    stationPositions.insert(QStringLiteral("B"), {10.0, 0.0, 10.0});

    const QVector<TherionScrap3DModel> scraps =
        TherionScrap3D::projectTh2File(QStringLiteral("folded.th2"), th2, QString(), stationPositions);

    QCOMPARE(scraps.size(), 1);
    const TherionScrap3DModel &scrap = scraps.first();
    QVERIFY(scrap.transform.valid);
    QCOMPARE(scrap.meshGroup.vertices.size(), 4);
    QCOMPARE(scrap.meshGroup.triangles.size(), 2);
    QCOMPARE(scrap.linework.size(), 5);
    QCOMPARE(scrap.linework.at(0).type, QStringLiteral("wall"));
    QCOMPARE(scrap.linework.at(4).type, QStringLiteral("contour"));
    QVERIFY(!scrap.linework.at(0).closed);
    QVERIFY(scrap.meshGroup.vertices.at(0).z != scrap.meshGroup.vertices.at(2).z);
}

void TherionScrap3DTest::keepsGeometrySeparatedBetweenScraps()
{
    const QString th2 = QStringLiteral(
        "scrap first -scale [0 0 10 0 0 0 1 0 m]\n"
        "  point 0 0 station -name A\n"
        "  line contour\n"
        "    0 0\n"
        "    10 0\n"
        "  endline\n"
        "endscrap\n"
        "scrap second -scale [0 0 10 0 0 0 1 0 m]\n"
        "  point 0 0 station -name B\n"
        "  line contour\n"
        "    0 0\n"
        "    5 5\n"
        "    10 0\n"
        "  endline\n"
        "endscrap\n");
    QHash<QString, ThreeDViewerVec3> stationPositions;
    stationPositions.insert(QStringLiteral("A"), {0.0, 0.0, 0.0});
    stationPositions.insert(QStringLiteral("B"), {10.0, 0.0, 0.0});

    const QVector<TherionScrap3DModel> scraps =
        TherionScrap3D::projectTh2File(QStringLiteral("two-scraps.th2"), th2, QString(), stationPositions);

    QCOMPARE(scraps.size(), 2);
    QVERIFY(scraps.at(0).meshGroup.triangles.isEmpty());
    QVERIFY(scraps.at(1).meshGroup.triangles.isEmpty());
    QCOMPARE(scraps.at(0).linework.size(), 1);
    QCOMPARE(scraps.at(1).linework.size(), 1);
    QCOMPARE(scraps.at(0).anchors.size(), 1);
    QCOMPARE(scraps.at(1).anchors.size(), 1);
    QCOMPARE(scraps.at(0).transform.matchedAnchorCount, 1);
    QCOMPARE(scraps.at(1).transform.matchedAnchorCount, 1);
    QVERIFY(scraps.at(0).meshGroup.vertices.isEmpty());
    QVERIFY(scraps.at(1).meshGroup.vertices.isEmpty());
}

void TherionScrap3DTest::objectsFollowTriangulatedWallSurface()
{
    const QString th2 = QStringLiteral(
        "scrap passage -scale [0 0 10 0 0 0 10 0 m]\n"
        "  point 0 0 station -name A\n"
        "  point 10 0 station -name B\n"
        "  point 0 10 station -name C\n"
        "  line wall -close on\n"
        "    0 0\n"
        "    10 0\n"
        "    0 10\n"
        "  endline\n"
        "  line rock-border\n"
        "    2 2\n"
        "    3 3\n"
        "  endline\n"
        "  point 2 2 stalactite\n"
        "endscrap\n");
    const QHash<QString, ThreeDViewerVec3> stationPositions{
        {QStringLiteral("A"), {0.0, 0.0, 0.0}},
        {QStringLiteral("B"), {10.0, 0.0, 10.0}},
        {QStringLiteral("C"), {0.0, 10.0, 20.0}},
    };

    const auto scraps = TherionScrap3D::projectTh2File(
        QStringLiteral("draped-objects.th2"), th2, QString(), stationPositions);

    QCOMPARE(scraps.size(), 1);
    const TherionScrap3DModel &scrap = scraps.first();
    QVERIFY(!scrap.meshGroup.triangles.isEmpty());
    QCOMPARE(scrap.pointFeatures.size(), 1);
    QVERIFY(std::abs(scrap.pointFeatures.first().position.z - 6.0) < 1e-6);

    const ThreeDViewerScrapPolyline *rockBorder = nullptr;
    for (const ThreeDViewerScrapPolyline &line : scrap.linework) {
        if (line.type == QStringLiteral("rock-border")) {
            rockBorder = &line;
            break;
        }
    }
    QVERIFY(rockBorder != nullptr);
    QVERIFY(std::abs(rockBorder->points.first().z - 6.0) < 1e-6);
}

void TherionScrap3DTest::joinedWallsAcrossScrapsFormOneSurface()
{
    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());

    const QString surveyText = QStringLiteral(
        "survey cave\n"
        "  centreline\n"
        "    data normal from to length compass clino\n"
        "    A B 10 0 90\n"
        "  endcentreline\n"
        "  input walls.th2\n"
        "  join left@cave right@cave -count 2\n"
        "  join leftWall:end rightWall:0\n"
        "endsurvey\n");
    const QString th2Text = QStringLiteral(
        "scrap left -scale [0 0 10 0 0 0 10 0 m]\n"
        "  point 0 0 station -name A\n"
        "  line wall -id leftWall\n"
        "    0 0\n"
        "    10 0\n"
        "    10 10\n"
        "  endline\n"
        "endscrap\n"
        "scrap right -scale [0 0 10 0 0 0 10 0 m]\n"
        "  point 0 0 station -name B\n"
        "  line wall -id rightWall\n"
        "    9 10\n"
        "    0 10\n"
        "    0 0\n"
        "  endline\n"
        "  line slope\n"
        "    5 5\n"
        "    6 5\n"
        "  endline\n"
        "  point 5 5 stalactite\n"
        "endscrap\n");
    const QString surveyPath = projectDir.filePath(QStringLiteral("cave.th"));
    QFile surveyFile(surveyPath);
    QVERIFY(surveyFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray surveyBytes = surveyText.toUtf8();
    QCOMPARE(surveyFile.write(surveyBytes), qint64(surveyBytes.size()));
    surveyFile.close();

    QFile th2File(projectDir.filePath(QStringLiteral("walls.th2")));
    QVERIFY(th2File.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray th2Bytes = th2Text.toUtf8();
    QCOMPARE(th2File.write(th2Bytes), qint64(th2Bytes.size()));
    th2File.close();

    const TherionCentrelineResult result = computeTherionCentreline(
        {surveyPath}, defaultTherionCentrelineFileReader());

    QVERIFY(result.diagnostics.isEmpty());
    QVERIFY(!result.scene.meshGroups.isEmpty());
    int triangleCount = 0;
    for (const ThreeDViewerMeshGroup &mesh : result.scene.meshGroups) {
        triangleCount += mesh.triangles.size();
    }
    QCOMPARE(triangleCount, 2);
    QCOMPARE(result.scrapModels.size(), 2);
    const ThreeDViewerVec3 &leftJoin = result.scrapModels.at(0).linework.first().points.last();
    const ThreeDViewerVec3 &rightJoin = result.scrapModels.at(1).linework.first().points.first();
    QVERIFY(std::abs(leftJoin.x - 9.5) < 1e-6);
    QVERIFY(std::abs(rightJoin.x - 9.5) < 1e-6);
    QVERIFY(std::abs(leftJoin.z - 5.0) < 1e-6);
    QCOMPARE(leftJoin.y, rightJoin.y);
    QCOMPARE(leftJoin.z, rightJoin.z);
    QCOMPARE(result.scrapModels.at(1).pointFeatures.size(), 1);
    QVERIFY(std::abs(result.scrapModels.at(1).pointFeatures.first().position.z - 5.0) < 1e-6);
    QVERIFY(std::abs(result.scrapModels.at(1).linework.at(1).points.first().z - 5.0) < 1e-6);
}

void TherionScrap3DTest::filtersHiddenScrapsAndWallsBeforeProjection()
{
    const QString source = QStringLiteral(
        "scrap shown -projection plan\n"          // 1
        "point 0 0 station -name A\n"             // 2
        "point 10 0 station -name B\n"            // 3
        "line wall -close on\n"                   // 4
        "0 0\n10 0\n10 10\n0 0\nendline\n"       // 5-9
        "line wall\n0 1\n10 1\nendline\n"        // 10-13
        "line contour\n0 2\n10 2\nendline\n"     // 14-17
        "endscrap\n"                             // 18
        "scrap hidden -projection plan\n"         // 19
        "point 100 100 station -name C\n"
        "line wall\n100 100\n110 100\nendline\n"
        "endscrap\n");
    const QHash<QString, ThreeDViewerVec3> stations{
        {QStringLiteral("A"), {0.0, 0.0, 0.0}},
        {QStringLiteral("B"), {10.0, 0.0, 10.0}},
        {QStringLiteral("C"), {500.0, 500.0, 500.0}},
    };
    const TherionScrap3DOptions options{{2, 10, 19}, true};
    const auto scraps = TherionScrap3D::projectTh2File(
        QStringLiteral("map.th2"), source, QString(), stations, 0, 0, options);
    QCOMPARE(scraps.size(), 1);
    const auto &scrap = scraps.first();
    QCOMPARE(scrap.scrapId, QStringLiteral("shown"));
    QCOMPARE(scrap.anchors.size(), 2);
    QCOMPARE(scrap.transform.matchedAnchorCount, 2);
    QVERIFY(scrap.transform.valid);
    QCOMPARE(scrap.linework.size(), 1);
    QCOMPARE(scrap.linework.first().type, QStringLiteral("wall"));
    QVERIFY(scrap.linework.first().closed);
    QVERIFY(scrap.meshGroup.triangles.isEmpty());
    QVERIFY(scrap.meshGroup.vertices.isEmpty());
    QVERIFY(scrap.pointFeatures.isEmpty());
    QVERIFY(scrap.diagnostics.isEmpty());
}

void TherionScrap3DTest::integratedComputeWithTh2()
{
    // Test that computing 1302.th (which inputs hp.th and clopy01.th2)
    // automatically creates both the centreline shots AND the scrap 3D mesh!
    const QString thPath = QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("projects/map_editor_regression/real_cave_raster/1302.th"));

    QVERIFY(QFile::exists(thPath));

    const TherionCentrelineResult result = computeTherionCentreline({thPath}, defaultTherionCentrelineFileReader());

    QVERIFY(result.diagnostics.isEmpty());
    QCOMPARE(result.statistics.stationCount, 10);
    QCOMPARE(result.statistics.shotCount, 9);

    // Mesh group created from clopy01.th2!
    QCOMPARE(result.scene.meshGroups.size(), 0);
    QVERIFY(!result.scene.scrapLinework.isEmpty());
    QCOMPARE(result.scrapModels.size(), 1);
    QCOMPARE(result.scrapModels.first().scrapId, QStringLiteral("clopy01"));

    // Verify scene bounds include both centreline and walls
    const ThreeDViewerBounds bounds = result.scene.bounds();
    QVERIFY(bounds.valid);
    QVERIFY(bounds.minimum.x < bounds.maximum.x);
    QVERIFY(bounds.minimum.y < bounds.maximum.y);
    QVERIFY(bounds.minimum.z < bounds.maximum.z);
}

int runTherionScrap3DTest(int argc, char **argv)
{
    TherionScrap3DTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionScrap3DTest.moc"
