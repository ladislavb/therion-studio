#include "ProjectStructureScanner.h"

#include "ProjectScanCacheService.h"
#include "ProjectSourceSnapshot.h"

#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>

#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
ProjectStructureScanner::ScanFunction makeScanFunction(
    const std::shared_ptr<ProjectScanCacheService> &scanCacheService)
{
    return [scanCacheService](const QString &projectRootPath,
                              const QString &preferredConfigPath,
                              const QHash<QString, QString> &inMemoryProjectContentsByPath,
                              quint64 requestSerial) {
        ProjectStructureScanner::Result result;
        result.requestSerial = requestSerial;
        result.projectRootPath = projectRootPath;
        bool sourceSnapshotCacheHit = false;
        const ProjectSourceSnapshot sourceSnapshot =
            scanCacheService->projectSourceSnapshot(projectRootPath,
                                                    preferredConfigPath,
                                                    inMemoryProjectContentsByPath,
                                                    -1,
                                                    &sourceSnapshotCacheHit);
        result.projectSourceSnapshotCacheHit = sourceSnapshotCacheHit;
        const QString sourceRequestKey = sourceSnapshot.requestKey.stableKey();
        const std::optional<ProjectIndexSnapshotCacheEntry> cachedProjectIndex =
            scanCacheService->projectIndexSnapshot(sourceRequestKey);
        if (cachedProjectIndex.has_value()) {
            result.projectIndexSnapshotCacheHit = true;
            result.errorMessage = cachedProjectIndex->errorMessage;
            result.projectIndex = cachedProjectIndex->snapshot;
        } else {
            result.projectIndex = ProjectStructureIndex::scanProjectIndex(
                projectStructureIndexSourceSet(sourceSnapshot),
                &result.errorMessage);
            scanCacheService->storeProjectIndexSnapshot(ProjectIndexSnapshotCacheEntry{
                sourceRequestKey,
                result.errorMessage,
                result.projectIndex,
            });
        }
        result.entries = result.projectIndex.entries;
        return result;
    };
}
}

ProjectStructureScanner::ProjectStructureScanner(QObject *parent)
    : ProjectStructureScanner(std::make_shared<ProjectScanCacheService>(), parent)
{
}

ProjectStructureScanner::ProjectStructureScanner(std::shared_ptr<ProjectScanCacheService> scanCacheService,
                                                 QObject *parent)
    : ProjectStructureScanner(makeScanFunction(scanCacheService != nullptr
                                                   ? std::move(scanCacheService)
                                                   : std::make_shared<ProjectScanCacheService>()),
                              parent)
{
}

ProjectStructureScanner::ProjectStructureScanner(ScanFunction scanFunction, QObject *parent)
    : QObject(parent)
    , debounceTimer_(new QTimer(this))
    , scanWatcher_(new QFutureWatcher<Result>(this))
    , scanFunction_(std::move(scanFunction))
{
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(180);
    connect(debounceTimer_, &QTimer::timeout, this, &ProjectStructureScanner::startScan);
    connect(scanWatcher_, &QFutureWatcher<Result>::finished, this, &ProjectStructureScanner::handleScanFinished);
}

void ProjectStructureScanner::requestScan(const QString &projectRootPath,
                                          const QHash<QString, QString> &inMemoryProjectContentsByPath)
{
    requestScan(projectRootPath, inMemoryProjectContentsByPath, QString());
}

void ProjectStructureScanner::requestScan(const QString &projectRootPath,
                                          const QHash<QString, QString> &inMemoryProjectContentsByPath,
                                          const QString &preferredConfigPath)
{
    pendingRequest_.requestSerial = ++latestRequestSerial_;
    pendingRequest_.projectRootPath = projectRootPath;
    pendingRequest_.preferredConfigPath = preferredConfigPath;
    pendingRequest_.inMemoryProjectContentsByPath = inMemoryProjectContentsByPath;
    hasPendingRequest_ = true;
    debounceTimer_->start();
}

void ProjectStructureScanner::setDebounceIntervalMs(int intervalMs)
{
    debounceTimer_->setInterval(intervalMs);
}

void ProjectStructureScanner::startScan()
{
    if (!hasPendingRequest_) {
        return;
    }

    if (scanWatcher_->isRunning()) {
        queuedScan_ = true;
        return;
    }

    const Request request = pendingRequest_;
    hasPendingRequest_ = false;
    const ScanFunction scanFunction = scanFunction_;
    auto future = QtConcurrent::run([request, scanFunction]() {
        return scanFunction(request.projectRootPath,
                            request.preferredConfigPath,
                            request.inMemoryProjectContentsByPath,
                            request.requestSerial);
    });
    scanWatcher_->setFuture(future);
}

void ProjectStructureScanner::handleScanFinished()
{
    const Result result = scanWatcher_->result();
    if (result.requestSerial == latestRequestSerial_) {
        emit scanFinished(result);
    }

    if (queuedScan_) {
        queuedScan_ = false;
        startScan();
    }
}
}
