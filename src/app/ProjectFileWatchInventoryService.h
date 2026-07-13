#pragma once

#include "ProjectFileWatchInventory.h"

#include <QObject>

#include <functional>

class QTimer;

template <typename T>
class QFutureWatcher;

namespace TherionStudio
{

class ProjectFileWatchInventoryService final : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        quint64 requestSerial = 0;
        QString projectRootPath;
        ProjectFileWatchInventory inventory;
        qint64 discoveryDurationMs = 0;
    };

    using InventoryFunction = std::function<ProjectFileWatchInventory(
        const ProjectFileWatchInventoryRequest &request)>;

    explicit ProjectFileWatchInventoryService(QObject *parent = nullptr);
    explicit ProjectFileWatchInventoryService(InventoryFunction inventoryFunction, QObject *parent = nullptr);

    void requestInventory(const QString &projectRootPath);
    void setDebounceIntervalMs(int intervalMs);
    bool isLatestRequestResult(const Result &result) const;

signals:
    void inventoryFinished(const TherionStudio::ProjectFileWatchInventoryService::Result &result);

private slots:
    void startInventory();
    void handleInventoryFinished();

private:
    struct Request
    {
        quint64 requestSerial = 0;
        ProjectFileWatchInventoryRequest inventoryRequest;
    };

    Request pendingRequest_;
    bool hasPendingRequest_ = false;
    bool queuedInventory_ = false;
    quint64 latestRequestSerial_ = 0;
    QTimer *debounceTimer_ = nullptr;
    QFutureWatcher<Result> *inventoryWatcher_ = nullptr;
    InventoryFunction inventoryFunction_;
};

} // namespace TherionStudio
