#include "ProjectFileWatchInventoryService.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>

#include <utility>

namespace TherionStudio
{
namespace
{
ProjectFileWatchInventoryService::Result collectInventory(
    const ProjectFileWatchInventoryService::InventoryFunction &inventoryFunction,
    const ProjectFileWatchInventoryRequest &request,
    quint64 requestSerial)
{
    QElapsedTimer elapsed;
    elapsed.start();

    ProjectFileWatchInventoryService::Result result;
    result.requestSerial = requestSerial;
    result.inventory = inventoryFunction(request);
    result.projectRootPath = result.inventory.projectRootPath;
    result.discoveryDurationMs = elapsed.elapsed();
    return result;
}
}

ProjectFileWatchInventoryService::ProjectFileWatchInventoryService(QObject *parent)
    : ProjectFileWatchInventoryService(ProjectFileWatchInventoryCollector::collect, parent)
{
}

ProjectFileWatchInventoryService::ProjectFileWatchInventoryService(InventoryFunction inventoryFunction,
                                                                     QObject *parent)
    : QObject(parent)
    , debounceTimer_(new QTimer(this))
    , inventoryWatcher_(new QFutureWatcher<Result>(this))
    , inventoryFunction_(std::move(inventoryFunction))
{
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(120);
    connect(debounceTimer_, &QTimer::timeout, this, &ProjectFileWatchInventoryService::startInventory);
    connect(inventoryWatcher_,
            &QFutureWatcher<Result>::finished,
            this,
            &ProjectFileWatchInventoryService::handleInventoryFinished);
}

void ProjectFileWatchInventoryService::requestInventory(const QString &projectRootPath)
{
    pendingRequest_.requestSerial = ++latestRequestSerial_;
    pendingRequest_.inventoryRequest.projectRootPath = projectRootPath;
    hasPendingRequest_ = true;
    debounceTimer_->start();
}

void ProjectFileWatchInventoryService::setDebounceIntervalMs(int intervalMs)
{
    debounceTimer_->setInterval(intervalMs);
}

bool ProjectFileWatchInventoryService::isLatestRequestResult(const Result &result) const
{
    return result.requestSerial != 0 && result.requestSerial == latestRequestSerial_;
}

void ProjectFileWatchInventoryService::startInventory()
{
    if (!hasPendingRequest_) {
        return;
    }
    if (inventoryWatcher_->isRunning()) {
        queuedInventory_ = true;
        return;
    }

    const Request request = pendingRequest_;
    hasPendingRequest_ = false;
    const InventoryFunction inventoryFunction = inventoryFunction_;
    inventoryWatcher_->setFuture(QtConcurrent::run(
        [inventoryFunction, request]() {
            return collectInventory(inventoryFunction, request.inventoryRequest, request.requestSerial);
        }));
}

void ProjectFileWatchInventoryService::handleInventoryFinished()
{
    const Result result = inventoryWatcher_->result();
    if (isLatestRequestResult(result)) {
        emit inventoryFinished(result);
    }

    if (queuedInventory_) {
        queuedInventory_ = false;
        startInventory();
    }
}

} // namespace TherionStudio
