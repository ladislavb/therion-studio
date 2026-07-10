#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorSmartAreaPlanner.h"
#include "../src/core/Th2GeometryProjection.h"
#include "../src/core/TherionDocumentEditor.h"
#include "../src/core/TherionDocumentParser.h"
#include "../src/core/TherionSourceDocument.h"
#include "../src/core/TherionSourceLogicalDocument.h"

#include <QApplication>
#include <QGraphicsPathItem>
#include <QGraphicsScene>

#include <iostream>
#include <cmath>
#include <limits>

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

bool expectMoveSetsEqual(const QVector<MapLineSecondaryMove> &expected,
                         const QVector<MapLineSecondaryMove> &actual,
                         const char *message)
{
    auto toMap = [](const QVector<MapLineSecondaryMove> &moves) {
        QHash<int, QPair<QPointF, QPointF>> map;
        for (const MapLineSecondaryMove &move : moves) {
            map.insert(move.sourceVertexIndex, qMakePair(move.oldPoint, move.newPoint));
        }
        return map;
    };

    const QHash<int, QPair<QPointF, QPointF>> expectedMap = toMap(expected);
    const QHash<int, QPair<QPointF, QPointF>> actualMap = toMap(actual);
    if (expectedMap != actualMap) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

bool pointsAreCollinear(const QPointF &first, const QPointF &middle, const QPointF &last)
{
    const QPointF a = first - middle;
    const QPointF b = last - middle;
    const qreal cross = (a.x() * b.y()) - (a.y() * b.x());
    const qreal lengthProduct = std::hypot(a.x(), a.y()) * std::hypot(b.x(), b.y());
    return std::abs(cross) <= qMax<qreal>(1e-6, lengthProduct * 1e-4);
}

const MapGeometryFeature *firstLineFeature(const QVector<MapGeometryFeature> &features)
{
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line) {
            return &feature;
        }
    }

    return nullptr;
}

const MapGeometryFeature *firstLineFeatureByLabel(const QVector<MapGeometryFeature> &features, const QString &label)
{
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line
            && feature.label == label) {
            return &feature;
        }
    }

    return nullptr;
}

QVector<const MapGeometryFeature *> pointFeatures(const QVector<MapGeometryFeature> &features)
{
    QVector<const MapGeometryFeature *> points;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Point) {
            points.append(&feature);
        }
    }
    return points;
}

int countGeometryItemsForLine(const QGraphicsScene &scene, int lineNumber)
{
    int count = 0;
    for (QGraphicsItem *item : scene.items()) {
        if (item != nullptr
            && item->data(kMapItemRole).toInt() == kMapItemGeometryValue
            && item->data(kMapSceneLineNumberRole).toInt() == lineNumber) {
            ++count;
        }
    }
    return count;
}

int countVertexIndexEntriesForLine(const QHash<QString, QGraphicsItem *> &itemsByKey, int lineNumber)
{
    int count = 0;
    for (QGraphicsItem *item : itemsByKey) {
        if (item != nullptr && item->data(kMapSceneLineNumberRole).toInt() == lineNumber) {
            ++count;
        }
    }
    return count;
}

int runPointTypeAndLabelOptionParsingTest()
{
    const QString text =
        QStringLiteral("point 10 20 station -name P1 -orientation 450\n"
                       "point 30 40 label -text \"large<br>dome\" -orient -45\n"
                       "line label -text \"stream<br>label\"\n"
                       "  10 10\n"
                       "  40 10\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const QVector<const MapGeometryFeature *> points = pointFeatures(features);
    if (!expect(points.size() == 2, "Expected two point features to parse.")) {
        return 1;
    }

    const MapGeometryFeature *station = points.at(0);
    if (!expect(station->label == QStringLiteral("station"),
                "Expected standard point-coordinate station syntax to resolve the point type.")) {
        return 1;
    }
    if (!expect(station->stationPoint,
                "Expected standard point-coordinate station syntax to mark the feature as a station point.")) {
        return 1;
    }
    if (!expect(station->optionValues.value(QStringLiteral("name")) == QStringLiteral("P1"),
                "Expected station -name option to be available for style-driven labels.")) {
        return 1;
    }
    if (!expect(station->orientationDegrees.has_value()
                    && std::abs(station->orientationDegrees.value() - 90.0) < 1e-6,
                "Expected point -orientation to parse and normalize into scene degrees.")) {
        return 1;
    }

    const MapGeometryFeature *label = points.at(1);
    if (!expect(label->label == QStringLiteral("label"),
                "Expected standard point-coordinate label syntax to resolve the point type.")) {
        return 1;
    }
    if (!expect(label->optionValues.value(QStringLiteral("text")) == QStringLiteral("large<br>dome"),
                "Expected label -text option to be available for style-driven labels.")) {
        return 1;
    }
    if (!expect(label->orientationDegrees.has_value()
                    && std::abs(label->orientationDegrees.value() - 315.0) < 1e-6,
                "Expected point -orient alias to parse and normalize negative degrees.")) {
        return 1;
    }

    const MapGeometryFeature *lineLabel = firstLineFeatureByLabel(features, QStringLiteral("label"));
    if (!expect(lineLabel != nullptr, "Expected line label feature to parse.")) {
        return 1;
    }
    if (!expect(lineLabel->optionValues.value(QStringLiteral("text")) == QStringLiteral("stream<br>label"),
                "Expected line label -text option to be available for style-driven labels.")) {
        return 1;
    }

    return 0;
}

int runProjectionGeometryFeatureAdapterParityTest()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan\n"
        "point 10 20 station -name P1\n"
        "line wall -id wall-1 -close on\n"
        "  0 0\n"
        "  10 0\n"
        "  10 10\n"
        "endline\n"
        "area water -id a1\n"
        "  wall-1\n"
        "endarea\n"
        "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> parsedFeatures = collectGeometryFeatures(parsedLines);

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(text, metadata);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);
    const Th2GeometryProjection projection =
        Th2GeometryProjection::fromDocuments(sourceDocument, logicalDocument);
    const QVector<MapGeometryFeature> projectedFeatures =
        collectGeometryFeatures(projection, logicalDocument.commands());

    if (!expect(projectedFeatures.size() == parsedFeatures.size(),
                "Expected projection geometry feature adapter to preserve feature count.")) {
        return 1;
    }
    for (int index = 0; index < parsedFeatures.size(); ++index) {
        const MapGeometryFeature &parsed = parsedFeatures.at(index);
        const MapGeometryFeature &projected = projectedFeatures.at(index);
        if (!expect(projected.kind == parsed.kind && projected.lineNumber == parsed.lineNumber,
                    "Expected projection geometry feature adapter to preserve kind and source line order.")) {
            return 1;
        }
        if (!expect(projected.vertices.size() == parsed.vertices.size()
                        && projected.lineVertices.size() == parsed.lineVertices.size(),
                    "Expected projection geometry feature adapter to preserve geometry vertex counts.")) {
            return 1;
        }
        if (!expect(projected.optionValues == parsed.optionValues,
                    "Expected projection geometry feature adapter to preserve option values.")) {
            return 1;
        }
    }
    return 0;
}

int runLogicalMapSceneEntryAdapterParityTest()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan\n"
        "point 10 20 station -name P1\n"
        "line wall -id wall-1 -close on\n"
        "  0 0\n"
        "  10 0\n"
        "  10 10\n"
        "endline\n"
        "area water -id a1\n"
        "  wall-1\n"
        "endarea\n"
        "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapSceneEntry> parsedEntries = collectMapSceneEntries(parsedLines);

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(text, metadata);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);
    const QVector<MapSceneEntry> logicalEntries = collectMapSceneEntries(logicalDocument.commands());

    if (!expect(logicalEntries.size() == parsedEntries.size(),
                "Expected logical map scene entry adapter to preserve entry count.")) {
        return 1;
    }

    for (int index = 0; index < parsedEntries.size(); ++index) {
        const MapSceneEntry &parsed = parsedEntries.at(index);
        const MapSceneEntry &logical = logicalEntries.at(index);
        if (!expect(logical.lineNumber == parsed.lineNumber
                        && logical.category == parsed.category
                        && logical.title == parsed.title
                        && logical.subtitle == parsed.subtitle,
                    "Expected logical map scene entry adapter to preserve scene entry content.")) {
            return 1;
        }
    }

    return 0;
}

int runBezierSegmentParsingTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  1174.0 744.5\n"
                       "  1194.0 756.5 1192.5 757.5 1176.0 791.0\n"
                       "  smooth off\n"
                       "  1205.5 788.0 1195.5 832.5 1173.5 879.0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature.")) {
        return 1;
    }

    if (!expect(line->lineVertices.size() == 3, "Expected three anchor vertices for two Bezier segments.")) {
        return 1;
    }
    if (!expect(line->lineSegments.size() == 2, "Expected two parsed line segments.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(0).type == MapGeometryFeature::LineSegment::Type::Cubic,
                "Expected first segment to be cubic.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(1).type == MapGeometryFeature::LineSegment::Type::Cubic,
                "Expected second segment to be cubic.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(0).control1VertexIndex == 1
                && line->lineSegments.at(0).control2VertexIndex == 2
                && line->lineSegments.at(0).endVertexIndex == 1,
                "Expected first cubic segment indices to map to first control/control/end triplet.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(1).control1VertexIndex == 4
                && line->lineSegments.at(1).control2VertexIndex == 5
                && line->lineSegments.at(1).endVertexIndex == 2,
                "Expected second cubic segment indices to map to second control/control/end triplet.")) {
        return 1;
    }
    if (!expect(line->lineVertices.at(1).isSmooth == false,
                "Expected smooth off to mark the current/last parsed line vertex as non-smooth.")) {
        return 1;
    }
    if (!expect(line->lineVertices.at(0).outgoingControl.has_value()
                && line->lineVertices.at(1).incomingControl.has_value()
                && line->lineVertices.at(1).outgoingControl.has_value()
                && line->lineVertices.at(2).incomingControl.has_value(),
                "Expected parsed Bezier controls to be attached to adjacent vertices.")) {
        return 1;
    }

    return 0;
}

int runSmoothAndOptionMetadataIgnoredTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  10 20 30 40\n"
                       "  smooth off 50 60\n"
                       "  -subtype temporary 100 200\n"
                       "  70 80\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for metadata-ignore test.")) {
        return 1;
    }

    if (!expect(line->lineVertices.size() == 3,
                "Expected only coordinate data lines to contribute vertices (smooth/subtype metadata ignored).")) {
        return 1;
    }
    if (!expect(line->lineSegments.size() == 2,
                "Expected two linear segments after metadata lines are ignored.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(0).type == MapGeometryFeature::LineSegment::Type::Linear
                && line->lineSegments.at(1).type == MapGeometryFeature::LineSegment::Type::Linear,
                "Expected both parsed segments to remain linear in metadata-ignore case.")) {
        return 1;
    }

    return 0;
}

int runLinePointSubtypeSegmentParsingTest()
{
    const QString text =
        QStringLiteral("line wall -reverse on\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  subtype invisible\n"
                       "  10 0 15 5 20 0\n"
                       "  smooth off\n"
                       "  20 0 25 -5 30 0\n"
                       "  subtype bedrock\n"
                       "  40 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for line-point subtype segment test.")) {
        return 1;
    }

    if (!expect(line->lineSegments.size() == 4,
                "Expected all line segments to be preserved across line-point subtype switches.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(0).subtype.isEmpty(),
                "Expected the initial wall segment to keep the header subtype.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(1).subtype == QStringLiteral("invisible")
                    && line->lineSegments.at(2).subtype == QStringLiteral("invisible"),
                "Expected subtype invisible to apply to following line segments until the next subtype row.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(3).subtype == QStringLiteral("bedrock"),
                "Expected subtype bedrock to apply to the following line segment.")) {
        return 1;
    }
    if (!expect(line->lineVertices.at(1).standaloneOptionRows.contains(QStringLiteral("subtype invisible")),
                "Expected line-point subtype rows to remain attached to the source vertex for editing.")) {
        return 1;
    }
    if (!expect(line->lineVertices.at(3).standaloneOptionRows.contains(QStringLiteral("subtype bedrock")),
                "Expected later line-point subtype rows to remain attached to the source vertex for editing.")) {
        return 1;
    }

    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> mapItemsByLine;
    renderMapWorkspaceScene(&scene,
                            QStringLiteral("fixture.th2"),
                            collectMapSceneEntries(parsedLines),
                            features,
                            std::nullopt,
                            false,
                            &mapItemsByLine,
                            nullptr,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {});

    int renderedPathItems = 0;
    for (QGraphicsItem *item : scene.items()) {
        if (item == nullptr || item->data(kMapSceneLineNumberRole).toInt() != line->lineNumber) {
            continue;
        }
        auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (pathItem == nullptr) {
            continue;
        }
        ++renderedPathItems;
    }
    if (!expect(renderedPathItems > 1,
                "Expected subtype-split lines to render separate path items for segment-specific styles.")) {
        return 1;
    }

    return 0;
}

int runLinePointSubtypeBlocksGuideRenderingTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  subtype blocks\n"
                       "  20 0\n"
                       "  30 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for wall:blocks segment guide test.")) {
        return 1;
    }
    if (!expect(line->lineSegments.size() == 3
                    && line->lineSegments.at(1).subtype == QStringLiteral("blocks")
                    && line->lineSegments.at(2).subtype == QStringLiteral("blocks"),
                "Expected line-point subtype blocks to apply to following wall segments.")) {
        return 1;
    }

    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> mapItemsByLine;
    renderMapWorkspaceScene(&scene,
                            QStringLiteral("fixture.th2"),
                            collectMapSceneEntries(parsedLines),
                            features,
                            std::nullopt,
                            false,
                            &mapItemsByLine,
                            nullptr,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {});

    bool foundBlocksSegmentGuide = false;
    for (QGraphicsItem *item : scene.items()) {
        if (item == nullptr || item->data(kMapSceneLineNumberRole).toInt() != line->lineNumber) {
            continue;
        }
        auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (pathItem == nullptr) {
            continue;
        }
        const QPen pen = pathItem->pen();
        if (std::abs(item->zValue() - 2.58) < 0.001
            && item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineDetail
            && pen.style() == Qt::CustomDashLine
            && pen.color().alpha() > 0) {
            foundBlocksSegmentGuide = true;
            break;
        }
    }

    if (!expect(foundBlocksSegmentGuide,
                "Expected decorated wall:blocks line-point subtype segments to render an editable guide spine above decorations.")) {
        return 1;
    }

    return 0;
}

int runLineGeometryItemGroupRemovalTest()
{
    const QString text =
        QStringLiteral("point 2 2 station -name P1\n"
                       "line wall\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  subtype blocks\n"
                       "  20 0\n"
                       "endline\n"
                       "line wall\n"
                       "  0 10\n"
                       "  10 10\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *targetLine = nullptr;
    const MapGeometryFeature *otherLine = nullptr;
    const MapGeometryFeature *point = nullptr;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line) {
            if (targetLine == nullptr) {
                targetLine = &feature;
            } else {
                otherLine = &feature;
            }
        } else if (feature.kind == MapGeometryFeature::Kind::Point) {
            point = &feature;
        }
    }

    if (!expect(targetLine != nullptr && otherLine != nullptr && point != nullptr,
                "Expected fixture to parse one point and two line features for item group removal.")) {
        return 1;
    }

    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> mapItemsByLine;
    QHash<QString, QGraphicsItem *> vertexItemsByKey;
    renderMapWorkspaceScene(&scene,
                            QStringLiteral("fixture.th2"),
                            collectMapSceneEntries(parsedLines),
                            features,
                            std::nullopt,
                            false,
                            &mapItemsByLine,
                            &vertexItemsByKey,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {});

    auto *nonGeometryItem = new QGraphicsRectItem(QRectF(0, 0, 1, 1));
    nonGeometryItem->setData(kMapSceneLineNumberRole, targetLine->lineNumber);
    scene.addItem(nonGeometryItem);

    const int targetGeometryBefore = countGeometryItemsForLine(scene, targetLine->lineNumber);
    const int otherLineGeometryBefore = countGeometryItemsForLine(scene, otherLine->lineNumber);
    const int pointGeometryBefore = countGeometryItemsForLine(scene, point->lineNumber);
    const int targetVertexEntriesBefore = countVertexIndexEntriesForLine(vertexItemsByKey, targetLine->lineNumber);
    if (!expect(targetGeometryBefore > 1,
                "Expected styled target line to render a multi-item geometry group before removal.")) {
        return 1;
    }
    if (!expect(otherLineGeometryBefore > 0 && pointGeometryBefore > 0,
                "Expected neighboring geometry to be present before target line removal.")) {
        return 1;
    }
    if (!expect(targetVertexEntriesBefore > 0,
                "Expected target line vertex handles to be indexed before removal.")) {
        return 1;
    }
    if (!expect(mapItemsByLine.contains(targetLine->lineNumber)
                    && mapItemsByLine.contains(otherLine->lineNumber)
                    && mapItemsByLine.contains(point->lineNumber),
                "Expected primary item index to include target and neighboring geometry before removal.")) {
        return 1;
    }

    const MapGeometryItemGroupRemovalResult result =
        removeMapGeometryItemGroupForLine(&scene,
                                          targetLine->lineNumber,
                                          &mapItemsByLine,
                                          &vertexItemsByKey);
    if (!expect(result.removedItems == targetGeometryBefore,
                "Expected item group removal to delete every target line geometry item.")) {
        return 1;
    }
    if (!expect(result.removedPrimaryItem,
                "Expected item group removal to remove the target primary item index entry.")) {
        return 1;
    }
    if (!expect(result.removedVertexIndexEntries == targetVertexEntriesBefore,
                "Expected item group removal to remove target vertex index entries.")) {
        return 1;
    }
    if (!expect(countGeometryItemsForLine(scene, targetLine->lineNumber) == 0,
                "Expected no target line geometry items to remain after group removal.")) {
        return 1;
    }
    if (!expect(countGeometryItemsForLine(scene, otherLine->lineNumber) == otherLineGeometryBefore
                    && countGeometryItemsForLine(scene, point->lineNumber) == pointGeometryBefore,
                "Expected neighboring geometry groups to remain after target line removal.")) {
        return 1;
    }
    if (!expect(scene.items().contains(nonGeometryItem),
                "Expected non-geometry items with the same line number to remain after geometry group removal.")) {
        return 1;
    }
    if (!expect(!mapItemsByLine.contains(targetLine->lineNumber)
                    && mapItemsByLine.contains(otherLine->lineNumber)
                    && mapItemsByLine.contains(point->lineNumber),
                "Expected primary item index removal to be limited to the target line.")) {
        return 1;
    }
    if (!expect(countVertexIndexEntriesForLine(vertexItemsByKey, targetLine->lineNumber) == 0
                    && countVertexIndexEntriesForLine(vertexItemsByKey, otherLine->lineNumber) > 0,
                "Expected vertex index removal to be limited to the target line.")) {
        return 1;
    }

    const MapGeometryItemGroupRenderResult renderResult =
        renderMapGeometryItemGroupForFeature(&scene,
                                             *targetLine,
                                             geometryBoundsForFeatures(features),
                                             &mapItemsByLine,
                                             &vertexItemsByKey,
                                             {},
                                             {},
                                             {},
                                             {});
    if (!expect(renderResult.addedItems == targetGeometryBefore,
                "Expected single-feature render to restore every target line geometry item.")) {
        return 1;
    }
    if (!expect(renderResult.addedPrimaryItem,
                "Expected single-feature render to restore the target primary item index entry.")) {
        return 1;
    }
    if (!expect(renderResult.addedVertexIndexEntries == targetVertexEntriesBefore,
                "Expected single-feature render to restore target vertex index entries.")) {
        return 1;
    }
    if (!expect(countGeometryItemsForLine(scene, targetLine->lineNumber) == targetGeometryBefore,
                "Expected target line geometry group to be present after single-feature render.")) {
        return 1;
    }
    if (!expect(countGeometryItemsForLine(scene, otherLine->lineNumber) == otherLineGeometryBefore
                    && countGeometryItemsForLine(scene, point->lineNumber) == pointGeometryBefore,
                "Expected neighboring geometry groups to remain unchanged after single-feature render.")) {
        return 1;
    }
    if (!expect(scene.items().contains(nonGeometryItem),
                "Expected non-geometry items with the same line number to survive single-feature render.")) {
        return 1;
    }
    if (!expect(mapItemsByLine.contains(targetLine->lineNumber)
                    && mapItemsByLine.contains(otherLine->lineNumber)
                    && mapItemsByLine.contains(point->lineNumber),
                "Expected primary item index to include target and neighboring geometry after single-feature render.")) {
        return 1;
    }
    if (!expect(countVertexIndexEntriesForLine(vertexItemsByKey, targetLine->lineNumber) == targetVertexEntriesBefore
                    && countVertexIndexEntriesForLine(vertexItemsByKey, otherLine->lineNumber) > 0,
                "Expected vertex index to include target and neighboring handles after single-feature render.")) {
        return 1;
    }

    return 0;
}

