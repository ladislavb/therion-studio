#include "../src/app/ConnectionPositionRunPolicy.h"
#include "../src/app/ProjectConnectionDataStatus.h"
#include "../src/app/ProjectStationPositionService.h"
#include "../src/core/ProjectStructureIndex.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
// The service runs Therion with only its config file name; this test binary stands in for Therion then.
const char *const kConfigArgument = "station-positions.thconfig";

bool writeStationExport(const QString &path)
{
    QFile sql(path);
    if (!sql.open(QIODevice::WriteOnly)) {
        return false;
    }
    return sql.write("create table SURVEY (ID integer, PARENT_ID integer, NAME varchar(4), FULL_NAME varchar(9), TITLE varchar(4));\n"
              "create table CENTRELINE (ID integer, SURVEY_ID integer, TITLE varchar(4), TOPO_DATE date, EXPLO_DATE date, "
              "LENGTH real, SURFACE_LENGTH real, DUPLICATE_LENGTH real);\n"
              "create table PERSON (ID integer, NAME varchar(8), SURNAME varchar(10));\n"
              "create table EXPLO (PERSON_ID integer, CENTRELINE_ID integer);\n"
              "create table TOPO (PERSON_ID integer, CENTRELINE_ID integer);\n"
              "create table STATION (ID integer, NAME varchar(5), SURVEY_ID integer, X real, Y real, Z real);\n"
              "create table STATION_FLAG (STATION_ID integer, FLAG char(3));\n"
              "create table SHOT (ID integer, FROM_ID integer, TO_ID integer, CENTRELINE_ID integer, LENGTH real, "
              "BEARING real, GRADIENT real, ADJ_LENGTH real, ADJ_BEARING real, ADJ_GRADIENT real, ERR_LENGTH real, "
              "ERR_BEARING real, ERR_GRADIENT real);\n"
              "create table SHOT_FLAG (SHOT_ID integer, FLAG char(3));\n"
              "insert into SURVEY values (1, 0, 'a', 'a.cave', NULL);\n"
              "insert into STATION values (1, '1', 1, 10.0, 20.0, 30.0);\n"
              "insert into STATION values (2, '-', 1, 0.0, 0.0, 0.0);\n"
              "insert into STATION values (3, '2', 1, 10.0, 25.0, 30.0);\n"
              "insert into SHOT values (1, 1, 3, 1, 5.0, 0.0, 0.0, 5.0, 0.0, 0.0, 0.0, 0.0, 0.0);\n"
              "insert into SHOT values (2, 1, 2, 1, 2.0, 90.0, 0.0, 2.0, 90.0, 0.0, 0.0, 0.0, 0.0);\n"
              "insert into SHOT_FLAG values (2, 'spl');\n"
              "insert into SHOT values (3, 3, 1, 1, 5.0, 180.0, 0.0, 5.0, 180.0, 0.0, 0.0, 0.0, 0.0);\n"
              "insert into SHOT_FLAG values (3, 'dpl');\n") > 0;
}

int runFakeTherion()
{
    QFile config(QString::fromLatin1(kConfigArgument));
    if (!config.open(QIODevice::ReadOnly) || !config.readAll().contains("export database -fmt sql")) {
        return 1;
    }
    return writeStationExport(QStringLiteral("station-positions.sql")) ? 0 : 1;
}

QString writeProject(const QTemporaryDir &dir)
{
    QFile source(QDir(dir.path()).filePath(QStringLiteral("cave.th")));
    QFile config(QDir(dir.path()).filePath(QStringLiteral("thconfig")));
    if (!source.open(QIODevice::WriteOnly) || !config.open(QIODevice::WriteOnly)) {
        return {};
    }
    config.write("source cave.th\nexport map -o cave.pdf\n");
    return config.fileName();
}
}

class ProjectStationPositionServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void exportsAndLoadsPositionsOutsideTheProject()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        QVERIFY(project.isValid() && temporaryBase.isValid());
        const QString configPath = writeProject(project);
        QVERIFY(!configPath.isEmpty());

        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy ready(&service, &ProjectStationPositionService::positionsReady);
        QSignalSpy failed(&service, &ProjectStationPositionService::positionsFailed);
        const ProjectStationPositionRequest request{QCoreApplication::applicationFilePath(),
                                                    configPath,
                                                    {QDir(project.path()).filePath(QStringLiteral("cave.th"))}};
        service.run(request);
        QVERIFY(service.isRunning());
        QVERIFY(ready.wait(10000));
        QCOMPARE(failed.count(), 0);
        QVERIFY(!service.isRunning());
        QCOMPARE(service.positions().size(), 2);
        QCOMPARE(service.dataSource(), ProjectStationPositionService::DataSource::Run);
        const Th2StationPosition station =
            service.positions().value(ProjectStructureIndex::stationKey(QStringLiteral("1"), QStringLiteral("a.cave")));
        QCOMPARE(station.y, 20.0);
        // Splays and duplicates are left out of the measured legs.
        QCOMPARE(service.legs().size(), 1);
        QCOMPARE(service.legs().first().toStationKey, ProjectStructureIndex::stationKey(QStringLiteral("2"), QStringLiteral("a.cave")));
        QCOMPARE(service.legs().first().vector.y, 5.0);
        // Nothing is written into the project.
        QCOMPARE(QDir(project.path()).entryList(QDir::Files).size(), 2);

        // A newer drawing does not make them out of date; a newer .th does. Whether to run again is the caller's
        // choice, so nothing runs by itself.
        const QDateTime later = QDateTime::currentDateTime().addSecs(60);
        QFile drawing(QDir(project.path()).filePath(QStringLiteral("cave.th2")));
        QVERIFY(drawing.open(QIODevice::WriteOnly));
        QVERIFY(drawing.setFileTime(later, QFileDevice::FileModificationTime));
        QVERIFY(!service.isOutOfDate({drawing.fileName()}));
        QFile source(request.sourceFiles.first());
        QVERIFY(source.open(QIODevice::ReadWrite));
        QVERIFY(source.setFileTime(later, QFileDevice::FileModificationTime));
        QVERIFY(service.isOutOfDate(request.sourceFiles));
        QVERIFY(!service.isRunning());
        service.run(request);
        QVERIFY(service.isRunning());
        QVERIFY(ready.wait(10000));

        service.clear();
        QVERIFY(service.positions().isEmpty());
        QVERIFY(service.legs().isEmpty());
    }

    void usesTheCompileExportWithoutRunningTherion()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        QVERIFY(project.isValid() && temporaryBase.isValid());
        const QString configPath = writeProject(project);
        const QString exportPath = QDir(project.path()).filePath(QStringLiteral("cave.sql"));
        QVERIFY(writeStationExport(exportPath));

        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy ready(&service, &ProjectStationPositionService::positionsReady);
        const QDateTime compileStartedAt = QDateTime::currentDateTime();
        service.loadCompileExport(exportPath, compileStartedAt);
        QVERIFY(!service.isRunning());
        QVERIFY(ready.wait(10000));
        QCOMPARE(service.positions().size(), 2);
        QCOMPARE(service.legs().size(), 1);
        QCOMPARE(service.dataSource(), ProjectStationPositionService::DataSource::Compile);
        QCOMPARE(service.dataTakenAt(), compileStartedAt);

        // A newer .th makes them out of date but keeps them.
        QFile source(QDir(project.path()).filePath(QStringLiteral("cave.th")));
        QVERIFY(source.open(QIODevice::ReadWrite));
        QVERIFY(source.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
        QVERIFY(!service.isRunning());
        QCOMPARE(service.positions().size(), 2);
        QVERIFY(service.isOutOfDate({source.fileName()}));
        QVERIFY(!configPath.isEmpty());
        // Saving a drawing does not make them out of date.
        QFile drawing(QDir(project.path()).filePath(QStringLiteral("cave.th2")));
        QVERIFY(drawing.open(QIODevice::WriteOnly));
        QVERIFY(drawing.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
        QVERIFY(!service.isOutOfDate({drawing.fileName()}));
    }

    void describesWhereStationPositionsComeFrom()
    {
        using Status = ProjectConnectionDataStatus;
        const QDateTime now(QDate(2026, 10, 23), QTime(15, 0));
        Status::State state;
        QVERIFY(Status::text(state, now).contains(QStringLiteral("export database -fmt sql")));
        state.compileWroteNoDatabase = true;
        QVERIFY(Status::text(state, now).startsWith(QStringLiteral("The last compile wrote no database export.")));
        // Without a target config nothing else is meaningful.
        state.noTargetConfig = true;
        QVERIFY(Status::text(state, now).startsWith(QStringLiteral("No target config selected")));
        state = Status::State();

        state.source = ProjectStationPositionService::DataSource::Compile;
        state.takenAt = QDateTime(QDate(2026, 10, 23), QTime(10, 42));
        const QString time = QLocale().toString(state.takenAt.time(), QLocale::ShortFormat);
        QCOMPARE(Status::text(state, now), QStringLiteral("Positions from the compile at %1.").arg(time));
        state.outOfDate = true;
        QVERIFY(Status::text(state, now).contains(QStringLiteral("Sources changed since")));

        // An earlier day shows the date too.
        state.takenAt = QDateTime(QDate(2026, 10, 20), QTime(10, 42));
        QVERIFY(Status::text(state, now).contains(QLocale().toString(state.takenAt, QLocale::ShortFormat)));

        state.source = ProjectStationPositionService::DataSource::Run;
        state.outOfDate = false;
        QVERIFY(Status::text(state, now).startsWith(QStringLiteral("Positions from a Therion run")));
        state.running = true;
        QCOMPARE(Status::text(state, now), QStringLiteral("Computing station positions with Therion..."));
        state.openPending = true;
        QVERIFY(Status::text(state, now).endsWith(QStringLiteral("the connection opens when they are ready.")));
    }

    void compileExportReplacesARunInProgress()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        QVERIFY(project.isValid() && temporaryBase.isValid());
        const QString configPath = writeProject(project);
        const QString exportPath = QDir(project.path()).filePath(QStringLiteral("cave.sql"));
        QVERIFY(writeStationExport(exportPath));

        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy ready(&service, &ProjectStationPositionService::positionsReady);
        service.run(ProjectStationPositionRequest{QCoreApplication::applicationFilePath(), configPath, {}});
        QVERIFY(service.isRunning());
        service.loadCompileExport(exportPath, QDateTime::currentDateTime());
        QVERIFY(!service.isRunning());
        QVERIFY(ready.wait(10000));
        QCOMPARE(ready.count(), 1);
        QCOMPARE(service.positions().size(), 2);
    }

    void remembersAFailedRun()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        const QString configPath = writeProject(project);
        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy failed(&service, &ProjectStationPositionService::positionsFailed);
        const QString source = QDir(project.path()).filePath(QStringLiteral("cave.th"));
        const ProjectStationPositionRequest request{QDir(project.path()).filePath(QStringLiteral("no-therion")),
                                                    configPath,
                                                    {source}};
        QVERIFY(!service.attemptFailed(request));
        service.run(request);
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 10000);
        QVERIFY(!service.isRunning());
        // Another run would fail the same way until a source, the config or the executable changes.
        QVERIFY(service.attemptFailed(request));
        ProjectStationPositionRequest otherExecutable = request;
        otherExecutable.executableInput = QCoreApplication::applicationFilePath();
        QVERIFY(!service.attemptFailed(otherExecutable));
        QFile file(source);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
        QVERIFY(!service.attemptFailed(request));
    }

    void cancelStopsARunAndKeepsThePositions()
    {
        QTemporaryDir project;
        QTemporaryDir temporaryBase;
        QVERIFY(project.isValid() && temporaryBase.isValid());
        const QString configPath = writeProject(project);
        const QString exportPath = QDir(project.path()).filePath(QStringLiteral("cave.sql"));
        QVERIFY(writeStationExport(exportPath));

        ProjectStationPositionService service(temporaryBase.path());
        QSignalSpy ready(&service, &ProjectStationPositionService::positionsReady);
        QSignalSpy failed(&service, &ProjectStationPositionService::positionsFailed);
        service.loadCompileExport(exportPath, QDateTime::currentDateTime());
        QVERIFY(ready.wait(10000));
        service.run(ProjectStationPositionRequest{QCoreApplication::applicationFilePath(), configPath, {}});
        QVERIFY(service.isRunning());
        service.cancel();
        QVERIFY(!service.isRunning());
        QCOMPARE(service.positions().size(), 2);
        // The cancelled run neither reports positions nor a failure.
        QTest::qWait(500);
        QCOMPARE(ready.count(), 1);
        QCOMPARE(failed.count(), 0);
    }

    void choosesWhetherToRunTherion()
    {
        using Action = ConnectionPositionRunAction;
        QCOMPARE(connectionPositionRunModeFromSetting(QStringLiteral("run")), ConnectionPositionRunMode::Run);
        QCOMPARE(connectionPositionRunModeFromSetting(QStringLiteral("never")), ConnectionPositionRunMode::Never);
        QCOMPARE(connectionPositionRunModeFromSetting(QString()), ConnectionPositionRunMode::Ask);
        QCOMPARE(connectionPositionRunModeSetting(ConnectionPositionRunMode::Never), QStringLiteral("never"));
        for (const ConnectionPositionRunMode mode :
             {ConnectionPositionRunMode::Ask, ConnectionPositionRunMode::Run, ConnectionPositionRunMode::Never}) {
            // Current positions open at once, whatever the setting.
            QCOMPARE(connectionPositionRunAction(mode, true, false), Action::OpenNow);
        }
        QCOMPARE(connectionPositionRunAction(ConnectionPositionRunMode::Ask, false, false), Action::AskFirst);
        QCOMPARE(connectionPositionRunAction(ConnectionPositionRunMode::Ask, true, true), Action::AskFirst);
        QCOMPARE(connectionPositionRunAction(ConnectionPositionRunMode::Run, false, false), Action::Run);
        QCOMPARE(connectionPositionRunAction(ConnectionPositionRunMode::Run, true, true), Action::Run);
        QCOMPARE(connectionPositionRunAction(ConnectionPositionRunMode::Never, false, false), Action::OpenNow);
        QCOMPARE(connectionPositionRunAction(ConnectionPositionRunMode::Never, true, true), Action::OpenNow);
    }
};

int main(int argc, char **argv)
{
    if (argc == 2 && qstrcmp(argv[1], kConfigArgument) == 0) {
        QCoreApplication app(argc, argv);
        return runFakeTherion();
    }
    QCoreApplication app(argc, argv);
    ProjectStationPositionServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectStationPositionServiceTest.moc"
