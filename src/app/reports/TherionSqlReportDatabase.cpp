#include "TherionSqlReportDatabase.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QThread>

#ifdef Q_OS_WIN
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif

#include <utility>

namespace TherionStudio
{
namespace
{
QString nativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

QString sqliteErrorText(sqlite3 *database)
{
    return database != nullptr ? QString::fromUtf8(sqlite3_errmsg(database)) : QString();
}

QString sqliteColumnText(sqlite3_stmt *statement, int column)
{
    const int columnType = sqlite3_column_type(statement, column);
    if (columnType == SQLITE_NULL) {
        return QString();
    }
    if (columnType == SQLITE_INTEGER) {
        return QString::number(sqlite3_column_int64(statement, column));
    }
    if (columnType == SQLITE_FLOAT) {
        return QString::number(sqlite3_column_double(statement, column), 'g', 15);
    }
    if (columnType == SQLITE_BLOB) {
        const auto *blob = static_cast<const char *>(sqlite3_column_blob(statement, column));
        const int byteCount = sqlite3_column_bytes(statement, column);
        return blob != nullptr ? QString::fromUtf8(blob, byteCount) : QString();
    }
    const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(statement, column));
    const int byteCount = sqlite3_column_bytes(statement, column);
    return text != nullptr ? QString::fromUtf8(text, byteCount) : QString();
}

class SqliteStatement final
{
public:
    ~SqliteStatement()
    {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }

    sqlite3_stmt **address()
    {
        return &statement_;
    }

    sqlite3_stmt *get() const
    {
        return statement_;
    }

private:
    sqlite3_stmt *statement_ = nullptr;
};

bool prepareStatement(sqlite3 *database,
                      const QString &sql,
                      SqliteStatement *statement,
                      QString *errorMessage)
{
    Q_ASSERT(statement != nullptr);
    const QByteArray utf8Sql = sql.toUtf8();
    const int result = sqlite3_prepare_v2(database,
                                          utf8Sql.constData(),
                                          utf8Sql.size(),
                                          statement->address(),
                                          nullptr);
    if (result == SQLITE_OK) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = sqliteErrorText(database);
    }
    return false;
}

int reportProgressCallback(void *context)
{
    auto *executionControl = static_cast<TherionSqlReportExecutionControl *>(context);
    return executionControl != nullptr && executionControl->shouldInterruptCurrentOperation() ? 1 : 0;
}

QString cleanedSqlForPrefix(QString statement)
{
    statement = statement.trimmed();
    if (statement.endsWith(QLatin1Char(';'))) {
        statement.chop(1);
    }
    return statement.trimmed();
}

QString translatedPresetTitle(const QString &id, const QString &fallbackTitle)
{
    if (id == QStringLiteral("overview")) {
        return TherionSqlReportDatabase::tr("Overview");
    }
    if (id == QStringLiteral("survey-lengths")) {
        return TherionSqlReportDatabase::tr("Survey Lengths");
    }
    if (id == QStringLiteral("exploration-by-person")) {
        return TherionSqlReportDatabase::tr("Exploration by Person");
    }
    if (id == QStringLiteral("surveying-by-person")) {
        return TherionSqlReportDatabase::tr("Surveying by Person");
    }
    if (id == QStringLiteral("length-by-survey-year")) {
        return TherionSqlReportDatabase::tr("Length by Survey Year");
    }
    if (id == QStringLiteral("exploration-by-year")) {
        return TherionSqlReportDatabase::tr("Exploration by Year");
    }
    if (id == QStringLiteral("recent-activity")) {
        return TherionSqlReportDatabase::tr("Recent Activity");
    }
    if (id == QStringLiteral("continuation-stations")) {
        return TherionSqlReportDatabase::tr("Continuation Stations");
    }
    if (id == QStringLiteral("lead-flags-by-survey")) {
        return TherionSqlReportDatabase::tr("Lead Flags by Survey");
    }
    if (id == QStringLiteral("entrances")) {
        return TherionSqlReportDatabase::tr("Entrances");
    }
    if (id == QStringLiteral("depth-by-survey")) {
        return TherionSqlReportDatabase::tr("Depth by Survey");
    }
    return fallbackTitle.trimmed().isEmpty() ? id : fallbackTitle;
}

