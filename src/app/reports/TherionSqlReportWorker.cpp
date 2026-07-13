#include "TherionSqlReportWorker.h"

#include <QCoreApplication>
#include <QThread>

#include <utility>

namespace TherionStudio
{
namespace
{
QString databaseNotLoadedErrorText()
{
    return QCoreApplication::translate("TherionStudio::TherionSqlReportDatabase",
                                       "No Therion SQL export is open.");
}

QString sourceMismatchErrorText()
{
    return QCoreApplication::translate(
        "TherionStudio::TherionSqlReportDatabase",
        "The query does not belong to the currently loaded SQL export.");
}
}

TherionSqlReportWorker::TherionSqlReportWorker(
    TherionSqlReportDatabase::ConnectionLifecycleObserver lifecycleObserver)
    : lifecycleObserver_(std::move(lifecycleObserver))
{
    qRegisterMetaType<TherionSqlReportImportRequest>();
    qRegisterMetaType<TherionSqlReportQueryRequest>();
    qRegisterMetaType<TherionSqlReportImportWorkerResult>();
    qRegisterMetaType<TherionSqlReportQueryWorkerResult>();
}

TherionSqlReportWorker::~TherionSqlReportWorker()
{
    verifyWorkerThread();
    database_.reset();
}

void TherionSqlReportWorker::importDatabase(const TherionSqlReportImportRequest &request)
{
    verifyWorkerThread();
    if (database_ == nullptr) {
        database_ = std::make_unique<TherionSqlReportDatabase>(lifecycleObserver_);
    }

    sourceIdentity_.clear();
    const TherionSqlReportImportResult importResult = database_->importFile(request.filePath);

    TherionSqlReportImportWorkerResult result;
    result.requestId = request.requestId;
    result.sourceIdentity = request.sourceIdentity;
    result.importedStatementCount = importResult.importedStatementCount;
    result.missingTables = importResult.missingTables;
    result.errorMessage = importResult.errorMessage;
    if (importResult.success) {
        sourceIdentity_ = request.sourceIdentity;
        result.schema = currentSchema();
    } else {
        result.errorCode = TherionSqlReportErrorCode::ImportFailed;
        database_.reset();
    }
    emit importFinished(result);
}

void TherionSqlReportWorker::executeQuery(const TherionSqlReportQueryRequest &request)
{
    verifyWorkerThread();
    TherionSqlReportQueryWorkerResult result;
    result.requestId = request.requestId;
    result.sourceIdentity = request.sourceIdentity;

    if (database_ == nullptr || !database_->isOpen()) {
        result.errorCode = TherionSqlReportErrorCode::DatabaseNotLoaded;
        result.errorMessage = databaseNotLoadedErrorText();
        emit queryFinished(result);
        return;
    }
    if (request.sourceIdentity != sourceIdentity_) {
        result.errorCode = TherionSqlReportErrorCode::SourceMismatch;
        result.errorMessage = sourceMismatchErrorText();
        emit queryFinished(result);
        return;
    }

    if (request.executionPolicy == TherionSqlReportExecutionPolicy::BuiltInReport) {
        result.table = database_->executeReportQuery(request.queryText,
                                                     &result.errorMessage,
                                                     request.rowLimit);
    } else {
        result.table = database_->executeCustomQuery(request.queryText,
                                                     &result.errorMessage,
                                                     request.rowLimit);
    }
    if (!result.errorMessage.isEmpty()) {
        result.errorCode = TherionSqlReportErrorCode::QueryFailed;
    }
    emit queryFinished(result);
}

void TherionSqlReportWorker::shutdown()
{
    verifyWorkerThread();
    sourceIdentity_.clear();
    database_.reset();
}

void TherionSqlReportWorker::verifyWorkerThread() const
{
    Q_ASSERT_X(QThread::currentThread() == thread(),
               "TherionSqlReportWorker",
               "SQL report worker invoked outside its affinity thread");
}

QVector<TherionSqlReportSchemaTable> TherionSqlReportWorker::currentSchema() const
{
    QVector<TherionSqlReportSchemaTable> schema;
    if (database_ == nullptr || !database_->isOpen()) {
        return schema;
    }
    const QStringList tableNames = database_->tableNames();
    schema.reserve(tableNames.size());
    for (const QString &tableName : tableNames) {
        schema.append({tableName, database_->tableColumns(tableName)});
    }
    return schema;
}

} // namespace TherionStudio
