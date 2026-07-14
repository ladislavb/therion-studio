#include "MapEditorObjectStyleCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcessEnvironment>
#include <QRectF>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <optional>

namespace TherionStudio
{
namespace
{
std::optional<qreal> optionalPositiveNumber(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (number <= 0.0) {
        return std::nullopt;
    }
    return number;
}

std::optional<qreal> optionalFiniteNumber(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (!std::isfinite(number)) {
        return std::nullopt;
    }
    return number;
}

std::optional<qreal> optionalNonNegativeNumber(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (number < 0.0) {
        return std::nullopt;
    }
    return number;
}

std::optional<bool> optionalBool(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isBool()) {
        return std::nullopt;
    }
    return value.toBool();
}

std::optional<qreal> optionalZeroToOneNumber(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (number < 0.0 || number > 1.0) {
        return std::nullopt;
    }
    return number;
}

std::optional<QColor> optionalColor(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) {
        return std::nullopt;
    }

    const QColor color(value.toString().trimmed());
    if (!color.isValid()) {
        return std::nullopt;
    }
    return color;
}

std::optional<MapEditorLineClosedFillStyle> optionalLineClosedFill(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) {
        return std::nullopt;
    }

    const QString raw = value.toString().trimmed();
    const QString normalized = raw.toLower();
    MapEditorLineClosedFillStyle closedFill;
    if (normalized == QStringLiteral("none")
        || normalized == QStringLiteral("off")
        || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("0")) {
        closedFill.mode = MapEditorLineClosedFillMode::None;
        return closedFill;
    }
    if (normalized == QStringLiteral("background")
        || normalized == QStringLiteral("clean")) {
        closedFill.mode = MapEditorLineClosedFillMode::Background;
        return closedFill;
    }

    const QColor color(raw);
    if (!color.isValid()) {
        return std::nullopt;
    }

    closedFill.mode = MapEditorLineClosedFillMode::Color;
    closedFill.color = color;
    return closedFill;
}

std::optional<QString> optionalFieldName(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) {
        return std::nullopt;
    }

    QString field = value.toString().trimmed().toLower();
    while (field.startsWith(QLatin1Char('-'))) {
        field.remove(0, 1);
    }
    if (field.isEmpty()) {
        return std::nullopt;
    }
    return field;
}

std::optional<MapEditorPointLabelOrientationMode> optionalPointLabelOrientation(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) {
        return std::nullopt;
    }

    const QString normalized = value.toString().trimmed().toLower();
    if (normalized == QStringLiteral("orientation")
        || normalized == QStringLiteral("oriented")
        || normalized == QStringLiteral("point")) {
        return MapEditorPointLabelOrientationMode::Orientation;
    }
    if (normalized == QStringLiteral("screen")
        || normalized == QStringLiteral("horizontal")
        || normalized == QStringLiteral("none")) {
        return MapEditorPointLabelOrientationMode::Screen;
    }
    return std::nullopt;
}

Qt::PenStyle penStyleFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("dash") || normalized == QStringLiteral("dashed")) {
        return Qt::DashLine;
    }
    if (normalized == QStringLiteral("dot") || normalized == QStringLiteral("dotted")) {
        return Qt::DotLine;
    }
    if (normalized == QStringLiteral("dash-dot")) {
        return Qt::DashDotLine;
    }
    if (normalized == QStringLiteral("dash-dot-dot")) {
        return Qt::DashDotDotLine;
    }
    return Qt::SolidLine;
}

std::optional<int> optionalInteger(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (!std::isfinite(number)) {
        return std::nullopt;
    }
    return static_cast<int>(std::llround(number));
}

std::optional<QVector<qreal>> optionalDashPattern(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isArray()) {
        return std::nullopt;
    }

    const QJsonArray array = value.toArray();
    if (array.isEmpty()) {
        return QVector<qreal>{};
    }

    QVector<qreal> pattern;
    pattern.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (!entry.isDouble()) {
            return std::nullopt;
        }
        const qreal number = entry.toDouble();
        if (number <= 0.0) {
            return std::nullopt;
        }
        pattern.append(number);
    }

    return pattern;
}

std::optional<QVector<qreal>> optionalNumberArray(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isArray()) {
        return std::nullopt;
    }

    const QJsonArray array = value.toArray();
    QVector<qreal> numbers;
    numbers.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (!entry.isDouble()) {
            return std::nullopt;
        }
        const qreal number = entry.toDouble();
        if (!std::isfinite(number)) {
            return std::nullopt;
        }
        numbers.append(number);
    }

    return numbers;
}

QString normalizedToken(const QString &value)
{
    return value.trimmed().toLower();
}

