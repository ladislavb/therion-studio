#include "../src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.h"
#include "../src/core/Th2GeometryProjection.h"
#include "../src/core/TherionDocumentParser.h"
#include "../src/core/TherionSourceDocument.h"
#include "../src/core/TherionSourceLogicalDocument.h"

#include <QString>

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

int runLineFeatureLookupPreservesPhysicalLineNumbersTest()
{
    const QString text = QStringLiteral(
        "# header\r\n"
        "\r\n"
        "scrap s1 -projection plan\r\n"
        "  # line comment\r\n"
        "line wall -id wall-1\r\n"
        "  0 0\r\n"
        "  10 0\r\n"
        "endline\r\n"
        "endscrap\r\n");

    const std::optional<MapGeometryFeature> feature = lineFeatureForLineNumber(text, 5);
    if (!expect(feature.has_value(), "Expected line feature lookup through source snapshot to find the wall line.")) {
        return 1;
    }
    if (!expect(feature->lineNumber == 5 && feature->lineVertices.size() == 2,
                "Expected line feature lookup to preserve physical line numbers after blank/comment lines.")) {
        return 1;
    }
    return expect(feature->optionValues.value(QStringLiteral("id")) == QStringLiteral("wall-1"),
                  "Expected line feature lookup to preserve parsed line options.")
        ? 0
        : 1;
}

int runLineFeatureLookupUsesLogicalCommandsTest()
{
    const QString text = QStringLiteral(
        "# header\n"
        "scrap s1 -projection plan\n"
        "line wall -id wall-1\n"
        "  0 0\n"
        "  subtype blocks\n"
        "  10 0 -smooth off\n"
        "endline\n"
        "endscrap\n");

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromText(text, metadata);

    const std::optional<MapGeometryFeature> textFeature = lineFeatureForLineNumber(text, 3);
    const std::optional<MapGeometryFeature> logicalFeature =
        lineFeatureForLineNumber(logicalDocument.commands(), 3);
    if (!expect(textFeature.has_value() && logicalFeature.has_value(),
                "Expected logical-command line feature lookup to find the same wall line as text lookup.")) {
        return 1;
    }
    if (!expect(logicalFeature->lineNumber == textFeature->lineNumber
                    && logicalFeature->lineVertices.size() == textFeature->lineVertices.size(),
                "Expected logical-command line feature lookup to preserve line number and vertex count.")) {
        return 1;
    }
    if (!expect(logicalFeature->optionValues.value(QStringLiteral("id"))
                    == textFeature->optionValues.value(QStringLiteral("id")),
                "Expected logical-command line feature lookup to preserve line options.")) {
        return 1;
    }
    if (!expect(logicalFeature->lineVertices.at(1).standaloneOptionRows
                    == textFeature->lineVertices.at(1).standaloneOptionRows,
                "Expected logical-command line feature lookup to preserve line-point standalone option rows.")) {
        return 1;
    }
    return 0;
}

int runLineFeatureLookupUsesTh2ProjectionTest()
{
    const QString text = QStringLiteral(
        "# header\n"
        "scrap s1 -projection plan\n"
        "line wall -id wall-1\n"
        "  0 0\n"
        "  subtype blocks\n"
        "  10 0 -smooth off\n"
        "endline\n"
        "endscrap\n");

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(text, metadata);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);
    const Th2GeometryProjection projection =
        Th2GeometryProjection::fromDocuments(sourceDocument, logicalDocument);

    const std::optional<MapGeometryFeature> headerFeature =
        lineFeatureForLineNumber(logicalDocument.commands(), 3);
    const std::optional<MapGeometryFeature> projectedFeature =
        lineFeatureForLineNumber(projection, logicalDocument.commands(), 5);
    if (!expect(headerFeature.has_value() && projectedFeature.has_value(),
                "Expected projection-backed line feature lookup to find a line from an interior source row.")) {
        return 1;
    }
    if (!expect(projectedFeature->lineNumber == headerFeature->lineNumber,
                "Expected projection-backed line feature lookup to resolve the line command header range.")) {
        return 1;
    }
    if (!expect(projectedFeature->lineVertices.size() == headerFeature->lineVertices.size(),
                "Expected projection-backed line feature lookup to preserve line vertices.")) {
        return 1;
    }
    return expect(projectedFeature->lineVertices.at(1).standaloneOptionRows
                      == headerFeature->lineVertices.at(1).standaloneOptionRows,
                  "Expected projection-backed line feature lookup to preserve standalone option rows.")
        ? 0
        : 1;
}

