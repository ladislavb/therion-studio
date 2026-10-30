#include "../src/app/three_d_viewer/ThreeDViewerInspectorState.h"
#include "../src/app/three_d_viewer/ThreeDViewerTab.h"
#include "../src/core/TherionCentreline.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <QLocale>

#include <cmath>

using namespace TherionStudio;

class ThreeDViewerInspectorStateTest : public QObject
{
    Q_OBJECT

private slots:
    void tracksFilePathAndSceneMetrics();
    void tracksSceneWarnings();
    void tracksMeshColorMode();
    void tracksCenterlineColorMode();
    void tracksBackgroundMode();
    void tracksMeasurementMode();
    void tracksSceneOverlayVisibility();
    void tracksAutoRotationSettings();
    void tracksCameraSettings();
    void loadsCentrelineFromThFile();
    void loadsScrapObjectsWithoutSurfaces();
    void selectsOnlyRequestedSurveySourceForTh2();
    void respectsTh2VisibilityAndSourceSnapshot();
};

void ThreeDViewerInspectorStateTest::respectsTh2VisibilityAndSourceSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString th2 = QStringLiteral(
        "scrap visible -projection plan\n"       // 1
        "point 0 0 station -name A\n"            // 2
        "point 10 0 station -name B\n"           // 3
        "line wall\n0 1\n10 1\nendline\n"       // 4-7
        "line wall\n0 2\n10 2\nendline\n"       // 8-11
        "endscrap\n"                            // 12
        "scrap hidden -projection plan\n"        // 13
        "point 0 0 station -name A\n"
        "point 10 0 station -name B\n"
        "line wall\n0 3\n10 3\nendline\n"
        "endscrap\n");
    const auto writeFile = [&directory](const QString &name, const QString &contents) {
        QFile file(directory.filePath(name));
        return file.open(QIODevice::WriteOnly) && file.write(contents.toUtf8()) == contents.toUtf8().size();
    };
    QVERIFY(writeFile(QStringLiteral("map.th2"), th2));
    QVERIFY(writeFile(QStringLiteral("other.th2"), th2));
    const QString survey = directory.filePath(QStringLiteral("survey.th"));
    QVERIFY(writeFile(QStringLiteral("survey.th"), QStringLiteral(
        "survey cave\ninput map.th2\ninput other.th2\n"
        "centreline\ndata normal from to length compass clino\nA B 10 90 0\n"
        "endcentreline\nendsurvey\n")));

    ThreeDViewerTab tab;
    const QString path = directory.filePath(QStringLiteral("map.th2"));
    QString error;
    QVERIFY2(tab.loadFile(path, survey, &error), qPrintable(error));
    QCOMPARE(tab.sceneModel().scrapLinework.size(), 3);

    tab.setTh2SourceSnapshot(th2, {8, 13});
    QVERIFY2(tab.loadFile(path, survey, &error), qPrintable(error));
    QCOMPARE(tab.sceneModel().scrapLinework.size(), 1);
    QVERIFY2(tab.reloadFile(&error), qPrintable(error));
    QCOMPARE(tab.sceneModel().scrapLinework.size(), 1);

    tab.setTh2SourceSnapshot(th2, {1, 13});
    QVERIFY2(tab.reloadFile(&error), qPrintable(error));
    QVERIFY(tab.sceneModel().scrapLinework.isEmpty());
    QCOMPARE(tab.sceneModel().stations.size(), 2);

    const QString editedTh2 = QString(th2).replace(QStringLiteral("0 1\n10 1"), QStringLiteral("0 5\n10 5"));
    ThreeDViewerTab firstOpenTab;
    firstOpenTab.setTh2SourceSnapshot(editedTh2, {8, 13});
    QVERIFY2(firstOpenTab.loadFile(path, survey, &error), qPrintable(error));
    QCOMPARE(firstOpenTab.sceneModel().scrapLinework.size(), 1);
    QVERIFY(std::abs(firstOpenTab.sceneModel().scrapLinework.first().points.first().y - 5.0) < 1e-6);

    tab.setTh2SourceSnapshot(editedTh2, {8, 13});
    QVERIFY2(tab.reloadFile(&error), qPrintable(error));
    QCOMPARE(tab.sceneModel().scrapLinework.size(), 1);
    QVERIFY(std::abs(tab.sceneModel().scrapLinework.first().points.first().y - 5.0) < 1e-6);

    const QString withObjects = QString(editedTh2).replace(
        QStringLiteral("endscrap\nscrap hidden"),
        QStringLiteral("line border\n0 2\n10 2\nendline\n"
                       "line slope\n0 3\n10 3\nendline\n"
                       "point 5 2 stalactite\npoint 7 2 stalagmite\nendscrap\nscrap hidden"));
    tab.setTh2SourceSnapshot(withObjects, {8, 23});
    QVERIFY2(tab.reloadFile(&error), qPrintable(error));
    QCOMPARE(tab.sceneModel().scrapLinework.size(), 2);
    QCOMPARE(tab.sceneModel().scrapPoints.size(), 2);
    const qreal wallWidth = tab.sceneModel().scrapLinework.first().strokeWidth;
    for (const auto &line : tab.sceneModel().scrapLinework) {
        QVERIFY(line.type != QStringLiteral("border"));
        if (line.type != QStringLiteral("wall")) {
            QVERIFY(line.strokeWidth < wallWidth);
            QVERIFY(line.strokeWidth <= wallWidth * 0.45);
        }
    }
    QVERIFY(!tab.sceneModel().scrapPoints.first().symbolStrokes.isEmpty());
    QVERIFY(tab.sceneModel().meshGroups.isEmpty());
    QVERIFY(tab.sceneModel().surfaces.isEmpty());
    tab.setTh2SourceSnapshot(withObjects, {8, 16, 20, 23});
    QVERIFY2(tab.reloadFile(&error), qPrintable(error));
    QCOMPARE(tab.sceneModel().scrapLinework.size(), 1);
    QCOMPARE(tab.sceneModel().scrapPoints.size(), 1);
    tab.setTh2SourceSnapshot(withObjects, {1, 23});
    QVERIFY2(tab.reloadFile(&error), qPrintable(error));
    QVERIFY(tab.sceneModel().scrapLinework.isEmpty());
    QVERIFY(tab.sceneModel().scrapPoints.isEmpty());

    const QString filteredObjects = withObjects.left(withObjects.indexOf(QStringLiteral("scrap hidden")))
        .replace(QStringLiteral("endscrap\n"),
                 QStringLiteral("line border:invisible -id water-border -close on\n"
                                "1000 1000\n1010 1000\n1010 1010\nendline\n"
                                "area water\nwater-border\nendarea\n"
                                "line wall:invisible\n2000 2000\n2010 2000\nendline\n"
                                "line wall -subtype invisible\n3000 3000\n3010 3000\nendline\nendscrap\n"));
    tab.setTh2SourceSnapshot(filteredObjects, {8});
    QVERIFY2(tab.reloadFile(&error), qPrintable(error));
    QCOMPARE(tab.sceneModel().scrapLinework.size(), 2);
    QCOMPARE(tab.sceneModel().scrapPoints.size(), 2);
    for (const auto &line : tab.sceneModel().scrapLinework) {
        QVERIFY(line.type == QStringLiteral("wall") || line.type == QStringLiteral("slope"));
        QVERIFY(line.subtype != QStringLiteral("invisible"));
    }
    QVERIFY(tab.sceneModel().meshGroups.isEmpty());
    QVERIFY(tab.sceneModel().surfaces.isEmpty());
}