std::optional<MapEditorLineDecorationKind> lineDecorationKindFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("offset_stroke") || normalized == QStringLiteral("offset-stroke")) {
        return MapEditorLineDecorationKind::OffsetStroke;
    }
    if (normalized == QStringLiteral("parallel")) {
        return MapEditorLineDecorationKind::Parallel;
    }
    if (normalized == QStringLiteral("ticks")) {
        return MapEditorLineDecorationKind::Ticks;
    }
    if (normalized == QStringLiteral("rungs")) {
        return MapEditorLineDecorationKind::Rungs;
    }
    if (normalized == QStringLiteral("teeth")) {
        return MapEditorLineDecorationKind::Teeth;
    }
    if (normalized == QStringLiteral("symbols")) {
        return MapEditorLineDecorationKind::Symbols;
    }
    if (normalized == QStringLiteral("waves") || normalized == QStringLiteral("wave")) {
        return MapEditorLineDecorationKind::Waves;
    }
    if (normalized == QStringLiteral("slope_ticks") || normalized == QStringLiteral("slope-ticks")) {
        return MapEditorLineDecorationKind::SlopeTicks;
    }
    return std::nullopt;
}

MapEditorLineDecorationSide lineDecorationSideFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("left")) {
        return MapEditorLineDecorationSide::Left;
    }
    if (normalized == QStringLiteral("right")) {
        return MapEditorLineDecorationSide::Right;
    }
    return MapEditorLineDecorationSide::Center;
}

MapEditorFillPatternKind fillPatternKindFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("hatch")) {
        return MapEditorFillPatternKind::Hatch;
    }
    if (normalized == QStringLiteral("cross_hatch")) {
        return MapEditorFillPatternKind::CrossHatch;
    }
    if (normalized == QStringLiteral("dots")) {
        return MapEditorFillPatternKind::Dots;
    }
    return MapEditorFillPatternKind::None;
}

MapEditorAreaDotPlacement areaDotPlacementFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("scatter") || normalized == QStringLiteral("random")) {
        return MapEditorAreaDotPlacement::Scatter;
    }
    return MapEditorAreaDotPlacement::Grid;
}

std::optional<MapEditorPointSymbol> pointSymbolFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("circle")) {
        return MapEditorPointSymbol::Circle;
    }
    if (normalized == QStringLiteral("square")) {
        return MapEditorPointSymbol::Square;
    }
    if (normalized == QStringLiteral("diamond")) {
        return MapEditorPointSymbol::Diamond;
    }
    if (normalized == QStringLiteral("triangle")) {
        return MapEditorPointSymbol::Triangle;
    }
    if (normalized == QStringLiteral("star")) {
        return MapEditorPointSymbol::Star;
    }
    if (normalized == QStringLiteral("asterisk")) {
        return MapEditorPointSymbol::Asterisk;
    }
    if (normalized == QStringLiteral("plus")) {
        return MapEditorPointSymbol::Plus;
    }
    if (normalized == QStringLiteral("x")) {
        return MapEditorPointSymbol::X;
    }
    return std::nullopt;
}

std::optional<MapEditorPointSymbol> dotPatternSymbolFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("oval")) {
        return MapEditorPointSymbol::Oval;
    }

    return pointSymbolFromString(value);
}

std::optional<QVector<QPointF>> optionalPointArray(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isArray()) {
        return std::nullopt;
    }

    QVector<QPointF> points;
    const QJsonArray array = value.toArray();
    points.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (entry.isArray()) {
            const QJsonArray pointArray = entry.toArray();
            if (pointArray.size() < 2 || !pointArray.at(0).isDouble() || !pointArray.at(1).isDouble()) {
                continue;
            }
            points.append(QPointF(pointArray.at(0).toDouble(), pointArray.at(1).toDouble()));
        } else if (entry.isObject()) {
            const QJsonObject pointObject = entry.toObject();
            const std::optional<qreal> x = optionalFiniteNumber(pointObject, "x");
            const std::optional<qreal> y = optionalFiniteNumber(pointObject, "y");
            if (x.has_value() && y.has_value()) {
                points.append(QPointF(x.value(), y.value()));
            }
        }
    }
    return points;
}

