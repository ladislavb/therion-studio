#pragma once

#include "ProjectSourceProjectionCache.h"

#include "../core/ProjectStructureIndex.h"
#include "../core/TherionSourceValidator.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

class QTimer;

template <typename T>
class QFutureWatcher;

namespace TherionStudio
{
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
};
}
