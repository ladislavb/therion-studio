#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPair>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int writeJsonFixtureFile(const QString &filePath, const QByteArray &content)
{
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create user style override test file.")) {
        return 1;
    }
    file.write(content);
    file.close();
    return 0;
}

int prepareUserOverrideFixture(QTemporaryDir *temporaryDir)
{
    if (!expect(temporaryDir != nullptr && temporaryDir->isValid(),
                "Expected valid temporary directory for user style overrides.")) {
        return 1;
    }

    const QString overrideDirectoryPath = temporaryDir->filePath(QStringLiteral("map_object_styles"));
    if (!expect(!overrideDirectoryPath.isEmpty(),
                "Expected non-empty user style override directory path.")) {
        return 1;
    }

    if (!expect(QDir().mkpath(overrideDirectoryPath),
                "Failed to create user style override test directory.")) {
        return 1;
    }

    const QDir overrideDirectory(overrideDirectoryPath);
    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("area.water.json")),
                                 R"json({
  "stroke_width": 4.25,
  "fill_opacity": 0.37
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("line.override.json")),
                                 R"json({
  "stroke_width": 5.0
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("line.override.aaa.json")),
                                 R"json({
  "stroke_width": 1.25
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("line.decorated.json")),
                                 R"json({
  "stroke_visible": false,
  "decorations": [
    {
      "kind": "symbols",
      "side": "center",
      "spacing": 11,
      "size": 7.5,
      "size_jitter": 2.0,
      "angle_jitter": 25.0,
      "stroke_color": "#112233",
      "fill_color": "#ffffff",
      "stroke_width": 1.1,
      "symbol_parts": [
        { "kind": "oval", "size": 7.5 }
      ]
    },
    {
      "kind": "rungs",
      "from_offset": -4,
      "to_offset": 4,
      "spacing": 9,
      "stroke_width": 0.9
    }
  ]
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("point.override.json")),
                                 R"json({
  "size": 13.0,
  "label_field": "-id",
  "symbol_parts": [
    { "kind": "x", "size": 13.0 }
  ]
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("area.symbols.json")),
                                 R"json({
  "fill_pattern": {
    "kind": "dots",
    "placement": "scatter",
    "size_jitter": 1.25,
    "angle_jitter": 17.5,
    "symbol_parts": [
      { "kind": "oval", "size": 5.5, "fill_color": "#223344" }
    ]
  }
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("point.oval.json")),
                                 R"json({
  "size": 14.0,
  "symbol_parts": [
    { "kind": "oval", "size": 14.0 }
  ]
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("point.parts.json")),
                                 R"json({
  "size": 16.0,
  "symbol_parts": [
    { "kind": "line", "x1": -5.0, "y1": 0.0, "x2": 5.0, "y2": 0.0 },
    { "kind": "polyline", "points": [[-3.0, -3.0], [0.0, 0.0], [3.0, -3.0]] },
    { "kind": "polygon", "points": [[-3.0, 3.0], [3.0, 3.0], [0.0, 6.0]], "fill": true, "fill_color": "#335577" },
    { "kind": "ellipse", "x": 0.0, "y": -5.0, "width": 5.0, "height": 2.0, "angle": 20.0, "fill": true, "stroke_color": "#884422", "stroke_width": 0.7 }
  ]
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("line.parts.json")),
                                 R"json({
  "decorations": [
    {
      "kind": "symbols",
      "size": 8.0,
      "symbol_parts": [
        { "kind": "line", "x1": -4.0, "y1": 0.0, "x2": 4.0, "y2": 0.0 }
      ]
    }
  ]
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("area.parts.json")),
                                 R"json({
  "fill_pattern": {
    "kind": "dots",
    "symbol_parts": [
      { "kind": "polygon", "points": [[-3.0, -3.0], [3.0, -3.0], [0.0, 3.0]], "fill": true, "fill_color": "#334455" }
    ]
  }
})json");
        result != 0) {
        return 1;
    }

    const QVector<QPair<QString, QString>> pointSymbolFiles = {
        {QStringLiteral("circle"), QStringLiteral("circle")},
        {QStringLiteral("square"), QStringLiteral("square")},
        {QStringLiteral("diamond"), QStringLiteral("diamond")},
        {QStringLiteral("triangle"), QStringLiteral("triangle")},
        {QStringLiteral("star"), QStringLiteral("star")},
        {QStringLiteral("asterisk"), QStringLiteral("asterisk")},
        {QStringLiteral("plus"), QStringLiteral("plus")},
        {QStringLiteral("x"), QStringLiteral("x")}
    };
    for (const QPair<QString, QString> &entry : pointSymbolFiles) {
        const QByteArray content = QByteArray("{\n  \"symbol_parts\": [\n    { \"kind\": \"")
            + entry.second.toUtf8()
            + QByteArray("\", \"size\": 11.2 }\n  ]\n}\n");
        if (const int result =
                writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("point.%1.json").arg(entry.first)),
                                     content);
            result != 0) {
            return 1;
        }
    }

    return 0;
}

