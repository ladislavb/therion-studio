#include "MapEditorSourceReferenceResolver.h"

#include "MapEditorSceneInternals.h"
#include "../../../core/Th2GeometryProjection.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionTokenRules.h"

#include <cmath>
#include <limits>

namespace TherionStudio
{
std::optional<MapGeometryFeature> lineFeatureForLineNumber(const QString &documentText, int lineNumber)
{
    return lineFeatureForLineNumber(TherionDocumentParser::parseTokenLines(documentText), lineNumber);
}

std::optional<MapGeometryFeature> lineFeatureForLineNumber(const QVector<TherionParsedLine> &parsedLines, int lineNumber)
{
    if (lineNumber <= 0) {
        return std::nullopt;
    }

    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line
            && feature.lineNumber == lineNumber
            && !feature.lineVertices.isEmpty()) {
            return feature;
        }
    }

    return std::nullopt;
}

std::optional<MapGeometryFeature> lineFeatureForLineNumber(const QVector<TherionSourceLogicalCommand> &commands,
                                                           int lineNumber)
{
    const std::optional<MapGeometryFeature> feature =
        geometryFeatureForLineNumber(commands, lineNumber, MapGeometryFeature::Kind::Line);
    if (feature.has_value() && !feature->lineVertices.isEmpty()) {
        return feature;
    }
    return std::nullopt;
}

std::optional<MapGeometryFeature> lineFeatureForLineNumber(const Th2GeometryProjection &projection,
                                                           const QVector<TherionSourceLogicalCommand> &commands,
                                                           int lineNumber)
{
    if (lineNumber <= 0) {
        return std::nullopt;
    }

    const Th2LineObject *lineObject = projection.lineAtLineNumber(lineNumber);
    if (lineObject == nullptr || !lineObject->command.sourceRange.isValid()) {
        return std::nullopt;
    }

    return lineFeatureForLineNumber(commands, lineObject->command.sourceRange.startLineNumber);
}

std::optional<MapGeometryFeature> geometryFeatureForLineNumber(const QVector<TherionSourceLogicalCommand> &commands,
                                                               int lineNumber,
                                                               MapGeometryFeature::Kind expectedKind)
{
    if (lineNumber <= 0) {
        return std::nullopt;
    }

    QVector<TherionParsedLine> parsedLines;
    parsedLines.reserve(commands.size());
    for (const TherionSourceLogicalCommand &command : commands) {
        parsedLines.append(command.parsed);
    }
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == expectedKind && feature.lineNumber == lineNumber) {
            return feature;
        }
    }

    return std::nullopt;
}

QString formatSourceCoordinate(qreal value)
{
    return QString::number(value, 'f', 1);
}

