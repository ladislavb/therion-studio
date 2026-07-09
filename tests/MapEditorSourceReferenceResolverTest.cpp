#include "../src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.h"
#include "../src/core/Th2GeometryProjection.h"
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
    return 0;
}
