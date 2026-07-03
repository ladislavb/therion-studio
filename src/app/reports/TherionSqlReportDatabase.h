#pragma once

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

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
    TherionSqlReportDatabase();
    ~TherionSqlReportDatabase();

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
    static QStringList splitSqlStatements(const QString &sqlText, QString *errorMessage);
    static bool isAllowedImportStatement(const QString &statement, QString *errorMessage);
    static bool isReadOnlySelectStatement(const QString &statement, QString *errorMessage);
    static QString normalizedStatementPrefix(QString statement);
    static QStringList expectedTableNames();

    void close();
    bool openMemoryDatabase(QString *errorMessage);
    bool validateExpectedSchema(QStringList *missingTables) const;

    QString connectionName_;
    QString filePath_;
    QSqlDatabase database_;
};

} // namespace TherionStudio