QString queryFromJsonValue(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (!value.isArray()) {
        return QString();
    }

    QStringList lines;
    const QJsonArray array = value.toArray();
    lines.reserve(array.size());
    for (const QJsonValue &line : array) {
        if (!line.isString()) {
            return QString();
        }
        lines.append(line.toString());
    }
    return lines.join(QLatin1Char('\n')).trimmed();
}

QVector<TherionSqlReportDefinition> loadPresetDefinitionsFromResource()
{
    QFile file(QStringLiteral(":/resources/sql_report_presets.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return {};
    }

    QVector<TherionSqlReportDefinition> reports;
    const QJsonArray array = document.array();
    reports.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString().trimmed();
        const QString query = queryFromJsonValue(object.value(QStringLiteral("query")));
        if (id.isEmpty() || query.isEmpty()) {
            continue;
        }

        reports.push_back({
            id,
            translatedPresetTitle(id, object.value(QStringLiteral("title")).toString()),
            query,
        });
    }
    return reports;
}
}

TherionSqlReportDatabase::TherionSqlReportDatabase(ConnectionLifecycleObserver lifecycleObserver)
    : TherionSqlReportDatabase(std::make_shared<TherionSqlReportExecutionControl>(),
                               std::move(lifecycleObserver))
{
}

TherionSqlReportDatabase::TherionSqlReportDatabase(
    TherionSqlReportExecutionControlPtr executionControl,
    ConnectionLifecycleObserver lifecycleObserver)
    : ownerThread_(QThread::currentThread())
    , executionControl_(std::move(executionControl))
    , lifecycleObserver_(std::move(lifecycleObserver))
{
    Q_ASSERT(executionControl_ != nullptr);
}

TherionSqlReportDatabase::~TherionSqlReportDatabase()
{
    close();
}

bool TherionSqlReportDatabase::isOpen() const
{
    verifyOwnerThread();
    return database_ != nullptr;
}

QString TherionSqlReportDatabase::filePath() const
{
    return filePath_;
}

QString TherionSqlReportDatabase::displayName() const
{
    if (filePath_.isEmpty()) {
        return tr("SQL Reports");
    }
    return QFileInfo(filePath_).fileName();
}

QStringList TherionSqlReportDatabase::tableNames() const
{
    verifyOwnerThread();
    QStringList names;
    if (!isOpen()) {
        return names;
    }

    SqliteStatement statement;
    if (!prepareStatement(database_,
                          QStringLiteral("select name from sqlite_master where type = 'table' order by name"),
                          &statement,
                          nullptr)) {
        return names;
    }
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        names.append(sqliteColumnText(statement.get(), 0));
    }
    return names;
}

QStringList TherionSqlReportDatabase::tableColumns(const QString &tableName) const
{
    verifyOwnerThread();
    QStringList columns;
    if (!isOpen()) {
        return columns;
    }

    QString escapedTableName = tableName;
    escapedTableName.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    SqliteStatement statement;
    if (!prepareStatement(database_,
                          QStringLiteral("pragma table_info(\"%1\")").arg(escapedTableName),
                          &statement,
                          nullptr)) {
        return columns;
    }
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        columns.append(sqliteColumnText(statement.get(), 1));
    }
    return columns;
}

QStringList TherionSqlReportDatabase::expectedTableNames()
{
    return {
        QStringLiteral("SURVEY"),
        QStringLiteral("CENTRELINE"),
        QStringLiteral("PERSON"),
        QStringLiteral("EXPLO"),
        QStringLiteral("TOPO"),
        QStringLiteral("STATION"),
        QStringLiteral("STATION_FLAG"),
        QStringLiteral("SHOT"),
        QStringLiteral("SHOT_FLAG"),
    };
}

