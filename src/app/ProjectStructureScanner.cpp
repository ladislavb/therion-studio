#include "ProjectStructureScanner.h"

#include "ProjectScanCacheService.h"
#include "ProjectSourceProjectionCache.h"
#include "ProjectSourceSnapshot.h"
#include "../platform/DiagnosticLogging.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>

#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
void logProjectStructureScan(const ProjectStructureScanner::Result &result,
                             qsizetype sourceCount,
                             qint64 collectMs,
                             qint64 indexMs)
{
    const ProjectStructureIndexScanStats &stats = result.projectIndex.scanStats;
    qInfo().noquote()
        << QStringLiteral("project-structure-scan sources=%1 collect_ms=%2 index_ms=%3 source_snapshot_cache_hit=%4 "
                          "index_snapshot_cache_hit=%5 logical_builds=%6 logical_hits=%7 prebuilt_logical_hits=%8 "
                          "structure_ms=%9 map_references_ms=%10 join_references_ms=%11 station_index_ms=%12 "
                          "station_references_ms=%13 duplicate_ids_ms=%14 retained_parses=%15 retained_reuses=%16")
               .arg(sourceCount)
               .arg(collectMs)
               .arg(indexMs)
               .arg(result.projectSourceSnapshotCacheHit ? 1 : 0)
               .arg(result.projectIndexSnapshotCacheHit ? 1 : 0)
               .arg(stats.logicalDocumentBuilds)
               .arg(stats.logicalDocumentHits)
               .arg(stats.prebuiltLogicalDocumentHits)
               .arg(stats.structureMs)
               .arg(stats.mapReferencesMs)
               .arg(stats.joinReferencesMs)
               .arg(stats.stationIndexMs)
               .arg(stats.stationReferencesMs)
               .arg(stats.duplicateIdsMs)
               .arg(result.projectionCacheStats.logicalDocumentBuilds)
               .arg(result.projectionCacheStats.logicalDocumentHits);
}

ProjectStructureScanner::ScanFunction makeScanFunction(
    const std::shared_ptr<ProjectScanCacheService> &scanCacheService)
{
    // Parsed documents kept between scans of one project, keyed by content like Validation's cache, so a rescan after
    // a save parses only changed files. Scans run one at a time, so only one worker uses it at once.
    struct RetainedProjections
    {
        QString projectRootPath;
        ProjectSourceProjectionCache cache;
    };
    auto retained = std::make_shared<RetainedProjections>();
    return [scanCacheService, retained](const QString &projectRootPath,
                              const QString &preferredConfigPath,
                              const QHash<QString, QString> &inMemoryProjectContentsByPath,
                              quint64 requestSerial) {
        ProjectStructureScanner::Result result;
        result.requestSerial = requestSerial;
        result.projectRootPath = projectRootPath;
        QElapsedTimer timer;
        timer.start();
        bool sourceSnapshotCacheHit = false;
        const ProjectSourceSnapshot sourceSnapshot =
            scanCacheService->projectSourceSnapshot(projectRootPath,
                                                    preferredConfigPath,
                                                    inMemoryProjectContentsByPath,
                                                    -1,
                                                    &sourceSnapshotCacheHit);
        result.projectSourceSnapshotCacheHit = sourceSnapshotCacheHit;
        const qint64 collectMs = timer.restart();
        const QString sourceRequestKey = sourceSnapshot.requestKey.stableKey();
        const std::optional<ProjectIndexSnapshotCacheEntry> cachedProjectIndex =
            scanCacheService->projectIndexSnapshot(sourceRequestKey);
        if (cachedProjectIndex.has_value()) {
            result.projectIndexSnapshotCacheHit = true;
            result.errorMessage = cachedProjectIndex->errorMessage;
            result.projectIndex = cachedProjectIndex->snapshot;
        } else {
            if (retained->projectRootPath != sourceSnapshot.requestKey.normalizedProjectRootPath) {
                retained->cache.clear();
                retained->projectRootPath = sourceSnapshot.requestKey.normalizedProjectRootPath;
            } else {
                retained->cache.resetStats();
            }
            result.projectIndex = ProjectStructureIndex::scanProjectIndex(
                projectStructureIndexSourceSetWithLogicalDocuments(
                    sourceSnapshot, retained->cache, ProjectSourceParsing::OnDemand),
                &result.errorMessage);
            result.projectionCacheStats = retained->cache.stats();
            scanCacheService->storeProjectIndexSnapshot(ProjectIndexSnapshotCacheEntry{
                sourceRequestKey,
                result.errorMessage,
                result.projectIndex,
            });
        }
        result.entries = result.projectIndex.entries;
        if (diagnosticLoggingEnabled()) {
            logProjectStructureScan(result, sourceSnapshot.documents.size(), collectMs, timer.elapsed());
        }
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

bool ProjectStructureScanner::isLatestRequestResult(const Result &result) const
{
    return result.requestSerial != 0 && result.requestSerial == latestRequestSerial_;
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
    if (isLatestRequestResult(result)) {
        emit scanFinished(result);
    }

    if (queuedScan_) {
        queuedScan_ = false;
        startScan();
    }
}
}