std::optional<MapEditorPointSymbolPart> readPointSymbolPart(const QJsonObject &object)
{
    const QString kind = normalizedToken(object.value(QStringLiteral("kind")).toString());
    if (kind.isEmpty()) {
        return std::nullopt;
    }

    MapEditorPointSymbolPart part;
    if (kind == QStringLiteral("line")) {
        part.kind = MapEditorSymbolPartKind::Line;
    } else if (kind == QStringLiteral("polyline")) {
        part.kind = MapEditorSymbolPartKind::Polyline;
    } else if (kind == QStringLiteral("polygon")) {
        part.kind = MapEditorSymbolPartKind::Polygon;
    } else if (kind == QStringLiteral("ellipse")) {
        part.kind = MapEditorSymbolPartKind::Ellipse;
    } else if (const std::optional<MapEditorPointSymbol> symbol = dotPatternSymbolFromString(kind)) {
        part.kind = MapEditorSymbolPartKind::Symbol;
        part.symbol = symbol.value();
    } else {
        return std::nullopt;
    }

    part.x = optionalFiniteNumber(object, "x").value_or(0.0);
    part.y = optionalFiniteNumber(object, "y").value_or(0.0);
    part.size = optionalPositiveNumber(object, "size").value_or(1.0);
    part.angle = optionalFiniteNumber(object, "angle").value_or(0.0);
    part.x1 = optionalFiniteNumber(object, "x1").value_or(0.0);
    part.y1 = optionalFiniteNumber(object, "y1").value_or(0.0);
    part.x2 = optionalFiniteNumber(object, "x2").value_or(0.0);
    part.y2 = optionalFiniteNumber(object, "y2").value_or(0.0);
    part.width = optionalPositiveNumber(object, "width").value_or(part.size);
    part.height = optionalPositiveNumber(object, "height").value_or(part.size);
    part.fill = optionalBool(object, "fill").value_or(false);
    part.fillColor = optionalColor(object, "fill_color");
    part.strokeColor = optionalColor(object, "stroke_color");
    part.strokeWidth = optionalNonNegativeNumber(object, "stroke_width");
    if (const std::optional<QVector<QPointF>> points = optionalPointArray(object, "points")) {
        part.points = points.value();
    }

    if ((part.kind == MapEditorSymbolPartKind::Polyline && part.points.size() < 2)
        || (part.kind == MapEditorSymbolPartKind::Polygon && part.points.size() < 3)) {
        return std::nullopt;
    }
    return part;
}

std::optional<QVector<MapEditorPointSymbolPart>> readPointSymbolParts(const QJsonObject &object)
{
    const QJsonValue value = object.value(QStringLiteral("symbol_parts"));
    if (value.isUndefined()) {
        return std::nullopt;
    }
    if (!value.isArray()) {
        return std::nullopt;
    }

    QVector<MapEditorPointSymbolPart> parts;
    const QJsonArray array = value.toArray();
    parts.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (!entry.isObject()) {
            continue;
        }
        if (const std::optional<MapEditorPointSymbolPart> part = readPointSymbolPart(entry.toObject())) {
            parts.append(part.value());
        }
    }
    return parts;
}

std::optional<qreal> symbolPartsNaturalSize(const QVector<MapEditorPointSymbolPart> &parts)
{
    QRectF bounds;
    bool hasBounds = false;
    const auto includePoint = [&](const QPointF &point) {
        if (hasBounds) {
            bounds = bounds.united(QRectF(point, QSizeF(0.0, 0.0)));
        } else {
            bounds = QRectF(point, QSizeF(0.0, 0.0));
            hasBounds = true;
        }
    };
    const auto includeRect = [&](const QRectF &rect) {
        if (hasBounds) {
            bounds = bounds.united(rect);
        } else {
            bounds = rect;
            hasBounds = true;
        }
    };

    for (const MapEditorPointSymbolPart &part : parts) {
        switch (part.kind) {
        case MapEditorSymbolPartKind::Symbol: {
            const qreal halfSize = part.size / 2.0;
            includeRect(QRectF(part.x - halfSize, part.y - halfSize, part.size, part.size));
            break;
        }
        case MapEditorSymbolPartKind::Line:
            includePoint(QPointF(part.x1, part.y1));
            includePoint(QPointF(part.x2, part.y2));
            break;
        case MapEditorSymbolPartKind::Polyline:
        case MapEditorSymbolPartKind::Polygon:
            for (const QPointF &point : part.points) {
                includePoint(point);
            }
            break;
        case MapEditorSymbolPartKind::Ellipse:
            includeRect(QRectF(part.x - (part.width / 2.0),
                              part.y - (part.height / 2.0),
                              part.width,
                              part.height));
            break;
        }
    }

    if (!hasBounds) {
        return std::nullopt;
    }

    const qreal size = qMax(bounds.width(), bounds.height());
    if (size <= 0.0 || !std::isfinite(size)) {
        return std::nullopt;
    }
    return size;
}