int runLinePointSubtypeBlocksPreviewRefreshTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  subtype blocks\n"
                       "  20 0\n"
                       "  30 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for wall:blocks preview refresh test.")) {
        return 1;
    }

    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> mapItemsByLine;
    renderMapWorkspaceScene(&scene,
                            QStringLiteral("fixture.th2"),
                            collectMapSceneEntries(parsedLines),
                            features,
                            std::nullopt,
                            false,
                            &mapItemsByLine,
                            nullptr,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {});

    QGraphicsItem *anchorItem = nullptr;
    QGraphicsPathItem *blocksGuideItem = nullptr;
    for (QGraphicsItem *item : scene.items()) {
        if (item == nullptr || item->data(kMapSceneLineNumberRole).toInt() != line->lineNumber) {
            continue;
        }
        if (item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineAnchor
            && item->data(kMapSceneOwnerVertexRole).toInt() == 2) {
            anchorItem = item;
            continue;
        }

        auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (pathItem == nullptr) {
            continue;
        }
        if (std::abs(item->zValue() - 2.58) < 0.001
            && item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineDetail
            && pathItem->pen().style() == Qt::CustomDashLine) {
            blocksGuideItem = pathItem;
            continue;
        }
    }

    if (!expect(anchorItem != nullptr, "Expected editable line anchor for the blocks segment preview test.")) {
        return 1;
    }
    if (!expect(blocksGuideItem != nullptr, "Expected blocks segment guide item before preview movement.")) {
        return 1;
    }
    const QRectF guideBoundsBefore = blocksGuideItem->path().boundingRect();
    anchorItem->setPos(anchorItem->pos() + QPointF(0.0, 80.0));
    const QRectF guideBoundsAfter = blocksGuideItem->path().boundingRect();

    if (!expect(std::abs(guideBoundsAfter.top() - guideBoundsBefore.top()) > 1e-6
                    || std::abs(guideBoundsAfter.bottom() - guideBoundsBefore.bottom()) > 1e-6,
                "Expected blocks segment guide path to update during line vertex preview movement.")) {
        return 1;
    }

    return 0;
}

int runLineLabelPreviewRefreshTest()
{
    const QString text =
        QStringLiteral("line label -text \"Dome\"\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  20 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for line-label preview refresh test.")) {
        return 1;
    }

    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> mapItemsByLine;
    renderMapWorkspaceScene(&scene,
                            QStringLiteral("fixture.th2"),
                            collectMapSceneEntries(parsedLines),
                            features,
                            std::nullopt,
                            false,
                            &mapItemsByLine,
                            nullptr,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {});

    QGraphicsItem *anchorItem = nullptr;
    QGraphicsItem *lineLabelItem = nullptr;
    for (QGraphicsItem *item : scene.items()) {
        if (item == nullptr || item->data(kMapSceneLineNumberRole).toInt() != line->lineNumber) {
            continue;
        }
        if (item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineAnchor
            && item->data(kMapSceneOwnerVertexRole).toInt() == 1) {
            anchorItem = item;
            continue;
        }
        if (std::abs(item->zValue() - 3.1) < 0.001) {
            lineLabelItem = item;
            continue;
        }
    }

    if (!expect(anchorItem != nullptr, "Expected editable line anchor for the line-label preview test.")) {
        return 1;
    }
    if (!expect(lineLabelItem != nullptr, "Expected line label item before preview movement.")) {
        return 1;
    }

    const QRectF labelBoundsBefore = lineLabelItem->boundingRect();
    anchorItem->setPos(anchorItem->pos() + QPointF(0.0, 80.0));
    const QRectF labelBoundsAfter = lineLabelItem->boundingRect();

    if (!expect(std::abs(labelBoundsAfter.top() - labelBoundsBefore.top()) > 1e-6
                    || std::abs(labelBoundsAfter.bottom() - labelBoundsBefore.bottom()) > 1e-6,
                "Expected line label path to update during line vertex preview movement.")) {
        return 1;
    }

    return 0;
}

int runInlineSubtypeParsingTest()
{
    const QString text =
        QStringLiteral("scrap s1 -projection plan\n"
                       "point 10 20 station:fixed -name P1\n"
                       "line wall:debris -id border-1 -close on\n"
                       "  0 0\n"
                       "  40 0\n"
                       "  20 20\n"
                       "endline\n"
                       "area water:temporary\n"
                       "  border-1\n"
                       "endarea\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const QVector<const MapGeometryFeature *> points = pointFeatures(features);
    if (!expect(points.size() == 1, "Expected one point feature for inline subtype parsing test.")) {
        return 1;
    }

    if (!expect(points.first()->label == QStringLiteral("station"),
                "Expected inline point subtype parsing to keep only the type in feature.label.")) {
        return 1;
    }
    if (!expect(points.first()->subtype == QStringLiteral("fixed"),
                "Expected inline point subtype parsing to populate feature.subtype.")) {
        return 1;
    }

    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one line feature for inline subtype parsing test.")) {
        return 1;
    }
    if (!expect(line->label == QStringLiteral("wall"),
                "Expected inline line subtype parsing to keep only the type in feature.label.")) {
        return 1;
    }
    if (!expect(line->subtype == QStringLiteral("debris"),
                "Expected inline line subtype parsing to populate feature.subtype.")) {
        return 1;
    }

    bool foundArea = false;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind != MapGeometryFeature::Kind::Area) {
            continue;
        }
        foundArea = true;
        if (!expect(feature.label == QStringLiteral("water"),
                    "Expected inline area subtype parsing to keep only the type in feature.label.")) {
            return 1;
        }
        if (!expect(feature.subtype == QStringLiteral("temporary"),
                    "Expected inline area subtype parsing to populate feature.subtype.")) {
            return 1;
        }
    }
    if (!expect(foundArea, "Expected one area feature for inline subtype parsing test.")) {
        return 1;
    }

    return 0;
}

int runSlopeLinePointOptionsParsingTest()
{
    const QString text =
        QStringLiteral("line slope\n"
                       "  0 0\n"
                       "  l-size 20\n"
                       "  orientation 0\n"
                       "  10 0\n"
                       "  l-size 40\n"
                       "  orientation 90\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed slope line feature.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2,
                "Expected slope line to preserve two anchor vertices.")) {
        return 1;
    }
    if (!expect(line->lineVertices.first().leftSize.has_value()
                    && std::abs(line->lineVertices.first().leftSize.value() - 20.0) < 1e-6
                    && line->lineVertices.first().orientationDegrees.has_value()
                    && std::abs(line->lineVertices.first().orientationDegrees.value()) < 1e-6,
                "Expected first slope anchor to retain l-size and orientation metadata.")) {
        return 1;
    }
    if (!expect(line->lineVertices.last().leftSize.has_value()
                    && std::abs(line->lineVertices.last().leftSize.value() - 40.0) < 1e-6
                    && line->lineVertices.last().orientationDegrees.has_value()
                    && std::abs(line->lineVertices.last().orientationDegrees.value() - 90.0) < 1e-6,
                "Expected second slope anchor to retain l-size and orientation metadata.")) {
        return 1;
    }

    return 0;
}

int runLineStandaloneRowsPreservedForRewriteRowsTest()
{
    const QString text =
        QStringLiteral("line slope\n"
                       "  0 0\n"
                       "  altitude .\n"
                       "  l-size 20\n"
                       "  orientation 45\n"
                       "  10 0\n"
                       "  adjust horizontal\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for standalone-row preservation test.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2,
                "Expected two parsed anchor vertices in standalone-row preservation test.")) {
        return 1;
    }
    if (!expect(line->lineVertices.at(0).standaloneOptionRows.contains(QStringLiteral("altitude .")),
                "Expected first line vertex to preserve unknown standalone row 'altitude .' for rewrite output.")) {
        return 1;
    }
    if (!expect(line->lineVertices.at(1).standaloneOptionRows.contains(QStringLiteral("adjust horizontal")),
                "Expected second line vertex to preserve unknown standalone row 'adjust horizontal' for rewrite output.")) {
        return 1;
    }

    return 0;
}

int runLineStandaloneRowsCoverageTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  0 0\n"
                       "  smooth auto\n"
                       "  adjust horizontal\n"
                       "  mark section\n"
                       "  altitude [fix 1510 m]\n"
                       "  direction point\n"
                       "  gradient point\n"
                       "  10 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for standalone-row coverage test.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2,
                "Expected two parsed anchor vertices in standalone-row coverage test.")) {
        return 1;
    }
    const QStringList firstVertexRows = line->lineVertices.at(0).standaloneOptionRows;
    if (!expect(firstVertexRows.contains(QStringLiteral("smooth auto")),
                "Expected first line vertex to preserve 'smooth auto' standalone row.")) {
        return 1;
    }
    if (!expect(firstVertexRows.contains(QStringLiteral("adjust horizontal")),
                "Expected first line vertex to preserve 'adjust horizontal' standalone row.")) {
        return 1;
    }
    if (!expect(firstVertexRows.contains(QStringLiteral("mark section")),
                "Expected first line vertex to preserve 'mark section' standalone row.")) {
        return 1;
    }
    if (!expect(firstVertexRows.contains(QStringLiteral("altitude [fix 1510 m]")),
                "Expected first line vertex to preserve 'altitude [fix 1510 m]' standalone row.")) {
        return 1;
    }
    if (!expect(firstVertexRows.contains(QStringLiteral("direction point")),
                "Expected first line vertex to preserve 'direction point' standalone row.")) {
        return 1;
    }
    if (!expect(firstVertexRows.contains(QStringLiteral("gradient point")),
                "Expected first line vertex to preserve 'gradient point' standalone row.")) {
        return 1;
    }

    return 0;
}

