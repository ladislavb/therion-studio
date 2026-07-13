#pragma once

#include "TherionSqlReportDatabase.h"

#include <memory>

class QSettings;

namespace TherionStudio
{

class TherionSqlReportPresetStore
{
public:
    virtual ~TherionSqlReportPresetStore() = default;

    virtual QVector<TherionSqlReportDefinition> loadCustomPresets() const = 0;
    virtual void saveCustomPresets(const QVector<TherionSqlReportDefinition> &presets) = 0;

    static QString createPresetId();
};

class TherionSqlReportSettingsPresetStore final : public TherionSqlReportPresetStore
{
public:
    explicit TherionSqlReportSettingsPresetStore(std::unique_ptr<QSettings> settings);

    QVector<TherionSqlReportDefinition> loadCustomPresets() const override;
    void saveCustomPresets(const QVector<TherionSqlReportDefinition> &presets) override;

private:
    std::unique_ptr<QSettings> settings_;
};

} // namespace TherionStudio
