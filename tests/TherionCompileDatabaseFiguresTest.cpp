#include "../src/app/reports/TherionCompileDatabaseFigures.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
// Tables and rows in the form Therion 6.4 writes them (thexpdb.cxx); flat is a -namespace off survey.
const char *const kExport = R"(create table SURVEY (ID integer, PARENT_ID integer, NAME varchar(6), FULL_NAME varchar(10), TITLE varchar(10));
create table CENTRELINE (ID integer, SURVEY_ID integer, TITLE varchar(4), TOPO_DATE date, EXPLO_DATE date, LENGTH real, SURFACE_LENGTH real, DUPLICATE_LENGTH real);
create table PERSON (ID integer, NAME varchar(10), SURNAME varchar(8));
create table EXPLO (PERSON_ID integer, CENTRELINE_ID integer);
create table TOPO (PERSON_ID integer, CENTRELINE_ID integer);
create table STATION (ID integer, NAME varchar(4), SURVEY_ID integer, X real, Y real, Z real);
create table STATION_FLAG (STATION_ID integer, FLAG char(3));
create table SHOT (ID integer, FROM_ID integer, TO_ID integer, CENTRELINE_ID integer, LENGTH real, BEARING real, GRADIENT real, ADJ_LENGTH real, ADJ_BEARING real, ADJ_GRADIENT real, ERR_LENGTH real, ERR_BEARING real, ERR_GRADIENT real);
create table SHOT_FLAG (SHOT_ID integer, FLAG char(3));
create table MAPS (ID integer, SURVEY_ID integer, NAME varchar(6), TITLE varchar(35), PROJID integer, LENGTH real, DEPTH real);
create table SCRAPS (ID integer, SURVEY_ID integer, NAME varchar(6), PROJID integer, MAX_DISTORTION real, AVG_DISTORTION real);
create table MAPITEMS (ID integer, TYPE integer, ITEMID integer);
insert into SURVEY values (1, 0, '', '', NULL);
 insert into SURVEY values (17, 1, 'cave', 'cave', 'Cave');
 insert into SURVEY values (19, 17, 'inner', 'inner.cave', NULL);
insert into SCRAPS values (22, 19, 'SI', 1, 30.77453, 2.75729);
 insert into MAPS values (27, 19, 'MI', 'Inner plan', 1, 15.000, -5.390);
 insert into MAPITEMS values (27, 4, 22);
 insert into SURVEY values (29, 17, 'flat', 'flat.cave', NULL);
insert into SCRAPS values (32, 29, 'SF', 1, NULL, NULL);
 insert into MAPS values (37, 29, 'MF', NULL, 1, 3.000, 0.000);
 insert into MAPITEMS values (37, 4, 32);
 insert into MAPS values (39, 17, 'MC', NULL, 1, NULL, NULL);
 insert into MAPITEMS values (39, 4, 27);
 insert into MAPITEMS values (39, 4, 37);
 insert into MAPITEMS values (39, 2, 27);
 insert into MAPS values (40, 1, 'TOP', NULL, 1, 1.000, 0.000);
)";

class TherionCompileDatabaseFiguresTest : public QObject
{
    Q_OBJECT

private slots:
    void readsMapFigures();
    void readsMapContentsInOrder();
    void readsScrapDistortion();
    void keysObjectsBySurveyBlock();
    void unavailableWithoutReadableExport();
    void skipsExportsThatDoNotImport();
};

