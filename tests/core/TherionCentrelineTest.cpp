#include "../../src/core/TherionCentreline.h"

#include <QtTest/QtTest>

#include <cmath>

using namespace TherionStudio;

class TherionCentrelineTest : public QObject
{
    Q_OBJECT

private slots:
    void singleLegCardinalDirections();
    void unitsAndDataFormats();
    void nestedSurveysAndNames();
    void equateJoinsSurveys();
    void simpleLoopClosure();
    void fixAnchorCoordinates();
    void realCaveFixtureMatchOracle();
    void diagnosticsForMissingOrInvalidInput();
};

void TherionCentrelineTest::singleLegCardinalDirections()
{
    // Test 4 legs from origin: North, East, Up, Down
    const QString surveyText = QStringLiteral(
        "survey test\n"
        "  centreline\n"
        "    data normal from to length compass clino\n"
        "    0 1 10.0   0.0   0.0\n" // North -> Y +10
        "    0 2 20.0  90.0   0.0\n" // East  -> X +20
        "    0 3  5.0   0.0  90.0\n" // Up    -> Z +5
        "    0 4  8.0   0.0 -90.0\n" // Down  -> Z -8
        "  endcentreline\n"
        "endsurvey\n"
    );

    TherionCentrelineFileReader reader = [surveyText](const QString &, QString *contents) {
        *contents = surveyText;
        return true;
    };

    const TherionCentrelineResult result = computeTherionCentreline({QStringLiteral("test.th")}, reader);

    QVERIFY(result.diagnostics.isEmpty());
    QCOMPARE(result.statistics.stationCount, 5);
    QCOMPARE(result.statistics.shotCount, 4);

    const auto &pos = result.stationPositions;
    QVERIFY(pos.contains(QStringLiteral("0@test")));
    QVERIFY(pos.contains(QStringLiteral("1@test")));
    QVERIFY(pos.contains(QStringLiteral("2@test")));
    QVERIFY(pos.contains(QStringLiteral("3@test")));
    QVERIFY(pos.contains(QStringLiteral("4@test")));

    const ThreeDViewerVec3 p0 = pos.value(QStringLiteral("0@test"));
    const ThreeDViewerVec3 p1 = pos.value(QStringLiteral("1@test"));
    const ThreeDViewerVec3 p2 = pos.value(QStringLiteral("2@test"));
    const ThreeDViewerVec3 p3 = pos.value(QStringLiteral("3@test"));
    const ThreeDViewerVec3 p4 = pos.value(QStringLiteral("4@test"));

    // Relative offsets from 0:
    // Therion coordinate convention: X = East, Y = North, Z = Up
    QCOMPARE(std::round(p1.x - p0.x), 0.0);
    QCOMPARE(std::round(p1.y - p0.y), 10.0);
    QCOMPARE(std::round(p1.z - p0.z), 0.0);

    QCOMPARE(std::round(p2.x - p0.x), 20.0);
    QCOMPARE(std::round(p2.y - p0.y), 0.0);
    QCOMPARE(std::round(p2.z - p0.z), 0.0);

    QCOMPARE(std::round(p3.x - p0.x), 0.0);
    QCOMPARE(std::round(p3.y - p0.y), 0.0);
    QCOMPARE(std::round(p3.z - p0.z), 5.0);

    QCOMPARE(std::round(p4.x - p0.x), 0.0);
    QCOMPARE(std::round(p4.y - p0.y), 0.0);
    QCOMPARE(std::round(p4.z - p0.z), -8.0);
}

