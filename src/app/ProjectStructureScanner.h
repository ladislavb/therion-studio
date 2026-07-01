#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

#include "../core/ProjectStructureIndex.h"

#include <memory>

class QTimer;

template <typename T>
class QFutureWatcher;

namespace TherionStudio
{
class ProjectScanCacheService;

class ProjectStructureScanner final : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        quint64 generation = 0;
        QString projectRootPath;
        QString errorMessage;
        ProjectIndexSnapshot projectIndex;
        QVector<ProjectStructureEntry> entries;
        bool projectSourceSnapshotCacheHit = false;
        bool projectIndexSnapshotCacheHit = false;
    };

    explicit ProjectStructureScanner(QObject *parent = nullptr);
    explicit ProjectStructureScanner(std::shared_ptr<ProjectScanCacheService> scanCacheService,
                                     QObject *parent = nullptr);
    void requestScan(const QString &projectRootPath,
                     const QHash<QString, QString> &inMemoryProjectContentsByPath);
    void requestScan(const QString &projectRootPath,
                     const QHash<QString, QString> &inMemoryProjectContentsByPath,
                     const QString &preferredConfigPath);
    void setDebounceIntervalMs(int intervalMs);

signals:
    void scanFinished(const TherionStudio::ProjectStructureScanner::Result &result);

private slots:
    void startScan();
    void handleScanFinished();

private:
    struct Request
    {
        QString projectRootPath;
        QString preferredConfigPath;
        QHash<QString, QString> inMemoryProjectContentsByPath;
    };

    Request pendingRequest_;
    bool hasPendingRequest_ = false;
    bool queuedScan_ = false;
    quint64 generation_ = 0;
    QTimer *debounceTimer_ = nullptr;
    QFutureWatcher<Result> *scanWatcher_ = nullptr;
    std::shared_ptr<ProjectScanCacheService> scanCacheService_;
};
}