int runCatalogTest(const QString &overrideDirectoryPath)
{
    const MapEditorObjectStyleCatalog catalog = loadMapEditorObjectStyleCatalog(overrideDirectoryPath);
    if (!expect(catalog.loadedFromResource,
                "Expected map object style catalog to load from resource JSON.")) {
        return 1;
    }
    if (!expect(catalog.loadedUserOverrides,
                "Expected map object style catalog to load user override JSON.")) {
        return 1;
    }

    if (!expect(catalog.point.symbolParts.size() == 1
                    && catalog.point.symbolParts.first().symbol == MapEditorPointSymbol::Circle,
                "Expected point default symbol part from resource catalog.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.point.size - 7.2) < 1e-6,
                "Expected point default size from resource catalog.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.line.strokeWidth - 2.5) < 1e-6,
                "Expected line default stroke width from resource catalog.")) {
        return 1;
    }
    if (!expect(catalog.line.penStyle == Qt::SolidLine,
                "Expected line default pen style to resolve as solid.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.area.strokeWidth - 2.0) < 1e-6,
                "Expected area default stroke width from resource catalog.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.area.fillOpacity - 0.11) < 1e-6,
                "Expected area default fill opacity from resource catalog.")) {
        return 1;
    }
    if (!expect(catalog.area.penStyle == Qt::DashLine,
                "Expected area default pen style to resolve as dashed.")) {
        return 1;
    }
    if (!expect(!catalog.lineStyles.isEmpty(),
                "Expected type-specific line style rules to load from resource catalog.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle wallStyle = resolveMapEditorLineStyle(catalog, QStringLiteral("wall"));
    if (!expect(std::abs(wallStyle.strokeWidth - 3.0) < 1e-6,
                "Expected line style override width for wall.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle slopeStyle = resolveMapEditorLineStyle(catalog, QStringLiteral("slope"));
    if (!expect(slopeStyle.decorations.size() == 1
                    && slopeStyle.decorations.first().kind == MapEditorLineDecorationKind::SlopeTicks,
                "Expected slope line style to render l-size/orientation-aware ticks.")) {
        return 1;
    }
    if (!expect(std::abs(slopeStyle.decorations.first().length - 18.0) < 1e-6,
                "Expected slope tick default length to parse from default_length.")) {
        return 1;
    }
    if (!expect(slopeStyle.decorations.first().adjustSpacing,
                "Expected slope tick adjusted spacing flag to parse.")) {
        return 1;
    }
    if (!expect(std::abs(slopeStyle.decorations.first().spacingDivisor - 2.0) < 1e-6,
                "Expected slope tick spacing divisor to parse.")) {
        return 1;
    }
    if (!expect(std::abs(slopeStyle.decorations.first().alternateLengthScale - 0.333) < 1e-6,
                "Expected slope tick alternate length scale to parse.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle wallPebblesStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("wall"), QStringLiteral("pebbles"));
    if (!expect(!wallPebblesStyle.strokeVisible,
                "Expected wall pebbles line style to hide the base stroke.")) {
        return 1;
    }
    if (!expect(wallPebblesStyle.decorations.size() == 1
                    && wallPebblesStyle.decorations.first().symbolParts.size() == 1
                    && wallPebblesStyle.decorations.first().symbolParts.first().symbol == MapEditorPointSymbol::Oval,
                "Expected wall pebbles line style to render oval symbol part decorations.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle wallSandStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("wall"), QStringLiteral("sand"));
    if (!expect(!wallSandStyle.decorations.isEmpty(),
                "Expected wall sand line style to render repeated material dots.")) {
        return 1;
    }
    for (const MapEditorLineDecorationStyle &decoration : wallSandStyle.decorations) {
        if (!expect(decoration.side == MapEditorLineDecorationSide::Right,
                    "Expected SKBB wall sand decorations to stay on the Therion right side.")) {
            return 1;
        }
    }

    const MapEditorResolvedLineStyle wallClayStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("wall"), QStringLiteral("clay"));
    if (!expect(wallClayStyle.decorations.size() == 1
                    && wallClayStyle.decorations.first().side == MapEditorLineDecorationSide::Right,
                "Expected SKBB wall clay decorations to stay on the Therion right side.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle wallBlocksStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("wall"), QStringLiteral("blocks"));
    if (!expect(wallBlocksStyle.decorations.size() == 1
                    && wallBlocksStyle.decorations.first().side == MapEditorLineDecorationSide::Center
                    && wallBlocksStyle.decorations.first().symbolParts.size() == 1
                    && wallBlocksStyle.decorations.first().symbolParts.first().symbol == MapEditorPointSymbol::Square,
                "Expected wall blocks line style to render centered square decorations.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle wallIceStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("wall"), QStringLiteral("ice"));
    if (!expect(wallIceStyle.decorations.size() == 1
                    && wallIceStyle.decorations.first().kind == MapEditorLineDecorationKind::Symbols
                    && wallIceStyle.decorations.first().symbolParts.size() == 1
                    && wallIceStyle.decorations.first().symbolParts.first().symbol == MapEditorPointSymbol::Plus,
                "Expected wall ice style to render repeated plus symbol parts.")) {
        return 1;
    }
    if (!expect(wallIceStyle.decorations.first().adjustSpacing,
                "Expected wall ice repeated symbols to use adjusted spacing.")) {
        return 1;
    }
    if (!expect(wallIceStyle.decorations.first().side == MapEditorLineDecorationSide::Right,
                "Expected SKBB wall ice decorations to stay on the Therion right side.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle pitStyle = resolveMapEditorLineStyle(catalog, QStringLiteral("pit"));
    if (!expect(pitStyle.decorations.size() == 1
                    && pitStyle.decorations.first().kind == MapEditorLineDecorationKind::Ticks,
                "Expected pit line style to render repeated ticks.")) {
        return 1;
    }
    if (!expect(pitStyle.decorations.first().side == MapEditorLineDecorationSide::Left,
                "Expected pit ticks to stay on the Therion left side.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle ceilingStepStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("ceiling-step"));
    if (!expect(ceilingStepStyle.decorations.size() == 1
                    && ceilingStepStyle.decorations.first().kind == MapEditorLineDecorationKind::Ticks,
                "Expected ceiling-step line style to render repeated ticks.")) {
        return 1;
    }
    if (!expect(ceilingStepStyle.decorations.first().side == MapEditorLineDecorationSide::Left,
                "Expected SKBB ceiling-step ticks to stay on the Therion left side.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle overhangStyle = resolveMapEditorLineStyle(catalog, QStringLiteral("overhang"));
    if (!expect(overhangStyle.decorations.size() == 1
                    && overhangStyle.decorations.first().kind == MapEditorLineDecorationKind::Teeth,
                "Expected overhang line style to render repeated teeth.")) {
        return 1;
    }
    if (!expect(std::abs(overhangStyle.decorations.first().size - 3.25) < 1e-6
                    && std::abs(overhangStyle.decorations.first().spacing - 4.5) < 1e-6,
                "Expected overhang teeth to use the reduced half-size motif.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle ropeLadderStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("rope-ladder"));
    if (!expect(!ropeLadderStyle.strokeVisible && ropeLadderStyle.decorations.size() == 2,
                "Expected rope-ladder line style to use parallel stroke and rung decorations.")) {
        return 1;
    }
    if (!expect(ropeLadderStyle.decorations.first().kind == MapEditorLineDecorationKind::Parallel
                    && ropeLadderStyle.decorations.at(1).kind == MapEditorLineDecorationKind::Rungs,
                "Expected rope-ladder line decorations to preserve declaration order.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle borderStyle = resolveMapEditorLineStyle(catalog, QStringLiteral("border"));
    if (!expect(borderStyle.penStyle == Qt::SolidLine && borderStyle.dashPattern.isEmpty(),
                "Expected default border line style to be solid.")) {
        return 1;
    }
    const MapEditorResolvedLineStyle labelLineStyle = resolveMapEditorLineStyle(catalog, QStringLiteral("label"));
    if (!expect(labelLineStyle.labelField.has_value()
                    && labelLineStyle.labelField.value() == QStringLiteral("text"),
                "Expected line label style to expose -text as its label field.")) {
        return 1;
    }
    if (!expect(labelLineStyle.strokeColor.has_value()
                    && labelLineStyle.strokeColor->alpha() < 255
                    && labelLineStyle.strokeWidth < 1.0,
                "Expected line label geometry to render as a subtle guide line.")) {
        return 1;
    }
    const MapEditorResolvedLineStyle rockBorderStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("rock-border"));
    if (!expect(rockBorderStyle.closedFill.mode == MapEditorLineClosedFillMode::Background,
                "Expected closed rock-border lines to clean-fill with the map background.")) {
        return 1;
    }
    const MapEditorResolvedLineStyle temporaryBorderStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("border"), QStringLiteral("temporary"));
    if (!expect(temporaryBorderStyle.penStyle == Qt::DashLine && !temporaryBorderStyle.dashPattern.isEmpty(),
                "Expected temporary border line style to be dashed.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle overrideSubtypeStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("override"), QStringLiteral("aaa"));
    if (!expect(std::abs(overrideSubtypeStyle.strokeWidth - 1.25) < 1e-6,
                "Expected subtype user override to take precedence over type override.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle decoratedLineStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("decorated"));
    if (!expect(!decoratedLineStyle.strokeVisible,
                "Expected line style stroke_visible override to load.")) {
        return 1;
    }
    if (!expect(decoratedLineStyle.decorations.size() == 2,
                "Expected line style decorations to load from override.")) {
        return 1;
    }
    if (!expect(decoratedLineStyle.decorations.first().kind == MapEditorLineDecorationKind::Symbols
                    && decoratedLineStyle.decorations.first().symbolParts.size() == 1
                    && decoratedLineStyle.decorations.first().symbolParts.first().symbol == MapEditorPointSymbol::Oval,
                "Expected oval symbol part line decoration to parse.")) {
        return 1;
    }
    if (!expect(decoratedLineStyle.decorations.first().fillColor.has_value(),
                "Expected line symbol decoration fill color to parse.")) {
        return 1;
    }
    if (!expect(decoratedLineStyle.decorations.at(1).kind == MapEditorLineDecorationKind::Rungs
                    && decoratedLineStyle.decorations.at(1).fromOffset.has_value()
                    && decoratedLineStyle.decorations.at(1).toOffset.has_value(),
                "Expected rung line decoration offsets to parse.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle partsLineStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("parts"));
    if (!expect(partsLineStyle.decorations.size() == 1
                    && partsLineStyle.decorations.first().symbolParts.size() == 1
                    && partsLineStyle.decorations.first().symbolParts.first().kind == MapEditorSymbolPartKind::Line,
                "Expected line decorations to parse symbol_parts primitives.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle stationStyle = resolveMapEditorPointStyle(catalog, QStringLiteral("station"));
    if (!expect(stationStyle.fillColor.has_value(),
                "Expected station point fill color override.")) {
        return 1;
    }
    if (!expect(stationStyle.symbolParts.size() == 1
                    && stationStyle.symbolParts.first().symbol == MapEditorPointSymbol::Triangle,
                "Expected station point symbol part override.")) {
        return 1;
    }
    if (!expect(stationStyle.labelField.has_value()
                    && stationStyle.labelField.value() == QStringLiteral("name"),
                "Expected station point label field override.")) {
        return 1;
    }
    if (!expect(std::abs(stationStyle.size - 8.4) < 1e-6,
                "Expected station point size override.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle labelStyle = resolveMapEditorPointStyle(catalog, QStringLiteral("label"));
    if (!expect(labelStyle.labelField.has_value()
                    && labelStyle.labelField.value() == QStringLiteral("text"),
                "Expected label point label field override.")) {
        return 1;
    }
    if (!expect(labelStyle.labelOrientation == MapEditorPointLabelOrientationMode::Orientation,
                "Expected label point text to use orientation-driven rendering.")) {
        return 1;
    }
    if (!expect(labelStyle.labelFontSize.has_value()
                    && std::abs(labelStyle.labelFontSize.value() - 8.5) < 1e-6,
                "Expected label point text to use the configured smaller font size.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle debrisPointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("debris"));
    if (!expect(debrisPointStyle.symbolParts.size() == 3,
                "Expected debris point style to use three symbol parts.")) {
        return 1;
    }
    if (!expect(debrisPointStyle.symbolParts.first().symbol == MapEditorPointSymbol::Triangle,
                "Expected debris point symbol parts to use triangles.")) {
        return 1;
    }
    if (!expect(debrisPointStyle.fillColor.has_value() && debrisPointStyle.fillColor->alpha() == 0,
                "Expected debris point symbol parts to render without fill.")) {
        return 1;
    }
    if (!expect(std::abs(debrisPointStyle.size - 15.5) < 1e-6
                    && std::abs(debrisPointStyle.symbolParts.first().angle - 28.0) < 1e-6,
                "Expected debris point style to use the tuned smaller irregular triangle motif.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle pebblesPointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("pebbles"));
    if (!expect(pebblesPointStyle.symbolParts.size() == 3,
                "Expected pebbles point style to use three symbol parts.")) {
        return 1;
    }
    if (!expect(pebblesPointStyle.symbolParts.first().symbol == MapEditorPointSymbol::Oval,
                "Expected pebbles point symbol parts to support oval.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle sandPointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("sand"));
    if (!expect(sandPointStyle.symbolParts.size() == 3
                    && sandPointStyle.symbolParts.first().symbol == MapEditorPointSymbol::Circle,
                "Expected sand point style to use three dot parts.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle blocksPointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("blocks"));
    if (!expect(blocksPointStyle.symbolParts.size() == 3,
                "Expected blocks point style to use composite symbol parts.")) {
        return 1;
    }
    if (!expect(blocksPointStyle.size > debrisPointStyle.size
                    && blocksPointStyle.symbolParts.first().size > debrisPointStyle.symbolParts.first().size,
                "Expected blocks point style to render larger than debris point style.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle partsPointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("parts"));
    if (!expect(partsPointStyle.symbolParts.size() == 4
                    && partsPointStyle.symbolParts.at(0).kind == MapEditorSymbolPartKind::Line
                    && partsPointStyle.symbolParts.at(1).kind == MapEditorSymbolPartKind::Polyline
                    && partsPointStyle.symbolParts.at(2).kind == MapEditorSymbolPartKind::Polygon
                    && partsPointStyle.symbolParts.at(3).kind == MapEditorSymbolPartKind::Ellipse,
                "Expected point symbol_parts primitive kinds to parse.")) {
        return 1;
    }
    if (!expect(partsPointStyle.symbolParts.at(2).fill && partsPointStyle.symbolParts.at(3).fill,
                "Expected fill flag for polygon and ellipse symbol parts to parse.")) {
        return 1;
    }
    if (!expect(partsPointStyle.symbolParts.at(2).fillColor.has_value()
                    && partsPointStyle.symbolParts.at(3).strokeColor.has_value()
                    && partsPointStyle.symbolParts.at(3).strokeWidth.has_value(),
                "Expected symbol part fill, stroke, and stroke width overrides to parse.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle overridePointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("override"));
    if (!expect(overridePointStyle.symbolParts.size() == 1
                    && overridePointStyle.symbolParts.first().symbol == MapEditorPointSymbol::X,
                "Expected user override point symbol part.")) {
        return 1;
    }
    if (!expect(std::abs(overridePointStyle.size - 13.0) < 1e-6,
                "Expected user override point size.")) {
        return 1;
    }
    if (!expect(overridePointStyle.labelField.has_value()
                    && overridePointStyle.labelField.value() == QStringLiteral("id"),
                "Expected user override point label field to normalize without leading dash.")) {
        return 1;
    }

    const QVector<QPair<QString, MapEditorPointSymbol>> symbolCases = {
        {QStringLiteral("circle"), MapEditorPointSymbol::Circle},
        {QStringLiteral("square"), MapEditorPointSymbol::Square},
        {QStringLiteral("diamond"), MapEditorPointSymbol::Diamond},
        {QStringLiteral("triangle"), MapEditorPointSymbol::Triangle},
        {QStringLiteral("star"), MapEditorPointSymbol::Star},
        {QStringLiteral("asterisk"), MapEditorPointSymbol::Asterisk},
        {QStringLiteral("plus"), MapEditorPointSymbol::Plus},
        {QStringLiteral("x"), MapEditorPointSymbol::X}
    };
    for (const QPair<QString, MapEditorPointSymbol> &symbolCase : symbolCases) {
        const MapEditorResolvedPointStyle symbolStyle = resolveMapEditorPointStyle(catalog, symbolCase.first);
        if (!expect(symbolStyle.symbolParts.size() == 1
                        && symbolStyle.symbolParts.first().symbol == symbolCase.second,
                    "Expected supported point symbol part style to resolve from user override.")) {
            return 1;
        }
    }
    const MapEditorResolvedPointStyle ovalPointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("oval"));
    if (!expect(ovalPointStyle.symbolParts.size() == 1
                    && ovalPointStyle.symbolParts.first().symbol == MapEditorPointSymbol::Oval,
                "Expected oval symbol part to parse for point styles.")) {
        return 1;
    }
    if (!expect(std::abs(ovalPointStyle.size - 14.0) < 1e-6,
                "Expected non-symbol point style fields to keep loading for oval point fixture.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle sandStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("sand"));
    if (!expect(sandStyle.fillPattern.has_value(),
                "Expected fill pattern for sand area style.")) {
        return 1;
    }
    if (!expect(!sandStyle.dashPattern.isEmpty(),
                "Expected stroke dash pattern for sand area style.")) {
        return 1;
    }
    if (!expect(sandStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern kind for sand area style.")) {
        return 1;
    }
    if (!expect(sandStyle.fillPattern->dotPlacement == MapEditorAreaDotPlacement::Grid,
                "Expected grid dot placement to remain the default for sand area style.")) {
        return 1;
    }
    if (!expect(std::abs(sandStyle.fillPattern->spacing - 6.4) < 1e-6,
                "Expected spacing override for sand area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle clayStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("clay"));
    if (!expect(clayStyle.fillPattern.has_value()
                    && clayStyle.fillPattern->kind == MapEditorFillPatternKind::Hatch,
                "Expected hatch fill pattern for clay area style.")) {
        return 1;
    }
    if (!expect(clayStyle.fillPattern->angleJitter > 0.0,
                "Expected angle jitter for clay area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle waterStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("water"));
    if (!expect(std::abs(waterStyle.strokeWidth - 4.25) < 1e-6,
                "Expected user override stroke width for water area style.")) {
        return 1;
    }
    if (!expect(std::abs(waterStyle.fillOpacity - 0.37) < 1e-6,
                "Expected user override fill opacity for water area style.")) {
        return 1;
    }
    if (!expect(waterStyle.fillPattern.has_value()
                    && waterStyle.fillPattern->kind == MapEditorFillPatternKind::Hatch,
                "Expected hatch fill pattern for water area style.")) {
        return 1;
    }
    if (!expect(waterStyle.fillPattern->strokeStyle == Qt::DashLine,
                "Expected dashed stroke style for water area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(waterStyle.fillPattern->angle) < 1e-6,
                "Expected horizontal hatch angle for water area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle sumpStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("sump"));
    if (!expect(sumpStyle.fillPattern.has_value()
                    && sumpStyle.fillPattern->kind == MapEditorFillPatternKind::Hatch,
                "Expected hatch fill pattern for sump area style.")) {
        return 1;
    }
    if (!expect(sumpStyle.fillPattern->strokeStyle == Qt::SolidLine,
                "Expected solid stroke style for sump area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle iceStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("ice"));
    if (!expect(iceStyle.fillPattern.has_value()
                    && iceStyle.fillPattern->kind == MapEditorFillPatternKind::Dots
                    && iceStyle.fillPattern->symbolParts.size() == 1
                    && iceStyle.fillPattern->symbolParts.first().symbol == MapEditorPointSymbol::Plus,
                "Expected plus dot fill pattern for ice area style.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle mudcrackStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("mudcrack"));
    if (!expect(mudcrackStyle.fillPattern.has_value()
                    && mudcrackStyle.fillPattern->kind == MapEditorFillPatternKind::CrossHatch,
                "Expected cross_hatch fill pattern for mudcrack area style.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle blocksStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("blocks"));
    if (!expect(blocksStyle.fillPattern.has_value()
                    && blocksStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for blocks area style.")) {
        return 1;
    }
    if (!expect(blocksStyle.fillPattern->symbolParts.size() == 1
                    && blocksStyle.fillPattern->symbolParts.first().symbol == MapEditorPointSymbol::Square,
                "Expected square dot symbol part for blocks area fill pattern.")) {
        return 1;
    }
    if (!expect(blocksStyle.fillPattern->dotPlacement == MapEditorAreaDotPlacement::Scatter,
                "Expected scatter dot placement for blocks area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(blocksStyle.fillPattern->size - 12.0) < 1e-6,
                "Expected derived dot symbol size for blocks area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle pebblesStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("pebbles"));
    if (!expect(pebblesStyle.fillPattern.has_value()
                    && pebblesStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for pebbles area style.")) {
        return 1;
    }
    if (!expect(pebblesStyle.fillPattern->symbolParts.size() == 1
                    && pebblesStyle.fillPattern->symbolParts.first().symbol == MapEditorPointSymbol::Oval,
                "Expected oval dot symbol part for pebbles area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle debrisStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("debris"));
    if (!expect(debrisStyle.fillPattern.has_value()
                    && debrisStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for debris area style.")) {
        return 1;
    }
    if (!expect(debrisStyle.fillPattern->symbolParts.size() == 3
                    && debrisStyle.fillPattern->symbolParts.first().symbol == MapEditorPointSymbol::Triangle,
                "Expected debris area fill pattern to use a three-triangle symbol motif.")) {
        return 1;
    }
    if (!expect(std::abs(debrisStyle.fillPattern->size - 4.8) < 1e-6,
                "Expected debris area fill pattern size to match the scaled point debris motif.")) {
        return 1;
    }
    if (!expect(debrisStyle.fillPattern->symbolParts.first().strokeColor.has_value()
                    && debrisStyle.fillPattern->symbolParts.first().fillColor.has_value()
                    && debrisStyle.fillPattern->symbolParts.first().fillColor->alpha() == 0,
                "Expected debris area symbol parts to render as stroked triangles.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle snowStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("snow"));
    if (!expect(snowStyle.fillPattern.has_value()
                    && snowStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for snow area style.")) {
        return 1;
    }
    if (!expect(snowStyle.fillPattern->symbolParts.size() == 1
                    && snowStyle.fillPattern->symbolParts.first().symbol == MapEditorPointSymbol::Asterisk,
                "Expected asterisk symbol part for snow area fill pattern.")) {
        return 1;
    }
    if (!expect(snowStyle.fillPattern->symbolParts.first().strokeWidth.has_value()
                    && std::abs(snowStyle.fillPattern->symbolParts.first().strokeWidth.value()) < 1e-6,
                "Expected zero-width area snow symbol stroke to parse as an explicit thin stroke.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle symbolStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("symbols"));
    if (!expect(symbolStyle.fillPattern.has_value()
                    && symbolStyle.fillPattern->symbolParts.size() == 1
                    && symbolStyle.fillPattern->symbolParts.first().symbol == MapEditorPointSymbol::Oval,
                "Expected user override dot symbol part for area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(symbolStyle.fillPattern->size - 5.5) < 1e-6,
                "Expected user override dot symbol size for area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(symbolStyle.fillPattern->sizeJitter - 1.25) < 1e-6,
                "Expected user override dot symbol size jitter for area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(symbolStyle.fillPattern->angleJitter - 17.5) < 1e-6,
                "Expected user override dot symbol angle jitter for area fill pattern.")) {
        return 1;
    }
    if (!expect(symbolStyle.fillPattern->dotPlacement == MapEditorAreaDotPlacement::Scatter,
                "Expected user override dot placement for area fill pattern.")) {
        return 1;
    }
    if (!expect(symbolStyle.fillPattern->symbolParts.first().fillColor.has_value(),
                "Expected area dot symbol color to parse from symbol part fill color.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle partsAreaStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("parts"));
    if (!expect(partsAreaStyle.fillPattern.has_value()
                    && partsAreaStyle.fillPattern->symbolParts.size() == 1
                    && partsAreaStyle.fillPattern->symbolParts.first().kind == MapEditorSymbolPartKind::Polygon
                    && partsAreaStyle.fillPattern->symbolParts.first().fill,
                "Expected area fill pattern to parse symbol_parts primitives.")) {
        return 1;
    }
    if (!expect(partsAreaStyle.fillPattern->symbolParts.first().fillColor.has_value(),
                "Expected area fill pattern symbol part fill color to parse.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Therion Studio"));
    QCoreApplication::setOrganizationName(QStringLiteral("Therion Studio"));

    QTemporaryDir temporaryDir;
    if (const int result = prepareUserOverrideFixture(&temporaryDir); result != 0) {
        return result;
    }

    return runCatalogTest(temporaryDir.filePath(QStringLiteral("map_object_styles")));
}