QVector<TherionSqlReportDefinition> TherionSqlReportDatabase::predefinedReports()
{
    return loadPresetDefinitionsFromResource();
}

void TherionSqlReportDatabase::close()
{
    verifyOwnerThread();
    if (database_ != nullptr) {
        sqlite3_progress_handler(database_, 0, nullptr, nullptr);
        executionControl_->detachConnection(database_);
        const int closeResult = sqlite3_close_v2(database_);
        Q_ASSERT(closeResult == SQLITE_OK);
        Q_UNUSED(closeResult);
        if (lifecycleObserver_) {
            lifecycleObserver_(ConnectionLifecycleEvent::Closed);
            lifecycleObserver_(ConnectionLifecycleEvent::Removed);
        }
        database_ = nullptr;
    }
    filePath_.clear();
}

bool TherionSqlReportDatabase::openMemoryDatabase(QString *errorMessage)
{
    verifyOwnerThread();
    close();
    if (lifecycleObserver_) {
        lifecycleObserver_(ConnectionLifecycleEvent::Added);
    }
    sqlite3 *openedDatabase = nullptr;
    const int openResult = sqlite3_open_v2(":memory:",
                                           &openedDatabase,
                                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                                           nullptr);
    if (openResult != SQLITE_OK) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Could not open in-memory SQLite database: %1")
                                .arg(sqliteErrorText(openedDatabase));
        }
        if (openedDatabase != nullptr) {
            sqlite3_close_v2(openedDatabase);
        }
        if (lifecycleObserver_) {
            lifecycleObserver_(ConnectionLifecycleEvent::Removed);
        }
        return false;
    }
    database_ = openedDatabase;
    executionControl_->attachConnection(database_);
    sqlite3_progress_handler(database_, 1000, reportProgressCallback, executionControl_.get());
    if (lifecycleObserver_) {
        lifecycleObserver_(ConnectionLifecycleEvent::Opened);
    }
    return true;
}

TherionSqlReportImportResult TherionSqlReportDatabase::importFile(const QString &path)
{
    verifyOwnerThread();
    TherionSqlReportImportResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = tr("Could not read %1.").arg(nativePath(path));
        return result;
    }

    QByteArray sqlBytes;
    constexpr qint64 kImportReadChunkSize = 256 * 1024;
    while (!file.atEnd()) {
        if (executionControl_->shouldInterruptCurrentOperation()) {
            return result;
        }
        const QByteArray chunk = file.read(kImportReadChunkSize);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            result.errorMessage = tr("Could not read %1.").arg(nativePath(path));
            return result;
        }
        sqlBytes.append(chunk);
    }

    QString errorMessage;
    if (!openMemoryDatabase(&errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    const QString sqlText = QString::fromUtf8(sqlBytes);
    const QStringList statements = splitSqlStatements(sqlText, &errorMessage);
    if (executionControl_->shouldInterruptCurrentOperation()) {
        close();
        return result;
    }
    if (!errorMessage.isEmpty()) {
        result.errorMessage = errorMessage;
        close();
        return result;
    }

    if (!executeStatement(QStringLiteral("begin immediate"), &errorMessage)) {
        result.errorMessage = tr("Could not start SQLite import transaction: %1")
                                  .arg(errorMessage);
        close();
        return result;
    }

    if (!importStatements(statements, &result, &errorMessage)) {
        result.errorMessage = errorMessage;
        close();
        return result;
    }

    if (!executeStatement(QStringLiteral("commit"), &errorMessage)) {
        result.errorMessage = tr("Could not commit SQLite import transaction: %1")
                                  .arg(errorMessage);
        close();
        return result;
    }

    QStringList missingTables;
    if (!validateExpectedSchema(&missingTables)) {
        result.missingTables = missingTables;
        result.errorMessage = tr("The SQL file does not look like a Therion database export. Missing tables: %1")
                                  .arg(missingTables.join(QStringLiteral(", ")));
        close();
        return result;
    }

    filePath_ = QFileInfo(path).absoluteFilePath();
    result.success = true;
    return result;
}

TherionSqlReportTable TherionSqlReportDatabase::executeReportQuery(const QString &queryText,
                                                                   QString *errorMessage,
                                                                   int rowLimit) const
{
    verifyOwnerThread();
    TherionSqlReportTable table;
    if (!isOpen()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No Therion SQL export is open.");
        }
        return table;
    }

    SqliteStatement statement;
    if (!prepareStatement(database_, queryText, &statement, errorMessage)) {
        return table;
    }

    const int columnCount = sqlite3_column_count(statement.get());
    table.columns.reserve(columnCount);
    for (int column = 0; column < columnCount; ++column) {
        table.columns.append(QString::fromUtf8(sqlite3_column_name(statement.get(), column)));
    }

    const int boundedLimit = qMax(1, rowLimit);
    while (true) {
        const int stepResult = sqlite3_step(statement.get());
        if (stepResult == SQLITE_DONE) {
            break;
        }
        if (stepResult != SQLITE_ROW) {
            if (errorMessage != nullptr) {
                *errorMessage = sqliteErrorText(database_);
            }
            return {};
        }
        if (table.rows.size() >= boundedLimit) {
            table.truncated = true;
            break;
        }
        QStringList row;
        row.reserve(columnCount);
        for (int column = 0; column < columnCount; ++column) {
            row.append(sqliteColumnText(statement.get(), column));
        }
        table.rows.append(row);
    }
    return table;
}

