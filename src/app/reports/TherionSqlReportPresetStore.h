#pragma once

#include "TherionSqlReportDatabase.h"

class QSettings;

namespace TherionStudio
{

class TherionSqlReportPresetStore final
{
public:
    explicit TherionSqlReportPresetStore(QSettings &settings);

    QVector<TherionSqlReportDefinition> loadCustomPresets() const;
    void saveCustomPresets(const QVector<TherionSqlReportDefinition> &presets);

    static QString createPresetId();

private:
    QSettings &settings_;
};

} // namespace TherionStudio
