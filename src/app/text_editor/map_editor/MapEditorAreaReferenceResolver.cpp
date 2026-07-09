#include "MapEditorAreaReferenceResolver.h"

#include "../../../core/Th2GeometryProjection.h"
#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceLogicalDocument.h"

#include <QHash>
#include <QStringList>

namespace TherionStudio
{
namespace
{
struct AreaReferenceBlock
{
    int lineNumber = 0;
    QString label;
    QSet<QString> borderLineIds;
};

bool tokenLooksReferenceId(const QString &token)
{
    if (token.isEmpty() || token.startsWith(QLatin1Char('-'))) {
        return false;
    }

    bool numeric = false;
    token.toDouble(&numeric);
    return !numeric;
}

QString areaLabel(const TherionParsedLine &parsedLine)
{
    QString label = parsedLine.tokens.value(1).trimmed();
    const QString id = commandOptionValue(parsedLine.tokens, QStringLiteral("-id")).trimmed();
    if (!id.isEmpty()) {
        label = label.isEmpty() ? id : QStringLiteral("%1 (%2)").arg(label, id);
    }
    return label.isEmpty() ? QStringLiteral("area at line %1").arg(parsedLine.lineNumber) : label;
}

QString areaLabel(const Th2AreaObject &area)
{
    QString label = area.command.type.trimmed();
    const QString id = area.command.id.trimmed();
    if (!id.isEmpty()) {
        label = label.isEmpty() ? id : QStringLiteral("%1 (%2)").arg(label, id);
    }
    return label.isEmpty()
        ? QStringLiteral("area at line %1").arg(area.command.sourceRange.startLineNumber)
        : label;
}

QSet<QString> referenceIdsFromAreaBodyLine(const TherionParsedLine &parsedLine)
{
    QSet<QString> references;
    for (const QString &token : parsedLine.tokens) {
        const QString normalizedToken = token.trimmed();
        if (tokenLooksReferenceId(normalizedToken)) {
            references.insert(normalizedToken.toLower());
        }
    }
    return references;
}

QHash<QString, int> lineNumbersById(const QVector<TherionParsedLine> &parsedLines)
{
    QHash<QString, int> result;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive != QStringLiteral("line")) {
            continue;
        }

        const QString lineId = commandOptionValue(parsedLine.tokens, QStringLiteral("-id")).trimmed().toLower();
        if (!lineId.isEmpty()) {
            result.insert(lineId, parsedLine.lineNumber);
        }
    }
    return result;
}

QHash<QString, int> lineNumbersById(const Th2GeometryProjection &projection)
{
    QHash<QString, int> result;
    for (const Th2LineObject &line : projection.lines()) {
        const QString lineId = line.command.id.trimmed().toLower();
        if (!lineId.isEmpty() && line.command.sourceRange.startLineNumber > 0) {
            result.insert(lineId, line.command.sourceRange.startLineNumber);
        }
    }
    return result;
}

QVector<AreaReferenceBlock> areaReferenceBlocks(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<AreaReferenceBlock> result;
    AreaReferenceBlock currentArea;
    bool inArea = false;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (!inArea && parsedLine.directive == QStringLiteral("area")) {
            currentArea = AreaReferenceBlock{};
            currentArea.lineNumber = parsedLine.lineNumber;
            currentArea.label = areaLabel(parsedLine);
            inArea = true;
            continue;
        }

        if (!inArea) {
            continue;
        }

        if (parsedLine.directive == QStringLiteral("endarea")) {
            result.append(currentArea);
            currentArea = AreaReferenceBlock{};
            inArea = false;
            continue;
        }

        currentArea.borderLineIds.unite(referenceIdsFromAreaBodyLine(parsedLine));
    }

    if (inArea && currentArea.lineNumber > 0) {
        result.append(currentArea);
    }
    return result;
}

QVector<AreaReferenceBlock> areaReferenceBlocks(const Th2GeometryProjection &projection)
{
    QVector<AreaReferenceBlock> result;
    result.reserve(projection.areas().size());
    for (const Th2AreaObject &area : projection.areas()) {
        AreaReferenceBlock block;
        block.lineNumber = area.command.sourceRange.startLineNumber;
        block.label = areaLabel(area);
        for (const QString &reference : area.borderReferences) {
            const QString normalizedReference = reference.trimmed();
            if (tokenLooksReferenceId(normalizedReference)) {
                block.borderLineIds.insert(normalizedReference.toLower());
            }
        }
        if (block.lineNumber > 0) {
            result.append(block);
        }
    }
    return result;
}

QVector<TherionParsedLine> parsedTokenLinesForText(const QString &text)
{
    return TherionDocumentParser::parseTokenLines(text);
}