void ThreeDViewerInspectorStateTest::tracksFilePathAndSceneMetrics()
{
    ThreeDViewerInspectorState state;

    QCOMPARE(state.filePath(), QString());
    QCOMPARE(state.meshColorMode(), int(ThreeDViewerMeshColorMode::Altitude));
    QCOMPARE(state.measurementMode(), false);
    QCOMPARE(state.caveLengthText(), QLocale::system().toString(0.0, 'f', 1) + QStringLiteral(" m"));
    QCOMPARE(state.caveDepthText(), QLocale::system().toString(0.0, 'f', 1) + QStringLiteral(" m"));
    QCOMPARE(state.surveyCount(), 0);
    QCOMPARE(state.stationCount(), 0);
    QCOMPARE(state.shotCount(), 0);
    QCOMPARE(state.meshCount(), 0);
    QCOMPARE(state.surfaceCount(), 0);

    ThreeDViewerSceneModel sceneModel;
    sceneModel.surveys.append(ThreeDViewerSurvey{1, 0, QStringLiteral("survey"), QStringLiteral("Survey")});
    sceneModel.stations.append(ThreeDViewerStation{1, 1, QStringLiteral("a"), QString(), {0.0, 0.0, 0.0}});
    sceneModel.stations.append(ThreeDViewerStation{2, 1, QStringLiteral("b"), QString(), {6.0, 8.0, -10.0}});
    sceneModel.shots.append(ThreeDViewerShot{1, 2});
    sceneModel.meshGroups.append(ThreeDViewerMeshGroup{});
    sceneModel.surfaces.append(ThreeDViewerSurface{});

    state.setFilePath(QStringLiteral("/tmp/example.lox"));
    state.setSceneModel(sceneModel);

    QCOMPARE(state.filePath(), QStringLiteral("/tmp/example.lox"));
    QCOMPARE(state.caveLengthText(), QStringLiteral("14 m"));
    QCOMPARE(state.caveDepthText(), QStringLiteral("10 m"));
    QCOMPARE(state.surveyCount(), 1);
    QCOMPARE(state.stationCount(), 2);
    QCOMPARE(state.shotCount(), 1);
    QCOMPARE(state.meshCount(), 1);
    QCOMPARE(state.surfaceCount(), 1);
}