QString formatLinePointOrientation(qreal value)
{
    qreal normalized = std::fmod(value, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return QString::number(normalized, 'f', 3);
}

void appendLinePointOptionRows(QStringList *rows, const MapGeometryFeature::TH2LineVertex &vertex)
{
    if (rows == nullptr) {
        return;
    }

    if (vertex.orientationDegrees.has_value()) {
        rows->append(QStringLiteral("orientation %1").arg(formatLinePointOrientation(vertex.orientationDegrees.value())));
    }
    if (!vertex.isSmooth) {
        rows->append(QStringLiteral("smooth off"));
    }
    if (vertex.leftSize.has_value()) {
        rows->append(QStringLiteral("l-size %1").arg(formatSourceCoordinate(vertex.leftSize.value())));
    }
    for (const QString &standaloneRow : vertex.standaloneOptionRows) {
        const QString trimmed = standaloneRow.trimmed();
        if (!trimmed.isEmpty()) {
            rows->append(trimmed);
        }
    }
}

QStringList coordinateRowsForLineVertices(const QVector<MapGeometryFeature::TH2LineVertex> &lineVertices,
                                          bool closed)
{
    QStringList rows;
    if (lineVertices.size() < 2) {
        return rows;
    }

    const auto pointRow = [](const QPointF &point) {
        return QStringLiteral("%1 %2")
            .arg(formatSourceCoordinate(point.x()), formatSourceCoordinate(point.y()));
    };

    rows.append(pointRow(lineVertices.first().anchor));
    appendLinePointOptionRows(&rows, lineVertices.first());

    for (int index = 1; index < lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &previous = lineVertices.at(index - 1);
        const MapGeometryFeature::TH2LineVertex &current = lineVertices.at(index);
        QStringList rowTokens;
        const bool cubic = previous.outgoingControl.has_value() || current.incomingControl.has_value();
        const auto appendPointTokens = [&rowTokens](const QPointF &point) {
            rowTokens.append(formatSourceCoordinate(point.x()));
            rowTokens.append(formatSourceCoordinate(point.y()));
        };
        if (cubic) {
            appendPointTokens(previous.outgoingControl.value_or(previous.anchor));
            appendPointTokens(current.incomingControl.value_or(current.anchor));
        }
        appendPointTokens(current.anchor);
        rows.append(rowTokens.join(QLatin1Char(' ')));
        appendLinePointOptionRows(&rows, current);
    }

    if (closed && lineVertices.size() >= 2) {
        const MapGeometryFeature::TH2LineVertex &first = lineVertices.first();
        const MapGeometryFeature::TH2LineVertex &last = lineVertices.last();
        const bool cubicClose = last.outgoingControl.has_value() || first.incomingControl.has_value();
        if (cubicClose) {
            QStringList rowTokens;
            const auto appendPointTokens = [&rowTokens](const QPointF &point) {
                rowTokens.append(formatSourceCoordinate(point.x()));
                rowTokens.append(formatSourceCoordinate(point.y()));
            };
            appendPointTokens(last.outgoingControl.value_or(last.anchor));
            appendPointTokens(first.incomingControl.value_or(first.anchor));
            appendPointTokens(first.anchor);
            rows.append(rowTokens.join(QLatin1Char(' ')));
        }
    }
    return rows;
}

QPair<int, int> tokenColumnsForParsedLine(const TherionParsedLine &parsedLine, int tokenIndex)
{
    if (tokenIndex < 0 || tokenIndex >= parsedLine.tokenSpans.size()) {
        return qMakePair(1, 1);
    }

    const TherionParsedToken &span = parsedLine.tokenSpans.at(tokenIndex);
    const int startColumn = qMax(1, span.start + 1);
    const int endColumn = qMax(startColumn, span.start + qMax(1, span.length));
    return qMakePair(startColumn, endColumn);
}

QVector<int> coordinateTokenIndicesFromLine(const TherionParsedLine &parsedLine, int startTokenIndex)
{
    QVector<int> numericIndices;
    const int firstTokenIndex = qMax(0, startTokenIndex);
    if (firstTokenIndex >= parsedLine.tokens.size()) {
        return numericIndices;
    }

    if (firstTokenIndex == 0) {
        int firstNonQuotedIndex = -1;
        for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
            if (index < parsedLine.tokenSpans.size()
                && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
                continue;
            }
            firstNonQuotedIndex = index;
            break;
        }

        if (firstNonQuotedIndex >= 0 && !TherionTokenRules::isNumericToken(parsedLine.tokens.at(firstNonQuotedIndex))) {
            return numericIndices;
        }
    }

    const QString firstToken = parsedLine.tokens.at(firstTokenIndex);
    if (firstTokenIndex == 0
        && firstToken.startsWith(QLatin1Char('-'))
        && !TherionTokenRules::isNumericToken(firstToken)) {
        return numericIndices;
    }

    bool sawCoordinateToken = false;
    bool skipOptionValueToken = false;
    for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
        if (skipOptionValueToken && !sawCoordinateToken) {
            skipOptionValueToken = false;
            continue;
        }

        const QString token = parsedLine.tokens.at(index);
        if (!sawCoordinateToken
            && firstTokenIndex > 0
            && token.startsWith(QLatin1Char('-'))
            && !TherionTokenRules::isNumericToken(token)) {
            if (index + 1 < parsedLine.tokens.size()) {
                const QString nextToken = parsedLine.tokens.at(index + 1);
                if (!TherionTokenRules::tokenStartsOption(nextToken)) {
                    skipOptionValueToken = true;
                }
            }
            continue;
        }

        if (!TherionTokenRules::isNumericToken(token)) {
            if (sawCoordinateToken) {
                break;
            }
            continue;
        }

        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }

        numericIndices.append(index);
        sawCoordinateToken = true;
    }

    return numericIndices;
}

