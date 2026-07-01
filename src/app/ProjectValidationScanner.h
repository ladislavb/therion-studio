#pragma once

#include "ProjectSourceProjectionCache.h"

#include "../core/ProjectStructureIndex.h"
#include "../core/TherionSourceValidator.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

#include <memory>
#include <optional>

class QTimer;

template <typename T>
class QFutureWatcher;

namespace TherionStudio
{
struct ProjectValidationIndexSnapshotCacheEntry
{
    QString sourceRequestKey;
    QString errorMessage;
    ProjectIndexSnapshot snapshot;
};

class ProjectValidationScanner final : public QObject
{
    Q_OBJECT

public:
    struct Finding
    {
        QString filePath;
        TherionSourceDiagnostic diagnostic;
    };

    struct Result
    {
        quint64 generation = 0;
        QString projectRootPath;
        QString errorMessage;
        QVector<Finding> findings;
        int searchedFileCount = 0;
        bool limitReached = false;
        ProjectSourceProjectionCacheStats projectionCacheStats;
        ProjectStructureIndexScanStats projectIndexScanStats;
        bool projectIndexSnapshotCacheHit = false;
        int documentValidationCacheHits = 0;
        int documentValidationCacheMisses = 0;
    };

    struct DocumentValidationCacheEntry
    {
        QVector<Finding> findings;
    };

    explicit ProjectValidationScanner(QObject *parent = nullptr);

    void requestScan(const QString &projectRootPath,
                     const TherionSourceValidationCatalog &validationCatalog,
                     const QHash<QString, QString> &inMemoryProjectContentsByPath);
    void setDebounceIntervalMs(int intervalMs);

signals:
    void validationStarted(quint64 generation, const QString &projectRootPath);
    void validationFinished(const TherionStudio::ProjectValidationScanner::Result &result);

private slots:
    void startScan();
    void handleScanFinished();

private:
    struct Request
    {
        QString projectRootPath;
        TherionSourceValidationCatalog validationCatalog;
        QHash<QString, QString> inMemoryProjectContentsByPath;
    };

    Request pendingRequest_;
    bool hasPendingRequest_ = false;
    bool queuedScan_ = false;
    quint64 generation_ = 0;
    QTimer *debounceTimer_ = nullptr;
    QFutureWatcher<Result> *scanWatcher_ = nullptr;
    std::shared_ptr<ProjectSourceProjectionCache> projectionCache_;
    std::shared_ptr<std::optional<ProjectValidationIndexSnapshotCacheEntry>> projectIndexSnapshotCache_;
    std::shared_ptr<QHash<QString, DocumentValidationCacheEntry>> documentValidationCache_;
    QString projectionCacheProjectRootPath_;
    QString projectionCacheCatalogSignature_;
};
}
