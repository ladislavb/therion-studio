#include "../src/app/reports/TherionSqlReportTab.h"
#include "../src/app/reports/TherionSqlReportCsvExporter.h"
#include "../src/app/reports/TherionSqlReportWorkerSession.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <atomic>
#include <memory>

using namespace TherionStudio;

namespace
{
QByteArray largeSqlExport(int surveyCount)
{
    QByteArray contents(
        "create table SURVEY (ID integer, NAME varchar(64));\n"
        "create table CENTRELINE (ID integer);\n"
        "create table PERSON (ID integer);\n"
        "create table EXPLO (PERSON_ID integer);\n"
        "create table TOPO (PERSON_ID integer);\n"
        "create table STATION (ID integer);\n"
        "create table STATION_FLAG (STATION_ID integer);\n"
        "create table SHOT (ID integer);\n"
        "create table SHOT_FLAG (SHOT_ID integer);\n");
    for (int i = 0; i < surveyCount; ++i) {
        contents.append("insert into SURVEY values (");
        contents.append(QByteArray::number(i));
        contents.append(", 'survey');\n");
    }
    return contents;
}

class MemoryPresetStore final : public TherionSqlReportPresetStore
{
public:
    QVector<TherionSqlReportDefinition> loadCustomPresets() const override
    {
        return presets_;
    }

    void saveCustomPresets(const QVector<TherionSqlReportDefinition> &presets) override
    {
        presets_ = presets;
    }

private:
    QVector<TherionSqlReportDefinition> presets_;
};

class MemoryCsvExporter final : public TherionSqlReportCsvExporter
{
public:
    bool writeTable(const QString &, const TherionSqlReportTable &, QString *) const override
    {
        return true;
    }
};

std::unique_ptr<TherionSqlReportPresetStore> makePresetStore()
{
    return std::make_unique<MemoryPresetStore>();
}

std::unique_ptr<TherionSqlReportCsvExporter> makeCsvExporter()
{
    return std::make_unique<MemoryCsvExporter>();
}
}

class FakeTherionSqlReportSession final : public TherionSqlReportSession
{
    Q_OBJECT

public:
    using TherionSqlReportSession::TherionSqlReportSession;

    bool requestImport(const TherionSqlReportImportRequest &request, QString *) override
    {
        importRequests.append(request);
        return true;
    }

    void requestQuery(const TherionSqlReportQueryRequest &request) override
    {
        queryRequests.append(request);
    }

    void shutdown() override
    {
        ++shutdownCount;
    }

    void publishImport(const TherionSqlReportImportWorkerResult &result)
    {
        emit importFinished(result);
    }

    void publishQuery(const TherionSqlReportQueryWorkerResult &result)
    {
        emit queryFinished(result);
    }

    QVector<TherionSqlReportImportRequest> importRequests;
    QVector<TherionSqlReportQueryRequest> queryRequests;
    int shutdownCount = 0;
};

class TherionSqlReportTabTest final : public QObject
{
    Q_OBJECT

private slots:
    void unreadablePathIsRejectedSynchronously();
    void largeImportKeepsEventLoopResponsive();
    void newerLoadSuppressesOlderSchema();
    void newerQuerySuppressesOlderTable();
    void importFailureLeavesEmptyCoherentState();
    void closingTabDisconnectsPublicationAndRequestsShutdown();
    void closingTabDuringRealImportIsBoundedAndTearsDownWorker();
};

void TherionSqlReportTabTest::unreadablePathIsRejectedSynchronously()
{
    auto *session = new TherionSqlReportWorkerSession;
    TherionSqlReportTab tab(session, makePresetStore(), makeCsvExporter());
    session->setParent(&tab);
    QString errorMessage;
    QVERIFY(!tab.loadFile(QStringLiteral("missing-report.sql"), &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(tab.filePath().isEmpty());
}

void TherionSqlReportTabTest::largeImportKeepsEventLoopResponsive()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("large.sql"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray contents = largeSqlExport(25000);
    QCOMPARE(file.write(contents), contents.size());
    file.close();

    auto *session = new TherionSqlReportWorkerSession;
    TherionSqlReportTab tab(session, makePresetStore(), makeCsvExporter());
    session->setParent(&tab);
    int heartbeatCount = 0;
    QTimer heartbeat;
    heartbeat.setInterval(1);
    connect(&heartbeat, &QTimer::timeout, this, [&heartbeatCount]() {
        ++heartbeatCount;
    });
    heartbeat.start();

    QString errorMessage;
    QElapsedTimer acceptanceTimer;
    acceptanceTimer.start();
    QVERIFY(tab.loadFile(filePath, &errorMessage));
    QVERIFY2(acceptanceTimer.elapsed() < 200, "loadFile blocked on SQL import");
    const auto *runButton = tab.findChild<QPushButton *>(QStringLiteral("sqlReportRunQueryButton"));
    QVERIFY(runButton != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(runButton->isEnabled(), 10000);
    QVERIFY(heartbeatCount > 1);
}

void TherionSqlReportTabTest::newerLoadSuppressesOlderSchema()
{
    FakeTherionSqlReportSession session;
    TherionSqlReportTab tab(&session, makePresetStore(), makeCsvExporter());
    QString errorMessage;
    QVERIFY(tab.loadFile(QStringLiteral("first.sql"), &errorMessage));
    QVERIFY(tab.loadFile(QStringLiteral("second.sql"), &errorMessage));
    QCOMPARE(session.importRequests.size(), 2);

    const TherionSqlReportImportRequest first = session.importRequests.at(0);
    const TherionSqlReportImportRequest second = session.importRequests.at(1);
    session.publishImport({first.requestId,
                           first.sourceIdentity,
                           {{QStringLiteral("FIRST_ONLY"), {QStringLiteral("ID")}}},
                           1});
    auto *schema = tab.findChild<QPlainTextEdit *>(QStringLiteral("sqlReportSchemaText"));
    QVERIFY(schema != nullptr);
    QVERIFY(!schema->toPlainText().contains(QStringLiteral("FIRST_ONLY")));

    session.publishImport({second.requestId,
                           second.sourceIdentity,
                           {{QStringLiteral("SECOND_ONLY"), {QStringLiteral("NAME")}}},
                           2});
    QVERIFY(schema->toPlainText().contains(QStringLiteral("SECOND_ONLY")));
    QVERIFY(!schema->toPlainText().contains(QStringLiteral("FIRST_ONLY")));
    QCOMPARE(tab.filePath(), second.filePath);
}

void TherionSqlReportTabTest::newerQuerySuppressesOlderTable()
{
    FakeTherionSqlReportSession session;
    TherionSqlReportTab tab(&session, makePresetStore(), makeCsvExporter());
    QString errorMessage;
    QVERIFY(tab.loadFile(QStringLiteral("report.sql"), &errorMessage));
    const TherionSqlReportImportRequest import = session.importRequests.constFirst();
    session.publishImport({import.requestId, import.sourceIdentity, {}, 1});

    auto *queryEdit = tab.findChild<QPlainTextEdit *>(QStringLiteral("sqlReportQueryEdit"));
    auto *runButton = tab.findChild<QPushButton *>(QStringLiteral("sqlReportRunQueryButton"));
    auto *tableView = tab.findChild<QTableView *>(QStringLiteral("sqlReportResultTable"));
    QVERIFY(queryEdit != nullptr);
    QVERIFY(runButton != nullptr);
    QVERIFY(tableView != nullptr);

    queryEdit->setPlainText(QStringLiteral("select 'A' as VALUE"));
    runButton->click();
    const TherionSqlReportQueryRequest queryA = session.queryRequests.constLast();
    QCOMPARE(queryA.executionPolicy, TherionSqlReportExecutionPolicy::CustomReadOnly);
    queryEdit->setPlainText(QStringLiteral("select 'B' as VALUE"));
    runButton->click();
    const TherionSqlReportQueryRequest queryB = session.queryRequests.constLast();
    QVERIFY(queryB.requestId > queryA.requestId);

    TherionSqlReportTable tableA;
    tableA.columns = {QStringLiteral("VALUE")};
    tableA.rows = {{QStringLiteral("A")}};
    session.publishQuery({queryA.requestId, queryA.sourceIdentity, tableA});
    QCOMPARE(tableView->model()->rowCount(), 0);

    TherionSqlReportTable tableB;
    tableB.columns = {QStringLiteral("VALUE")};
    tableB.rows = {{QStringLiteral("B")}};
    session.publishQuery({queryB.requestId, queryB.sourceIdentity, tableB});
    QCOMPARE(tableView->model()->rowCount(), 1);
    QCOMPARE(tableView->model()->index(0, 0).data().toString(), QStringLiteral("B"));
}

void TherionSqlReportTabTest::importFailureLeavesEmptyCoherentState()
{
    FakeTherionSqlReportSession session;
    TherionSqlReportTab tab(&session, makePresetStore(), makeCsvExporter());
    QString errorMessage;
    QVERIFY(tab.loadFile(QStringLiteral("broken.sql"), &errorMessage));
    const TherionSqlReportImportRequest request = session.importRequests.constFirst();
    TherionSqlReportImportWorkerResult result;
    result.requestId = request.requestId;
    result.sourceIdentity = request.sourceIdentity;
    result.errorCode = TherionSqlReportErrorCode::ImportFailed;
    result.errorMessage = QStringLiteral("broken import");
    session.publishImport(result);

    const auto *status = tab.findChild<QLabel *>(QStringLiteral("sqlReportStatusLabel"));
    const auto *schema = tab.findChild<QPlainTextEdit *>(QStringLiteral("sqlReportSchemaText"));
    const auto *table = tab.findChild<QTableView *>(QStringLiteral("sqlReportResultTable"));
    const auto *runButton = tab.findChild<QPushButton *>(QStringLiteral("sqlReportRunQueryButton"));
    QCOMPARE(status->text(), QStringLiteral("broken import"));
    QCOMPARE(schema->toPlainText(), QStringLiteral("Tables"));
    QCOMPARE(table->model()->rowCount(), 0);
    QVERIFY(!runButton->isEnabled());
}

void TherionSqlReportTabTest::closingTabDisconnectsPublicationAndRequestsShutdown()
{
    FakeTherionSqlReportSession session;
    auto *tab = new TherionSqlReportTab(&session, makePresetStore(), makeCsvExporter());
    QString errorMessage;
    QVERIFY(tab->loadFile(QStringLiteral("report.sql"), &errorMessage));
    const TherionSqlReportImportRequest request = session.importRequests.constFirst();

    QElapsedTimer timer;
    timer.start();
    delete tab;
    QVERIFY(timer.elapsed() < 100);
    QCOMPARE(session.shutdownCount, 1);

    session.publishImport({request.requestId, request.sourceIdentity, {}, 1});
    QCOMPARE(session.shutdownCount, 1);
}

void TherionSqlReportTabTest::closingTabDuringRealImportIsBoundedAndTearsDownWorker()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("closing.sql"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray contents = largeSqlExport(50000);
    QCOMPARE(file.write(contents), contents.size());
    file.close();

    std::atomic<int> openedConnections = 0;
    std::atomic<int> closedConnections = 0;
    auto *session = new TherionSqlReportWorkerSession(
        [&openedConnections, &closedConnections](TherionSqlReportDatabase::ConnectionLifecycleEvent event) {
            if (event == TherionSqlReportDatabase::ConnectionLifecycleEvent::Opened) {
                openedConnections.fetch_add(1, std::memory_order_relaxed);
            } else if (event == TherionSqlReportDatabase::ConnectionLifecycleEvent::Closed) {
                closedConnections.fetch_add(1, std::memory_order_relaxed);
            }
        });
    auto *tab = new TherionSqlReportTab(session, makePresetStore(), makeCsvExporter());
    session->setParent(tab);
    QString errorMessage;
    QVERIFY(tab->loadFile(filePath, &errorMessage));
    QTRY_COMPARE_WITH_TIMEOUT(openedConnections.load(std::memory_order_relaxed), 1, 3000);

    QElapsedTimer closeTimer;
    closeTimer.start();
    delete tab;
    QVERIFY2(closeTimer.elapsed() < 100, "closing the tab waited for the worker import");
    QTRY_COMPARE_WITH_TIMEOUT(closedConnections.load(std::memory_order_relaxed), 1, 10000);
}

QTEST_MAIN(TherionSqlReportTabTest)
#include "TherionSqlReportTabTest.moc"
