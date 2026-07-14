#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <Qt>

#include <optional>

namespace TherionStudio
{
enum class MapEditorFillPatternKind
{
    None,
    Hatch,
    CrossHatch,
    Dots
};

enum class MapEditorAreaDotPlacement
{
    Grid,
    Scatter
};

enum class MapEditorPointSymbol
{
    Circle,
    Oval,
    Square,
    Diamond,
    Triangle,
    Star,
    Asterisk,
    Plus,
    X
};

enum class MapEditorLineDecorationKind
{
    OffsetStroke,
    Parallel,
    Ticks,
    Rungs,
    Teeth,
    Symbols,
    Waves,
    SlopeTicks
};

enum class MapEditorLineDecorationSide
{
    Center,
    Left,
    Right
};

enum class MapEditorLineClosedFillMode
{
    None,
    Background,
    Color
};

enum class MapEditorPointLabelOrientationMode
{
    Screen,
    Orientation
};

enum class MapEditorSymbolPartKind
{
    Symbol,
    Line,
    Polyline,
    Polygon,
    Ellipse
};

struct MapEditorPointSymbolPart
{
    MapEditorSymbolPartKind kind = MapEditorSymbolPartKind::Symbol;
    MapEditorPointSymbol symbol = MapEditorPointSymbol::Circle;
    qreal x = 0.0;
    qreal y = 0.0;
    qreal size = 1.0;
    qreal angle = 0.0;
    qreal x1 = 0.0;
    qreal y1 = 0.0;
    qreal x2 = 0.0;
    qreal y2 = 0.0;
    qreal width = 1.0;
    qreal height = 1.0;
    QVector<QPointF> points;
    bool fill = false;
    std::optional<QColor> fillColor;
    std::optional<QColor> strokeColor;
    std::optional<qreal> strokeWidth;
};

struct MapEditorLineDecorationStyle
{
    MapEditorLineDecorationKind kind = MapEditorLineDecorationKind::Symbols;
    MapEditorLineDecorationSide side = MapEditorLineDecorationSide::Center;
    qreal spacing = 12.0;
    bool adjustSpacing = false;
    qreal spacingDivisor = 1.0;
    qreal offset = 0.0;
    QVector<qreal> offsets;
    std::optional<qreal> fromOffset;
    std::optional<qreal> toOffset;
    qreal size = 8.0;
    qreal length = 8.0;
    qreal alternateLengthScale = 1.0;
    qreal strokeWidth = 1.2;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    std::optional<QColor> fillColor;
    QVector<qreal> dashPattern;
    MapEditorPointSymbol symbol = MapEditorPointSymbol::Circle;
    QVector<MapEditorPointSymbolPart> symbolParts;
    qreal angle = 0.0;
    qreal angleJitter = 0.0;
    qreal sizeJitter = 0.0;
    qreal offsetJitter = 0.0;
    qreal distanceJitter = 0.0;
    std::optional<int> seed;
};

struct MapEditorLineClosedFillStyle
{
    MapEditorLineClosedFillMode mode = MapEditorLineClosedFillMode::None;
    std::optional<QColor> color;
};

struct MapEditorAreaFillPatternStyle
{
    MapEditorFillPatternKind kind = MapEditorFillPatternKind::None;
    MapEditorAreaDotPlacement dotPlacement = MapEditorAreaDotPlacement::Grid;
    qreal spacing = 6.0;
    qreal angle = 45.0;
    qreal strokeWidth = 1.0;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    QVector<qreal> dashPattern;
    qreal size = 2.4;
    qreal sizeJitter = 0.0;
    MapEditorPointSymbol symbol = MapEditorPointSymbol::Circle;
    QVector<MapEditorPointSymbolPart> symbolParts;
    qreal angleJitter = 0.0;
    qreal offsetJitter = 0.0;
    std::optional<int> seed;
};

struct MapEditorPointStyleDefaults
{
    MapEditorPointSymbol symbol = MapEditorPointSymbol::Circle;
    QVector<MapEditorPointSymbolPart> symbolParts;
    qreal size = 11.2;
    qreal outlineWidth = 1.1;
    std::optional<QColor> fillColor;
    std::optional<QColor> strokeColor;
    std::optional<QString> labelField;
    std::optional<qreal> labelFontSize;
    MapEditorPointLabelOrientationMode labelOrientation = MapEditorPointLabelOrientationMode::Screen;
};

struct MapEditorLineStyleDefaults
{
    bool strokeVisible = true;
    bool guideSpineVisible = false;
    qreal strokeWidth = 3.2;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    QVector<qreal> dashPattern;
    QVector<MapEditorLineDecorationStyle> decorations;
    MapEditorLineClosedFillStyle closedFill;
    std::optional<QString> labelField;
};

struct MapEditorAreaStyleDefaults
{
    qreal strokeWidth = 2.0;
    qreal fillOpacity = 0.11;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    std::optional<QColor> fillColor;
    QVector<qreal> dashPattern;
    std::optional<MapEditorAreaFillPatternStyle> fillPattern;
};

struct MapEditorStyleSelector
{
    QString rawType;
    QString subtype;
};

struct MapEditorPointStyleRule
{
    MapEditorStyleSelector selector;
    std::optional<QVector<MapEditorPointSymbolPart>> symbolParts;
    std::optional<qreal> size;
    std::optional<qreal> outlineWidth;
    std::optional<QColor> fillColor;
    std::optional<QColor> strokeColor;
    std::optional<QString> labelField;
    std::optional<qreal> labelFontSize;
    std::optional<MapEditorPointLabelOrientationMode> labelOrientation;
};

struct MapEditorLineStyleRule
{
    MapEditorStyleSelector selector;
    std::optional<bool> strokeVisible;
    std::optional<bool> guideSpineVisible;
    std::optional<qreal> strokeWidth;
    std::optional<Qt::PenStyle> penStyle;
    std::optional<QColor> strokeColor;
    std::optional<QVector<qreal>> dashPattern;
    std::optional<QVector<MapEditorLineDecorationStyle>> decorations;
    std::optional<MapEditorLineClosedFillStyle> closedFill;
    std::optional<QString> labelField;
};

struct MapEditorAreaStyleRule
{
    MapEditorStyleSelector selector;
    std::optional<qreal> strokeWidth;
    std::optional<qreal> fillOpacity;
    std::optional<Qt::PenStyle> penStyle;
    std::optional<QColor> strokeColor;
    std::optional<QColor> fillColor;
    std::optional<QVector<qreal>> dashPattern;
    std::optional<MapEditorAreaFillPatternStyle> fillPattern;
};

struct MapEditorObjectStyleCatalog
{
    MapEditorPointStyleDefaults point;
    MapEditorLineStyleDefaults line;
    MapEditorAreaStyleDefaults area;
    QVector<MapEditorPointStyleRule> pointStyles;
    QVector<MapEditorLineStyleRule> lineStyles;
    QVector<MapEditorAreaStyleRule> areaStyles;
    bool loadedFromResource = false;
    bool loadedUserOverrides = false;
};

struct MapEditorResolvedPointStyle
{
    MapEditorPointSymbol symbol = MapEditorPointSymbol::Circle;
    QVector<MapEditorPointSymbolPart> symbolParts;
    qreal size = 11.2;
    qreal outlineWidth = 1.1;
    std::optional<QColor> fillColor;
    std::optional<QColor> strokeColor;
    std::optional<QString> labelField;
    std::optional<qreal> labelFontSize;
    MapEditorPointLabelOrientationMode labelOrientation = MapEditorPointLabelOrientationMode::Screen;
};

struct MapEditorResolvedLineStyle
{
    bool strokeVisible = true;
    bool guideSpineVisible = false;
    qreal strokeWidth = 3.2;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    QVector<qreal> dashPattern;
    QVector<MapEditorLineDecorationStyle> decorations;
    MapEditorLineClosedFillStyle closedFill;
    std::optional<QString> labelField;
};

struct MapEditorResolvedAreaStyle
{
    qreal strokeWidth = 2.0;
    qreal fillOpacity = 0.11;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    std::optional<QColor> fillColor;
    QVector<qreal> dashPattern;
    std::optional<MapEditorAreaFillPatternStyle> fillPattern;
};

QString mapEditorUserObjectStylesDirectory();
MapEditorObjectStyleCatalog loadMapEditorObjectStyleCatalog(const QString &userOverrideDirectory);
MapEditorObjectStyleCatalog loadDefaultMapEditorObjectStyleCatalog();
MapEditorResolvedPointStyle resolveMapEditorPointStyle(const MapEditorObjectStyleCatalog &catalog,
                                                       const QString &rawType,
                                                       const QString &subtype = QString());
MapEditorResolvedLineStyle resolveMapEditorLineStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype = QString());
MapEditorResolvedAreaStyle resolveMapEditorAreaStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype = QString());
}
