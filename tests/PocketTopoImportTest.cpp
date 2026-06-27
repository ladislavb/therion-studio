#include "../src/core/PocketTopoImport.h"
#include "../src/core/TherionXviParser.h"

#include <QtTest/QtTest>

namespace
{
QString samplePocketTopoText()
{
    return QStringLiteral(
        "FIX\n"
        "A1 10 20 30\n"
        "TRIP\n"
        "DATE 2026-06-07\n"
        "DECLINATION 1.5\n"
        "DATA\n"
        "1 2 12.3 45.0 3.0 >\n"
        "2 3 5.0 90.0 -1.0 >\n"
        "3 4 4.0 120.0 0.5 <\n"
        "PLAN\n"
        "STATIONS\n"
        "0 0 1\n"
        "1 0 2\n"
        "SHOTS\n"
        "0 0 1 0\n"
        "POLYLINE RED\n"
        "0 0\n"
        "1 1\n"
        "ELEVATION\n"
        "STATIONS\n"
        "0 0 1\n");
}

class PocketTopoImportTest final : public QObject
{
    Q_OBJECT

private slots:
    void convertsCentreline();
    void convertsXvi();
};
}

void PocketTopoImportTest::convertsCentreline()
{
    const QString result = TherionStudio::convertPocketTopoTextToTherionCentreline(samplePocketTopoText());
    QVERIFY(result.contains(QStringLiteral("centreline\n  fix A1 10 20 30\nendcentreline")));
    QVERIFY(result.contains(QStringLiteral("  date 2026.06.07\n")));
    QVERIFY(result.contains(QStringLiteral("  data normal from to compass clino tape\n")));
    QVERIFY(result.contains(QStringLiteral("  extend right\n  1 2 12.3 45.0 3.0\n")));
    QVERIFY(result.contains(QStringLiteral("  extend left\n  3 4 4.0 120.0 0.5\n")));
}

void PocketTopoImportTest::convertsXvi()
{
    TherionStudio::PocketTopoXviImportOptions options;
    options.scale = 200;
    options.resolutionDpi = 200;
    options.gridSpacingMeters = 1.0;
    options.projection = TherionStudio::PocketTopoProjection::Plan;

    bool hasData = false;
    const QString result = TherionStudio::convertPocketTopoTextToXvi(samplePocketTopoText(), options, &hasData);
    TherionStudio::TherionXviDocument document;
    QVERIFY(hasData);
    QVERIFY(result.contains(QStringLiteral("set XVIgrids {1 m}")));
    QVERIFY(TherionStudio::parseTherionXviDocumentText(result, &document));
    QVERIFY(document.hasGridDefinition);
    QCOMPARE(document.stationEntries.size(), 2);
    QCOMPARE(document.shots.size(), 1);
    QCOMPARE(document.sketchLines.size(), 1);
    QCOMPARE(document.sketchLines.first().colorToken, QStringLiteral("red"));
    QCOMPARE(document.sketchLines.first().points.size(), 2);
}

int runPocketTopoImportTest(int argc, char **argv)
{
    PocketTopoImportTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "PocketTopoImportTest.moc"