QVector<SourceVertexTextReference> lineSourceVertexReferencesFromParsedLine(const TherionParsedLine &parsedLine,
                                                                            int startTokenIndex,
                                                                            int *nextSourceVertexIndex)
{
    QVector<SourceVertexTextReference> references;
    if (nextSourceVertexIndex == nullptr) {
        return references;
    }

    const QVector<int> numericIndices = coordinateTokenIndicesFromLine(parsedLine, startTokenIndex);
    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        SourceVertexTextReference reference;
        reference.lineNumber = parsedLine.lineNumber;
        reference.sourceVertexIndex = *nextSourceVertexIndex;
        ++(*nextSourceVertexIndex);
        const auto xColumns = tokenColumnsForParsedLine(parsedLine, numericIndices.at(index));
        const auto yColumns = tokenColumnsForParsedLine(parsedLine, numericIndices.at(index + 1));
        reference.xStartColumn = xColumns.first;
        reference.xEndColumn = xColumns.second;
        reference.yStartColumn = yColumns.first;
        reference.yEndColumn = yColumns.second;
        references.append(reference);
    }

    return references;
}

QVector<SourceVertexTextReference> areaSourceVertexReferencesFromParsedLine(const TherionParsedLine &parsedLine,
                                                                            int startTokenIndex,
                                                                            int *nextSourceVertexIndex)
{
    QVector<SourceVertexTextReference> references;
    if (nextSourceVertexIndex == nullptr) {
        return references;
    }

    QVector<int> numericIndices;
    for (int index = qMax(0, startTokenIndex); index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (!TherionTokenRules::isNumericToken(token)) {
            continue;
        }
        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }
        numericIndices.append(index);
    }

    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        SourceVertexTextReference reference;
        reference.lineNumber = parsedLine.lineNumber;
        reference.sourceVertexIndex = *nextSourceVertexIndex;
        ++(*nextSourceVertexIndex);
        const auto xColumns = tokenColumnsForParsedLine(parsedLine, numericIndices.at(index));
        const auto yColumns = tokenColumnsForParsedLine(parsedLine, numericIndices.at(index + 1));
        reference.xStartColumn = xColumns.first;
        reference.xEndColumn = xColumns.second;
        reference.yStartColumn = yColumns.first;
        reference.yEndColumn = yColumns.second;
        references.append(reference);
    }

    return references;
}

std::optional<SourceVertexTextReference> sourceVertexReferenceAtCursor(const QVector<SourceVertexTextReference> &references,
                                                                       int cursorColumn,
                                                                       bool preferLastOutsideTokenRange = false)
{
    if (references.isEmpty()) {
        return std::nullopt;
    }

    const int normalizedColumn = qMax(1, cursorColumn);
    int minColumn = std::numeric_limits<int>::max();
    int maxColumn = std::numeric_limits<int>::min();
    for (const SourceVertexTextReference &reference : references) {
        minColumn = qMin(minColumn, qMin(reference.xStartColumn, reference.yStartColumn));
        maxColumn = qMax(maxColumn, qMax(reference.xEndColumn, reference.yEndColumn));
        if ((normalizedColumn >= reference.xStartColumn && normalizedColumn <= reference.xEndColumn)
            || (normalizedColumn >= reference.yStartColumn && normalizedColumn <= reference.yEndColumn)) {
            return reference;
        }
    }

    if (preferLastOutsideTokenRange
        && (normalizedColumn < minColumn || normalizedColumn > maxColumn)) {
        return references.last();
    }

    int bestDistance = std::numeric_limits<int>::max();
    std::optional<SourceVertexTextReference> bestReference;
    for (const SourceVertexTextReference &reference : references) {
        const int distance = qMin(qAbs(normalizedColumn - reference.xStartColumn),
                                  qAbs(normalizedColumn - reference.yStartColumn));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestReference = reference;
        }
    }

    return bestReference;
}

