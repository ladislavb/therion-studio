#include "../src/app/ProjectFileWatchInventoryService.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTest>
#include <QTimer>

#include <atomic>
#include <memory>

using namespace TherionStudio;

namespace
{
ProjectFileWatchInventory inventoryForPath(const QString &path)
{
    ProjectFileWatchInventory inventory;
    inventory.projectRootPath = path;
    inventory.directories = {path};
    return inventory;
}
}

class ProjectFileWatchInventoryServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void publishesInventoryWithTiming();
    void publishesOnlyLatestPendingRequest();
    void keepsEventLoopResponsiveDuringInventory();
    void reportsDiscoveryError();
    void teardownDoesNotPublishCompletedWorker();
};

void ProjectFileWatchInventoryServiceTest::publishesInventoryWithTiming()
{
    ProjectFileWatchInventoryService service([](const ProjectFileWatchInventoryRequest &request) {
        return inventoryForPath(request.projectRootPath);
    });
    service.setDebounceIntervalMs(0);

    ProjectFileWatchInventoryService::Result result;
    bool received = false;
    connect(&service,
            &ProjectFileWatchInventoryService::inventoryFinished,
            &service,
            [&](const ProjectFileWatchInventoryService::Result &nextResult) {
                result = nextResult;
                received = true;
            });

    service.requestInventory(QStringLiteral("project-a"));
    QTRY_VERIFY_WITH_TIMEOUT(received, 2000);
    QCOMPARE(result.requestSerial, quint64(1));
    QCOMPARE(result.projectRootPath, QStringLiteral("project-a"));
    QCOMPARE(result.inventory.directories, QStringList{QStringLiteral("project-a")});
    QVERIFY(result.discoveryDurationMs >= 0);
    QVERIFY(service.isLatestRequestResult(result));
}

void ProjectFileWatchInventoryServiceTest::publishesOnlyLatestPendingRequest()
{
    QSemaphore firstInventoryStarted;
    QSemaphore releaseFirstInventory;
    std::atomic_int callCount = 0;
    ProjectFileWatchInventoryService service([&](const ProjectFileWatchInventoryRequest &request) {
        const int callNumber = ++callCount;
        if (callNumber == 1) {
            firstInventoryStarted.release();
            releaseFirstInventory.acquire();
        }
        return inventoryForPath(request.projectRootPath);
    });
    auto releaseGuard = qScopeGuard([&]() { releaseFirstInventory.release(); });
    service.setDebounceIntervalMs(0);

    QVector<ProjectFileWatchInventoryService::Result> publishedResults;
    connect(&service,
            &ProjectFileWatchInventoryService::inventoryFinished,
            &service,
            [&](const ProjectFileWatchInventoryService::Result &result) {
                publishedResults.append(result);
            });

    service.requestInventory(QStringLiteral("project-a"));
    QTRY_COMPARE_WITH_TIMEOUT(firstInventoryStarted.available(), 1, 2000);
    firstInventoryStarted.acquire();

    service.requestInventory(QStringLiteral("project-a"));
    ProjectFileWatchInventoryService::Result firstIdentity;
    firstIdentity.requestSerial = 1;
    QVERIFY(!service.isLatestRequestResult(firstIdentity));
    service.requestInventory(QStringLiteral("project-b"));
    releaseFirstInventory.release();
    releaseGuard.dismiss();

    QTRY_COMPARE_WITH_TIMEOUT(publishedResults.size(), 1, 2000);
    QCOMPARE(callCount.load(), 2);
    QCOMPARE(publishedResults.constFirst().requestSerial, quint64(3));
    QCOMPARE(publishedResults.constFirst().projectRootPath, QStringLiteral("project-b"));
}

void ProjectFileWatchInventoryServiceTest::keepsEventLoopResponsiveDuringInventory()
{
    QSemaphore workerStarted;
    QSemaphore releaseWorker;
    ProjectFileWatchInventoryService service([&](const ProjectFileWatchInventoryRequest &request) {
        workerStarted.release();
        releaseWorker.acquire();
        return inventoryForPath(request.projectRootPath);
    });
    auto releaseGuard = qScopeGuard([&]() { releaseWorker.release(); });
    service.setDebounceIntervalMs(0);

    int heartbeatCount = 0;
    QTimer heartbeat;
    heartbeat.setInterval(1);
    connect(&heartbeat, &QTimer::timeout, &service, [&]() { ++heartbeatCount; });
    heartbeat.start();
    bool received = false;
    connect(&service,
            &ProjectFileWatchInventoryService::inventoryFinished,
            &service,
            [&](const ProjectFileWatchInventoryService::Result &) { received = true; });

    service.requestInventory(QStringLiteral("project"));
    QTRY_COMPARE_WITH_TIMEOUT(workerStarted.available(), 1, 2000);
    workerStarted.acquire();
    QTRY_VERIFY_WITH_TIMEOUT(heartbeatCount > 0, 2000);
    releaseWorker.release();
    releaseGuard.dismiss();
    QTRY_VERIFY_WITH_TIMEOUT(received, 2000);
}

void ProjectFileWatchInventoryServiceTest::reportsDiscoveryError()
{
    ProjectFileWatchInventoryService service([](const ProjectFileWatchInventoryRequest &request) {
        ProjectFileWatchInventory inventory = inventoryForPath(request.projectRootPath);
        inventory.discoveryErrors.append(QStringLiteral("root is unavailable"));
        return inventory;
    });
    service.setDebounceIntervalMs(0);

    ProjectFileWatchInventoryService::Result result;
    bool received = false;
    connect(&service,
            &ProjectFileWatchInventoryService::inventoryFinished,
            &service,
            [&](const ProjectFileWatchInventoryService::Result &nextResult) {
                result = nextResult;
                received = true;
            });

    service.requestInventory(QStringLiteral("missing-project"));
    QTRY_VERIFY_WITH_TIMEOUT(received, 2000);
    QCOMPARE(result.inventory.discoveryErrors, QStringList{QStringLiteral("root is unavailable")});
}

void ProjectFileWatchInventoryServiceTest::teardownDoesNotPublishCompletedWorker()
{
    QSemaphore workerStarted;
    QSemaphore releaseWorker;
    QSemaphore workerFinished;
    int publishedResultCount = 0;
    auto service = std::make_unique<ProjectFileWatchInventoryService>(
        [&](const ProjectFileWatchInventoryRequest &request) {
            workerStarted.release();
            releaseWorker.acquire();
            workerFinished.release();
            return inventoryForPath(request.projectRootPath);
        });
    auto releaseGuard = qScopeGuard([&]() { releaseWorker.release(); });
    service->setDebounceIntervalMs(0);
    connect(service.get(),
            &ProjectFileWatchInventoryService::inventoryFinished,
            service.get(),
            [&](const ProjectFileWatchInventoryService::Result &) { ++publishedResultCount; });

    service->requestInventory(QStringLiteral("project"));
    QTRY_COMPARE_WITH_TIMEOUT(workerStarted.available(), 1, 2000);
    workerStarted.acquire();
    releaseWorker.release();
    releaseGuard.dismiss();
    QVERIFY(workerFinished.tryAcquire(1, 2000));
    service.reset();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(publishedResultCount, 0);
}

int runProjectFileWatchInventoryServiceTest(int argc, char **argv)
{
    ProjectFileWatchInventoryServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectFileWatchInventoryServiceTest.moc"
