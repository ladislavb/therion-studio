#include "../src/app/text_editor/map_editor/MapEditorInspectorData.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/TherionDocumentParser.h"
#include "../src/core/TherionSourceLogicalDocument.h"
#include "../src/core/TherionSourceSnapshotCache.h"

#include <QComboBox>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
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

std::optional<InspectorObjectQuickFields> firstLogicalCommandQuickFields(const QString &contents)
{
    TherionSourceSnapshotCache sourceSnapshotCache;
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    metadata.revisionId = 1;
    const TherionSourceLogicalDocument &logicalDocument =
        sourceSnapshotCache.logicalDocument(contents, metadata);
    if (logicalDocument.commands().isEmpty()) {
        return std::nullopt;
    }
    return inspectorObjectQuickFieldsFromLogicalCommand(logicalDocument.commands().constFirst());
}

int runInspectorTypeCatalogLoadTest()
{
    const CommandCatalogStore catalogStore;
    const InspectorSymbolCatalog catalog = inspectorSymbolCatalogFromCommandCatalog(catalogStore.catalogObject());

    const QStringList pointTypes = inspectorTypeValuesForCommand(catalog, QStringLiteral("point"));
    if (!expect(!pointTypes.isEmpty(), "Point type values should not be empty.")) {
        return 1;
    }
    if (!expect(pointTypes.contains(QStringLiteral("station")),
                "Point type values should include 'station'.")) {
        return 1;
    }

    const QStringList lineTypes = inspectorTypeValuesForCommand(catalog, QStringLiteral("line"));
    if (!expect(!lineTypes.isEmpty(), "Line type values should not be empty.")) {
        return 1;
    }
    if (!expect(lineTypes.contains(QStringLiteral("wall")),
                "Line type values should include 'wall'.")) {
        return 1;
    }

    const QStringList areaTypes = inspectorTypeValuesForCommand(catalog, QStringLiteral("area"));
    if (!expect(!areaTypes.isEmpty(), "Area type values should not be empty.")) {
        return 1;
    }
    if (!expect(areaTypes.contains(QStringLiteral("water")),
                "Area type values should include 'water'.")) {
        return 1;
    }

    return 0;
}

int runAreaQuickTypeComboPopulationTest()
{
    const CommandCatalogStore catalogStore;
    const InspectorSymbolCatalog catalog = inspectorSymbolCatalogFromCommandCatalog(catalogStore.catalogObject());

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(QStringLiteral("area water"), 1);
    const std::optional<InspectorObjectQuickFields> fields = inspectorObjectQuickFieldsFromParsedLine(parsedLine);
    if (!expect(fields.has_value(), "Area quick fields should be extracted from parsed area line.")) {
        return 1;
    }
    if (!expect(fields->commandKind == QStringLiteral("area") && fields->type == QStringLiteral("water"),
                "Area quick fields should preserve command kind and parsed type.")) {
        return 1;
    }

    QComboBox combo;
    combo.setEditable(true);
    combo.setInsertPolicy(QComboBox::NoInsert);
    setEditableComboValues(&combo, inspectorTypeValuesForCommand(catalog, fields->commandKind), fields->type);

    if (!expect(combo.count() > 1,
                "Area type combo should contain catalog values, not only the parsed type.")) {
        return 1;
    }
    if (!expect(combo.currentText() == QStringLiteral("water"),
                "Area type combo should keep parsed type as current text.")) {
        return 1;
    }
    if (!expect(combo.findText(QStringLiteral("sand")) >= 0,
                "Area type combo should include alternate catalog values such as sand.")) {
        return 1;
    }

    return 0;
}