std::optional<MapEditorLineDecorationStyle> readLineDecoration(const QJsonObject &object)
{
    if (object.isEmpty()) {
        return std::nullopt;
    }

    const std::optional<MapEditorLineDecorationKind> kind =
        lineDecorationKindFromString(object.value(QStringLiteral("kind")).toString());
    if (!kind.has_value()) {
        return std::nullopt;
    }

    MapEditorLineDecorationStyle decoration;
    decoration.kind = kind.value();
    decoration.side = lineDecorationSideFromString(object.value(QStringLiteral("side")).toString());

    if (decoration.kind == MapEditorLineDecorationKind::Teeth
        && object.value(QStringLiteral("side")).toString().trimmed().isEmpty()) {
        decoration.side = MapEditorLineDecorationSide::Right;
    }

    if (const std::optional<qreal> spacing = optionalPositiveNumber(object, "spacing")) {
        decoration.spacing = spacing.value();
    }
    if (const std::optional<bool> adjustSpacing = optionalBool(object, "adjust_spacing")) {
        decoration.adjustSpacing = adjustSpacing.value();
    }
    if (const std::optional<qreal> spacingDivisor = optionalPositiveNumber(object, "spacing_divisor")) {
        decoration.spacingDivisor = spacingDivisor.value();
    }
    if (const std::optional<qreal> offset = optionalFiniteNumber(object, "offset")) {
        decoration.offset = offset.value();
    }
    if (const std::optional<QVector<qreal>> offsets = optionalNumberArray(object, "offsets")) {
        decoration.offsets = offsets.value();
    }
    decoration.fromOffset = optionalFiniteNumber(object, "from_offset");
    decoration.toOffset = optionalFiniteNumber(object, "to_offset");
    if (const std::optional<qreal> size = optionalPositiveNumber(object, "size")) {
        decoration.size = size.value();
    }
    if (const std::optional<qreal> length = optionalPositiveNumber(object, "length")) {
        decoration.length = length.value();
    } else if (const std::optional<qreal> defaultLength = optionalPositiveNumber(object, "default_length")) {
        decoration.length = defaultLength.value();
    }
    if (const std::optional<qreal> alternateLengthScale = optionalPositiveNumber(object, "alternate_length_scale")) {
        decoration.alternateLengthScale = alternateLengthScale.value();
    }
    if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(object, "stroke_width")) {
        decoration.strokeWidth = strokeWidth.value();
    }
    if (object.value(QStringLiteral("stroke_style")).isString()) {
        decoration.strokeStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
    }
    decoration.strokeColor = optionalColor(object, "stroke_color");
    decoration.fillColor = optionalColor(object, "fill_color");
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
        decoration.dashPattern = dashPattern.value();
    }
    if (const std::optional<QVector<MapEditorPointSymbolPart>> symbolParts = readPointSymbolParts(object)) {
        decoration.symbolParts = symbolParts.value();
    }
    if (const std::optional<qreal> angle = optionalFiniteNumber(object, "angle")) {
        decoration.angle = angle.value();
    }
    if (const std::optional<qreal> angleJitter = optionalNonNegativeNumber(object, "angle_jitter")) {
        decoration.angleJitter = angleJitter.value();
    }
    if (const std::optional<qreal> sizeJitter = optionalNonNegativeNumber(object, "size_jitter")) {
        decoration.sizeJitter = sizeJitter.value();
    }
    if (const std::optional<qreal> offsetJitter = optionalNonNegativeNumber(object, "offset_jitter")) {
        decoration.offsetJitter = offsetJitter.value();
    }
    if (const std::optional<qreal> distanceJitter = optionalNonNegativeNumber(object, "distance_jitter")) {
        decoration.distanceJitter = distanceJitter.value();
    }
    if (const std::optional<int> seed = optionalInteger(object, "seed")) {
        decoration.seed = seed;
    }

    return decoration;
}

std::optional<QVector<MapEditorLineDecorationStyle>> readLineDecorations(const QJsonObject &object)
{
    const QJsonValue value = object.value(QStringLiteral("decorations"));
    if (!value.isArray()) {
        return std::nullopt;
    }

    QVector<MapEditorLineDecorationStyle> decorations;
    const QJsonArray array = value.toArray();
    decorations.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (!entry.isObject()) {
            continue;
        }
        if (const std::optional<MapEditorLineDecorationStyle> decoration =
                readLineDecoration(entry.toObject())) {
            decorations.append(decoration.value());
        }
    }
    return decorations;
}

