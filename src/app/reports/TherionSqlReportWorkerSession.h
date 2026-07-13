#pragma once

#include "TherionSqlReportSession.h"

class QThread;

namespace TherionStudio
{

class TherionSqlReportWorker;

class TherionSqlReportWorkerSession final : public TherionSqlReportSession
{
    Q_OBJECT

public:
    explicit TherionSqlReportWorkerSession(QObject *parent = nullptr);
    TherionSqlReportWorkerSession(
        TherionSqlReportDatabase::ConnectionLifecycleObserver lifecycleObserver,
        QObject *parent = nullptr);
    ~TherionSqlReportWorkerSession() override;

    bool requestImport(const TherionSqlReportImportRequest &request,
                       QString *errorMessage) override;
    void requestQuery(const TherionSqlReportQueryRequest &request) override;
    void shutdown() override;

private:
    TherionSqlReportExecutionControlPtr executionControl_;
    QThread *workerThread_ = nullptr;
    TherionSqlReportWorker *worker_ = nullptr;
    bool shuttingDown_ = false;
};

} // namespace TherionStudio
