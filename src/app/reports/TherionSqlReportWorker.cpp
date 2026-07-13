#include "TherionSqlReportWorker.h"

#include <QCoreApplication>
#include <QScopeGuard>
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

QString cancelledErrorText()
{
    return QCoreApplication::translate("TherionStudio::TherionSqlReportWorker",
                                       "The SQL report operation was cancelled.");
}

QString timedOutErrorText()
{
    return QCoreApplication::translate("TherionStudio::TherionSqlReportWorker",
                                       "The SQL query exceeded its execution deadline.");
}
}

TherionSqlReportWorker::TherionSqlReportWorker(
    TherionSqlReportDatabase::ConnectionLifecycleObserver lifecycleObserver)
    : TherionSqlReportWorker(std::make_shared<TherionSqlReportExecutionControl>(),
                             std::move(lifecycleObserver))
{
}

TherionSqlReportWorker::TherionSqlReportWorker(
    TherionSqlReportExecutionControlPtr executionControl,
    TherionSqlReportDatabase::ConnectionLifecycleObserver lifecycleObserver)
    : lifecycleObserver_(std::move(lifecycleObserver))
    , executionControl_(std::move(executionControl))
{
    Q_ASSERT(executionControl_ != nullptr);
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
    TherionSqlReportImportWorkerResult result;
    result.requestId = request.requestId;
    result.sourceIdentity = request.sourceIdentity;
    if (!executionControl_->beginOperation(request.requestId, 0)) {
        result.errorCode = TherionSqlReportErrorCode::Cancelled;
        result.errorMessage = cancelledErrorText();
        result.cancelled = true;
        emit importFinished(result);
        return;
    }
    auto operationGuard = qScopeGuard([this, request]() {
        executionControl_->finishOperation(request.requestId);
    });

    if (database_ == nullptr) {
        database_ = std::make_unique<TherionSqlReportDatabase>(executionControl_, lifecycleObserver_);
    }

    sourceIdentity_.clear();
    const TherionSqlReportImportResult importResult = database_->importFile(request.filePath);

    result.importedStatementCount = importResult.importedStatementCount;
    result.missingTables = importResult.missingTables;
    result.errorMessage = importResult.errorMessage;
    const auto interruptionReason = executionControl_->interruptionReason(request.requestId);
    if (interruptionReason == TherionSqlReportExecutionControl::InterruptionReason::Cancelled) {
        result.errorCode = TherionSqlReportErrorCode::Cancelled;
        result.errorMessage = cancelledErrorText();
        result.cancelled = true;
        database_.reset();
    } else if (importResult.success) {
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

    if (!executionControl_->beginOperation(request.requestId,
                                           request.executionTimeoutMilliseconds)) {
        result.errorCode = TherionSqlReportErrorCode::Cancelled;
        result.errorMessage = cancelledErrorText();
        result.cancelled = true;
        emit queryFinished(result);
        return;
    }
    auto operationGuard = qScopeGuard([this, request]() {
        executionControl_->finishOperation(request.requestId);
    });

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
    const auto interruptionReason = executionControl_->interruptionReason(request.requestId);
    if (interruptionReason == TherionSqlReportExecutionControl::InterruptionReason::Cancelled) {
        result.table = {};
        result.errorCode = TherionSqlReportErrorCode::Cancelled;
        result.errorMessage = cancelledErrorText();
        result.cancelled = true;
    } else if (interruptionReason == TherionSqlReportExecutionControl::InterruptionReason::TimedOut) {
        result.table = {};
        result.errorCode = TherionSqlReportErrorCode::TimedOut;
        result.errorMessage = timedOutErrorText();
    } else if (!result.errorMessage.isEmpty()) {
        result.errorCode = TherionSqlReportErrorCode::QueryFailed;
    }
    emit queryFinished(result);
}

void TherionSqlReportWorker::shutdown()
{
    verifyWorkerThread();
    executionControl_->requestShutdown();
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
