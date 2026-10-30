#pragma once

#include <QDebug>
#include <QRegularExpression>
#include <QString>

#include <cmath>

namespace TherionStudio
{
struct MapAppearanceSettings
{
    enum class XviCenterlineColorMode
    {
        SingleColor,
        ByAltitude
    };

    static constexpr double minimumCenterlineWidth = 0.1;
    static constexpr double maximumCenterlineWidth = 12.0;

    int altitudeUnderlayStrengthPercent = 19;
    QString xviCenterlineColor = QStringLiteral("#82122c58");
    double xviCenterlineWidth = 1.8;
    XviCenterlineColorMode xviCenterlineColorMode = XviCenterlineColorMode::ByAltitude;

    bool isValid() const
    {
        return altitudeUnderlayStrengthPercent >= 0 && altitudeUnderlayStrengthPercent <= 100
            && isValidCenterlineColor(xviCenterlineColor) && std::isfinite(xviCenterlineWidth)
            && xviCenterlineWidth >= minimumCenterlineWidth
            && xviCenterlineWidth <= maximumCenterlineWidth
            && (xviCenterlineColorMode == XviCenterlineColorMode::SingleColor
                || xviCenterlineColorMode == XviCenterlineColorMode::ByAltitude);
    }

    static bool isValidCenterlineColor(const QString &color)
    {
        static const QRegularExpression pattern(QStringLiteral("\\A#[0-9a-fA-F]{8}\\z"));
        return pattern.match(color).hasMatch();
    }
};
}
