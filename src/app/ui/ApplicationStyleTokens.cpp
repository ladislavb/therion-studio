#include "ApplicationStyleTokens.h"

#include <algorithm>

namespace TherionStudio::UiStyleTokens
{

QString rgbaColorCss(const QColor &color, qreal alpha)
{
    const qreal clampedAlpha = std::clamp(alpha, 0.0, 1.0);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(clampedAlpha, 0, 'f', 3);
}

QColor mapModeSelectAccentColor()
{
    return QColor(QStringLiteral("#2e9f5c"));
}

QColor joinConnectionColor(JoinConnectionColorRole role, bool darkBackground)
{
    switch (role) {
    case JoinConnectionColorRole::AutomaticScrapJoin:
        return QColor(darkBackground ? QStringLiteral("#5fd38a") : QStringLiteral("#1b7f3a"));
    case JoinConnectionColorRole::CountedScrapJoin:
        return QColor(darkBackground ? QStringLiteral("#4fc3b0") : QStringLiteral("#1a7f72"));
    case JoinConnectionColorRole::PointLineJoin:
        return QColor(darkBackground ? QStringLiteral("#b2d65a") : QStringLiteral("#5f7f12"));
    }
    return QColor();
}

QColor mapModeInsertAccentColor()
{
    return QColor(QStringLiteral("#d34a4a"));
}

} // namespace TherionStudio::UiStyleTokens