TherionSqlReportTable TherionSqlReportDatabase::executeCustomQuery(const QString &query,
                                                                   QString *errorMessage,
                                                                   int rowLimit) const
{
    verifyOwnerThread();
    QString validationError;
    if (!isReadOnlySelectStatement(query, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return {};
    }
    return executeReportQuery(cleanedSqlForPrefix(query), errorMessage, rowLimit);
}

QStringList TherionSqlReportDatabase::splitSqlStatements(const QString &sqlText,
                                                         QString *errorMessage) const
{
    QStringList statements;
    QString current;
    bool inString = false;
    QChar stringQuote;
    for (int index = 0; index < sqlText.size(); ++index) {
        if ((index % 4096) == 0 && executionControl_->shouldInterruptCurrentOperation()) {
            return {};
        }
        const QChar character = sqlText.at(index);
        current.append(character);

        if (inString) {
            if (character == stringQuote) {
                if (index + 1 < sqlText.size() && sqlText.at(index + 1) == stringQuote) {
                    current.append(sqlText.at(index + 1));
                    ++index;
                    continue;
                }
                inString = false;
            }
            continue;
        }

        if (character == QLatin1Char('\'') || character == QLatin1Char('"')) {
            inString = true;
            stringQuote = character;
            continue;
        }

        if (character == QLatin1Char(';')) {
            const QString statement = current.trimmed();
            if (!statement.isEmpty()) {
                statements.append(statement.left(statement.size() - 1).trimmed());
            }
            current.clear();
        }
    }

    if (inString) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("The SQL export contains an unterminated string literal.");
        }
        return {};
    }

    const QString tail = current.trimmed();
    if (!tail.isEmpty()) {
        statements.append(tail);
    }
    return statements;
}

QString TherionSqlReportDatabase::normalizedStatementPrefix(QString statement)
{
    statement = cleanedSqlForPrefix(statement);
    const int spaceIndex = statement.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString firstToken = spaceIndex >= 0 ? statement.left(spaceIndex) : statement;
    return firstToken.trimmed().toUpper();
}

bool TherionSqlReportDatabase::isAllowedImportStatement(const QString &statement, QString *errorMessage)
{
    const QString cleaned = cleanedSqlForPrefix(statement);
    const QString upper = cleaned.toUpper();
    const QString prefix = normalizedStatementPrefix(cleaned);
    if (prefix == QStringLiteral("BEGIN")
        || prefix == QStringLiteral("COMMIT")
        || prefix == QStringLiteral("END")
        || prefix == QStringLiteral("INSERT")) {
        return true;
    }
    if (upper.startsWith(QStringLiteral("CREATE TABLE "))) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = tr("Unsupported SQL export statement: %1").arg(cleaned.left(80));
    }
    return false;
}

