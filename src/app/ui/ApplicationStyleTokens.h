#pragma once

#include <QColor>
#include <QString>

namespace TherionStudio::UiStyleTokens
{

QString rgbaColorCss(const QColor &color, qreal alpha);

QColor mapModeSelectAccentColor();
QColor mapModeInsertAccentColor();

enum class JoinConnectionColorRole
{
    AutomaticScrapJoin,
    CountedScrapJoin,
    PointLineJoin,
};

// Three distinguishable greens for the Joins list, with variants readable on light and dark backgrounds.
QColor joinConnectionColor(JoinConnectionColorRole role, bool darkBackground);

} // namespace TherionStudio::UiStyleTokens