int runLineStandaloneKnownOptionsNormalizationTest()
{
    const QString slopeText =
        QStringLiteral("line slope\n"
                       "  0 0\n"
                       "  -orientation 45\n"
                       "  -size 20\n"
                       "  10 0\n"
                       "endline\n");

    QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(slopeText);
    QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *slopeLine = firstLineFeature(features);
    if (!expect(slopeLine != nullptr, "Expected one parsed slope line feature for normalization test.")) {
        return 1;
    }
    if (!expect(slopeLine->lineVertices.size() == 2,
                "Expected two parsed slope anchors in normalization test.")) {
        return 1;
    }
    if (!expect(slopeLine->lineVertices.at(0).orientationDegrees.has_value()
                    && std::abs(slopeLine->lineVertices.at(0).orientationDegrees.value() - 45.0) < 1e-6,
                "Expected -orientation option row to normalize into first slope vertex orientation.")) {
        return 1;
    }
    if (!expect(slopeLine->lineVertices.at(0).leftSize.has_value()
                    && std::abs(slopeLine->lineVertices.at(0).leftSize.value() - 20.0) < 1e-6,
                "Expected -size option row to normalize into first slope vertex l-size metadata.")) {
        return 1;
    }
    if (!expect(!slopeLine->lineVertices.at(0).standaloneOptionRows.contains(QStringLiteral("-orientation 45"))
                    && !slopeLine->lineVertices.at(0).standaloneOptionRows.contains(QStringLiteral("-size 20")),
                "Expected recognized dashed slope option rows to avoid duplicate standalone preservation.")) {
        return 1;
    }

    const QString wallText =
        QStringLiteral("line wall\n"
                       "  0 0\n"
                       "  size 15\n"
                       "  10 0\n"
                       "endline\n");

    parsedLines = TherionDocumentParser::parseTokenLines(wallText);
    features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *wallLine = firstLineFeature(features);
    if (!expect(wallLine != nullptr, "Expected one parsed wall line feature in normalization test.")) {
        return 1;
    }
    if (!expect(wallLine->lineVertices.size() == 2,
                "Expected two parsed wall anchors in normalization test.")) {
        return 1;
    }
    if (!expect(wallLine->lineVertices.at(0).standaloneOptionRows.contains(QStringLiteral("size 15")),
                "Expected non-slope size row to remain preserved as standalone data for round-trip safety.")) {
        return 1;
    }

    return 0;
}

int runAnchorEquivalentControlNormalizationTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  0 0\n"
                       "  0 0 10 0 10 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for control normalization test.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2, "Expected two anchors in normalization test.")) {
        return 1;
    }
    if (!expect(!line->lineVertices.at(0).outgoingControl.has_value()
                && !line->lineVertices.at(1).incomingControl.has_value(),
                "Expected controls equal to anchor coordinates to normalize back to nil handles in memory.")) {
        return 1;
    }

    return 0;
}

int runLineOptionToggleParsingTest()
{
    const QString text =
        QStringLiteral("line wall -close on -reverse on\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  10 10\n"
                       "endline\n"
                       "line wall -close off -reverse off\n"
                       "  20 20\n"
                       "  30 20\n"
                       "  30 30\n"
                       "endline\n"
                       "line wall -close\n"
                       "  40 40\n"
                       "  50 40\n"
                       "  50 50\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);

    QVector<const MapGeometryFeature *> lines;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line) {
            lines.append(&feature);
        }
    }

    if (!expect(lines.size() == 3, "Expected three parsed line features for option-toggle parsing test.")) {
        return 1;
    }

    if (!expect(lines.at(0)->closed && lines.at(0)->reversed,
                "Expected first line to parse -close on and -reverse on as enabled.")) {
        return 1;
    }
    if (!expect(!lines.at(1)->closed && !lines.at(1)->reversed,
                "Expected second line to parse -close off and -reverse off as disabled.")) {
        return 1;
    }
    if (!expect(lines.at(2)->closed,
                "Expected bare -close option to enable closed-state parsing.")) {
        return 1;
    }

    return 0;
}

int runClosedLineClosingBezierParsingTest()
{
    const QString text =
        QStringLiteral("line wall -close on\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  12 2 -2 2 0 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed closed line feature for closing-bezier test.")) {
        return 1;
    }
    if (!expect(line->closed, "Expected line to remain marked as closed.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2,
                "Expected closing Bezier row to map into last->first controls without adding duplicate anchor vertex.")) {
        return 1;
    }
    if (!expect(line->lineSegments.size() == 2,
                "Expected closed line to include explicit closing segment metadata.")) {
        return 1;
    }
    if (!expect(line->lineVertices.first().incomingControl.has_value()
                && line->lineVertices.last().outgoingControl.has_value(),
                "Expected closing Bezier controls to attach to first incoming and last outgoing handles.")) {
        return 1;
    }
    if (!expect(std::abs(line->lineVertices.first().incomingControl.value().x() - (-2.0)) < 1e-6
                && std::abs(line->lineVertices.first().incomingControl.value().y() - 2.0) < 1e-6,
                "Expected first incoming control to preserve closing-segment control point.")) {
        return 1;
    }
    if (!expect(std::abs(line->lineVertices.last().outgoingControl.value().x() - 12.0) < 1e-6
                && std::abs(line->lineVertices.last().outgoingControl.value().y() - 2.0) < 1e-6,
                "Expected last outgoing control to preserve closing-segment control point.")) {
        return 1;
    }

    return 0;
}

int runTwoAnchorClosedBezierRenderingTest()
{
    const QString text =
        QStringLiteral("encoding utf-8\n"
                       "\n"
                       "scrap scrap-1\n"
                       "  line wall -close on\n"
                       "    0.2 0.6\n"
                       "    0.4 0.4 0.4 0.4 0.2 0.4\n"
                       "  endline\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for two-anchor closed Bezier rendering test.")) {
        return 1;
    }
    if (!expect(line->closed && line->lineVertices.size() == 2,
                "Expected the fixture to parse as a closed line with exactly two anchors.")) {
        return 1;
    }

    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> mapItemsByLine;
    renderMapWorkspaceScene(&scene,
                            QStringLiteral("fixture.th2"),
                            collectMapSceneEntries(parsedLines),
                            features,
                            std::nullopt,
                            false,
                            &mapItemsByLine,
                            nullptr,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {});

    auto *pathItem = dynamic_cast<QGraphicsPathItem *>(mapItemsByLine.value(line->lineNumber, nullptr));
    if (!expect(pathItem != nullptr, "Expected rendered closed line item to be available by source line.")) {
        return 1;
    }

    const QPainterPath path = pathItem->path();
    if (!expect(path.elementCount() >= 5,
                "Expected two-anchor closed Bezier rendering to include the closing segment back to the first anchor.")) {
        return 1;
    }

    const QPainterPath::Element first = path.elementAt(0);
    const QPainterPath::Element last = path.elementAt(path.elementCount() - 1);
    if (!expect(std::abs(first.x - last.x) < 1e-6 && std::abs(first.y - last.y) < 1e-6,
                "Expected the rendered two-anchor closed Bezier path to end at its first anchor.")) {
        return 1;
    }

    int checkedLinePathItems = 0;
    for (QGraphicsItem *item : scene.items()) {
        if (item == nullptr || item->data(kMapSceneLineNumberRole).toInt() != line->lineNumber) {
            continue;
        }
        auto *candidatePathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (candidatePathItem == nullptr) {
            continue;
        }
        const QPainterPath candidatePath = candidatePathItem->path();
        if (candidatePath.elementCount() < 2 || candidatePath.boundingRect().isEmpty()) {
            continue;
        }
        ++checkedLinePathItems;
        if (!expect(candidatePath.toFillPolygon().isClosed(),
                    "Expected every rendered path layer for a closed line to expose a closed polygon.")) {
            return 1;
        }
    }
    if (!expect(checkedLinePathItems >= 1,
                "Expected at least one path layer for the two-anchor closed Bezier line.")) {
        return 1;
    }

    return 0;
}

int runExplicitClosedBezierRowRenderingTest()
{
    const QString text =
        QStringLiteral("scrap hipd_r2\n"
                       "line wall -close on\n"
                       "  301.5 1049.5\n"
                       "  356.0 1057.5 374.0 1159.0 319.0 1182.0\n"
                       "  251.04 1210.42 253.5 1060.5 301.5 1049.5\n"
                       "  smooth off\n"
                       "endline\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for explicit closed Bezier fixture.")) {
        return 1;
    }
    if (!expect(line->closed, "Expected explicit closed Bezier fixture to parse as closed.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2,
                "Expected explicit closing Bezier row to avoid adding a duplicate first anchor.")) {
        return 1;
    }
    if (!expect(line->lineSegments.size() == 2,
                "Expected explicit closed Bezier fixture to keep forward and closing segments.")) {
        return 1;
    }
    if (!expect(line->lineSegments.last().endVertexIndex == 0,
                "Expected explicit closing Bezier segment to target the first anchor.")) {
        return 1;
    }
    if (!expect(line->lineVertices.last().outgoingControl.has_value()
                && line->lineVertices.first().incomingControl.has_value(),
                "Expected explicit closing Bezier controls to be attached to last and first anchors.")) {
        return 1;
    }

    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> mapItemsByLine;
    renderMapWorkspaceScene(&scene,
                            QStringLiteral("fixture.th2"),
                            collectMapSceneEntries(parsedLines),
                            features,
                            std::nullopt,
                            false,
                            &mapItemsByLine,
                            nullptr,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {});

    auto *pathItem = dynamic_cast<QGraphicsPathItem *>(mapItemsByLine.value(line->lineNumber, nullptr));
    if (!expect(pathItem != nullptr, "Expected rendered explicit closed Bezier line item to be available.")) {
        return 1;
    }

    const QPainterPath path = pathItem->path();
    if (!expect(path.elementCount() >= 7,
                "Expected explicit closed Bezier path to contain two cubic segments plus closure.")) {
        return 1;
    }
    if (!expect(path.toFillPolygon().isClosed(),
                "Expected explicit closed Bezier path to expose a closed polygon.")) {
        return 1;
    }

    return 0;
}