int runPointSelectionLineLookupUsesTh2ProjectionTest()
{
    const QString text = QStringLiteral(
        "# header\n"
        "scrap s1 -projection plan\n"
        "point 1 2 station -name a1\n"
        "point 10 20 label -text \"A\"\n"
        "endscrap\n");

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(text, metadata);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);
    const Th2GeometryProjection projection =
        Th2GeometryProjection::fromDocuments(sourceDocument, logicalDocument);

    const std::optional<int> textLine =
        sourcePointLineNumberForSelection(TherionDocumentParser::parseTokenLines(text), QPointF(10.0, 20.0));
    const std::optional<int> projectedLine =
        sourcePointLineNumberForSelection(projection, QPointF(10.0, 20.0));
    if (!expect(textLine.has_value() && projectedLine.has_value(),
                "Expected projection-backed point selection lookup to find a point line.")) {
        return 1;
    }
    return expect(projectedLine.value() == textLine.value() && projectedLine.value() == 4,
                  "Expected projection-backed point selection lookup to preserve source line number.")
        ? 0
        : 1;
}

int runObjectKindLookupUsesTh2ProjectionTest()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan\n"
        "point 1 2 station -name a1\n"
        "line wall -id wall-1\n"
        "  0 0\n"
        "  10 0\n"
        "endline\n"
        "area water -id a1\n"
        "  wall-1\n"
        "endarea\n"
        "endscrap\n");

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(text, metadata);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);
    const Th2GeometryProjection projection =
        Th2GeometryProjection::fromDocuments(sourceDocument, logicalDocument);

    if (!expect(mapObjectKindForSourceLine(projection, 1) == QStringLiteral("scrap"),
                "Expected projection-backed object kind lookup to classify scrap lines.")) {
        return 1;
    }
    if (!expect(mapObjectKindForSourceLine(projection, 2) == QStringLiteral("point"),
                "Expected projection-backed object kind lookup to classify point lines.")) {
        return 1;
    }
    if (!expect(mapObjectKindForSourceLine(projection, 4) == QStringLiteral("line"),
                "Expected projection-backed object kind lookup to classify interior line rows as line objects.")) {
        return 1;
    }
    return expect(mapObjectKindForSourceLine(projection, 8) == QStringLiteral("area"),
                  "Expected projection-backed object kind lookup to classify area body rows as area objects.")
        ? 0
        : 1;
}

int runCursorGeometryLookupUsesLogicalCommandsTest()
{
    const QString text = QStringLiteral(
        "scrap s1\n"
        "line wall -id wall-1\n"
        "  0 0\n"
        "  10 0 -smooth off\n"
        "endline\n"
        "endscrap\n");

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromText(text, metadata);

    const CursorGeometrySelection parsedSelection =
        cursorGeometrySelectionForTextCursor(TherionDocumentParser::parseTokenLines(text), 4, 4);
    const CursorGeometrySelection logicalSelection =
        cursorGeometrySelectionForTextCursor(logicalDocument.commands(), 4, 4);
    if (!expect(logicalSelection.featureLineNumber == parsedSelection.featureLineNumber
                    && logicalSelection.geometryKind == parsedSelection.geometryKind,
                "Expected logical-command cursor geometry lookup to preserve selected feature.")) {
        return 1;
    }
    if (!expect(logicalSelection.sourceVertexReference.has_value()
                    && parsedSelection.sourceVertexReference.has_value()
                    && logicalSelection.sourceVertexReference->sourceVertexIndex
                        == parsedSelection.sourceVertexReference->sourceVertexIndex,
                "Expected logical-command cursor geometry lookup to preserve source vertex index.")) {
        return 1;
    }
    return 0;
}

int runScrapObjectLinesLookupUsesLogicalCommandsTest()
{
    const QString text = QStringLiteral(
        "scrap s1\n"
        "point 0 0 station -name a1\n"
        "line wall -id wall-1\n"
        "  0 0\n"
        "  10 0\n"
        "endline\n"
        "endscrap\n");

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromText(text, metadata);

    const std::optional<QSet<int>> parsedLines =
        scrapObjectLinesForCursor(TherionDocumentParser::parseTokenLines(text), 1);
    const std::optional<QSet<int>> logicalLines =
        scrapObjectLinesForCursor(logicalDocument.commands(), 1);
    if (!expect(parsedLines.has_value() && logicalLines.has_value(),
                "Expected logical-command scrap lookup to find scrap object lines.")) {
        return 1;
    }
    return expect(logicalLines.value() == parsedLines.value() && logicalLines->contains(2) && logicalLines->contains(3),
                  "Expected logical-command scrap lookup to preserve object line set.")
        ? 0
        : 1;
}
}

int main()
{
    if (const int rc = runLineFeatureLookupPreservesPhysicalLineNumbersTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineFeatureLookupUsesLogicalCommandsTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineFeatureLookupUsesTh2ProjectionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runPointSelectionLineLookupUsesTh2ProjectionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runObjectKindLookupUsesTh2ProjectionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runCursorGeometryLookupUsesLogicalCommandsTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runScrapObjectLinesLookupUsesLogicalCommandsTest(); rc != 0) {
        return rc;
    }
    return 0;
}
