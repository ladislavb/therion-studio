#include "TherionSqlReportPresetStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>

namespace TherionStudio
{
namespace
{
constexpr auto kCustomPresetsKey = "reports/sqlReportCustomPresets";

QString trimmedString(const QJsonObject &object, const QString &key)
{
    return object.value(key).toString().trimmed();
}
}

TherionSqlReportPresetStore::TherionSqlReportPresetStore(QSettings &settings)
    : settings_(settings)
{
}

QVector<TherionSqlReportDefinition> TherionSqlReportPresetStore::loadCustomPresets() const
{
    const QString encoded = settings_.value(QString::fromLatin1(kCustomPresetsKey)).toString();
    if (encoded.trimmed().isEmpty()) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(encoded.toUtf8());
    if (!document.isArray()) {
        return {};
    }

    QVector<TherionSqlReportDefinition> presets;
    const QJsonArray array = document.array();
    presets.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        TherionSqlReportDefinition preset;
        preset.id = trimmedString(object, QStringLiteral("id"));
        preset.title = trimmedString(object, QStringLiteral("title"));
        preset.query = object.value(QStringLiteral("query")).toString().trimmed();
        if (preset.id.isEmpty() || preset.title.isEmpty() || preset.query.isEmpty()) {
            continue;
        }
        presets.append(preset);
    }
    return presets;
}

void TherionSqlReportPresetStore::saveCustomPresets(const QVector<TherionSqlReportDefinition> &presets)
{
    QJsonArray array;
    for (const TherionSqlReportDefinition &preset : presets) {
        if (preset.id.trimmed().isEmpty() || preset.title.trimmed().isEmpty() || preset.query.trimmed().isEmpty()) {
            continue;
        }

        QJsonObject object;
        object.insert(QStringLiteral("id"), preset.id.trimmed());
        object.insert(QStringLiteral("title"), preset.title.trimmed());
        object.insert(QStringLiteral("query"), preset.query.trimmed());
        array.append(object);
    }

    settings_.setValue(QString::fromLatin1(kCustomPresetsKey),
                       QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

QString TherionSqlReportPresetStore::createPresetId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace TherionStudio
