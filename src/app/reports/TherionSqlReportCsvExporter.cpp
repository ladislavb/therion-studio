#include "TherionSqlReportCsvExporter.h"

#include <QFile>

namespace TherionStudio
{
namespace
{
QString csvEscaped(QString value)
{
    if (value.contains(QLatin1Char('"'))) {
        value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    }
    if (value.contains(QLatin1Char(','))
        || value.contains(QLatin1Char('\n'))
        || value.contains(QLatin1Char('\r'))
        || value.contains(QLatin1Char('"'))) {
        return QStringLiteral("\"%1\"").arg(value);
    }
    return value;
}
}

bool TherionSqlReportCsvFileExporter::writeTable(const QString &filePath,
                                                  const TherionSqlReportTable &table,
                                                  QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const QByteArray contents = serialize(table);
    if (file.write(contents) == contents.size()) {
        return true;
    }
    if (errorMessage != nullptr) {
        *errorMessage = file.errorString();
    }
    return false;
}

QByteArray TherionSqlReportCsvFileExporter::serialize(const TherionSqlReportTable &table)
{
    QByteArray csv;
    auto appendRow = [&csv](const QStringList &values) {
        QStringList escapedValues;
        escapedValues.reserve(values.size());
        for (const QString &value : values) {
            escapedValues.append(csvEscaped(value));
        }
        csv.append(escapedValues.join(QLatin1Char(',')).toUtf8());
        csv.append('\n');
    };

    appendRow(table.columns);
    for (const QStringList &row : table.rows) {
        appendRow(row);
    }
    return csv;
}

} // namespace TherionStudio
