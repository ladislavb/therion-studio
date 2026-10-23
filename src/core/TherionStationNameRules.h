#pragma once

#include "TherionSourceLogicalDocument.h"

#include <QHash>
#include <QString>

#include <optional>

namespace TherionStudio
{
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

// The `-name` value of a `point <x> <y> station` command; nullopt for any other command.
[[nodiscard]] std::optional<TherionSourceLogicalArgumentRange> therionStationPointNameRange(
    const TherionSourceLogicalCommand &command);
}
