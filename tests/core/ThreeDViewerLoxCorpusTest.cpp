#include "../../src/core/ThreeDViewerLoxLoader.h"

#include <QDir>
#include <QFile>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class ThreeDViewerLoxCorpusTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsKnownFixture_data();
    void loadsKnownFixture();
};

void ThreeDViewerLoxCorpusTest::loadsKnownFixture_data()
{
    QTest::addColumn<QString>("relativePath");
    QTest::addColumn<bool>("expectedNestedSurvey");
    QTest::addColumn<bool>("expectedMesh");
    QTest::addColumn<bool>("expectedSurfaceShot");
    QTest::addColumn<bool>("expectedDuplicateShot");
    QTest::addColumn<bool>("expectedSplayShot");
    QTest::addColumn<bool>("expectedStationFlag");

    QTest::newRow("1303")
        << QStringLiteral("babice/01_zadni_pole/1303_dvanactka/_output/1303.lox")
        << true << true << false << false << true << false;
    QTest::newRow("1303-1974")
        << QStringLiteral("babice/01_zadni_pole/1303_dvanactka/_output/1303_1974.lox")
        << true << true << false << false << false << false;
    QTest::newRow("1318")
        << QStringLiteral("babice/01_zadni_pole/1318_vetrna_propast/_output/1318.lox")
        << true << true << false << true << false << false;
    QTest::newRow("1319")
        << QStringLiteral("babice/01_zadni_pole/1319_devitka/_output/1319.lox")
        << true << true << false << false << false << false;
    QTest::newRow("zadni-pole")
        << QStringLiteral("babice/01_zadni_pole/_output/zadni_pole.lox")
        << true << true << true << true << true << true;
    QTest::newRow("1313")
        << QStringLiteral("babice/03_skalky/1313_babicka/_output/1313.lox")
        << true << true << false << false << false << false;
    QTest::newRow("1313-II")
        << QStringLiteral("babice/03_skalky/1313_babicka_II/_output/1313_II.lox")
        << true << true << false << false << false << false;
    QTest::newRow("skalky")
        << QStringLiteral("babice/03_skalky/_output/skalky.lox")
        << true << true << false << false << false << false;
    QTest::newRow("babice")
        << QStringLiteral("babice/_output/babice.lox")
        << true << true << true << true << false << true;
    QTest::newRow("1302")
        << QStringLiteral("clopy/_output/1302.lox")
        << true << true << false << false << false << false;
}

void ThreeDViewerLoxCorpusTest::loadsKnownFixture()
{
    QFETCH(QString, relativePath);
    QFETCH(bool, expectedNestedSurvey);
    QFETCH(bool, expectedMesh);
    QFETCH(bool, expectedSurfaceShot);
    QFETCH(bool, expectedDuplicateShot);
    QFETCH(bool, expectedSplayShot);
    QFETCH(bool, expectedStationFlag);

    const QString path = QDir(QStringLiteral(THERION_STUDIO_LOX_CORPUS_ROOT)).filePath(relativePath);
    if (!QFile::exists(path)) {
        QSKIP(qPrintable(QStringLiteral("Optional .lox corpus fixture is missing: %1").arg(path)));
    }

    const ThreeDViewerLoxLoader loader;
    const ThreeDViewerLoxLoader::Result result = loader.loadFile(path);

    QVERIFY2(result.ok(), qPrintable(QStringLiteral("%1: %2").arg(path, result.error)));
    QVERIFY2(!result.scene.isEmpty(), qPrintable(path));
    QVERIFY2(result.scene.bounds().valid, qPrintable(path));
    QVERIFY2(!result.scene.surveys.isEmpty(), qPrintable(path));
    QVERIFY2(!result.scene.stations.isEmpty(), qPrintable(path));

    bool hasNestedSurvey = false;
    for (const ThreeDViewerSurvey &survey : result.scene.surveys) {
        hasNestedSurvey = hasNestedSurvey || survey.parentId != 0;
    }

    bool hasSurfaceShot = false;
    bool hasDuplicateShot = false;
    bool hasSplayShot = false;
    for (const ThreeDViewerShot &shot : result.scene.shots) {
        hasSurfaceShot = hasSurfaceShot || shot.surface;
        hasDuplicateShot = hasDuplicateShot || shot.duplicate;
        hasSplayShot = hasSplayShot || shot.splay;
    }

    bool hasStationFlag = false;
    for (const ThreeDViewerStation &station : result.scene.stations) {
        hasStationFlag = hasStationFlag || station.surface || station.entrance || station.fixed
            || station.continuation || station.hasWalls;
    }

    QCOMPARE(hasNestedSurvey, expectedNestedSurvey);
    QCOMPARE(!result.scene.meshGroups.isEmpty(), expectedMesh);
    QCOMPARE(hasSurfaceShot, expectedSurfaceShot);
    QCOMPARE(hasDuplicateShot, expectedDuplicateShot);
    QCOMPARE(hasSplayShot, expectedSplayShot);
    QCOMPARE(hasStationFlag, expectedStationFlag);
}
}

QTEST_APPLESS_MAIN(ThreeDViewerLoxCorpusTest)

#include "ThreeDViewerLoxCorpusTest.moc"