bool TherionSqlReportDatabase::isReadOnlySelectStatement(const QString &statement, QString *errorMessage)
{
    const QString cleaned = cleanedSqlForPrefix(statement);
    if (cleaned.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Enter a SELECT query.");
        }
        return false;
    }
    if (cleaned.contains(QLatin1Char(';'))) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Only one SQL statement is allowed.");
        }
        return false;
    }

    const QString upper = cleaned.toUpper();
    if (!upper.startsWith(QStringLiteral("SELECT "))
        && !upper.startsWith(QStringLiteral("WITH "))) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Only read-only SELECT queries are allowed.");
        }
        return false;
    }

    static const QRegularExpression blockedTokens(
        QStringLiteral("\\b(INSERT|UPDATE|DELETE|DROP|ALTER|CREATE|REPLACE|ATTACH|DETACH|PRAGMA|VACUUM|REINDEX|TRIGGER|VIEW)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    if (blockedTokens.match(cleaned).hasMatch()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("The query contains a blocked SQL keyword.");
        }
        return false;
    }

    return true;
}

bool TherionSqlReportDatabase::validateExpectedSchema(QStringList *missingTables) const
{
    verifyOwnerThread();
    const QStringList actualTables = tableNames();
    QStringList normalizedTables;
    normalizedTables.reserve(actualTables.size());
    for (const QString &table : actualTables) {
        normalizedTables.append(table.toUpper());
    }

    QStringList missing;
    for (const QString &table : expectedTableNames()) {
        if (!normalizedTables.contains(table)) {
            missing.append(table);
        }
    }
    if (missingTables != nullptr) {
        *missingTables = missing;
    }
    return missing.isEmpty();
}

bool TherionSqlReportDatabase::importStatements(const QStringList &statements,
                                                TherionSqlReportImportResult *result,
                                                QString *errorMessage)
{
    Q_ASSERT(result != nullptr);
    for (const QString &statement : statements) {
        if (executionControl_->shouldInterruptCurrentOperation()) {
            executeStatement(QStringLiteral("rollback"), nullptr);
            return false;
        }
        if (statement.trimmed().isEmpty()) {
            continue;
        }
        if (!isAllowedImportStatement(statement, errorMessage)) {
            executeStatement(QStringLiteral("rollback"), nullptr);
            return false;
        }
        const QString prefix = normalizedStatementPrefix(statement);
        if (prefix == QStringLiteral("BEGIN")
            || prefix == QStringLiteral("COMMIT")
            || prefix == QStringLiteral("END")) {
            continue;
        }
        QString statementError;
        if (!executeStatement(statement, &statementError)) {
            executeStatement(QStringLiteral("rollback"), nullptr);
            if (errorMessage != nullptr) {
                *errorMessage = tr("Could not import SQL statement %1: %2")
                                    .arg(result->importedStatementCount + 1)
                                    .arg(statementError);
            }
            return false;
        }
        ++result->importedStatementCount;
    }
    return true;
}

bool TherionSqlReportDatabase::executeStatement(const QString &statement, QString *errorMessage) const
{
    verifyOwnerThread();
    SqliteStatement preparedStatement;
    if (!prepareStatement(database_, statement, &preparedStatement, errorMessage)) {
        return false;
    }
    const int result = sqlite3_step(preparedStatement.get());
    if (result == SQLITE_DONE) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = sqliteErrorText(database_);
    }
    return false;
}

void TherionSqlReportDatabase::verifyOwnerThread() const
{
    Q_ASSERT_X(QThread::currentThread() == ownerThread_,
               "TherionSqlReportDatabase",
               "SQLite connection accessed outside its owner thread");
}

} // namespace TherionStudio
