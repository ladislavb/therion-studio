#pragma once

#include "../core/TherionSourceValidator.h"

#include <QColor>
#include <QPalette>

namespace TherionStudio
{
inline bool validationSeverityUsesDarkPalette(const QPalette &palette)
{
    const QColor baseColor = palette.color(QPalette::Base);
    const QColor referenceColor = baseColor.isValid() ? baseColor : palette.color(QPalette::Window);
    return referenceColor.isValid() && referenceColor.lightness() < 128;
}

inline QColor validationSeverityForeground(TherionSourceDiagnosticSeverity severity, const QPalette &palette)
{
    const bool darkPalette = validationSeverityUsesDarkPalette(palette);
    switch (severity) {
    case TherionSourceDiagnosticSeverity::Error:
        return darkPalette ? QColor(248, 113, 113) : QColor(185, 28, 28);
    case TherionSourceDiagnosticSeverity::Warning:
        return darkPalette ? QColor(251, 191, 36) : QColor(180, 83, 9);
    case TherionSourceDiagnosticSeverity::Information:
        return darkPalette ? QColor(96, 165, 250) : QColor(29, 78, 216);
    }
    return palette.color(QPalette::Text);
}

inline QColor validationSeverityAccent(TherionSourceDiagnosticSeverity severity, const QPalette &palette)
{
    const bool darkPalette = validationSeverityUsesDarkPalette(palette);
    switch (severity) {
    case TherionSourceDiagnosticSeverity::Error:
        return darkPalette ? QColor(248, 113, 113) : QColor(220, 38, 38);
    case TherionSourceDiagnosticSeverity::Warning:
        return darkPalette ? QColor(251, 191, 36) : QColor(217, 119, 6);
    case TherionSourceDiagnosticSeverity::Information:
        return darkPalette ? QColor(96, 165, 250) : QColor(37, 99, 235);
    }
    return palette.color(QPalette::Highlight);
}

inline QColor validationSeverityBackground(TherionSourceDiagnosticSeverity severity,
                                           const QPalette &palette,
                                           qreal alpha)
{
    QColor color = validationSeverityAccent(severity, palette);
    color.setAlphaF(qBound(0.0, alpha, 1.0));
    return color;
}
}