std::optional<MapEditorAreaFillPatternStyle> readAreaFillPattern(const QJsonObject &object)
{
    if (object.isEmpty()) {
        return std::nullopt;
    }

    MapEditorAreaFillPatternStyle pattern;
    pattern.kind = fillPatternKindFromString(object.value(QStringLiteral("kind")).toString());
    if (pattern.kind == MapEditorFillPatternKind::None) {
        return std::nullopt;
    }
    pattern.dotPlacement = areaDotPlacementFromString(object.value(QStringLiteral("placement")).toString());

    if (const std::optional<qreal> spacing = optionalPositiveNumber(object, "spacing")) {
        pattern.spacing = spacing.value();
    }
    if (object.value(QStringLiteral("stroke_angle")).isDouble()) {
        pattern.angle = object.value(QStringLiteral("stroke_angle")).toDouble(pattern.angle);
    } else if (object.value(QStringLiteral("angle")).isDouble()) {
        pattern.angle = object.value(QStringLiteral("angle")).toDouble(pattern.angle);
    }
    if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(object, "stroke_width")) {
        pattern.strokeWidth = strokeWidth.value();
    }
    if (object.value(QStringLiteral("stroke_style")).isString()) {
        pattern.strokeStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
    }
    pattern.strokeColor = optionalColor(object, "stroke_color");
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
        pattern.dashPattern = dashPattern.value();
    }
    const std::optional<qreal> explicitSize = optionalPositiveNumber(object, "size");
    if (explicitSize.has_value()) {
        pattern.size = explicitSize.value();
    }
    if (const std::optional<qreal> sizeJitter = optionalNonNegativeNumber(object, "size_jitter")) {
        pattern.sizeJitter = sizeJitter.value();
    }
    if (const std::optional<QVector<MapEditorPointSymbolPart>> symbolParts = readPointSymbolParts(object)) {
        pattern.symbolParts = symbolParts.value();
    }
    if (!explicitSize.has_value()) {
        pattern.size = symbolPartsNaturalSize(pattern.symbolParts).value_or(pattern.size);
    }
    if (const std::optional<qreal> angleJitter = optionalNonNegativeNumber(object, "angle_jitter")) {
        pattern.angleJitter = angleJitter.value();
    }
    if (const std::optional<qreal> offsetJitter = optionalNonNegativeNumber(object, "offset_jitter")) {
        pattern.offsetJitter = offsetJitter.value();
    }
    if (const std::optional<int> seed = optionalInteger(object, "seed")) {
        pattern.seed = seed;
    }

    return pattern;
}

MapEditorStyleSelector readSelector(const QJsonObject &object)
{
    MapEditorStyleSelector selector;
    selector.rawType = normalizedToken(object.value(QStringLiteral("raw_type")).toString());
    selector.subtype = normalizedToken(object.value(QStringLiteral("raw_subtype")).toString());
    return selector;
}

bool selectorMatches(const MapEditorStyleSelector &selector,
                    const QString &rawType,
                    const QString &subtype)
{
    if (selector.rawType.isEmpty()) {
        return false;
    }
    if (selector.rawType != rawType) {
        return false;
    }
    if (selector.subtype.isEmpty()) {
        return true;
    }
    return selector.subtype == subtype;
}

void applyPointDefaults(MapEditorObjectStyleCatalog *catalog, const QJsonObject &point)
{
    if (catalog == nullptr || point.isEmpty()) {
        return;
    }

    if (const std::optional<QVector<MapEditorPointSymbolPart>> symbolParts = readPointSymbolParts(point)) {
        catalog->point.symbolParts = symbolParts.value();
    }
    if (const std::optional<qreal> size = optionalPositiveNumber(point, "size")) {
        catalog->point.size = size.value();
    }
    if (const std::optional<qreal> outlineWidth = optionalPositiveNumber(point, "stroke_width")) {
        catalog->point.outlineWidth = outlineWidth.value();
    }
    if (const std::optional<QColor> fillColor = optionalColor(point, "fill_color")) {
        catalog->point.fillColor = fillColor;
    }
    if (const std::optional<QColor> strokeColor = optionalColor(point, "stroke_color")) {
        catalog->point.strokeColor = strokeColor;
    }
    if (const std::optional<QString> labelField = optionalFieldName(point, "label_field")) {
        catalog->point.labelField = labelField;
    }
    if (const std::optional<qreal> labelFontSize = optionalPositiveNumber(point, "label_font_size")) {
        catalog->point.labelFontSize = labelFontSize;
    }
    if (const std::optional<MapEditorPointLabelOrientationMode> labelOrientation =
            optionalPointLabelOrientation(point, "label_orientation")) {
        catalog->point.labelOrientation = labelOrientation.value();
    }
}

void applyLineDefaults(MapEditorObjectStyleCatalog *catalog, const QJsonObject &line)
{
    if (catalog == nullptr || line.isEmpty()) {
        return;
    }

    if (const std::optional<bool> strokeVisible = optionalBool(line, "stroke_visible")) {
        catalog->line.strokeVisible = strokeVisible.value();
    }
    if (const std::optional<bool> guideSpineVisible = optionalBool(line, "guide_spine_visible")) {
        catalog->line.guideSpineVisible = guideSpineVisible.value();
    }
    if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(line, "stroke_width")) {
        catalog->line.strokeWidth = strokeWidth.value();
    }
    if (line.value(QStringLiteral("stroke_style")).isString()) {
        catalog->line.penStyle = penStyleFromString(line.value(QStringLiteral("stroke_style")).toString());
    }
    if (const std::optional<QColor> strokeColor = optionalColor(line, "stroke_color")) {
        catalog->line.strokeColor = strokeColor;
    }
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(line, "dash_pattern")) {
        catalog->line.dashPattern = dashPattern.value();
    }
    if (const std::optional<QVector<MapEditorLineDecorationStyle>> decorations = readLineDecorations(line)) {
        catalog->line.decorations = decorations.value();
    }
    if (const std::optional<MapEditorLineClosedFillStyle> closedFill = optionalLineClosedFill(line, "closed_fill")) {
        catalog->line.closedFill = closedFill.value();
    }
    if (const std::optional<QString> labelField = optionalFieldName(line, "label_field")) {
        catalog->line.labelField = labelField;
    }
}