int runDeCasteljauInsertionTest()
{
    QVector<MapGeometryFeature::TH2LineVertex> vertices;
    MapGeometryFeature::TH2LineVertex start;
    start.anchor = QPointF(0.0, 0.0);
    start.outgoingControl = QPointF(10.0, 0.0);
    MapGeometryFeature::TH2LineVertex end;
    end.anchor = QPointF(10.0, 10.0);
    end.incomingControl = QPointF(12.0, 10.0);
    vertices.append(start);
    vertices.append(end);

    int insertedIndex = -1;
    if (!expect(insertLineVertexByDeCasteljau(&vertices, 0, 0.5, &insertedIndex),
                "Expected de Casteljau insertion to succeed for cubic segment.")) {
        return 1;
    }
    if (!expect(vertices.size() == 3 && insertedIndex == 1,
                "Expected one inserted anchor between the original segment endpoints.")) {
        return 1;
    }

    const MapGeometryFeature::TH2LineVertex &inserted = vertices.at(1);
    if (!expect(std::abs(inserted.anchor.x() - 9.5) < 1e-6 && std::abs(inserted.anchor.y() - 5.0) < 1e-6,
                "Expected inserted anchor to match de Casteljau midpoint for the source cubic.")) {
        return 1;
    }
    if (!expect(vertices.at(0).outgoingControl.has_value(),
                "Expected start outgoing control to remain defined after split.")) {
        return 1;
    }
    if (!expect(vertices.at(2).incomingControl.has_value(),
                "Expected end incoming control to be synthesized by split when original segment was cubic.")) {
        return 1;
    }
    if (!expect(inserted.incomingControl.has_value() && inserted.outgoingControl.has_value(),
                "Expected inserted vertex to carry both incoming and outgoing controls for cubic split.")) {
        return 1;
    }

    return 0;
}

int runLineVertexRemovalReconnectTest()
{
    QVector<MapGeometryFeature::TH2LineVertex> vertices;
    MapGeometryFeature::TH2LineVertex first;
    first.anchor = QPointF(0.0, 0.0);
    first.outgoingControl = QPointF(2.0, 0.0);
    MapGeometryFeature::TH2LineVertex middle;
    middle.anchor = QPointF(5.0, 5.0);
    middle.incomingControl = QPointF(4.0, 5.0);
    middle.outgoingControl = QPointF(6.0, 5.0);
    MapGeometryFeature::TH2LineVertex last;
    last.anchor = QPointF(10.0, 10.0);
    last.incomingControl = QPointF(9.0, 10.0);
    vertices.append(first);
    vertices.append(middle);
    vertices.append(last);

    if (!expect(removeLineVertexWithReconnect(&vertices, 1),
                "Expected middle-vertex removal to succeed for reconnect test.")) {
        return 1;
    }
    if (!expect(vertices.size() == 2, "Expected removal to shrink line vertex sequence by one.")) {
        return 1;
    }
    if (!expect(vertices.at(0).outgoingControl.has_value()
                && vertices.at(0).outgoingControl.value() == QPointF(4.0, 5.0),
                "Expected previous outgoing control to reconnect through removed incoming control.")) {
        return 1;
    }
    if (!expect(vertices.at(1).incomingControl.has_value()
                && vertices.at(1).incomingControl.value() == QPointF(6.0, 5.0),
                "Expected next incoming control to reconnect through removed outgoing control.")) {
        return 1;
    }

    return 0;
}

int runSmoothMirrorHelperTest()
{
    const QPointF anchor(10.0, 20.0);
    const QPointF movedIncoming(16.0, 24.0); // vector length sqrt(52)
    const QPointF oldOutgoing(4.0, 20.0);    // opposite length 6.0

    const std::optional<QPointF> mirroredFromExisting = mirroredSmoothControlPoint(anchor,
                                                                                     movedIncoming,
                                                                                     oldOutgoing);
    if (!expect(mirroredFromExisting.has_value(),
                "Expected mirroredSmoothControlPoint to produce a point for non-zero drag vector.")) {
        return 1;
    }
    const QPointF mirrored = mirroredFromExisting.value();
    const qreal incomingLength = std::hypot(movedIncoming.x() - anchor.x(), movedIncoming.y() - anchor.y());
    const qreal outgoingLength = std::hypot(mirrored.x() - anchor.x(), mirrored.y() - anchor.y());
    if (!expect(std::abs(outgoingLength - 6.0) < 1e-6,
                "Expected mirrored smooth opposite to preserve existing opposite-handle length.")) {
        return 1;
    }
    if (!expect(std::abs(((movedIncoming.x() - anchor.x()) * (mirrored.x() - anchor.x()))
                       + ((movedIncoming.y() - anchor.y()) * (mirrored.y() - anchor.y()))
                       + (incomingLength * outgoingLength)) < 1e-4,
                "Expected mirrored smooth opposite to remain collinear and opposite to moved handle.")) {
        return 1;
    }

    const std::optional<QPointF> mirroredFromMissing = mirroredSmoothControlPoint(anchor,
                                                                                   movedIncoming,
                                                                                   std::nullopt);
    if (!expect(mirroredFromMissing.has_value(),
                "Expected mirroredSmoothControlPoint to create a mirrored point when opposite handle is missing.")) {
        return 1;
    }
    const qreal mirroredFromMissingLength = std::hypot(mirroredFromMissing->x() - anchor.x(),
                                                       mirroredFromMissing->y() - anchor.y());
    if (!expect(std::abs(mirroredFromMissingLength - incomingLength) < 1e-6,
                "Expected missing opposite handle to mirror using moved-handle length.")) {
        return 1;
    }

    const std::optional<QPointF> collapsed = mirroredSmoothControlPoint(anchor, anchor, oldOutgoing);
    if (!expect(!collapsed.has_value(),
                "Expected mirroredSmoothControlPoint to return nullopt for zero-length moved vector.")) {
        return 1;
    }

    return 0;
}

int runPreviewToSourceUnclampedTest()
{
    const QRectF sourceBounds(0.0, -50.0, 100.0, 50.0);
    const QRectF previewBounds(0.0, 0.0, 200.0, 200.0);

    // This point is below the fitted preview rect for source bounds.
    const QPointF previewPoint(100.0, 190.0);
    const QPointF sourcePoint = mapGeometryPreviewToSource(previewPoint, sourceBounds, previewBounds);
    if (!expect(sourcePoint.y() < sourceBounds.top(),
                "Expected preview-to-source conversion to stay unclamped beyond fitted bounds (y below source top).")) {
        return 1;
    }

    // This point is above the fitted preview rect for source bounds.
    const QPointF previewPointAbove(100.0, 10.0);
    const QPointF sourcePointAbove = mapGeometryPreviewToSource(previewPointAbove, sourceBounds, previewBounds);
    if (!expect(sourcePointAbove.y() > sourceBounds.bottom(),
                "Expected preview-to-source conversion to stay unclamped beyond fitted bounds (y above source bottom).")) {
        return 1;
    }

    return 0;
}

int runLineAnchorSecondaryMoveCouplingTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 190.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 210.0);
    vertex.outgoingSourceVertexIndex = 12;
    lineFeature.lineVertices.append(vertex);

    const QPointF oldAnchor(100.0, 200.0);
    const QPointF newAnchor(104.0, 197.0);
    const QPointF delta = newAnchor - oldAnchor;

    const QVector<MapLineSecondaryMove> secondaryMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                  10,
                                                                                                  oldAnchor,
                                                                                                  newAnchor);
    if (!expect(secondaryMoves.size() == 2,
                "Expected anchor drag to produce two secondary moves (incoming + outgoing control).")) {
        return 1;
    }

    bool incomingVerified = false;
    bool outgoingVerified = false;
    for (const MapLineSecondaryMove &move : secondaryMoves) {
        if (move.sourceVertexIndex == 11) {
            incomingVerified = (move.oldPoint == QPointF(90.0, 190.0))
                && (move.newPoint == QPointF(90.0, 190.0) + delta);
        }
        if (move.sourceVertexIndex == 12) {
            outgoingVerified = (move.oldPoint == QPointF(110.0, 210.0))
                && (move.newPoint == QPointF(110.0, 210.0) + delta);
        }
    }

    if (!expect(incomingVerified,
                "Expected incoming control secondary move to translate by anchor drag delta.")) {
        return 1;
    }
    if (!expect(outgoingVerified,
                "Expected outgoing control secondary move to translate by anchor drag delta.")) {
        return 1;
    }

    return 0;
}

int runLineControlSecondaryMoveSmoothTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 200.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 200.0);
    vertex.outgoingSourceVertexIndex = 12;
    vertex.isSmooth = true;
    lineFeature.lineVertices.append(vertex);

    const QPointF oldIncoming = vertex.incomingControl.value();
    const QPointF newIncoming(85.0, 195.0);

    const QVector<MapLineSecondaryMove> secondaryMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                  11,
                                                                                                  oldIncoming,
                                                                                                  newIncoming);
    if (!expect(secondaryMoves.size() == 1,
                "Expected smooth incoming-control drag to produce one opposite-control secondary move.")) {
        return 1;
    }

    const MapLineSecondaryMove &move = secondaryMoves.first();
    if (!expect(move.sourceVertexIndex == 12,
                "Expected smooth incoming-control drag to target outgoing control source vertex.")) {
        return 1;
    }
    if (!expect(move.oldPoint == QPointF(110.0, 200.0),
                "Expected opposite-control secondary move to preserve original outgoing control point.")) {
        return 1;
    }

    const qreal oldOppositeLength = std::hypot(move.oldPoint.x() - vertex.anchor.x(),
                                                move.oldPoint.y() - vertex.anchor.y());
    const qreal newOppositeLength = std::hypot(move.newPoint.x() - vertex.anchor.x(),
                                                move.newPoint.y() - vertex.anchor.y());
    if (!expect(std::abs(oldOppositeLength - newOppositeLength) < 1e-6,
                "Expected smooth opposite-control move to preserve prior opposite handle length.")) {
        return 1;
    }

    return 0;
}

int runLineControlSecondaryMoveCornerTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 200.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 200.0);
    vertex.outgoingSourceVertexIndex = 12;
    vertex.isSmooth = false;
    lineFeature.lineVertices.append(vertex);

    const QVector<MapLineSecondaryMove> secondaryMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                  11,
                                                                                                  QPointF(90.0, 200.0),
                                                                                                  QPointF(85.0, 195.0));
    if (!expect(secondaryMoves.isEmpty(),
                "Expected corner vertex control drag to keep handles independent (no secondary moves).")) {
        return 1;
    }

    return 0;
}