void ThreeDViewerInspectorStateTest::tracksSceneWarnings()
{
    ThreeDViewerInspectorState state;
    QSignalSpy spy(&state, &ThreeDViewerInspectorState::sceneWarningsChanged);

    const QStringList warnings{QStringLiteral("Scrap is approximate.")};
    state.setSceneWarnings(warnings);

    QCOMPARE(state.sceneWarnings(), warnings);
    QCOMPARE(spy.count(), 1);

    state.setSceneWarnings({});
    QVERIFY(state.sceneWarnings().isEmpty());
    QCOMPARE(spy.count(), 2);
}

void ThreeDViewerInspectorStateTest::tracksMeshColorMode()
{
    ThreeDViewerInspectorState state;
    QSignalSpy spy(&state, &ThreeDViewerInspectorState::meshColorModeChanged);

    state.setMeshColorMode(int(ThreeDViewerMeshColorMode::None));

    QCOMPARE(state.meshColorMode(), int(ThreeDViewerMeshColorMode::None));
    QCOMPARE(spy.count(), 1);
}

void ThreeDViewerInspectorStateTest::tracksCenterlineColorMode()
{
    ThreeDViewerInspectorState state;
    QSignalSpy spy(&state, &ThreeDViewerInspectorState::centerlineColorModeChanged);

    QCOMPARE(state.centerlineColorMode(), int(ThreeDViewerCenterlineColorMode::Red));
    state.setCenterlineColorMode(int(ThreeDViewerCenterlineColorMode::Altitude));
    QCOMPARE(state.centerlineColorMode(), int(ThreeDViewerCenterlineColorMode::Altitude));
    QCOMPARE(spy.count(), 1);
}

void ThreeDViewerInspectorStateTest::tracksBackgroundMode()
{
    ThreeDViewerInspectorState state;
    QSignalSpy spy(&state, &ThreeDViewerInspectorState::backgroundModeChanged);

    QCOMPARE(state.backgroundMode(), int(ThreeDViewerBackgroundMode::Black));

    state.setBackgroundMode(int(ThreeDViewerBackgroundMode::White));

    QCOMPARE(state.backgroundMode(), int(ThreeDViewerBackgroundMode::White));
    QCOMPARE(spy.count(), 1);

    state.setBackgroundMode(99);

    QCOMPARE(state.backgroundMode(), int(ThreeDViewerBackgroundMode::Black));
    QCOMPARE(spy.count(), 2);
}

void ThreeDViewerInspectorStateTest::tracksMeasurementMode()
{
    ThreeDViewerInspectorState state;
    QSignalSpy spy(&state, &ThreeDViewerInspectorState::measurementModeChanged);

    state.setMeasurementMode(true);

    QCOMPARE(state.measurementMode(), true);
    QCOMPARE(spy.count(), 1);
}

