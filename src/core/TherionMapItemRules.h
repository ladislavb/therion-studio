#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class TherionSourceLogicalDocument;

// The map body syntax errors Therion stops on (`thmap::parse_item`, `thmap::parse_preview`).
enum class TherionMapItemProblem
{
    None,
    InvalidItem,
    InvalidShift,
    UnsupportedPreviewType,
};

struct TherionMapItemSyntax
{
    QString name;
    TherionMapItemProblem problem = TherionMapItemProblem::None;
    // The value the problem is reported at.
    int problemValueIndex = 0;
};

// A map item line, `<name>` or `<name> [<x> <y> [<units>]] above|below|none`, as split by Therion.
// The shift is read only when the config has `maps-offset` on.
[[nodiscard]] TherionMapItemSyntax therionMapItemSyntax(const QStringList &values, bool mapsOffset);

// A `preview above|below <map>` line, as split by Therion, `preview` included.
[[nodiscard]] TherionMapItemSyntax therionMapPreviewSyntax(const QStringList &values);

// Whether a config reads map item shifts: its last `maps-offset` switch, on by default.
[[nodiscard]] bool therionConfigUsesMapsOffset(const TherionSourceLogicalDocument &configDocument);
}
