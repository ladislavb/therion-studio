#pragma once

#include <QHash>
#include <QString>

#include <optional>

namespace TherionStudio
{
struct TherionSourceLogicalArgumentRange;
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

// The `-name` value of a `point ... station`, or nothing for any other command.
[[nodiscard]] std::optional<TherionSourceLogicalArgumentRange> therionPointStationNameRange(
    const TherionSourceLogicalCommand &command);

[[nodiscard]] std::optional<TherionStationNameTransform>
therionActiveScrapStationNameTransform(
    const TherionSourceLogicalCommand &command,
    const QHash<int, TherionStationNameTransform> &transformsByStartLine);
}