void ThreeDViewerInspectorStateTest::tracksSceneOverlayVisibility()
{
    ThreeDViewerInspectorState state;
    QSignalSpy boundingBoxSpy(&state, &ThreeDViewerInspectorState::showBoundingBoxChanged);
    QSignalSpy hudSpy(&state, &ThreeDViewerInspectorState::showHudChanged);
    QSignalSpy infoSpy(&state, &ThreeDViewerInspectorState::showInfoChanged);

    QCOMPARE(state.showBoundingBox(), true);
    QCOMPARE(state.showHud(), true);
    QCOMPARE(state.showInfo(), true);

    state.setShowBoundingBox(false);
    state.setShowHud(false);
    state.setShowInfo(false);

    QCOMPARE(state.showBoundingBox(), false);
    QCOMPARE(state.showHud(), false);
    QCOMPARE(state.showInfo(), false);
    QCOMPARE(boundingBoxSpy.count(), 1);
    QCOMPARE(hudSpy.count(), 1);
    QCOMPARE(infoSpy.count(), 1);
}

void ThreeDViewerInspectorStateTest::tracksAutoRotationSettings()
{
    ThreeDViewerInspectorState state;
    QSignalSpy enabledSpy(&state, &ThreeDViewerInspectorState::autoRotationEnabledChanged);
    QSignalSpy orthographicSpy(&state, &ThreeDViewerInspectorState::orthographicProjectionChanged);
    QSignalSpy speedSpy(&state, &ThreeDViewerInspectorState::autoRotationSpeedChanged);

    QCOMPARE(state.autoRotationEnabled(), false);
    QCOMPARE(state.orthographicProjection(), false);
    QCOMPARE(state.autoRotationSpeed(), 30.0);

    state.setAutoRotationEnabled(true);
    state.setOrthographicProjection(true);
    state.setAutoRotationSpeed(45.0);

    QCOMPARE(state.autoRotationEnabled(), true);
    QCOMPARE(state.orthographicProjection(), true);
    QCOMPARE(state.autoRotationSpeed(), 45.0);
    QCOMPARE(enabledSpy.count(), 1);
    QCOMPARE(orthographicSpy.count(), 1);
    QCOMPARE(speedSpy.count(), 1);

    state.setAutoRotationSpeed(0.0);
    QCOMPARE(state.autoRotationSpeed(), 5.0);

    state.setAutoRotationSpeed(360.0);
    QCOMPARE(state.autoRotationSpeed(), 90.0);
}

void ThreeDViewerInspectorStateTest::tracksCameraSettings()
{
    ThreeDViewerInspectorState state;
    QSignalSpy facingSpy(&state, &ThreeDViewerInspectorState::cameraFacingDegreesChanged);
    QSignalSpy tiltSpy(&state, &ThreeDViewerInspectorState::cameraTiltDegreesChanged);
    QSignalSpy distanceSpy(&state, &ThreeDViewerInspectorState::cameraDistanceMetersChanged);
    QSignalSpy focusSpy(&state, &ThreeDViewerInspectorState::cameraFocalLengthMmChanged);

    state.setCameraFacingDegrees(725.0);
    state.setCameraTiltDegrees(90.0);
    state.setCameraDistanceMeters(0.0);
    state.setCameraFocalLengthMm(200.0);

    QCOMPARE(state.cameraFacingDegrees(), 5.0);
    QCOMPARE(state.cameraTiltDegrees(), 90.0);
    QCOMPARE(state.cameraDistanceMeters(), 4.0);
    QCOMPARE(state.cameraFocalLengthMm(), 80.0);
    QCOMPARE(facingSpy.count(), 1);
    QCOMPARE(tiltSpy.count(), 1);
    QCOMPARE(distanceSpy.count(), 1);
    QCOMPARE(focusSpy.count(), 1);

    state.setCameraDistanceMeters(250.0);
    QCOMPARE(state.cameraDistanceMeters(), 250.0);

    state.setCameraFocalLengthMm(0.0);
    QCOMPARE(state.cameraFocalLengthMm(), 10.0);
}

void ThreeDViewerInspectorStateTest::loadsCentrelineFromThFile()
{
    // Test that computeTherionCentreline creates a ThreeDViewerSceneModel compatible with ThreeDViewerInspectorState
    const QString hpPath = QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("projects/map_editor_regression/real_cave_raster/data/hp.th"));

    ThreeDViewerTab tab;
    QString error;
    QVERIFY(tab.loadFile(hpPath, &error));
    QVERIFY(error.isEmpty());

    const ThreeDViewerSceneModel &scene = tab.sceneModel();
    QCOMPARE(scene.stations.size(), 10);
    QCOMPARE(scene.shots.size(), 9);
    QVERIFY(scene.meshGroups.isEmpty());
    QVERIFY(scene.scrapLinework.isEmpty());
    QVERIFY(scene.scrapPoints.isEmpty());
    QVERIFY(scene.surfaces.isEmpty());
    QVERIFY(!scene.stations.first().name.isEmpty());
}