void applyAreaDefaults(MapEditorObjectStyleCatalog *catalog, const QJsonObject &area)
{
    if (catalog == nullptr || area.isEmpty()) {
        return;
    }

    if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(area, "stroke_width")) {
        catalog->area.strokeWidth = strokeWidth.value();
    }
    if (const std::optional<qreal> fillOpacity = optionalZeroToOneNumber(area, "fill_opacity")) {
        catalog->area.fillOpacity = fillOpacity.value();
    }
    if (area.value(QStringLiteral("stroke_style")).isString()) {
        catalog->area.penStyle = penStyleFromString(area.value(QStringLiteral("stroke_style")).toString());
    }
    if (const std::optional<QColor> strokeColor = optionalColor(area, "stroke_color")) {
        catalog->area.strokeColor = strokeColor;
    }
    if (const std::optional<QColor> fillColor = optionalColor(area, "fill_color")) {
        catalog->area.fillColor = fillColor;
    }
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(area, "dash_pattern")) {
        catalog->area.dashPattern = dashPattern.value();
    }
    if (area.contains(QStringLiteral("fill_pattern"))) {
        catalog->area.fillPattern = readAreaFillPattern(area.value(QStringLiteral("fill_pattern")).toObject());
    }
}

std::optional<MapEditorPointStyleRule> readPointStyleRule(const QJsonObject &object)
{
    MapEditorPointStyleRule rule;
    rule.selector = readSelector(object);
    if (rule.selector.rawType.isEmpty()) {
        return std::nullopt;
    }

    rule.symbolParts = readPointSymbolParts(object);
    rule.size = optionalPositiveNumber(object, "size");
    rule.outlineWidth = optionalPositiveNumber(object, "stroke_width");
    rule.fillColor = optionalColor(object, "fill_color");
    rule.strokeColor = optionalColor(object, "stroke_color");
    rule.labelField = optionalFieldName(object, "label_field");
    rule.labelFontSize = optionalPositiveNumber(object, "label_font_size");
    rule.labelOrientation = optionalPointLabelOrientation(object, "label_orientation");
    return rule;
}

std::optional<MapEditorLineStyleRule> readLineStyleRule(const QJsonObject &object)
{
    MapEditorLineStyleRule rule;
    rule.selector = readSelector(object);
    if (rule.selector.rawType.isEmpty()) {
        return std::nullopt;
    }

    rule.strokeVisible = optionalBool(object, "stroke_visible");
    rule.guideSpineVisible = optionalBool(object, "guide_spine_visible");
    rule.strokeWidth = optionalPositiveNumber(object, "stroke_width");
    if (object.value(QStringLiteral("stroke_style")).isString()) {
        rule.penStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
    }
    rule.strokeColor = optionalColor(object, "stroke_color");
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
        rule.dashPattern = dashPattern;
    }
    rule.decorations = readLineDecorations(object);
    rule.closedFill = optionalLineClosedFill(object, "closed_fill");
    rule.labelField = optionalFieldName(object, "label_field");
    return rule;
}

std::optional<MapEditorAreaStyleRule> readAreaStyleRule(const QJsonObject &object)
{
    MapEditorAreaStyleRule rule;
    rule.selector = readSelector(object);
    if (rule.selector.rawType.isEmpty()) {
        return std::nullopt;
    }

    rule.strokeWidth = optionalPositiveNumber(object, "stroke_width");
    rule.fillOpacity = optionalZeroToOneNumber(object, "fill_opacity");
    if (object.value(QStringLiteral("stroke_style")).isString()) {
        rule.penStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
    }

    rule.strokeColor = optionalColor(object, "stroke_color");
    rule.fillColor = optionalColor(object, "fill_color");
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
        rule.dashPattern = dashPattern;
    }
    rule.fillPattern = readAreaFillPattern(object.value(QStringLiteral("fill_pattern")).toObject());
    return rule;
}

std::optional<QJsonObject> jsonObjectFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        return std::nullopt;
    }
    return json.object();
}