CursorGeometrySelection cursorGeometrySelectionForTextCursor(const QVector<TherionParsedLine> &parsedLines,
                                                             int cursorLine,
                                                             int cursorColumn)
{
    CursorGeometrySelection selection;
    bool inLineBlock = false;
    bool inAreaBlock = false;
    int activeFeatureLine = 0;
    int nextLineSourceVertexIndex = 0;
    int nextAreaSourceVertexIndex = 0;

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (directive == QStringLiteral("endline")) {
            if (inLineBlock && cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("line");
                return selection;
            }
            inLineBlock = false;
            activeFeatureLine = 0;
            nextLineSourceVertexIndex = 0;
            continue;
        }

        if (directive == QStringLiteral("endarea")) {
            if (inAreaBlock && cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("area");
                return selection;
            }
            inAreaBlock = false;
            activeFeatureLine = 0;
            nextAreaSourceVertexIndex = 0;
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("line")) {
            inLineBlock = true;
            activeFeatureLine = parsedLine.lineNumber;
            nextLineSourceVertexIndex = 0;
            if (cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("line");
                const auto references = lineSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextLineSourceVertexIndex);
                selection.sourceVertexReference = sourceVertexReferenceAtCursor(references, cursorColumn, true);
                return selection;
            }

            lineSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextLineSourceVertexIndex);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("area")) {
            inAreaBlock = true;
            activeFeatureLine = parsedLine.lineNumber;
            nextAreaSourceVertexIndex = 0;
            if (cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("area");
                const auto references = areaSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextAreaSourceVertexIndex);
                selection.sourceVertexReference = sourceVertexReferenceAtCursor(references, cursorColumn);
                return selection;
            }

            areaSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextAreaSourceVertexIndex);
            continue;
        }

        if (inLineBlock) {
            const auto references = lineSourceVertexReferencesFromParsedLine(parsedLine, 0, &nextLineSourceVertexIndex);
            if (cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("line");
                selection.sourceVertexReference = sourceVertexReferenceAtCursor(references, cursorColumn, true);
                // Any non-coordinate row inside a line block should still map to
                // the current/last parsed line vertex.
                if (!selection.sourceVertexReference.has_value()
                    && references.isEmpty()
                    && !parsedLine.tokens.isEmpty()
                    && nextLineSourceVertexIndex > 0) {
                    SourceVertexTextReference optionReference;
                    optionReference.lineNumber = parsedLine.lineNumber;
                    optionReference.sourceVertexIndex = nextLineSourceVertexIndex - 1;
                    optionReference.xStartColumn = 1;
                    optionReference.xEndColumn = 1;
                    optionReference.yStartColumn = 1;
                    optionReference.yEndColumn = 1;
                    selection.sourceVertexReference = optionReference;
                }
                return selection;
            }
            continue;
        }

        if (inAreaBlock) {
            const auto references = areaSourceVertexReferencesFromParsedLine(parsedLine, 0, &nextAreaSourceVertexIndex);
            if (cursorLine == parsedLine.lineNumber) {
                selection.featureLineNumber = activeFeatureLine;
                selection.geometryKind = QStringLiteral("area");
                selection.sourceVertexReference = sourceVertexReferenceAtCursor(references, cursorColumn);
                return selection;
            }
            continue;
        }
    }

    return selection;
}

std::optional<SourceVertexTextReference> sourceVertexTextReferenceForSelection(const QVector<TherionParsedLine> &parsedLines,
                                                                               int featureLineNumber,
                                                                               const QString &geometryKind,
                                                                               int sourceVertexIndex)
{
    if (featureLineNumber <= 0 || sourceVertexIndex < 0) {
        return std::nullopt;
    }

    const bool forLine = geometryKind.startsWith(QStringLiteral("line"));
    const bool forArea = geometryKind.startsWith(QStringLiteral("area"));
    if (!forLine && !forArea) {
        return std::nullopt;
    }

    bool inLineBlock = false;
    bool inAreaBlock = false;
    int nextLineSourceVertexIndex = 0;
    int nextAreaSourceVertexIndex = 0;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (directive == QStringLiteral("endline")) {
            inLineBlock = false;
            nextLineSourceVertexIndex = 0;
            continue;
        }
        if (directive == QStringLiteral("endarea")) {
            inAreaBlock = false;
            nextAreaSourceVertexIndex = 0;
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("line")) {
            inLineBlock = (parsedLine.lineNumber == featureLineNumber) && forLine;
            nextLineSourceVertexIndex = 0;
            const auto references = lineSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextLineSourceVertexIndex);
            if (inLineBlock) {
                for (const SourceVertexTextReference &reference : references) {
                    if (reference.sourceVertexIndex == sourceVertexIndex) {
                        return reference;
                    }
                }
            }
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("area")) {
            inAreaBlock = (parsedLine.lineNumber == featureLineNumber) && forArea;
            nextAreaSourceVertexIndex = 0;
            const auto references = areaSourceVertexReferencesFromParsedLine(parsedLine, 1, &nextAreaSourceVertexIndex);
            if (inAreaBlock) {
                for (const SourceVertexTextReference &reference : references) {
                    if (reference.sourceVertexIndex == sourceVertexIndex) {
                        return reference;
                    }
                }
            }
            continue;
        }

        if (inLineBlock) {
            const auto references = lineSourceVertexReferencesFromParsedLine(parsedLine, 0, &nextLineSourceVertexIndex);
            for (const SourceVertexTextReference &reference : references) {
                if (reference.sourceVertexIndex == sourceVertexIndex) {
                    return reference;
                }
            }
            continue;
        }

        if (inAreaBlock) {
            const auto references = areaSourceVertexReferencesFromParsedLine(parsedLine, 0, &nextAreaSourceVertexIndex);
            for (const SourceVertexTextReference &reference : references) {
                if (reference.sourceVertexIndex == sourceVertexIndex) {
                    return reference;
                }
            }
            continue;
        }
    }

    return std::nullopt;
}