void TherionCentrelineTest::unitsAndDataFormats()
{
    // Test units: length in feet, angles in grads, clino in percent
    // 100 grads = 90 deg (East)
    // 50 ft = 15.24 m
    const QString surveyText = QStringLiteral(
        "survey units_test\n"
        "  centreline\n"
        "    units length ft\n"
        "    units compass grad\n"
        "    units clino percent\n"
        "    data normal from to length compass clino\n"
        "    A B 50 100 0\n" // 50 ft = 15.24m, 100 grad = 90 deg East
        "  endcentreline\n"
        "endsurvey\n"
    );

    TherionCentrelineFileReader reader = [surveyText](const QString &, QString *contents) {
        *contents = surveyText;
        return true;
    };

    const TherionCentrelineResult result = computeTherionCentreline({QStringLiteral("units.th")}, reader);
    QVERIFY(result.diagnostics.isEmpty());

    const auto &pos = result.stationPositions;
    QVERIFY(pos.contains(QStringLiteral("A@units_test")));
    QVERIFY(pos.contains(QStringLiteral("B@units_test")));

    const ThreeDViewerVec3 pA = pos.value(QStringLiteral("A@units_test"));
    const ThreeDViewerVec3 pB = pos.value(QStringLiteral("B@units_test"));

    const double dx = pB.x - pA.x;
    const double dy = pB.y - pA.y;
    const double dz = pB.z - pA.z;

    QVERIFY(std::abs(dx - 15.24) < 1e-3);
    QVERIFY(std::abs(dy) < 1e-3);
    QVERIFY(std::abs(dz) < 1e-3);
}

void TherionCentrelineTest::nestedSurveysAndNames()
{
    // Survey outer containing survey inner
    const QString surveyText = QStringLiteral(
        "survey outer\n"
        "  survey inner\n"
        "    centreline\n"
        "      data normal from to length compass clino\n"
        "      1 2 10.0 0.0 0.0\n"
        "    endcentreline\n"
        "  endsurvey\n"
        "endsurvey\n"
    );

    TherionCentrelineFileReader reader = [surveyText](const QString &, QString *contents) {
        *contents = surveyText;
        return true;
    };

    const TherionCentrelineResult result = computeTherionCentreline({QStringLiteral("nested.th")}, reader);
    QVERIFY(result.diagnostics.isEmpty());

    // Therion qualified name convention: innermost.outer
    QVERIFY(result.stationPositions.contains(QStringLiteral("1@inner.outer")));
    QVERIFY(result.stationPositions.contains(QStringLiteral("2@inner.outer")));
}

void TherionCentrelineTest::equateJoinsSurveys()
{
    // Survey s1 and s2 connected by equate
    const QString surveyText = QStringLiteral(
        "survey s1\n"
        "  centreline\n"
        "    data normal from to length compass clino\n"
        "    1 2 10.0 0.0 0.0\n"
        "  endcentreline\n"
        "endsurvey\n"
        "survey s2\n"
        "  centreline\n"
        "    data normal from to length compass clino\n"
        "    A B 10.0 90.0 0.0\n"
        "  endcentreline\n"
        "endsurvey\n"
        "equate 2@s1 A@s2\n"
    );

    TherionCentrelineFileReader reader = [surveyText](const QString &, QString *contents) {
        *contents = surveyText;
        return true;
    };

    const TherionCentrelineResult result = computeTherionCentreline({QStringLiteral("equate.th")}, reader);
    QVERIFY(result.diagnostics.isEmpty());

    // Because of equate, 2@s1 and A@s2 must have the exact same position
    QVERIFY(result.stationPositions.contains(QStringLiteral("2@s1")));
    QVERIFY(result.stationPositions.contains(QStringLiteral("A@s2")));

    const ThreeDViewerVec3 p2 = result.stationPositions.value(QStringLiteral("2@s1"));
    const ThreeDViewerVec3 pA = result.stationPositions.value(QStringLiteral("A@s2"));

    QCOMPARE(p2.x, pA.x);
    QCOMPARE(p2.y, pA.y);
    QCOMPARE(p2.z, pA.z);

    // And total components should be 1, not 2
    QCOMPARE(result.statistics.componentCount, 1);
}