QVector<TherionParsedLine> parsedTokenLinesForLogicalCommands(const QVector<TherionSourceLogicalCommand> &commands)
{
    QVector<TherionParsedLine> parsedLines;
    parsedLines.reserve(commands.size());
    for (const TherionSourceLogicalCommand &command : commands) {
        parsedLines.append(command.parsed);
    }
    return parsedLines;
}

QSet<int> borderLineNumbersForArea(const QVector<TherionParsedLine> &parsedLines, int areaLineNumber)
{
    QSet<int> borderLineNumbers;
    if (areaLineNumber <= 0) {
        return borderLineNumbers;
    }

    const QHash<QString, int> lineNumbers = lineNumbersById(parsedLines);
    for (const AreaReferenceBlock &area : areaReferenceBlocks(parsedLines)) {
        if (area.lineNumber != areaLineNumber) {
            continue;
        }

        for (const QString &lineId : area.borderLineIds) {
            const auto lineIt = lineNumbers.constFind(lineId);
            if (lineIt != lineNumbers.constEnd()) {
                borderLineNumbers.insert(lineIt.value());
            }
        }
        break;
    }
    return borderLineNumbers;
}

QSet<int> borderLineNumbersForArea(const Th2GeometryProjection &projection, int areaLineNumber)
{
    QSet<int> borderLineNumbers;
    if (areaLineNumber <= 0) {
        return borderLineNumbers;
    }

    const QHash<QString, int> lineNumbers = lineNumbersById(projection);
    for (const AreaReferenceBlock &area : areaReferenceBlocks(projection)) {
        if (area.lineNumber != areaLineNumber) {
            continue;
        }

        for (const QString &lineId : area.borderLineIds) {
            const auto lineIt = lineNumbers.constFind(lineId);
            if (lineIt != lineNumbers.constEnd()) {
                borderLineNumbers.insert(lineIt.value());
            }
        }
        break;
    }
    return borderLineNumbers;
}

QVector<MapEditorAreaReference> areaReferencesForBorderLine(const QVector<TherionParsedLine> &parsedLines,
                                                            int borderLineNumber)
{
    QVector<MapEditorAreaReference> references;
    if (borderLineNumber <= 0) {
        return references;
    }

    QString targetLineId;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.lineNumber != borderLineNumber || parsedLine.directive != QStringLiteral("line")) {
            continue;
        }
        targetLineId = commandOptionValue(parsedLine.tokens, QStringLiteral("-id")).trimmed().toLower();
        break;
    }
    if (targetLineId.isEmpty()) {
        return references;
    }

    for (const AreaReferenceBlock &area : areaReferenceBlocks(parsedLines)) {
        if (!area.borderLineIds.contains(targetLineId)) {
            continue;
        }
        references.append(MapEditorAreaReference{area.lineNumber, area.label, targetLineId});
    }
    return references;
}

QVector<MapEditorAreaReference> areaReferencesForBorderLine(const Th2GeometryProjection &projection,
                                                            int borderLineNumber)
{
    QVector<MapEditorAreaReference> references;
    if (borderLineNumber <= 0) {
        return references;
    }

    QString targetLineId;
    for (const Th2LineObject &line : projection.lines()) {
        if (line.command.sourceRange.startLineNumber != borderLineNumber) {
            continue;
        }
        targetLineId = line.command.id.trimmed().toLower();
        break;
    }
    if (targetLineId.isEmpty()) {
        return references;
    }

    for (const AreaReferenceBlock &area : areaReferenceBlocks(projection)) {
        if (!area.borderLineIds.contains(targetLineId)) {
            continue;
        }
        references.append(MapEditorAreaReference{area.lineNumber, area.label, targetLineId});
    }
    return references;
}
}

QSet<int> mapEditorBorderLineNumbersForArea(const QString &text, int areaLineNumber)
{
    return borderLineNumbersForArea(parsedTokenLinesForText(text), areaLineNumber);
}

QSet<int> mapEditorBorderLineNumbersForArea(const QVector<TherionSourceLogicalCommand> &commands, int areaLineNumber)
{
    return borderLineNumbersForArea(parsedTokenLinesForLogicalCommands(commands), areaLineNumber);
}

QSet<int> mapEditorBorderLineNumbersForArea(const Th2GeometryProjection &projection, int areaLineNumber)
{
    return borderLineNumbersForArea(projection, areaLineNumber);
}

QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const QString &text, int borderLineNumber)
{
    return areaReferencesForBorderLine(parsedTokenLinesForText(text), borderLineNumber);
}

QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const QVector<TherionSourceLogicalCommand> &commands,
                                                                     int borderLineNumber)
{
    return areaReferencesForBorderLine(parsedTokenLinesForLogicalCommands(commands), borderLineNumber);
}

QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const Th2GeometryProjection &projection,
                                                                     int borderLineNumber)
{
    return areaReferencesForBorderLine(projection, borderLineNumber);
}
}