int runLinePreviewSecondaryMoveSelfFilterTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 200.0);
    vertex.incomingSourceVertexIndex = 11;
    // Malformed but possible in partially rewritten sources: opposite maps to same source index.
    vertex.outgoingControl = QPointF(110.0, 200.0);
    vertex.outgoingSourceVertexIndex = 11;
    vertex.isSmooth = true;
    lineFeature.lineVertices.append(vertex);

    const QVector<MapLineSecondaryMove> commandMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                               11,
                                                                                               QPointF(90.0, 200.0),
                                                                                               QPointF(86.0, 196.0));
    if (!expect(commandMoves.size() == 1,
                "Expected command-time coupling to include malformed self-target move before preview filtering.")) {
        return 1;
    }

    const QVector<MapLineSecondaryMove> previewMoves = collectLinePreviewSecondaryMovesForVertexDrag(lineFeature,
                                                                                                      11,
                                                                                                      QPointF(90.0, 200.0),
                                                                                                      QPointF(86.0, 196.0));
    if (!expect(previewMoves.isEmpty(),
                "Expected preview coupling helper to filter out self-target secondary moves.")) {
        return 1;
    }

    return 0;
}

int runLinePreviewAnchorSequencingNoStaleTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 190.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 210.0);
    vertex.outgoingSourceVertexIndex = 12;
    lineFeature.lineVertices.append(vertex);

    QHash<int, QPointF> currentControls;
    currentControls.insert(11, QPointF(90.0, 190.0));
    currentControls.insert(12, QPointF(110.0, 210.0));

    const QPointF step0(100.0, 200.0);
    const QPointF step1(103.0, 198.0);
    const QPointF step2(106.0, 197.0);

    const QVector<MapLineSecondaryMove> firstStepMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                        10,
                                                                                                        step0,
                                                                                                        step1,
                                                                                                        currentControls);
    for (const MapLineSecondaryMove &move : firstStepMoves) {
        currentControls.insert(move.sourceVertexIndex, move.newPoint);
    }

    const QVector<MapLineSecondaryMove> secondStepMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                         10,
                                                                                                         step1,
                                                                                                         step2,
                                                                                                         currentControls);
    for (const MapLineSecondaryMove &move : secondStepMoves) {
        currentControls.insert(move.sourceVertexIndex, move.newPoint);
    }

    const QPointF totalDelta = step2 - step0;
    if (!expect(currentControls.value(11) == QPointF(90.0, 190.0) + totalDelta,
                "Expected incoming preview control transport to accumulate drag deltas (no stale absolute reset).")) {
        return 1;
    }
    if (!expect(currentControls.value(12) == QPointF(110.0, 210.0) + totalDelta,
                "Expected outgoing preview control transport to accumulate drag deltas (no stale absolute reset).")) {
        return 1;
    }

    return 0;
}

int runLinePreviewCommitParityTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex smoothVertex;
    smoothVertex.anchor = QPointF(100.0, 200.0);
    smoothVertex.anchorSourceVertexIndex = 10;
    smoothVertex.incomingControl = QPointF(90.0, 190.0);
    smoothVertex.incomingSourceVertexIndex = 11;
    smoothVertex.outgoingControl = QPointF(110.0, 210.0);
    smoothVertex.outgoingSourceVertexIndex = 12;
    smoothVertex.isSmooth = true;
    lineFeature.lineVertices.append(smoothVertex);

    MapGeometryFeature::TH2LineVertex cornerVertex;
    cornerVertex.anchor = QPointF(300.0, 400.0);
    cornerVertex.anchorSourceVertexIndex = 20;
    cornerVertex.incomingControl = QPointF(290.0, 390.0);
    cornerVertex.incomingSourceVertexIndex = 21;
    cornerVertex.outgoingControl = QPointF(310.0, 410.0);
    cornerVertex.outgoingSourceVertexIndex = 22;
    cornerVertex.isSmooth = false;
    lineFeature.lineVertices.append(cornerVertex);

    QHash<int, QPointF> currentControls;
    currentControls.insert(11, smoothVertex.incomingControl.value());
    currentControls.insert(12, smoothVertex.outgoingControl.value());
    currentControls.insert(21, cornerVertex.incomingControl.value());
    currentControls.insert(22, cornerVertex.outgoingControl.value());

    // 1) Anchor drag parity.
    const QVector<MapLineSecondaryMove> commandAnchorMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                     10,
                                                                                                     smoothVertex.anchor,
                                                                                                     QPointF(104.0, 197.0));
    const QVector<MapLineSecondaryMove> previewAnchorMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                            10,
                                                                                                            smoothVertex.anchor,
                                                                                                            QPointF(104.0, 197.0),
                                                                                                            currentControls);
    if (!expectMoveSetsEqual(commandAnchorMoves,
                             previewAnchorMoves,
                             "Expected preview and commit coupling parity for anchor drag.")) {
        return 1;
    }

    // 2) Smooth control drag parity.
    const QVector<MapLineSecondaryMove> commandSmoothControlMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                            11,
                                                                                                            smoothVertex.incomingControl.value(),
                                                                                                            QPointF(84.0, 196.0));
    const QVector<MapLineSecondaryMove> previewSmoothControlMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                                   11,
                                                                                                                   smoothVertex.incomingControl.value(),
                                                                                                                   QPointF(84.0, 196.0),
                                                                                                                   currentControls);
    if (!expectMoveSetsEqual(commandSmoothControlMoves,
                             previewSmoothControlMoves,
                             "Expected preview and commit coupling parity for smooth control drag.")) {
        return 1;
    }

    // 3) Corner control drag parity (no coupled move).
    const QVector<MapLineSecondaryMove> commandCornerControlMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                            21,
                                                                                                            cornerVertex.incomingControl.value(),
                                                                                                            QPointF(286.0, 388.0));
    const QVector<MapLineSecondaryMove> previewCornerControlMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                                   21,
                                                                                                                   cornerVertex.incomingControl.value(),
                                                                                                                   QPointF(286.0, 388.0),
                                                                                                                   currentControls);
    if (!expectMoveSetsEqual(commandCornerControlMoves,
                             previewCornerControlMoves,
                             "Expected preview and commit coupling parity for corner control drag.")) {
        return 1;
    }

    return 0;
}

int runSmoothControlCommitKeepsTangentContinuityTest()
{
    const QString beforeText =
        QStringLiteral("line wall\n"
                       "  0 0\n"
                       "  40 0 60 0 100 0\n"
                       "  140 0 160 0 200 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> beforeParsedLines = TherionDocumentParser::parseTokenLines(beforeText);
    const QVector<MapGeometryFeature> beforeFeatures = collectGeometryFeatures(beforeParsedLines);
    const MapGeometryFeature *beforeLine = firstLineFeature(beforeFeatures);
    if (!expect(beforeLine != nullptr, "Expected smooth-control commit fixture to parse a line.")) {
        return 1;
    }
    if (!expect(beforeLine->lineVertices.size() == 3,
                "Expected smooth-control commit fixture to parse three line anchors.")) {
        return 1;
    }

    const MapGeometryFeature::TH2LineVertex &middleVertex = beforeLine->lineVertices.at(1);
    if (!expect(middleVertex.incomingControl.has_value()
                    && middleVertex.outgoingControl.has_value()
                    && middleVertex.isSmooth,
                "Expected middle vertex to start as a smooth Bezier vertex with both controls.")) {
        return 1;
    }

    const int movedSourceVertexIndex = middleVertex.incomingSourceVertexIndex;
    const QPointF oldIncoming = middleVertex.incomingControl.value();
    const QPointF newIncoming(70.0, 35.0);
    const QVector<MapLineSecondaryMove> secondaryMoves = collectLineSecondaryMovesForVertexDrag(*beforeLine,
                                                                                                  movedSourceVertexIndex,
                                                                                                  oldIncoming,
                                                                                                  newIncoming);
    if (!expect(secondaryMoves.size() == 1,
                "Expected smooth control source commit to rewrite the opposite control.")) {
        return 1;
    }

    QString afterText = beforeText;
    QVector<TherionSourceTextEdit> sourceEdits;
    QString errorMessage;
    if (!expect(TherionDocumentEditor::lineAreaVertexRewriteEdits(afterText,
                                                                  beforeLine->lineNumber,
                                                                  QStringLiteral("line"),
                                                                  movedSourceVertexIndex,
                                                                  newIncoming,
                                                                  &sourceEdits,
                                                                  &errorMessage)
                    && TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage),
                errorMessage.toUtf8().constData())) {
        return 1;
    }

    for (const MapLineSecondaryMove &move : secondaryMoves) {
        sourceEdits.clear();
        if (!expect(TherionDocumentEditor::lineAreaVertexRewriteEdits(afterText,
                                                                      beforeLine->lineNumber,
                                                                      QStringLiteral("line"),
                                                                      move.sourceVertexIndex,
                                                                      move.newPoint,
                                                                      &sourceEdits,
                                                                      &errorMessage)
                        && TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage),
                    errorMessage.toUtf8().constData())) {
            return 1;
        }
    }

    const QVector<TherionParsedLine> afterParsedLines = TherionDocumentParser::parseTokenLines(afterText);
    const QVector<MapGeometryFeature> afterFeatures = collectGeometryFeatures(afterParsedLines);
    const MapGeometryFeature *afterLine = firstLineFeature(afterFeatures);
    if (!expect(afterLine != nullptr && afterLine->lineVertices.size() == 3,
                "Expected smooth-control commit output to parse the rewritten line.")) {
        return 1;
    }

    const MapGeometryFeature::TH2LineVertex &rewrittenMiddle = afterLine->lineVertices.at(1);
    if (!expect(rewrittenMiddle.incomingControl.has_value() && rewrittenMiddle.outgoingControl.has_value(),
                "Expected rewritten smooth vertex to retain both controls.")) {
        return 1;
    }
    const bool tangentContinuous = pointsAreCollinear(rewrittenMiddle.incomingControl.value(),
                                                      rewrittenMiddle.anchor,
                                                      rewrittenMiddle.outgoingControl.value());
    if (!tangentContinuous) {
        std::cerr << "Rewritten source:\n" << afterText.toStdString();
        std::cerr << "Incoming: "
                  << rewrittenMiddle.incomingControl->x() << ','
                  << rewrittenMiddle.incomingControl->y()
                  << " anchor: " << rewrittenMiddle.anchor.x() << ','
                  << rewrittenMiddle.anchor.y()
                  << " outgoing: " << rewrittenMiddle.outgoingControl->x() << ','
                  << rewrittenMiddle.outgoingControl->y() << '\n';
    }
    if (!expect(tangentContinuous,
                "Expected rewritten smooth controls to remain tangent-continuous around the anchor.")) {
        return 1;
    }

    return 0;
}

