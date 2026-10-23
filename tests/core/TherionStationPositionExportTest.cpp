#include "../../src/core/ProjectStructureIndex.h"
#include "../../src/core/TherionStationPositionExport.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtMath>
#include <QtTest/QtTest>

using namespace TherionStudio;

class TherionStationPositionExportTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsConfigSources()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("data")));
        for (const QString &name : {QStringLiteral("cave.th"), QStringLiteral("data/extra.th")}) {
            QFile file(QDir(dir.path()).filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const QString configPath = QDir(dir.path()).filePath(QStringLiteral("thconfig"));
        // Layouts, exports and missing files are not sources; a source without `.th` resolves as Therion does.
        const QStringList sources = therionConfigSourcePaths(configPath,
                                                             QStringLiteral("encoding utf-8\n"
                                                                            "source cave.th\n"
                                                                            "source data/extra\n"
                                                                            "source missing.th\n"
                                                                            "export map -o out.pdf\n"
                                                                            "layout plan\n"
                                                                            "endlayout\n"));
        QCOMPARE(sources.size(), 2);
        QVERIFY(sources.at(0).endsWith(QStringLiteral("/cave.th")));
        QVERIFY(sources.at(1).endsWith(QStringLiteral("/data/extra.th")));
    }

    void writesDatabaseOnlyConfig()
    {
        QCOMPARE(therionStationPositionExportConfig({QStringLiteral("/caves/my cave/cave.th")}, QStringLiteral("out.sql")),
                 QStringLiteral("encoding utf-8\n"
                                "source \"/caves/my cave/cave.th\"\n"
                                "export database -fmt sql -o \"out.sql\"\n"));
    }

    void keysPositionsLikeTheProjectIndex()
    {
        const QHash<QString, Th2StationPosition> positions = therionStationPositionsFromRows(
            {{QStringLiteral("12"), QStringLiteral("Lower.Cave"), QStringLiteral("1.5"), QStringLiteral("2"), QStringLiteral("-3")},
             {QStringLiteral("-"), QStringLiteral("Lower.Cave"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0")},
             {QStringLiteral("0"), QString(), QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6")},
             {QStringLiteral("9"), QStringLiteral("Lower.Cave"), QStringLiteral("x"), QStringLiteral("0"), QStringLiteral("0")},
             {QStringLiteral("short")}});
        QCOMPARE(positions.size(), 2);
        const Th2StationPosition station = positions.value(ProjectStructureIndex::stationKey(QStringLiteral("12"),
                                                                                            QStringLiteral("lower.cave")));
        QCOMPARE(station.x, 1.5);
        QCOMPARE(station.z, -3.0);
        QVERIFY(positions.contains(ProjectStructureIndex::stationKey(QStringLiteral("0"), QString())));
        QVERIFY(therionStationPositionQuery().contains(QStringLiteral("FULL_NAME")));
    }

    void readsMeasuredLegVectors()
    {
        const QVector<Th2MeasuredLeg> legs = therionMeasuredLegsFromRows(
            {{QStringLiteral("1"), QStringLiteral("a"), QStringLiteral("2"), QStringLiteral("a"), QStringLiteral("10"),
              QStringLiteral("90"), QStringLiteral("30")},
             {QStringLiteral("2"), QStringLiteral("a"), QStringLiteral("-"), QStringLiteral("a"), QStringLiteral("2"),
              QStringLiteral("0"), QStringLiteral("0")},
             {QStringLiteral("2"), QStringLiteral("a"), QStringLiteral("3"), QStringLiteral("a"), QStringLiteral("x"),
              QStringLiteral("0"), QStringLiteral("0")},
             {QStringLiteral("short")}});
        QCOMPARE(legs.size(), 1);
        QCOMPARE(legs.first().fromStationKey, ProjectStructureIndex::stationKey(QStringLiteral("1"), QStringLiteral("a")));
        QCOMPARE(legs.first().toStationKey, ProjectStructureIndex::stationKey(QStringLiteral("2"), QStringLiteral("a")));
        // 10 m east, 30 degrees up.
        QVERIFY(qAbs(legs.first().vector.x - 10.0 * qCos(qDegreesToRadians(30.0))) < 1e-9);
        QVERIFY(qAbs(legs.first().vector.y) < 1e-9);
        QVERIFY(qAbs(legs.first().vector.z - 5.0) < 1e-9);
        QVERIFY(therionMeasuredLegQuery().contains(QStringLiteral("'spl'")));
    }
};

int runTherionStationPositionExportTest(int argc, char **argv)
{
    TherionStationPositionExportTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionStationPositionExportTest.moc"