void ThreeDViewerInspectorStateTest::loadsScrapObjectsWithoutSurfaces()
{
    const QString th2Path = QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("projects/map_editor_regression/real_cave_raster/data/clopy01.th2"));

    ThreeDViewerTab tab;
    QString error;
    QVERIFY(tab.loadFile(th2Path, &error));
    QVERIFY(error.isEmpty());

    const ThreeDViewerSceneModel &model = tab.sceneModel();
    QCOMPARE(model.stations.size(), 10);
    QCOMPARE(model.shots.size(), 9);
    QVERIFY(model.bounds().valid);
    QVERIFY(model.meshGroups.isEmpty());
    QVERIFY(!model.scrapLinework.isEmpty());
    for (const ThreeDViewerScrapPolyline &line : model.scrapLinework) {
        QVERIFY(line.points.size() >= 2);
        if (line.type == QStringLiteral("wall")) {
            QCOMPARE(line.strokeWidth, qreal(3.0));
        } else {
            QVERIFY(line.strokeWidth < 3.0);
        }
    }
    QVERIFY(model.surfaces.isEmpty());
    const auto *inspectorState = tab.findChild<ThreeDViewerInspectorState *>();
    QVERIFY(inspectorState != nullptr);
    QVERIFY(inspectorState->sceneWarnings().isEmpty());
}

void ThreeDViewerInspectorStateTest::selectsOnlyRequestedSurveySourceForTh2()
{
    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());

    const QString th2Path = QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("projects/map_editor_regression/real_cave_raster/data/clopy01.th2"));
    const QString sharedTh2Path = projectDir.filePath(QStringLiteral("shared.th2"));
    QVERIFY(QFile::copy(th2Path, sharedTh2Path));

    const auto writeSurvey = [&projectDir](const QString &fileName, const QString &readings) {
        QFile file(projectDir.filePath(fileName));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        const QByteArray contents = QStringLiteral(
            "survey hp\n"
            "  input shared.th2\n"
            "  centreline\n"
            "    data normal from to length compass clino\n"
            "%1"
            "  endcentreline\n"
            "endsurvey\n")
                                        .arg(readings)
                                        .toUtf8();
        return file.write(contents) == contents.size();
    };
    QVERIFY(writeSurvey(QStringLiteral("source_a.th"),
                        QStringLiteral("    0 1 10 90 0\n"
                                       "    1 2 10 90 0\n")));
    QVERIFY(writeSurvey(QStringLiteral("source_b.th"), QStringLiteral("    0 1 20 90 0\n")));

    const QStringList candidates =
        ThreeDViewerTab::surveySourceCandidatesForTh2(sharedTh2Path, projectDir.path());
    QCOMPARE(candidates.size(), 2);
    QVERIFY(candidates.at(0).endsWith(QStringLiteral("source_a.th")));
    QVERIFY(candidates.at(1).endsWith(QStringLiteral("source_b.th")));

    ThreeDViewerTab tab;
    tab.setProjectRootPath(projectDir.path());
    QString error;
    QVERIFY(tab.loadFile(sharedTh2Path, candidates.at(1), &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(tab.sceneModel().stations.size(), 2);
    QVERIFY(tab.sceneModel().meshGroups.isEmpty());
    QVERIFY(!tab.sceneModel().scrapLinework.isEmpty());
    for (const ThreeDViewerScrapPolyline &line : tab.sceneModel().scrapLinework) {
        QVERIFY(line.points.size() >= 2);
    }

    QVERIFY(tab.reloadFile(&error));
    QVERIFY(error.isEmpty());
    QCOMPARE(tab.sceneModel().stations.size(), 2);

    QVERIFY(tab.loadFile(sharedTh2Path, candidates.at(0), &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(tab.sceneModel().stations.size(), 3);
    QVERIFY(tab.sceneModel().meshGroups.isEmpty());
    QVERIFY(!tab.sceneModel().scrapLinework.isEmpty());
    for (const ThreeDViewerScrapPolyline &line : tab.sceneModel().scrapLinework) {
        QVERIFY(line.points.size() >= 2);
    }
}

int runThreeDViewerInspectorStateTest(int argc, char **argv)
{
    ThreeDViewerInspectorStateTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerInspectorStateTest.moc"
