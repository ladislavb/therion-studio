#pragma once

#include <QString>

namespace TherionStudio
{
struct TherionSourceLogicalCommand;

// The `-projection` (or `-proj`) value of a scrap or map as written, without brackets; empty when there is none.
[[nodiscard]] QString therionCommandProjection(const TherionSourceLogicalCommand &command);

// A scrap's projection as written, `plan` when it has none (Therion's default).
[[nodiscard]] QString therionScrapProjection(const TherionSourceLogicalCommand &scrapCommand);

// Therion's identity of a projection (`thdb2d::parse_projection`): its type, `:index`, and elevation angle as
// written, so `[elevation 315]` and `[elevation 315.0]` differ. Empty when the type is not one Therion knows.
[[nodiscard]] QString therionProjectionIdentity(const QString &projection);

[[nodiscard]] bool therionProjectionIsCrossSection(const QString &projection);
}
