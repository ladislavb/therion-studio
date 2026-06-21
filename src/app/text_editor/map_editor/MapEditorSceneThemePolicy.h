#pragma once

#include <QColor>

namespace TherionStudio
{

struct MapEditorSceneThemeColors
{
    QColor canvasBorder;
    QColor canvasFill;
    QColor geometryStroke;
    QColor areaFill;
    QColor labelText;
    QColor mutedText;
    QColor pointHandleStroke;
    QColor pointHandleFill;
    QColor controlConnector;
    QColor controlHandleStroke;
    QColor controlHandleFill;
};

inline QColor mapEditorCanvasViewportBackgroundColor()
{
    return QColor(QStringLiteral("#f2f0df"));
}

inline MapEditorSceneThemeColors mapEditorSceneThemeColors()
{
    return {
        QColor(QStringLiteral("#cfc9a7")),
        QColor(QStringLiteral("#fffde7")),
        QColor(QStringLiteral("#1d2837")),
        QColor(48, 73, 105, 28),
        QColor(QStringLiteral("#344a67")),
        QColor(QStringLiteral("#556b84")),
        QColor(18, 26, 37, 220),
        QColor(24, 30, 42, 190),
        QColor(0, 70, 220, 235),
        QColor(20, 73, 148, 230),
        QColor(96, 176, 248, 220),
    };
}

inline QColor mapEditorDirectionTickColor()
{
    return QColor(QStringLiteral("#ffda00"));
}

} // namespace TherionStudio
