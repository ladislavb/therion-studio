#pragma once

#include <QHash>
#include <QString>

#include <optional>

namespace TherionStudio
{
struct TherionSourceLogicalCommand;
class TherionSourceLogicalDocument;

struct TherionStationNameTransform
{
    QString prefix;
    QString suffix;
};

[[nodiscard]] QString therionStationReferenceWithScrapNameTransform(
    const QString &referenceName,
    const TherionStationNameTransform &transform);

[[nodiscard]] QHash<int, TherionStationNameTransform>
therionScrapStationNameTransformsByStartLine(const TherionSourceLogicalDocument &logicalDocument);

[[nodiscard]] std::optional<TherionStationNameTransform>
therionActiveScrapStationNameTransform(
    const TherionSourceLogicalCommand &command,
    const QHash<int, TherionStationNameTransform> &transformsByStartLine);
}