bool applySplitStyleFile(MapEditorObjectStyleCatalog *catalog, const QFileInfo &fileInfo)
{
    if (catalog == nullptr || !fileInfo.isFile()) {
        return false;
    }

    const std::optional<QJsonObject> jsonObject = jsonObjectFromFile(fileInfo.absoluteFilePath());
    if (!jsonObject.has_value()) {
        return false;
    }

    const QStringList nameParts =
        fileInfo.completeBaseName().split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (nameParts.isEmpty()) {
        return false;
    }

    const QString kind = normalizedToken(nameParts.at(0));
    if (kind != QStringLiteral("point") && kind != QStringLiteral("line") && kind != QStringLiteral("area")) {
        return false;
    }

    if (nameParts.size() == 1) {
        if (kind == QStringLiteral("point")) {
            applyPointDefaults(catalog, jsonObject.value());
        } else if (kind == QStringLiteral("line")) {
            applyLineDefaults(catalog, jsonObject.value());
        } else {
            applyAreaDefaults(catalog, jsonObject.value());
        }
        return true;
    }

    QJsonObject object = jsonObject.value();
    object.insert(QStringLiteral("raw_type"), normalizedToken(nameParts.at(1)));
    if (nameParts.size() > 2) {
        object.insert(QStringLiteral("raw_subtype"), normalizedToken(nameParts.mid(2).join(QLatin1Char('.'))));
    }

    if (kind == QStringLiteral("point")) {
        if (const std::optional<MapEditorPointStyleRule> rule = readPointStyleRule(object)) {
            catalog->pointStyles.append(rule.value());
            return true;
        }
    } else if (kind == QStringLiteral("line")) {
        if (const std::optional<MapEditorLineStyleRule> rule = readLineStyleRule(object)) {
            catalog->lineStyles.append(rule.value());
            return true;
        }
    } else if (const std::optional<MapEditorAreaStyleRule> rule = readAreaStyleRule(object)) {
        catalog->areaStyles.append(rule.value());
        return true;
    }

    return false;
}

int splitStyleOverrideScopeRank(const QFileInfo &fileInfo)
{
    const QStringList nameParts =
        fileInfo.completeBaseName().split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (nameParts.size() <= 1) {
        return 0;
    }
    if (nameParts.size() == 2) {
        return 1;
    }
    return 2;
}

bool applySplitStyleDirectory(MapEditorObjectStyleCatalog *catalog, const QString &directoryPath)
{
    if (catalog == nullptr || directoryPath.isEmpty()) {
        return false;
    }

    const QDir directory(directoryPath);
    if (!directory.exists()) {
        return false;
    }

    bool loaded = false;
    QFileInfoList files = directory.entryInfoList({QStringLiteral("*.json")},
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Name);
    std::sort(files.begin(), files.end(), [](const QFileInfo &left, const QFileInfo &right) {
        const int leftRank = splitStyleOverrideScopeRank(left);
        const int rightRank = splitStyleOverrideScopeRank(right);
        if (leftRank != rightRank) {
            return leftRank < rightRank;
        }
        return left.fileName() < right.fileName();
    });
    for (const QFileInfo &fileInfo : files) {
        loaded = applySplitStyleFile(catalog, fileInfo) || loaded;
    }
    return loaded;
}

} // namespace

QString mapEditorUserObjectStylesDirectory()
{
    const QString environmentOverride = QProcessEnvironment::systemEnvironment()
        .value(QStringLiteral("THERION_STUDIO_MAP_OBJECT_STYLES_DIR"))
        .trimmed();
    if (!environmentOverride.isEmpty()) {
        return environmentOverride;
    }

    const QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appDataLocation.isEmpty()
        ? QString()
        : QDir(appDataLocation).filePath(QStringLiteral("map_object_styles"));
}

MapEditorObjectStyleCatalog loadMapEditorObjectStyleCatalog(const QString &userOverrideDirectory)
{
    MapEditorObjectStyleCatalog catalog;
    catalog.loadedFromResource =
        applySplitStyleDirectory(&catalog, QStringLiteral(":/resources/map_object_styles"));
    catalog.loadedUserOverrides = applySplitStyleDirectory(&catalog, userOverrideDirectory);
    return catalog;
}

MapEditorObjectStyleCatalog loadDefaultMapEditorObjectStyleCatalog()
{
    return loadMapEditorObjectStyleCatalog(mapEditorUserObjectStylesDirectory());
}