int runLineClipOptionDoesNotBecomeSubtypeTest()
{
    const std::optional<InspectorObjectQuickFields> fields =
        firstLogicalCommandQuickFields(QStringLiteral("line rock-border -close on -clip off\n"));
    if (!expect(fields.has_value(), "Line quick fields should be extracted from a logical command.")) {
        return 1;
    }
    if (!expect(fields->type == QStringLiteral("rock-border"),
                "Line quick fields should preserve the line type before options.")) {
        return 1;
    }
    if (!expect(fields->subtype.isEmpty(),
                "Line -clip off option must not be interpreted as a line subtype.")) {
        return 1;
    }

    const TherionParsedLine corruptedSubtypeLine = TherionDocumentParser::parseLine(
        QStringLiteral("line rock-border -close on -clip off -subtype \"-clip off\""),
        1);
    const std::optional<InspectorObjectQuickFields> corruptedFields =
        inspectorObjectQuickFieldsFromParsedLine(corruptedSubtypeLine);
    if (!expect(corruptedFields.has_value(), "Corrupted line quick fields should still be extracted.")) {
        return 1;
    }
    if (!expect(corruptedFields->subtype.isEmpty(),
                "Option-like quoted subtype values should be ignored instead of shown as a subtype.")) {
        return 1;
    }

    const TherionParsedLine legacyExtraTokensLine = TherionDocumentParser::parseLine(
        QStringLiteral("line rock-border -close on -clip off \"-clip off\" \"-clip off\""),
        1);
    const std::optional<InspectorObjectQuickFields> legacyFields =
        inspectorObjectQuickFieldsFromParsedLine(legacyExtraTokensLine);
    if (!expect(legacyFields.has_value(), "Legacy line quick fields should still be extracted.")) {
        return 1;
    }
    if (!expect(legacyFields->subtype.isEmpty(),
                "Legacy quoted option-like tokens should not be shown as a subtype.")) {
        return 1;
    }

    return 0;
}

int runInjectedCatalogTest()
{
    QJsonObject pointCommand;
    pointCommand.insert(QStringLiteral("name"), QStringLiteral("point"));
    pointCommand.insert(QStringLiteral("type_values"),
                        QJsonArray({QStringLiteral("z-custom-point"), QStringLiteral("a-custom-point")}));
    QJsonObject pointSubtypes;
    pointSubtypes.insert(QStringLiteral("a-custom-point"),
                         QJsonArray({QStringLiteral("z-custom-subtype"), QStringLiteral("a-custom-subtype")}));
    pointCommand.insert(QStringLiteral("subtype_by_type"), pointSubtypes);
    QJsonObject pointValueOption;
    pointValueOption.insert(QStringLiteral("option_key"), QStringLiteral("-value"));
    pointCommand.insert(QStringLiteral("options"), QJsonArray({pointValueOption}));

    QJsonObject scrapProjectionOption;
    scrapProjectionOption.insert(QStringLiteral("option_key"), QStringLiteral("-projection"));
    scrapProjectionOption.insert(QStringLiteral("allowed_values"), QJsonArray({QStringLiteral("custom-projection")}));
    QJsonObject scrapCommand;
    scrapCommand.insert(QStringLiteral("name"), QStringLiteral("scrap"));
    scrapCommand.insert(QStringLiteral("options"), QJsonArray({scrapProjectionOption}));

    QJsonObject catalogObject;
    catalogObject.insert(QStringLiteral("commands"), QJsonArray({pointCommand, scrapCommand}));

    const InspectorSymbolCatalog catalog = inspectorSymbolCatalogFromCommandCatalog(catalogObject);
    const QStringList pointTypeValues = inspectorTypeValuesForCommand(catalog, QStringLiteral("point"));
    if (!expect(pointTypeValues == QStringList({QStringLiteral("a-custom-point"), QStringLiteral("z-custom-point")}),
                "Injected inspector catalog should provide sorted point type values without resource loading.")) {
        return 1;
    }
    const QStringList subtypeValues =
        inspectorSubtypeValuesForCommandType(catalog,
                                             QStringLiteral("point"),
                                             QStringLiteral("a-custom-point"));
    if (!expect(subtypeValues == QStringList({QStringLiteral("a-custom-subtype"), QStringLiteral("z-custom-subtype")}),
                "Injected inspector catalog should provide sorted subtype values without resource loading.")) {
        return 1;
    }
    const QStringList subtypeValuesWithEmptyChoice =
        inspectorSubtypeValuesForCommandTypeWithEmptyChoice(catalog,
                                                            QStringLiteral("point"),
                                                            QStringLiteral("a-custom-point"));
    if (!expect(!subtypeValuesWithEmptyChoice.isEmpty() && subtypeValuesWithEmptyChoice.first().isEmpty(),
                "Subtype combo values should expose an empty choice for clearing subtype.")) {
        return 1;
    }
    if (!expect(subtypeValuesWithEmptyChoice.mid(1) == subtypeValues,
                "Subtype combo values with an empty choice should preserve sorted catalog subtype values.")) {
        return 1;
    }
    if (!expect(inspectorProjectionValues(catalog).contains(QStringLiteral("custom-projection")),
                "Injected inspector catalog should provide projection values without resource loading.")) {
        return 1;
    }
    if (!expect(inspectorValueOptionSupportedForCommandType(catalog,
                                                            QStringLiteral("point"),
                                                            QStringLiteral("height")),
                "Point height should expose the -value quick field when the catalog contains the point -value option.")) {
        return 1;
    }
    if (!expect(!inspectorValueOptionSupportedForCommandType(catalog,
                                                             QStringLiteral("point"),
                                                             QStringLiteral("station")),
                "Point station should not expose the -value quick field.")) {
        return 1;
    }
    if (!expect(!inspectorValueOptionSupportedForCommandType(catalog,
                                                             QStringLiteral("line"),
                                                             QStringLiteral("height")),
                "Line objects should not expose the point -value quick field.")) {
        return 1;
    }

    return 0;
}

