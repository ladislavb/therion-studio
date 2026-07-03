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
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <QVariant>

namespace TherionStudio
{
namespace
{
QString nativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

QString valueToDisplayString(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) {
        return QString();
    }
    return value.toString();
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

TherionSqlReportDatabase::TherionSqlReportDatabase()
    : connectionName_(QStringLiteral("therion-sql-report-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

TherionSqlReportDatabase::~TherionSqlReportDatabase()
{
    close();
}

bool TherionSqlReportDatabase::isOpen() const
{
    return database_.isValid() && database_.isOpen();
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
    return isOpen() ? database_.tables(QSql::Tables) : QStringList();
}

QStringList TherionSqlReportDatabase::tableColumns(const QString &tableName) const
{
    QStringList columns;
    if (!isOpen()) {
        return columns;
    }

    const QSqlRecord record = database_.record(tableName);
    columns.reserve(record.count());
    for (int column = 0; column < record.count(); ++column) {
        columns.append(record.fieldName(column));
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
    if (database_.isValid()) {
        database_.close();
        database_ = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool TherionSqlReportDatabase::openMemoryDatabase(QString *errorMessage)
{
    close();
    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(QStringLiteral(":memory:"));
    if (!database_.open()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Could not open in-memory SQLite database: %1")
                                .arg(database_.lastError().text());
        }
        return false;
    }
    return true;
}

TherionSqlReportImportResult TherionSqlReportDatabase::importFile(const QString &path)
{
    TherionSqlReportImportResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = tr("Could not read %1.").arg(nativePath(path));
        return result;
    }

    QString errorMessage;
    if (!openMemoryDatabase(&errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    const QString sqlText = QString::fromUtf8(file.readAll());
    const QStringList statements = splitSqlStatements(sqlText, &errorMessage);
    if (!errorMessage.isEmpty()) {
        result.errorMessage = errorMessage;
        close();
        return result;
    }

    if (!database_.transaction()) {
        result.errorMessage = tr("Could not start SQLite import transaction: %1")
                                  .arg(database_.lastError().text());
        close();
        return result;
    }

    QSqlQuery query(database_);
    for (const QString &statement : statements) {
        if (statement.trimmed().isEmpty()) {
            continue;
        }
        if (!isAllowedImportStatement(statement, &errorMessage)) {
            database_.rollback();
            result.errorMessage = errorMessage;
            close();
            return result;
        }
        const QString prefix = normalizedStatementPrefix(statement);
        if (prefix == QStringLiteral("BEGIN")
            || prefix == QStringLiteral("COMMIT")
            || prefix == QStringLiteral("END")) {
            continue;
        }
        if (!query.exec(statement)) {
            database_.rollback();
            result.errorMessage = tr("Could not import SQL statement %1: %2")
                                      .arg(result.importedStatementCount + 1)
                                      .arg(query.lastError().text());
            close();
            return result;
        }
        ++result.importedStatementCount;
    }

    if (!database_.commit()) {
        result.errorMessage = tr("Could not commit SQLite import transaction: %1")
                                  .arg(database_.lastError().text());
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
    TherionSqlReportTable table;
    if (!isOpen()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No Therion SQL export is open.");
        }
        return table;
    }

    QSqlQuery query(database_);
    query.setForwardOnly(true);
    if (!query.exec(queryText)) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return table;
    }

    const QSqlRecord record = query.record();
    for (int column = 0; column < record.count(); ++column) {
        table.columns.append(record.fieldName(column));
    }

    const int boundedLimit = qMax(1, rowLimit);
    while (query.next()) {
        if (table.rows.size() >= boundedLimit) {
            table.truncated = true;
            break;
        }
        QStringList row;
        row.reserve(record.count());
        for (int column = 0; column < record.count(); ++column) {
            row.append(valueToDisplayString(query.value(column)));
        }
        table.rows.append(row);
    }
    return table;
}

TherionSqlReportTable TherionSqlReportDatabase::executeCustomQuery(const QString &query,
                                                                   QString *errorMessage,
                                                                   int rowLimit) const
{
    QString validationError;
    if (!isReadOnlySelectStatement(query, &validationError)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationError;
        }
        return {};
    }
    return executeReportQuery(cleanedSqlForPrefix(query), errorMessage, rowLimit);
}

QStringList TherionSqlReportDatabase::splitSqlStatements(const QString &sqlText, QString *errorMessage)
{
    QStringList statements;
    QString current;
    bool inString = false;
    QChar stringQuote;
    for (int index = 0; index < sqlText.size(); ++index) {
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

} // namespace TherionStudio
