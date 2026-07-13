#pragma once

#include "TherionSqlReportExecutionControl.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QThread;
struct sqlite3;

namespace TherionStudio
{

struct TherionSqlReportTable
{
    QStringList columns;
    QVector<QStringList> rows;
    bool truncated = false;
};

struct TherionSqlReportDefinition
{
    QString id;
    QString title;
    QString query;
};

struct TherionSqlReportImportResult
{
    bool success = false;
    QString errorMessage;
    int importedStatementCount = 0;
    QStringList missingTables;
};

class TherionSqlReportDatabase final
{
public:
    Q_DECLARE_TR_FUNCTIONS(TherionStudio::TherionSqlReportDatabase)

public:
    enum class ConnectionLifecycleEvent
    {
        Added,
        Opened,
        Closed,
        Removed
    };

    using ConnectionLifecycleObserver = std::function<void(ConnectionLifecycleEvent)>;

public:
    explicit TherionSqlReportDatabase(ConnectionLifecycleObserver lifecycleObserver = {});
    TherionSqlReportDatabase(TherionSqlReportExecutionControlPtr executionControl,
                             ConnectionLifecycleObserver lifecycleObserver = {});
    ~TherionSqlReportDatabase();

    TherionSqlReportDatabase(const TherionSqlReportDatabase &) = delete;
    TherionSqlReportDatabase &operator=(const TherionSqlReportDatabase &) = delete;

    bool isOpen() const;
    QString filePath() const;
    QString displayName() const;
    QStringList tableNames() const;
    QStringList tableColumns(const QString &tableName) const;

    TherionSqlReportImportResult importFile(const QString &filePath);
    TherionSqlReportTable executeReportQuery(const QString &query, QString *errorMessage, int rowLimit = 1000) const;
    TherionSqlReportTable executeCustomQuery(const QString &query, QString *errorMessage, int rowLimit = 1000) const;

    static QVector<TherionSqlReportDefinition> predefinedReports();

private:
    QStringList splitSqlStatements(const QString &sqlText, QString *errorMessage) const;
    static bool isAllowedImportStatement(const QString &statement, QString *errorMessage);
    static bool isReadOnlySelectStatement(const QString &statement, QString *errorMessage);
    static QString normalizedStatementPrefix(QString statement);
    static QStringList expectedTableNames();

    void close();
    bool importStatements(const QStringList &statements,
                          TherionSqlReportImportResult *result,
                          QString *errorMessage);
    bool openMemoryDatabase(QString *errorMessage);
    bool validateExpectedSchema(QStringList *missingTables) const;
    void verifyOwnerThread() const;
    bool executeStatement(const QString &statement, QString *errorMessage) const;

    QString filePath_;
    sqlite3 *database_ = nullptr;
    QThread *ownerThread_ = nullptr;
    TherionSqlReportExecutionControlPtr executionControl_;
    ConnectionLifecycleObserver lifecycleObserver_;
};

} // namespace TherionStudio
