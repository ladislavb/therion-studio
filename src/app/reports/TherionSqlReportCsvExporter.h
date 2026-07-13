#pragma once

#include "TherionSqlReportDatabase.h"

#include <QByteArray>
#include <QString>

namespace TherionStudio
{

class TherionSqlReportCsvExporter
{
public:
    virtual ~TherionSqlReportCsvExporter() = default;

    virtual bool writeTable(const QString &filePath,
                            const TherionSqlReportTable &table,
                            QString *errorMessage) const = 0;
};

class TherionSqlReportCsvFileExporter final : public TherionSqlReportCsvExporter
{
public:
    bool writeTable(const QString &filePath,
                    const TherionSqlReportTable &table,
                    QString *errorMessage) const override;

    static QByteArray serialize(const TherionSqlReportTable &table);
};

} // namespace TherionStudio