int runScrapScaleSourceUnitsPerMeterTest()
{
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap clopy01 -projection plan -scale [-128 -1152 851 -1152 0.0 0.0 24.8666 0.0 m]"),
        1);
    const std::optional<qreal> sourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(parsedLine.tokens);
    if (!expect(sourceUnitsPerMeter.has_value(), "Expected source-units-per-meter ratio to parse from scrap scale.")) {
        return 1;
    }
    if (!expect(std::abs(sourceUnitsPerMeter.value() - 39.3698) < 0.01,
                "Expected TH2 source units per meter to account for scrap -scale length.")) {
        return 1;
    }

    const TherionParsedLine centimeterScaleLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap cm -scale [0 0 100 0 0 0 1000 0 cm]"),
        1);
    const std::optional<qreal> centimeterSourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(centimeterScaleLine.tokens);
    if (!expect(centimeterSourceUnitsPerMeter.has_value(), "Expected centimeter scrap scale unit to parse.")) {
        return 1;
    }
    if (!expect(std::abs(centimeterSourceUnitsPerMeter.value() - 10.0) < 0.001,
                "Expected centimeter unit conversion to produce source units per meter.")) {
        return 1;
    }

    const TherionParsedLine numericScaleLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap numeric -scale 0.0254"),
        1);
    const std::optional<qreal> numericSourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(numericScaleLine.tokens);
    if (!expect(numericSourceUnitsPerMeter.has_value(), "Expected numeric scrap scale to parse.")) {
        return 1;
    }
    if (!expect(std::abs(numericSourceUnitsPerMeter.value() - 39.3700787) < 0.001,
                "Expected numeric scrap scale to convert meters per source unit.")) {
        return 1;
    }

    const TherionParsedLine unitScaleLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap unit -scale [2.54 cm]"),
        1);
    const std::optional<qreal> unitSourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(unitScaleLine.tokens);
    if (!expect(unitSourceUnitsPerMeter.has_value(), "Expected two-token unit scrap scale to parse.")) {
        return 1;
    }
    if (!expect(std::abs(unitSourceUnitsPerMeter.value() - 39.3700787) < 0.001,
                "Expected unit scrap scale to convert length units.")) {
        return 1;
    }

    const TherionParsedLine ratioScaleLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap ratio -scale [100 1 m]"),
        1);
    const std::optional<qreal> ratioSourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(ratioScaleLine.tokens);
    if (!expect(ratioSourceUnitsPerMeter.has_value(), "Expected ratio scrap scale to parse.")) {
        return 1;
    }
    if (!expect(std::abs(ratioSourceUnitsPerMeter.value() - 100.0) < 0.001,
                "Expected ratio scrap scale to convert drawing units per real length.")) {
        return 1;
    }

    const CoordinateTransform coordinateTransform = coordinateTransformFromScrapScale(parsedLine.tokens);
    if (!expect(coordinateTransform.valid, "Expected full scrap scale coordinate transform to parse.")) {
        return 1;
    }
    const QPointF realPoint1 = coordinateTransform.sourceToMap.map(QPointF(-128.0, -1152.0));
    if (!expect(std::abs(realPoint1.x()) < 0.001 && std::abs(realPoint1.y()) < 0.001,
                "Expected scrap scale source point 1 to map to real origin.")) {
        return 1;
    }
    const QPointF sourceAtTenMeters = coordinateTransform.mapToSource.map(QPointF(10.0, 0.0));
    if (!expect(std::abs(sourceAtTenMeters.x() - 265.698) < 0.01
                    && std::abs(sourceAtTenMeters.y() + 1152.0) < 0.01,
                "Expected real 10m grid interval to map back through scrap scale calibration.")) {
        return 1;
    }

    return 0;
}

int runAreaScrapClipMetadataParsingTest()
{
    const QString text =
        QStringLiteral("scrap s1\n"
                       "line wall -id w1\n"
                       "  0 0\n"
                       "  10 0\n"
                       "endline\n"
                       "line wall -id w2\n"
                       "  10 0\n"
                       "  10 10\n"
                       "endline\n"
                       "line wall -id w3\n"
                       "  10 10\n"
                       "  0 10\n"
                       "endline\n"
                       "line wall -id w4\n"
                       "  0 10\n"
                       "  0 0\n"
                       "endline\n"
                       "area sand\n"
                       "  w1 w2 w3 w4\n"
                       "endarea\n"
                       "area clay -clip off -place top\n"
                       "  w1 w2 w3 w4\n"
                       "endarea\n"
                       "area bedrock -place bottom\n"
                       "  w1 w2 w3 w4\n"
                       "endarea\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);

    QVector<const MapGeometryFeature *> areas;
    QVector<const MapGeometryFeature *> lines;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Area) {
            areas.append(&feature);
        } else if (feature.kind == MapGeometryFeature::Kind::Line) {
            lines.append(&feature);
        }
    }

    if (!expect(lines.size() == 4, "Expected four wall line features for scrap clip metadata test.")) {
        return 1;
    }
    if (!expect(areas.size() == 3, "Expected three area features for scrap clip metadata test.")) {
        return 1;
    }
    if (!expect(lines.first()->scrapLineNumber == 1 && areas.first()->scrapLineNumber == 1,
                "Expected line and area features to retain their owning scrap source line.")) {
        return 1;
    }
    if (!expect(areas.first()->clipToScrap,
                "Expected area without -clip to default to clip-on rendering metadata.")) {
        return 1;
    }
    if (!expect(!areas.at(1)->clipToScrap,
                "Expected area -clip off to disable scrap clipping metadata.")) {
        return 1;
    }
    if (!expect(areas.at(0)->areaPlace == MapGeometryAreaPlace::Default
                    && areas.at(1)->areaPlace == MapGeometryAreaPlace::Top
                    && areas.at(2)->areaPlace == MapGeometryAreaPlace::Bottom,
                "Expected area -place metadata to parse as default, top, and bottom.")) {
        return 1;
    }
    if (!expect(!areas.first()->verticesEditable && areas.first()->vertices.size() >= 3,
                "Expected referenced area border lines to resolve into non-editable area geometry.")) {
        return 1;
    }

    return 0;
}

int runIntersectingReferencedAreaParsingTest()
{
    const QString text =
        QStringLiteral("scrap s1\n"
                       "line wall -id top\n"
                       "  -5 0\n"
                       "  15 0\n"
                       "endline\n"
                       "line wall -id right\n"
                       "  10 -5\n"
                       "  10 15\n"
                       "endline\n"
                       "line wall -id bottom\n"
                       "  15 10\n"
                       "  -5 10\n"
                       "endline\n"
                       "line wall -id left\n"
                       "  0 15\n"
                       "  0 -5\n"
                       "endline\n"
                       "area water\n"
                       "  top right bottom left\n"
                       "endarea\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);

    const MapGeometryFeature *area = nullptr;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Area) {
            area = &feature;
            break;
        }
    }

    if (!expect(area != nullptr, "Expected intersecting referenced lines to resolve an area feature.")) {
        return 1;
    }
    if (!expect(!area->verticesEditable, "Expected intersecting referenced area geometry to be non-editable.")) {
        return 1;
    }
    if (!expect(area->vertices.size() == 4,
                "Expected intersecting referenced area to resolve to the four intersection corners.")) {
        return 1;
    }

    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::lowest();
    qreal maxY = std::numeric_limits<qreal>::lowest();
    for (const QPointF &vertex : area->vertices) {
        minX = qMin(minX, vertex.x());
        minY = qMin(minY, vertex.y());
        maxX = qMax(maxX, vertex.x());
        maxY = qMax(maxY, vertex.y());
    }

    const bool resolvedBounds = std::abs(minX) < 0.001
        && std::abs(minY) < 0.001
        && std::abs(maxX - 10.0) < 0.001
        && std::abs(maxY - 10.0) < 0.001;
    if (!resolvedBounds) {
        std::cerr << "Resolved intersecting area bounds: "
                  << minX << ',' << minY << " - "
                  << maxX << ',' << maxY << '\n';
        for (const QPointF &vertex : area->vertices) {
            std::cerr << "  vertex " << vertex.x() << ',' << vertex.y() << '\n';
        }
    }
    return expect(resolvedBounds,
                  "Expected intersecting referenced area bounds to use line intersections, not open-line endpoints.")
        ? 0
        : 1;
}

