#pragma once

#include "TherionSqlReportWorker.h"

#include <QObject>

namespace TherionStudio
{

class TherionSqlReportSession : public QObject
{
    Q_OBJECT

public:
    explicit TherionSqlReportSession(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~TherionSqlReportSession() override = default;

    virtual bool requestImport(const TherionSqlReportImportRequest &request,
                               QString *errorMessage) = 0;
    virtual void requestQuery(const TherionSqlReportQueryRequest &request) = 0;
    virtual void shutdown() = 0;

signals:
    void importFinished(const TherionStudio::TherionSqlReportImportWorkerResult &result);
    void queryFinished(const TherionStudio::TherionSqlReportQueryWorkerResult &result);
};

} // namespace TherionStudio
