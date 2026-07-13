#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

#include "../core/ProjectStructureIndex.h"

#include <functional>
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
        quint64 requestSerial = 0;
        QString projectRootPath;
        QString errorMessage;
        ProjectIndexSnapshot projectIndex;
        QVector<ProjectStructureEntry> entries;
        bool projectSourceSnapshotCacheHit = false;
        bool projectIndexSnapshotCacheHit = false;
    };

    using ScanFunction = std::function<Result(const QString &projectRootPath,
                                              const QString &preferredConfigPath,
                                              const QHash<QString, QString> &inMemoryProjectContentsByPath,
                                              quint64 requestSerial)>;

    explicit ProjectStructureScanner(QObject *parent = nullptr);
    explicit ProjectStructureScanner(std::shared_ptr<ProjectScanCacheService> scanCacheService,
                                     QObject *parent = nullptr);
    explicit ProjectStructureScanner(ScanFunction scanFunction, QObject *parent = nullptr);
    void requestScan(const QString &projectRootPath,
                     const QHash<QString, QString> &inMemoryProjectContentsByPath);
    void requestScan(const QString &projectRootPath,
                     const QHash<QString, QString> &inMemoryProjectContentsByPath,
                     const QString &preferredConfigPath);
    void setDebounceIntervalMs(int intervalMs);
    bool isLatestRequestResult(const Result &result) const;

signals:
    void scanFinished(const TherionStudio::ProjectStructureScanner::Result &result);

private slots:
    void startScan();
    void handleScanFinished();

private:
    struct Request
    {
        quint64 requestSerial = 0;
        QString projectRootPath;
        QString preferredConfigPath;
        QHash<QString, QString> inMemoryProjectContentsByPath;
    };

    Request pendingRequest_;
    bool hasPendingRequest_ = false;
    bool queuedScan_ = false;
    quint64 latestRequestSerial_ = 0;
    QTimer *debounceTimer_ = nullptr;
    QFutureWatcher<Result> *scanWatcher_ = nullptr;
    ScanFunction scanFunction_;
};
}