int runCurvedIntersectingReferencedAreaParsingTest()
{
    const QString text =
        QStringLiteral("encoding utf-8\n"
                       "\n"
                       "scrap scrap-1\n"
                       "  line wall -id line-3\n"
                       "    0.1 0.3\n"
                       "    0.1 0.3 -0.1 0.6 0.2 0.6\n"
                       "    0.4 0.6 0.5 0.6 0.6 0.7\n"
                       "    0.6 0.7 0.6 0.9 0.7 0.9\n"
                       "  endline\n"
                       "  line wall -id line-1\n"
                       "    -0.1 0.7\n"
                       "    -0.1 0.7 -0.1 1.0 0.1 0.9\n"
                       "    0.4 0.8 0.5 1.1 0.6 1.1\n"
                       "    0.7 1.2 1.0 1.1 1.0 1.0\n"
                       "  endline\n"
                       "  line border -id line-4\n"
                       "    -0.1 0.9\n"
                       "    0.2 0.5\n"
                       "  endline\n"
                       "  line border -id line-2\n"
                       "    0.5 1.2\n"
                       "    0.7 0.7\n"
                       "  endline\n"
                       "\n"
                       "  area water\n"
                       "    line-1\n"
                       "    line-2\n"
                       "    line-3\n"
                       "    line-4\n"
                       "  endarea\n"
                       "\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);

    const MapGeometryFeature *area = nullptr;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Area) {
            area = &feature;
            break;
        }
    }

    if (!expect(area != nullptr, "Expected curved intersecting referenced lines to resolve an area feature.")) {
        return 1;
    }
    if (!expect(area->vertices.size() > 8,
                "Expected curved intersecting referenced area to preserve sampled boundary points, not only four chords.")) {
        return 1;
    }

    for (const QPointF &vertex : area->vertices) {
        if (std::abs(vertex.x() - 0.2) < 0.0001 && std::abs(vertex.y() - 0.5) < 0.0001) {
            return expect(false,
                          "Curved referenced area should not include the dangling line-4 endpoint below the closed face.")
                ? 0
                : 1;
        }
    }

    QGraphicsScene scene;
    QHash<int, QGraphicsItem *> mapItemsByLine;
    renderMapWorkspaceScene(&scene,
                            QStringLiteral("fixture.th2"),
                            collectMapSceneEntries(parsedLines),
                            features,
                            std::nullopt,
                            false,
                            &mapItemsByLine,
                            nullptr,
                            {},
                            {},
                            {},
                            {},
                            {},
                            {});

    auto *fillItem = dynamic_cast<QGraphicsPathItem *>(mapItemsByLine.value(area->lineNumber, nullptr));
    if (!expect(fillItem != nullptr, "Expected rendered curved referenced area fill item to be available.")) {
        return 1;
    }

    const QRectF sourceBounds = geometryBoundsForFeatures(features);
    const QRectF previewBounds = QRectF(0, 0, 1200, 900).adjusted(24.0, 24.0, -24.0, -24.0).adjusted(20.0, 20.0, -20.0, -20.0);
    QPainterPath expectedAreaPath;
    expectedAreaPath.moveTo(mapGeometryPointToPreview(area->vertices.first(), sourceBounds, previewBounds));
    for (int index = 1; index < area->vertices.size(); ++index) {
        expectedAreaPath.lineTo(mapGeometryPointToPreview(area->vertices.at(index), sourceBounds, previewBounds));
    }
    expectedAreaPath.closeSubpath();

    const QRectF expectedBounds = expectedAreaPath.boundingRect();
    const QRectF renderedBounds = fillItem->path().boundingRect();
    const bool sameBounds = std::abs(expectedBounds.left() - renderedBounds.left()) < 0.5
        && std::abs(expectedBounds.top() - renderedBounds.top()) < 0.5
        && std::abs(expectedBounds.right() - renderedBounds.right()) < 0.5
        && std::abs(expectedBounds.bottom() - renderedBounds.bottom()) < 0.5;
    if (!expect(sameBounds,
                "Expected open scrap wall fragments not to clip the curved referenced area fill path.")) {
        return 1;
    }

    return 0;
}

int runReferencedAreaPrefersFaceUsingMoreReferencedLinesTest()
{
    const QString text =
        QStringLiteral("scrap k_sifonu_iii_02.s -projection plan\n"
                       "line rock-border -close on -id line-1\n"
                       "  527.5 807.0\n"
                       "  528.5 801.0\n"
                       "  538.5 801.0\n"
                       "  527.5 807.0\n"
                       "endline\n"
                       "line wall -id line-3\n"
                       "  477.5 808.0\n"
                       "  477.5 808.0 507.54 814.27 543.5 804.0\n"
                       "  smooth off\n"
                       "  543.5 804.0 560.17 797.23 571.5 795.0\n"
                       "  602.0 789.0 603.5 792.0 623.5 780.0\n"
                       "  628.12 777.23 615.51 780.01 610.5 774.0\n"
                       "  608.0 771.0 586.5 778.0 550.5 779.0\n"
                       "  514.5 780.0 497.5 784.0 497.5 784.0\n"
                       "  smooth off\n"
                       "  497.5 784.0 471.5 774.0 456.5 766.0\n"
                       "  450.13 762.6 432.96 775.15 396.5 756.0\n"
                       "  376.6 745.55 339.0 769.5 339.5 776.0\n"
                       "  smooth off\n"
                       "endline\n"
                       "line border -id line-2\n"
                       "  544.6 807.0\n"
                       "  543.5 806.0 536.1 803.8 536.4 799.6\n"
                       "  536.6 796.7 537.1 793.5 537.2 789.5\n"
                       "  537.3 784.9 547.4 779.2 549.7 776.6\n"
                       "endline\n"
                       "line border -id line-4\n"
                       "  608.2 790.9\n"
                       "  608.8 789.6 612.6 786.0 611.9 782.8\n"
                       "  610.8 777.5 605.1 773.6 603.7 771.8\n"
                       "endline\n"
                       "area water\n"
                       "  line-1\n"
                       "  line-2\n"
                       "  line-3\n"
                       "  line-4\n"
                       "endarea\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);

    const MapGeometryFeature *area = nullptr;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Area) {
            area = &feature;
            break;
        }
    }

    if (!expect(area != nullptr, "Expected mixed closed/open referenced area to resolve an area feature.")) {
        return 1;
    }

    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::lowest();
    qreal maxY = std::numeric_limits<qreal>::lowest();
    for (const QPointF &vertex : area->vertices) {
        minX = qMin(minX, vertex.x());
        minY = qMin(minY, vertex.y());
        maxX = qMax(maxX, vertex.x());
        maxY = qMax(maxY, vertex.y());
    }

    const bool resolvedLargeFace = minX < 537.0 && maxX > 611.0 && minY < 774.0 && maxY > 804.0;
    if (!resolvedLargeFace) {
        std::cerr << "Resolved mixed referenced area bounds: "
                  << minX << ',' << minY << " - "
                  << maxX << ',' << maxY << '\n';
    }
    return expect(resolvedLargeFace,
                  "Expected referenced area resolver to prefer the face using more referenced lines over the tiny closed rock-border face.")
        ? 0
        : 1;
}

int runForwardReferencedClosedBorderAreaParsingTest()
{
    const QString text =
        QStringLiteral("encoding utf-8\n"
                       "\n"
                       "scrap scrap-1\n"
                       "  area water\n"
                       "    jaz.b.6\n"
                       "  endarea\n"
                       "\n"
                       "  line border -id jaz.b.6 -close on\n"
                       "    863.0 727.5\n"
                       "    889.0 716.0\n"
                       "    889.0 716.0 892.98 723.95 891.0 725.0\n"
                       "    847.73 747.94 860.0 732.5 863.0 727.5\n"
                       "    smooth off\n"
                       "  endline\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);

    const MapGeometryFeature *area = nullptr;
    const MapGeometryFeature *border = nullptr;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Area) {
            area = &feature;
        } else if (feature.kind == MapGeometryFeature::Kind::Line) {
            border = &feature;
        }
    }

    if (!expect(border != nullptr && border->closed,
                "Expected forward-reference fixture border line to parse as a closed line.")) {
        return 1;
    }
    if (!expect(area != nullptr,
                "Expected area before its referenced closed border line to resolve after the scrap is parsed.")) {
        return 1;
    }
    if (!expect(!area->verticesEditable && area->closed,
                "Expected forward-referenced area geometry to be closed and non-editable.")) {
        return 1;
    }
    if (!expect(area->lineVertices.size() == border->lineVertices.size(),
                "Expected area referencing one closed border line to reuse that border geometry.")) {
        return 1;
    }

    return 0;
}

int runSmartAreaCandidatePlannerTest()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan\n"
        "line border\n"
        "  0 1\n"
        "  1 1\n"
        "endline\n"
        "line border\n"
        "  1 1\n"
        "  1 0\n"
        "endline\n"
        "line border\n"
        "  1 0\n"
        "  0 0\n"
        "endline\n"
        "line border\n"
        "  0 0\n"
        "  0 1\n"
        "endline\n"
        "endscrap\n");

    const QVector<MapGeometryFeature> features =
        collectGeometryFeatures(TherionDocumentParser::parseTokenLines(text));
    const QVector<MapEditorSmartAreaCandidate> candidates =
        mapEditorSmartAreaCandidatesAt(features, QPointF(0.5, 0.5));
    if (!expect(!candidates.isEmpty(), "Expected Smart Area planner to find a closed face under the click.")) {
        return 1;
    }

    const MapEditorSmartAreaCandidate &candidate = candidates.first();
    if (!expect(candidate.scrapLineNumber == 1,
                "Expected Smart Area candidate to preserve same-scrap ownership.")) {
        return 1;
    }
    if (!expect(candidate.vertices.size() == 4,
                "Expected Smart Area candidate to resolve the four face corners.")) {
        return 1;
    }
    if (!expect(candidate.boundaryLines.size() == 4,
                "Expected Smart Area candidate to reference the four boundary lines.")) {
        return 1;
    }
    for (const MapEditorSmartAreaBoundaryLine &line : candidate.boundaryLines) {
        if (!expect(line.lineNumber > 1 && line.identifier.isEmpty(),
                    "Expected Smart Area candidate to preserve missing boundary line ids for later source insertion.")) {
            return 1;
        }
    }

    const QVector<MapEditorSmartAreaCandidate> outsideCandidates =
        mapEditorSmartAreaCandidatesAt(features, QPointF(2.0, 2.0));
    if (!expect(outsideCandidates.isEmpty(),
                "Expected Smart Area planner not to find a candidate outside the closed face.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (const int rc = runPointTypeAndLabelOptionParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runProjectionGeometryFeatureAdapterParityTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLogicalMapSceneEntryAdapterParityTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runBezierSegmentParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSmoothAndOptionMetadataIgnoredTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePointSubtypeSegmentParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePointSubtypeBlocksGuideRenderingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineGeometryItemGroupRemovalTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePointSubtypeBlocksPreviewRefreshTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineLabelPreviewRefreshTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runInlineSubtypeParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSlopeLinePointOptionsParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineStandaloneRowsPreservedForRewriteRowsTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineStandaloneRowsCoverageTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineStandaloneKnownOptionsNormalizationTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAnchorEquivalentControlNormalizationTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineOptionToggleParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runClosedLineClosingBezierParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runTwoAnchorClosedBezierRenderingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runExplicitClosedBezierRowRenderingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runDeCasteljauInsertionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineVertexRemovalReconnectTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSmoothMirrorHelperTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runPreviewToSourceUnclampedTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineAnchorSecondaryMoveCouplingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineControlSecondaryMoveSmoothTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineControlSecondaryMoveCornerTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePreviewSecondaryMoveSelfFilterTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePreviewAnchorSequencingNoStaleTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePreviewCommitParityTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSmoothControlCommitKeepsTangentContinuityTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runScrapScaleSourceUnitsPerMeterTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaScrapClipMetadataParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runIntersectingReferencedAreaParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runCurvedIntersectingReferencedAreaParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runReferencedAreaPrefersFaceUsingMoreReferencedLinesTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runForwardReferencedClosedBorderAreaParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSmartAreaCandidatePlannerTest(); rc != 0) {
        return rc;
    }

    return 0;
}
