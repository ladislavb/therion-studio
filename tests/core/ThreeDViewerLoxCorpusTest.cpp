#include "../../src/core/ThreeDViewerLoxLoader.h"
#include "ThreeDViewerLoxCorpusFixtures.h"

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
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("available");
    QTest::addColumn<bool>("expectedNestedSurvey");
    QTest::addColumn<bool>("expectedMesh");
    QTest::addColumn<bool>("expectedSurfaceShot");
    QTest::addColumn<bool>("expectedDuplicateShot");
    QTest::addColumn<bool>("expectedSplayShot");
    QTest::addColumn<bool>("expectedStationFlag");

    const QVector<ResolvedThreeDViewerLoxCorpusFixture> fixtures =
        resolveThreeDViewerLoxCorpusFixtures(QStringLiteral(THERION_STUDIO_LOX_CORPUS_ROOT));
    for (const ResolvedThreeDViewerLoxCorpusFixture &resolved : fixtures) {
        const ThreeDViewerLoxCorpusFixture &fixture = resolved.fixture;
        const QByteArray rowName = fixture.rowName.toUtf8();
        QTest::newRow(rowName.constData()) << resolved.absolutePath << resolved.available
                                          << fixture.expectedNestedSurvey << fixture.expectedMesh
                                          << fixture.expectedSurfaceShot << fixture.expectedDuplicateShot
                                          << fixture.expectedSplayShot << fixture.expectedStationFlag;
    }
}

void ThreeDViewerLoxCorpusTest::loadsKnownFixture()
{
    QFETCH(QString, path);
    QFETCH(bool, available);
    QFETCH(bool, expectedNestedSurvey);
    QFETCH(bool, expectedMesh);
    QFETCH(bool, expectedSurfaceShot);
    QFETCH(bool, expectedDuplicateShot);
    QFETCH(bool, expectedSplayShot);
    QFETCH(bool, expectedStationFlag);

    if (!available) {
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