MapEditorResolvedPointStyle resolveMapEditorPointStyle(const MapEditorObjectStyleCatalog &catalog,
                                                       const QString &rawType,
                                                       const QString &subtype)
{
    MapEditorResolvedPointStyle resolved;
    resolved.symbol = catalog.point.symbol;
    resolved.symbolParts = catalog.point.symbolParts;
    resolved.size = catalog.point.size;
    resolved.outlineWidth = catalog.point.outlineWidth;
    resolved.fillColor = catalog.point.fillColor;
    resolved.strokeColor = catalog.point.strokeColor;
    resolved.labelField = catalog.point.labelField;
    resolved.labelFontSize = catalog.point.labelFontSize;
    resolved.labelOrientation = catalog.point.labelOrientation;

    const QString normalizedRawType = normalizedToken(rawType);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorPointStyleRule &rule : catalog.pointStyles) {
        if (!selectorMatches(rule.selector, normalizedRawType, normalizedSubtype)) {
            continue;
        }
        if (rule.symbolParts.has_value()) {
            resolved.symbolParts = rule.symbolParts.value();
        }
        if (rule.size.has_value()) {
            resolved.size = rule.size.value();
        }
        if (rule.outlineWidth.has_value()) {
            resolved.outlineWidth = rule.outlineWidth.value();
        }
        if (rule.fillColor.has_value()) {
            resolved.fillColor = rule.fillColor;
        }
        if (rule.strokeColor.has_value()) {
            resolved.strokeColor = rule.strokeColor;
        }
        if (rule.labelField.has_value()) {
            resolved.labelField = rule.labelField;
        }
        if (rule.labelFontSize.has_value()) {
            resolved.labelFontSize = rule.labelFontSize;
        }
        if (rule.labelOrientation.has_value()) {
            resolved.labelOrientation = rule.labelOrientation.value();
        }
    }

    return resolved;
}

MapEditorResolvedLineStyle resolveMapEditorLineStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype)
{
    MapEditorResolvedLineStyle resolved;
    resolved.strokeVisible = catalog.line.strokeVisible;
    resolved.guideSpineVisible = catalog.line.guideSpineVisible;
    resolved.strokeWidth = catalog.line.strokeWidth;
    resolved.penStyle = catalog.line.penStyle;
    resolved.strokeColor = catalog.line.strokeColor;
    resolved.dashPattern = catalog.line.dashPattern;
    resolved.decorations = catalog.line.decorations;
    resolved.closedFill = catalog.line.closedFill;
    resolved.labelField = catalog.line.labelField;

    const QString normalizedRawType = normalizedToken(rawType);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorLineStyleRule &rule : catalog.lineStyles) {
        if (!selectorMatches(rule.selector, normalizedRawType, normalizedSubtype)) {
            continue;
        }
        if (rule.strokeVisible.has_value()) {
            resolved.strokeVisible = rule.strokeVisible.value();
        }
        if (rule.guideSpineVisible.has_value()) {
            resolved.guideSpineVisible = rule.guideSpineVisible.value();
        }
        if (rule.strokeWidth.has_value()) {
            resolved.strokeWidth = rule.strokeWidth.value();
        }
        if (rule.penStyle.has_value()) {
            resolved.penStyle = rule.penStyle.value();
        }
        if (rule.strokeColor.has_value()) {
            resolved.strokeColor = rule.strokeColor;
        }
        if (rule.dashPattern.has_value()) {
            resolved.dashPattern = rule.dashPattern.value();
        }
        if (rule.decorations.has_value()) {
            resolved.decorations = rule.decorations.value();
        }
        if (rule.closedFill.has_value()) {
            resolved.closedFill = rule.closedFill.value();
        }
        if (rule.labelField.has_value()) {
            resolved.labelField = rule.labelField;
        }
    }

    return resolved;
}

MapEditorResolvedAreaStyle resolveMapEditorAreaStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype)
{
    MapEditorResolvedAreaStyle resolved;
    resolved.strokeWidth = catalog.area.strokeWidth;
    resolved.fillOpacity = catalog.area.fillOpacity;
    resolved.penStyle = catalog.area.penStyle;
    resolved.strokeColor = catalog.area.strokeColor;
    resolved.fillColor = catalog.area.fillColor;
    resolved.dashPattern = catalog.area.dashPattern;
    resolved.fillPattern = catalog.area.fillPattern;

    const QString normalizedRawType = normalizedToken(rawType);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorAreaStyleRule &rule : catalog.areaStyles) {
        if (!selectorMatches(rule.selector, normalizedRawType, normalizedSubtype)) {
            continue;
        }
        if (rule.strokeWidth.has_value()) {
            resolved.strokeWidth = rule.strokeWidth.value();
        }
        if (rule.fillOpacity.has_value()) {
            resolved.fillOpacity = rule.fillOpacity.value();
        }
        if (rule.penStyle.has_value()) {
            resolved.penStyle = rule.penStyle.value();
        }
        if (rule.strokeColor.has_value()) {
            resolved.strokeColor = rule.strokeColor;
        }
        if (rule.fillColor.has_value()) {
            resolved.fillColor = rule.fillColor;
        }
        if (rule.dashPattern.has_value()) {
            resolved.dashPattern = rule.dashPattern.value();
        }
        if (rule.fillPattern.has_value()) {
            resolved.fillPattern = rule.fillPattern;
        }
    }

    return resolved;
}
} // namespace TherionStudio
