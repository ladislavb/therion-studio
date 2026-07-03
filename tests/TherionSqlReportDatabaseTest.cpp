#include "../src/app/reports/TherionSqlReportDatabase.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>

using namespace TherionStudio;

namespace
{
class TherionSqlReportDatabaseTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsPredefinedReportsFromResource();
    void importsTherionSqlExportAndRunsReports();
    void rejectsCustomMutationQuery();
};

QString minimalTherionSqlExport()
{
    return QStringLiteral(
        "create table SURVEY (ID integer, PARENT_ID integer, NAME varchar(64), FULL_NAME varchar(128), TITLE varchar(128));\n"
        "create table CENTRELINE (ID integer, SURVEY_ID integer, TITLE varchar(128), TOPO_DATE date, EXPLO_DATE date, LENGTH real, SURFACE_LENGTH real, DUPLICATE_LENGTH real);\n"
        "create table PERSON (ID integer, NAME varchar(64), SURNAME varchar(64));\n"
        "create table EXPLO (PERSON_ID integer, CENTRELINE_ID integer);\n"
        "create table TOPO (PERSON_ID integer, CENTRELINE_ID integer);\n"
        "create table STATION (ID integer, NAME varchar(64), SURVEY_ID integer, X real, Y real, Z real);\n"
        "create table STATION_FLAG (STATION_ID integer, FLAG char(3));\n"
        "create table SHOT (ID integer, FROM_ID integer, TO_ID integer, CENTRELINE_ID integer, LENGTH real, BEARING real, GRADIENT real, ADJ_LENGTH real, ADJ_BEARING real, ADJ_GRADIENT real, ERR_LENGTH real, ERR_BEARING real, ERR_GRADIENT real);\n"
        "create table SHOT_FLAG (SHOT_ID integer, FLAG char(3));\n"
        "insert into SURVEY values (1, NULL, 'main', 'main', 'Main survey');\n"
        "insert into CENTRELINE values (1, 1, 'main centreline', '2026-07-02', '2026-07-01', 12.5, 1.5, 0.5);\n"
        "insert into PERSON values (1, 'Ada', 'Lovelace');\n"
        "insert into EXPLO values (1, 1);\n"
        "insert into TOPO values (1, 1);\n"
        "insert into STATION values (1, 'A', 1, 0, 0, 100);\n"
        "insert into STATION values (2, 'B', 1, 10, 0, 110);\n"
        "insert into STATION_FLAG values (1, 'ent');\n"
        "insert into STATION_FLAG values (2, 'con');\n"
        "insert into SHOT values (1, 1, 2, 1, 12.5, 90, 10, 12.4, 91, 9, 0.1, 1, 1);\n"
        "insert into SHOT_FLAG values (1, 'srf');\n");
}

QString writeSqlFixture(QTemporaryDir *tempDir)
{
    const QString path = QDir(tempDir->path()).filePath(QStringLiteral("therion.sql"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    file.write(minimalTherionSqlExport().toUtf8());
    file.close();
    return path;
}

void TherionSqlReportDatabaseTest::loadsPredefinedReportsFromResource()
{
    const QVector<TherionSqlReportDefinition> reports = TherionSqlReportDatabase::predefinedReports();
    QCOMPARE(reports.size(), 11);

    auto findPreset = [&reports](const QString &id) {
        return std::find_if(reports.cbegin(), reports.cend(), [&id](const TherionSqlReportDefinition &report) {
            return report.id == id;
        });
    };

    const auto exploration = findPreset(QStringLiteral("exploration-by-person"));
    QVERIFY(exploration != reports.cend());
    QCOMPARE(exploration->title, QStringLiteral("Exploration by Person"));
    QVERIFY(exploration->query.contains(QStringLiteral("join EXPLO")));

    const auto surveying = findPreset(QStringLiteral("surveying-by-person"));
    QVERIFY(surveying != reports.cend());
    QCOMPARE(surveying->title, QStringLiteral("Surveying by Person"));
    QVERIFY(surveying->query.contains(QStringLiteral("join TOPO")));

    const auto continuations = findPreset(QStringLiteral("continuation-stations"));
    QVERIFY(continuations != reports.cend());
    QVERIFY(continuations->query.contains(QStringLiteral("F.FLAG = 'con'")));
}

void TherionSqlReportDatabaseTest::importsTherionSqlExportAndRunsReports()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString path = writeSqlFixture(&tempDir);
    QVERIFY(!path.isEmpty());

    TherionSqlReportDatabase database;
    const TherionSqlReportImportResult importResult = database.importFile(path);
    QVERIFY2(importResult.success, qPrintable(importResult.errorMessage));
    QVERIFY(database.tableNames().contains(QStringLiteral("SURVEY")));
    QVERIFY(database.tableColumns(QStringLiteral("STATION")).contains(QStringLiteral("NAME")));

    QString errorMessage;
    const TherionSqlReportTable table =
        database.executeCustomQuery(QStringLiteral("select count(*) as Stations from STATION"), &errorMessage);
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(table.columns, QStringList{QStringLiteral("Stations")});
    QCOMPARE(table.rows.size(), 1);
    QCOMPARE(table.rows.first().first(), QStringLiteral("2"));

    const QVector<TherionSqlReportDefinition> reports = TherionSqlReportDatabase::predefinedReports();
    const auto overview = std::find_if(reports.cbegin(), reports.cend(), [](const TherionSqlReportDefinition &report) {
        return report.id == QStringLiteral("overview");
    });
    QVERIFY(overview != reports.cend());

    errorMessage.clear();
    const TherionSqlReportTable overviewTable = database.executeReportQuery(overview->query, &errorMessage);
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(overviewTable.columns, (QStringList{QStringLiteral("Metric"), QStringLiteral("Value")}));
    QVERIFY(!overviewTable.rows.contains(QStringList{QStringLiteral("People"), QStringLiteral("1")}));
    QVERIFY(!overviewTable.rows.contains(QStringList{QStringLiteral("Total length"), QStringLiteral("12.5")}));
    QVERIFY(overviewTable.rows.contains(QStringList{QStringLiteral("Explorers"), QStringLiteral("1")}));
    QVERIFY(overviewTable.rows.contains(QStringList{QStringLiteral("Surveyors"), QStringLiteral("1")}));
    QVERIFY(overviewTable.rows.contains(QStringList{QStringLiteral("Length"), QStringLiteral("12.5")}));
    QVERIFY(overviewTable.rows.contains(QStringList{QStringLiteral("Depth"), QStringLiteral("10")}));
}

void TherionSqlReportDatabaseTest::rejectsCustomMutationQuery()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString path = writeSqlFixture(&tempDir);
    QVERIFY(!path.isEmpty());

    TherionSqlReportDatabase database;
    const TherionSqlReportImportResult importResult = database.importFile(path);
    QVERIFY2(importResult.success, qPrintable(importResult.errorMessage));

    QString errorMessage;
    const TherionSqlReportTable table =
        database.executeCustomQuery(QStringLiteral("delete from STATION"), &errorMessage);
    QVERIFY(table.columns.isEmpty());
    QVERIFY(table.rows.isEmpty());
    QVERIFY(!errorMessage.isEmpty());
}
}

int runTherionSqlReportDatabaseTest(int argc, char **argv)
{
    TherionSqlReportDatabaseTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSqlReportDatabaseTest.moc"