TherionCompileDatabaseFigures readFixture(QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("db.sql"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(kExport);
    file.close();
    return readTherionCompileDatabaseFigures({path});
}

void TherionCompileDatabaseFiguresTest::readsMapFigures()
{
    QTemporaryDir dir;
    const TherionCompileDatabaseFigures figures = readFixture(dir);

    QVERIFY(figures.isAvailable());
    const TherionMapFigures map = figures.mapsByKey.value(QStringLiteral("MI@inner.cave"));
    QCOMPARE(map.title, QStringLiteral("Inner plan"));
    QVERIFY(map.length);
    QCOMPARE(*map.length, 15.0);
    // DEPTH is bottom minus top in the export; shown positive like the PDF legend.
    QVERIFY(map.depth);
    QCOMPARE(*map.depth, 5.39);
    const TherionMapFigures unmeasured = figures.mapsByKey.value(QStringLiteral("MC@cave"));
    QVERIFY(!unmeasured.length);
    QVERIFY(!unmeasured.depth);
}

void TherionCompileDatabaseFiguresTest::readsMapContentsInOrder()
{
    QTemporaryDir dir;
    const QVector<TherionMapItemFigures> items =
        readFixture(dir).mapsByKey.value(QStringLiteral("MC@cave")).items;

    QCOMPARE(items.size(), 3);
    QVERIFY(items.at(0).isMap);
    QCOMPARE(items.at(0).name, QStringLiteral("MI"));
    QCOMPARE(items.at(0).surveyFullName, QStringLiteral("inner.cave"));
    QCOMPARE(items.at(0).type, 4);
    QCOMPARE(items.at(1).name, QStringLiteral("MF"));
    QCOMPARE(items.at(2).type, 2);
    const QVector<TherionMapItemFigures> scrapItems =
        readFixture(dir).mapsByKey.value(QStringLiteral("MI@inner.cave")).items;
    QCOMPARE(scrapItems.size(), 1);
    QVERIFY(!scrapItems.at(0).isMap);
    QCOMPARE(scrapItems.at(0).name, QStringLiteral("SI"));
}

void TherionCompileDatabaseFiguresTest::readsScrapDistortion()
{
    QTemporaryDir dir;
    const TherionCompileDatabaseFigures figures = readFixture(dir);

    const TherionScrapFigures scrap = figures.scrapsByKey.value(QStringLiteral("SI@inner.cave"));
    QVERIFY(scrap.averageDistortion);
    QCOMPARE(*scrap.averageDistortion, 2.75729);
    QVERIFY(scrap.maximumDistortion);
    QCOMPARE(*scrap.maximumDistortion, 30.77453);
    QVERIFY(!figures.scrapsByKey.value(QStringLiteral("SF@flat.cave")).averageDistortion);
}

void TherionCompileDatabaseFiguresTest::keysObjectsBySurveyBlock()
{
    QTemporaryDir dir;
    const TherionCompileDatabaseFigures figures = readFixture(dir);

    // Objects of a -namespace off survey are filed under that survey, not its namespace.
    QVERIFY(figures.mapsByKey.contains(QStringLiteral("MF@flat.cave")));
    QVERIFY(figures.scrapsByKey.contains(QStringLiteral("SF@flat.cave")));
    QVERIFY(figures.mapsByKey.contains(QStringLiteral("TOP")));
    QCOMPARE(therionCompiledObjectKey(QStringLiteral("TOP"), QString()), QStringLiteral("TOP"));
}

void TherionCompileDatabaseFiguresTest::unavailableWithoutReadableExport()
{
    QTemporaryDir dir;
    QVERIFY(!readTherionCompileDatabaseFigures({}).isAvailable());
    QVERIFY(!readTherionCompileDatabaseFigures({dir.filePath(QStringLiteral("missing.sql"))}).isAvailable());
}

void TherionCompileDatabaseFiguresTest::skipsExportsThatDoNotImport()
{
    QTemporaryDir dir;
    const QString broken = dir.filePath(QStringLiteral("other.sql"));
    QFile file(broken);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("create table OTHER (ID integer);\n");
    file.close();
    readFixture(dir);

    const TherionCompileDatabaseFigures figures =
        readTherionCompileDatabaseFigures({broken, dir.filePath(QStringLiteral("db.sql"))});
    QCOMPARE(figures.databasePath, dir.filePath(QStringLiteral("db.sql")));
    QVERIFY(figures.mapsByKey.contains(QStringLiteral("MI@inner.cave")));
}
}

int runTherionCompileDatabaseFiguresTest(int argc, char **argv)
{
    TherionCompileDatabaseFiguresTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionCompileDatabaseFiguresTest.moc"
