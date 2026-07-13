#include "../src/app/reports/TherionSqlReportWorker.h"

#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

using namespace TherionStudio;

namespace
{
QString minimalTherionSqlExport()
{
    return QStringLiteral(
        "create table SURVEY (ID integer, NAME varchar(64));\n"
        "create table CENTRELINE (ID integer);\n"
        "create table PERSON (ID integer);\n"
        "create table EXPLO (PERSON_ID integer);\n"
        "create table TOPO (PERSON_ID integer);\n"
        "create table STATION (ID integer);\n"
        "create table STATION_FLAG (STATION_ID integer);\n"
        "create table SHOT (ID integer);\n"
        "create table SHOT_FLAG (SHOT_ID integer);\n"
        "insert into SURVEY values (1, 'main');\n");
}

QString writeFixture(QTemporaryDir *tempDir, const QString &fileName, const QString &contents)
{
    const QString filePath = QDir(tempDir->path()).filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return QString();
    }
    if (file.write(contents.toUtf8()) != contents.toUtf8().size()) {
        return QString();
    }
    return filePath;
}
}

class TherionSqlReportWorkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void ownsConnectionOnWorkerThreadAndTearsDownCleanly();
};

void TherionSqlReportWorkerTest::ownsConnectionOnWorkerThreadAndTearsDownCleanly()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString validPath = writeFixture(&tempDir,
                                           QStringLiteral("valid.sql"),
                                           minimalTherionSqlExport());
    const QString malformedPath = writeFixture(
        &tempDir,
        QStringLiteral("malformed.sql"),
        minimalTherionSqlExport() + QStringLiteral("insert into MISSING values (1);\n"));
    QVERIFY(!validPath.isEmpty());
    QVERIFY(!malformedPath.isEmpty());

    QThread workerThread;
    QMutex lifecycleMutex;
    QVector<QThread *> lifecycleThreads;
    QVector<TherionSqlReportDatabase::ConnectionLifecycleEvent> lifecycleEvents;
    QPointer<TherionSqlReportWorker> worker = new TherionSqlReportWorker(
        [&](TherionSqlReportDatabase::ConnectionLifecycleEvent event) {
            QMutexLocker locker(&lifecycleMutex);
            lifecycleThreads.append(QThread::currentThread());
            lifecycleEvents.append(event);
        });
    worker->moveToThread(&workerThread);
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    workerThread.start();
    auto workerCleanupGuard = qScopeGuard([&]() {
        if (worker != nullptr && workerThread.isRunning()) {
            QMetaObject::invokeMethod(worker,
                                      [worker]() {
                                          if (worker != nullptr) {
                                              worker->shutdown();
                                          }
                                      },
                                      Qt::BlockingQueuedConnection);
        }
        workerThread.quit();
        workerThread.wait(2000);
    });

    QVector<TherionSqlReportImportWorkerResult> importResults;
    QVector<TherionSqlReportQueryWorkerResult> queryResults;
    connect(worker,
            &TherionSqlReportWorker::importFinished,
            this,
            [&](const TherionSqlReportImportWorkerResult &result) {
                importResults.append(result);
            });
    connect(worker,
            &TherionSqlReportWorker::queryFinished,
            this,
            [&](const TherionSqlReportQueryWorkerResult &result) {
                queryResults.append(result);
            });

    const TherionSqlReportImportRequest importRequest{
        1,
        QStringLiteral("source-a"),
        validPath,
    };
    QVERIFY(QMetaObject::invokeMethod(worker,
                                      [worker, importRequest]() {
                                          worker->importDatabase(importRequest);
                                      },
                                      Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(importResults.size(), 1, 3000);
    QCOMPARE(importResults.constFirst().requestId, quint64(1));
    QCOMPARE(importResults.constFirst().sourceIdentity, QStringLiteral("source-a"));
    QCOMPARE(importResults.constFirst().errorCode, TherionSqlReportErrorCode::None);
    QVERIFY(importResults.constFirst().errorMessage.isEmpty());
    QCOMPARE(importResults.constFirst().schema.size(), 9);
    QVERIFY(!importResults.constFirst().cancelled);

    const TherionSqlReportQueryRequest queryRequest{
        2,
        QStringLiteral("source-a"),
        QStringLiteral("select NAME from SURVEY"),
        100,
        TherionSqlReportExecutionPolicy::CustomReadOnly,
    };
    QVERIFY(QMetaObject::invokeMethod(worker,
                                      [worker, queryRequest]() {
                                          worker->executeQuery(queryRequest);
                                      },
                                      Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(queryResults.size(), 1, 3000);
    QCOMPARE(queryResults.constFirst().requestId, quint64(2));
    QCOMPARE(queryResults.constFirst().sourceIdentity, QStringLiteral("source-a"));
    QCOMPARE(queryResults.constFirst().errorCode, TherionSqlReportErrorCode::None);
    QCOMPARE(queryResults.constFirst().table.columns, QStringList{QStringLiteral("NAME")});
    QCOMPARE(queryResults.constFirst().table.rows.size(), 1);
    QCOMPARE(queryResults.constFirst().table.rows.constFirst(), QStringList{QStringLiteral("main")});

    TherionSqlReportQueryRequest mismatchedQuery = queryRequest;
    mismatchedQuery.requestId = 3;
    mismatchedQuery.sourceIdentity = QStringLiteral("source-b");
    QVERIFY(QMetaObject::invokeMethod(worker,
                                      [worker, mismatchedQuery]() {
                                          worker->executeQuery(mismatchedQuery);
                                      },
                                      Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(queryResults.size(), 2, 3000);
    QCOMPARE(queryResults.constLast().errorCode, TherionSqlReportErrorCode::SourceMismatch);
    QVERIFY(!queryResults.constLast().errorMessage.isEmpty());

    const TherionSqlReportImportRequest malformedRequest{
        4,
        QStringLiteral("source-b"),
        malformedPath,
    };
    QVERIFY(QMetaObject::invokeMethod(worker,
                                      [worker, malformedRequest]() {
                                          worker->importDatabase(malformedRequest);
                                      },
                                      Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(importResults.size(), 2, 3000);
    QCOMPARE(importResults.constLast().requestId, quint64(4));
    QCOMPARE(importResults.constLast().errorCode, TherionSqlReportErrorCode::ImportFailed);
    QVERIFY(!importResults.constLast().errorMessage.isEmpty());
    QVERIFY(!importResults.constLast().cancelled);

    const TherionSqlReportQueryRequest queryAfterFailure{
        5,
        QStringLiteral("source-b"),
        QStringLiteral("select * from SURVEY"),
        100,
        TherionSqlReportExecutionPolicy::BuiltInReport,
    };
    QVERIFY(QMetaObject::invokeMethod(worker,
                                      [worker, queryAfterFailure]() {
                                          worker->executeQuery(queryAfterFailure);
                                      },
                                      Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(queryResults.size(), 3, 3000);
    QCOMPARE(queryResults.constLast().errorCode, TherionSqlReportErrorCode::DatabaseNotLoaded);
    QVERIFY(!queryResults.constLast().errorMessage.isEmpty());

    TherionSqlReportImportRequest recoveryRequest = importRequest;
    recoveryRequest.requestId = 6;
    QVERIFY(QMetaObject::invokeMethod(worker,
                                      [worker, recoveryRequest]() {
                                          worker->importDatabase(recoveryRequest);
                                      },
                                      Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(importResults.size(), 3, 3000);
    QCOMPARE(importResults.constLast().errorCode, TherionSqlReportErrorCode::None);

    QSemaphore workerDestroyed;
    connect(worker, &QObject::destroyed, [&]() { workerDestroyed.release(); });
    QVERIFY(QMetaObject::invokeMethod(worker,
                                      [worker]() {
                                          worker->shutdown();
                                          worker->deleteLater();
                                      },
                                      Qt::QueuedConnection));
    QVERIFY(workerDestroyed.tryAcquire(1, 3000));
    worker = nullptr;
    workerThread.quit();
    QVERIFY(workerThread.wait(3000));

    {
        QMutexLocker locker(&lifecycleMutex);
        QVERIFY(!lifecycleEvents.isEmpty());
        QCOMPARE(lifecycleThreads.size(), lifecycleEvents.size());
        for (QThread *lifecycleThread : lifecycleThreads) {
            QCOMPARE(lifecycleThread, &workerThread);
        }
        QVERIFY(lifecycleEvents.contains(TherionSqlReportDatabase::ConnectionLifecycleEvent::Added));
        QVERIFY(lifecycleEvents.contains(TherionSqlReportDatabase::ConnectionLifecycleEvent::Opened));
        QVERIFY(lifecycleEvents.contains(TherionSqlReportDatabase::ConnectionLifecycleEvent::Closed));
        QVERIFY(lifecycleEvents.contains(TherionSqlReportDatabase::ConnectionLifecycleEvent::Removed));
    }
}

int runTherionSqlReportWorkerTest(int argc, char **argv)
{
    TherionSqlReportWorkerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSqlReportWorkerTest.moc"
