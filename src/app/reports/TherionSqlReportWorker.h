#pragma once

#include "TherionSqlReportDatabase.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

namespace TherionStudio
{

enum class TherionSqlReportExecutionPolicy
{
    BuiltInReport,
    CustomReadOnly
};

enum class TherionSqlReportErrorCode
{
    None,
    ImportFailed,
    DatabaseNotLoaded,
    SourceMismatch,
    QueryFailed
};

struct TherionSqlReportSchemaTable
{
    QString name;
    QStringList columns;
};

struct TherionSqlReportImportRequest
{
    quint64 requestId = 0;
    QString sourceIdentity;
    QString filePath;
};

struct TherionSqlReportQueryRequest
{
    quint64 requestId = 0;
    QString sourceIdentity;
    QString queryText;
    int rowLimit = 1000;
    TherionSqlReportExecutionPolicy executionPolicy = TherionSqlReportExecutionPolicy::CustomReadOnly;
};

struct TherionSqlReportImportWorkerResult
{
    quint64 requestId = 0;
    QString sourceIdentity;
    QVector<TherionSqlReportSchemaTable> schema;
    int importedStatementCount = 0;
    QStringList missingTables;
    TherionSqlReportErrorCode errorCode = TherionSqlReportErrorCode::None;
    QString errorMessage;
    bool cancelled = false;
};

struct TherionSqlReportQueryWorkerResult
{
    quint64 requestId = 0;
    QString sourceIdentity;
    TherionSqlReportTable table;
    TherionSqlReportErrorCode errorCode = TherionSqlReportErrorCode::None;
    QString errorMessage;
    bool cancelled = false;
};

class TherionSqlReportWorker final : public QObject
{
    Q_OBJECT

public:
    explicit TherionSqlReportWorker(
        TherionSqlReportDatabase::ConnectionLifecycleObserver lifecycleObserver = {});
    ~TherionSqlReportWorker() override;

public slots:
    void importDatabase(const TherionStudio::TherionSqlReportImportRequest &request);
    void executeQuery(const TherionStudio::TherionSqlReportQueryRequest &request);
    void shutdown();

signals:
    void importFinished(const TherionStudio::TherionSqlReportImportWorkerResult &result);
    void queryFinished(const TherionStudio::TherionSqlReportQueryWorkerResult &result);

private:
    void verifyWorkerThread() const;
    QVector<TherionSqlReportSchemaTable> currentSchema() const;

    TherionSqlReportDatabase::ConnectionLifecycleObserver lifecycleObserver_;
    std::unique_ptr<TherionSqlReportDatabase> database_;
    QString sourceIdentity_;
};

} // namespace TherionStudio

Q_DECLARE_METATYPE(TherionStudio::TherionSqlReportImportRequest)
Q_DECLARE_METATYPE(TherionStudio::TherionSqlReportQueryRequest)
Q_DECLARE_METATYPE(TherionStudio::TherionSqlReportImportWorkerResult)
Q_DECLARE_METATYPE(TherionStudio::TherionSqlReportQueryWorkerResult)