void TherionCentrelineTest::simpleLoopClosure()
{
    // Closed triangle:
    // 1 -> 2: 10m East  (90 deg)
    // 2 -> 3: 10m North (0 deg)
    // 3 -> 1: hypot(10, 10) = 14.1421356m towards South-West (225 deg)
    const QString surveyText = QStringLiteral(
        "survey loop_test\n"
        "  centreline\n"
        "    data normal from to length compass clino\n"
        "    1 2 10.000 90.0 0.0\n"
        "    2 3 10.000  0.0 0.0\n"
        "    3 1 14.142 225.0 0.0\n"
        "  endcentreline\n"
        "endsurvey\n"
    );

    TherionCentrelineFileReader reader = [surveyText](const QString &, QString *contents) {
        *contents = surveyText;
        return true;
    };

    const TherionCentrelineResult result = computeTherionCentreline({QStringLiteral("loop.th")}, reader);
    QVERIFY(result.diagnostics.isEmpty());
    QCOMPARE(result.statistics.loopCount, 1);
    QCOMPARE(result.statistics.stationCount, 3);
    QCOMPARE(result.statistics.shotCount, 3);

    // Stations 1, 2, 3 should form a closed loop with low residual adjustments
    QVERIFY(result.statistics.maxAdjustment < 0.05);
}

void TherionCentrelineTest::fixAnchorCoordinates()
{
    const QString surveyText = QStringLiteral(
        "survey fix_test\n"
        "  centreline\n"
        "    fix 1 1000.0 2000.0 300.0\n"
        "    data normal from to length compass clino\n"
        "    1 2 10.0 0.0 0.0\n"
        "  endcentreline\n"
        "endsurvey\n"
    );

    TherionCentrelineFileReader reader = [surveyText](const QString &, QString *contents) {
        *contents = surveyText;
        return true;
    };

    const TherionCentrelineResult result = computeTherionCentreline({QStringLiteral("fix.th")}, reader);
    QVERIFY(result.diagnostics.isEmpty());

    QVERIFY(result.stationPositions.contains(QStringLiteral("1@fix_test")));
    const ThreeDViewerVec3 p1 = result.stationPositions.value(QStringLiteral("1@fix_test"));

    QCOMPARE(p1.x, 1000.0);
    QCOMPARE(p1.y, 2000.0);
    QCOMPARE(p1.z, 300.0);

    const ThreeDViewerVec3 p2 = result.stationPositions.value(QStringLiteral("2@fix_test"));
    QCOMPARE(p2.x, 1000.0);
    QCOMPARE(p2.y, 2010.0);
    QCOMPARE(p2.z, 300.0);
}

void TherionCentrelineTest::realCaveFixtureMatchOracle()
{
    // Real cave fixture: tests/fixtures/projects/map_editor_regression/real_cave_raster/data/hp.th
    const QString hpPath = QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("projects/map_editor_regression/real_cave_raster/data/hp.th"));

    const TherionCentrelineResult result = computeTherionCentreline({hpPath}, defaultTherionCentrelineFileReader());
    QVERIFY(result.diagnostics.isEmpty());
    QCOMPARE(result.statistics.stationCount, 10);
    QCOMPARE(result.statistics.shotCount, 9);

    const auto &pos = result.stationPositions;
    QVERIFY(pos.contains(QStringLiteral("0@hp")));
    QVERIFY(pos.contains(QStringLiteral("9@hp")));

    const ThreeDViewerVec3 p0 = pos.value(QStringLiteral("0@hp"));
    const ThreeDViewerVec3 p9 = pos.value(QStringLiteral("9@hp"));

    // Exactly matching Cavern/Therion calculation for station 9@hp relative to 0@hp:
    // Station 9: East = -5.36m, North = -13.75m, Up = -17.04m
    const double dx = p9.x - p0.x;
    const double dy = p9.y - p0.y;
    const double dz = p9.z - p0.z;

    QVERIFY(std::abs(dx - (-5.36)) < 0.05);
    QVERIFY(std::abs(dy - (-13.75)) < 0.05);
    QVERIFY(std::abs(dz - (-17.04)) < 0.05);
}

void TherionCentrelineTest::diagnosticsForMissingOrInvalidInput()
{
    // Reading nonexistent file should emit diagnostic
    TherionCentrelineFileReader reader = [](const QString &, QString *) {
        return false;
    };

    const TherionCentrelineResult result = computeTherionCentreline({QStringLiteral("missing.th")}, reader);
    QVERIFY(!result.diagnostics.isEmpty());
    QVERIFY(result.diagnostics.first().message.contains(QStringLiteral("read")));
}

int runTherionCentrelineTest(int argc, char **argv)
{
    TherionCentrelineTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionCentrelineTest.moc"