std::optional<QSet<int>> scrapObjectLinesForCursor(const QVector<TherionParsedLine> &parsedLines,
                                                   int cursorLine)
{
    if (cursorLine <= 0) {
        return std::nullopt;
    }

    struct ScrapContext
    {
        int startLine = 0;
        QSet<int> objectLines;
    };

    QVector<ScrapContext> scrapStack;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;
        if (directive == QStringLiteral("scrap")) {
            ScrapContext context;
            context.startLine = parsedLine.lineNumber;
            scrapStack.append(context);
            continue;
        }

        if (!scrapStack.isEmpty()
            && (directive == QStringLiteral("point")
                || directive == QStringLiteral("station")
                || directive == QStringLiteral("line")
                || directive == QStringLiteral("area"))) {
            for (ScrapContext &context : scrapStack) {
                context.objectLines.insert(parsedLine.lineNumber);
            }
        }

        if (directive != QStringLiteral("endscrap")) {
            continue;
        }

        if (scrapStack.isEmpty()) {
            continue;
        }

        const ScrapContext context = scrapStack.takeLast();
        if (cursorLine == context.startLine || cursorLine == parsedLine.lineNumber) {
            return context.objectLines;
        }
    }

    for (const ScrapContext &context : std::as_const(scrapStack)) {
        if (cursorLine == context.startLine) {
            return context.objectLines;
        }
    }

    return std::nullopt;
}

std::optional<int> sourcePointLineNumberForSelection(const QVector<TherionParsedLine> &parsedLines,
                                                     const QPointF &sourcePoint)
{
    int bestLineNumber = 0;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive != QStringLiteral("point")
            && parsedLine.directive != QStringLiteral("station")) {
            continue;
        }

        const QVector<QPointF> points = pointsFromTokens(parsedLine.tokens.mid(1));
        if (points.isEmpty()) {
            continue;
        }

        const QPointF delta = points.first() - sourcePoint;
        const qreal distance = std::hypot(delta.x(), delta.y());
        if (distance < bestDistance) {
            bestDistance = distance;
            bestLineNumber = parsedLine.lineNumber;
        }
    }

    if (bestLineNumber > 0 && bestDistance <= 0.5) {
        return bestLineNumber;
    }

    return std::nullopt;
}

std::optional<int> sourcePointLineNumberForSelection(const Th2GeometryProjection &projection,
                                                     const QPointF &sourcePoint)
{
    int bestLineNumber = 0;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (const Th2PointObject &point : projection.points()) {
        if (!point.hasPosition || point.command.sourceRange.startLineNumber <= 0) {
            continue;
        }

        const QPointF delta = point.position - sourcePoint;
        const qreal distance = std::hypot(delta.x(), delta.y());
        if (distance < bestDistance) {
            bestDistance = distance;
            bestLineNumber = point.command.sourceRange.startLineNumber;
        }
    }

    if (bestLineNumber > 0 && bestDistance <= 0.5) {
        return bestLineNumber;
    }

    return std::nullopt;
}
}