int runScrapContextMetadataTest()
{
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(QStringLiteral(
        "scrap first\n"
        "point 0 0 station -name a1\n"
        "endscrap\n"
        "scrap second\n"
        "line wall\n"
        "  0 0\n"
        "  1 1\n"
        "endline\n"
        "endscrap\n"));

    const std::optional<InspectorScrapContext> firstScrap = inspectorScrapContextForSourceLine(parsedLines, 2);
    if (!expect(firstScrap.has_value() && firstScrap->identifier == QStringLiteral("first"),
                "A selected point should report its enclosing first scrap.")) {
        return 1;
    }

    const std::optional<InspectorScrapContext> secondScrap = inspectorScrapContextForSourceLine(parsedLines, 5);
    if (!expect(secondScrap.has_value() && secondScrap->identifier == QStringLiteral("second"),
                "A selected line should report its enclosing second scrap.")) {
        return 1;
    }

    const InspectorScrapContext insertionScrap = inspectorDraftInsertionScrapContext(parsedLines);
    if (!expect(insertionScrap.identifier == QStringLiteral("second") && !insertionScrap.willBeCreated,
                "Pending draft insertion should target the last existing scrap.")) {
        return 1;
    }

    const QVector<InspectorScrapContext> scrapContexts = inspectorScrapContexts(parsedLines);
    if (!expect(scrapContexts.size() == 2
                    && scrapContexts.at(0).identifier == QStringLiteral("first")
                    && scrapContexts.at(1).identifier == QStringLiteral("second"),
                "Scrap target choices should list existing scrap identifiers in source order.")) {
        return 1;
    }

    const InspectorScrapContext createdScrap = inspectorDraftInsertionScrapContext({});
    if (!expect(createdScrap.identifier == QStringLiteral("scrap-1") && createdScrap.willBeCreated,
                "Pending draft insertion should report the generated scrap when no scrap exists.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int result = runInspectorTypeCatalogLoadTest(); result != 0) {
        return result;
    }
    if (const int result = runInjectedCatalogTest(); result != 0) {
        return result;
    }
    if (const int result = runLineClipOptionDoesNotBecomeSubtypeTest(); result != 0) {
        return result;
    }
    if (const int result = runScrapContextMetadataTest(); result != 0) {
        return result;
    }
    return runAreaQuickTypeComboPopulationTest();
}
