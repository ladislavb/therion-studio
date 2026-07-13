#include "MapEditorLineSplitPlanner.h"

#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/TherionSourceDocument.h"
#include "../../../core/TherionSourceText.h"
#include "../../../core/TherionStringUtils.h"

#include <QCoreApplication>
#include <QSet>

namespace TherionStudio
{
namespace
{
QSet<QString> lineIdentifiersInDocument(const QString &text)
{
    QSet<QString> identifiers;
    const QVector<TherionParsedLine> parsedLines = TherionSourceDocument::fromText(text).tokenLines();
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive != QStringLiteral("line")) {
            continue;
        }
        const QString identifier = commandOptionValue(parsedLine.tokens, QStringLiteral("-id")).trimmed();
        if (!identifier.isEmpty()) {
            identifiers.insert(identifier.toLower());
        }
    }
    return identifiers;
}

QString uniqueSplitLineIdentifier(const QString &originalIdentifier, const QSet<QString> &existingIdentifiers)
{
    QString base = originalIdentifier.trimmed();
    if (base.isEmpty()) {
        base = QStringLiteral("line-split");
    }

    int suffix = 1;
    while (true) {
        const QString candidate = QStringLiteral("%1-split-%2").arg(base).arg(suffix++);
        if (!existingIdentifiers.contains(candidate.toLower())) {
            return candidate;
        }
    }
}

QString lineStartWithReplacementIdentifier(const QString &lineText,
                                           const TherionParsedLine &parsedLine,
                                           const QString &identifier)
{
    for (int index = 0; index + 1 < parsedLine.tokens.size() && index + 1 < parsedLine.tokenSpans.size(); ++index) {
        if (parsedLine.tokens.at(index).toLower() != QStringLiteral("-id")) {
            continue;
        }
        QString rewritten = lineText;
        const TherionParsedToken &valueSpan = parsedLine.tokenSpans.at(index + 1);
        rewritten.replace(valueSpan.start, valueSpan.length, identifier);
        return rewritten;
    }

    const int insertIndex = parsedLine.commentStart >= 0 ? parsedLine.commentStart : lineText.size();
    QString rewritten = lineText;
    const QString insertion = insertIndex > 0 && !rewritten.at(insertIndex - 1).isSpace()
        ? QStringLiteral(" -id %1").arg(identifier)
        : QStringLiteral("-id %1").arg(identifier);
    rewritten.insert(insertIndex, insertion + (parsedLine.commentStart >= 0 ? QStringLiteral(" ") : QString()));
    return rewritten;
}

QStringList lineBlockForRows(const QString &startLine, const QStringList &coordinateRows, const QString &endLine)
{
    QStringList block;
    block.reserve(coordinateRows.size() + 2);
    block.append(startLine);
    for (const QString &row : coordinateRows) {
        const QString trimmed = row.trimmed();
        if (!trimmed.isEmpty()) {
            block.append(QStringLiteral("  %1").arg(trimmed));
        }
    }
    block.append(endLine);
    return block;
}

bool isAreaOptionLikeLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return false;
    }
    const QString firstToken = parsedLine.tokens.first().trimmed().toLower();
    if (firstToken.startsWith(QLatin1Char('-'))) {
        return true;
    }
    return firstToken == QStringLiteral("place")
        || firstToken == QStringLiteral("clip")
        || firstToken == QStringLiteral("visibility")
        || firstToken == QStringLiteral("context")
        || firstToken == QStringLiteral("id");
}

bool lineContainsAreaReference(const TherionParsedLine &parsedLine, const QString &lineId)
{
    if (lineId.isEmpty() || parsedLine.tokens.isEmpty() || isAreaOptionLikeLine(parsedLine)) {
        return false;
    }

    const QString normalizedLineId = lineId.trimmed().toLower();
    for (const QString &token : parsedLine.tokens) {
        if (token.trimmed().toLower() == normalizedLineId) {
            return true;
        }
    }
    return false;
}

bool rewriteAreaReferenceLine(QString *lineText,
                              const TherionParsedLine &parsedLine,
                              const QString &originalLineId,
                              const QString &splitLineId)
{
    if (lineText == nullptr
        || originalLineId.isEmpty()
        || splitLineId.isEmpty()
        || parsedLine.tokens.isEmpty()
        || isAreaOptionLikeLine(parsedLine)) {
        return false;
    }

    const QString normalizedOriginalId = originalLineId.trimmed().toLower();
    const QString appendedToken = QStringLiteral("%1 %2").arg(originalLineId, splitLineId);
    bool changed = false;
    for (int tokenIndex = parsedLine.tokens.size() - 1; tokenIndex >= 0; --tokenIndex) {
        if (tokenIndex >= parsedLine.tokenSpans.size()) {
            continue;
        }
        const QString token = parsedLine.tokens.at(tokenIndex).trimmed();
        if (token.toLower() != normalizedOriginalId) {
            continue;
        }
        const TherionParsedToken &tokenSpan = parsedLine.tokenSpans.at(tokenIndex);
        lineText->replace(tokenSpan.start, tokenSpan.length, appendedToken);
        changed = true;
    }
    return changed;
}

const TherionParsedLine *parsedLineAt(const QVector<TherionParsedLine> &parsedLines, int lineNumber)
{
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.lineNumber == lineNumber) {
            return &parsedLine;
        }
    }
    return nullptr;
}

bool rewriteAreaBorderReferences(QStringList *lines,
                                 const QVector<TherionParsedLine> &parsedLines,
                                 const QString &originalLineId,
                                 const QString &splitLineId)
{
    if (lines == nullptr || originalLineId.isEmpty() || splitLineId.isEmpty()) {
        return false;
    }

    bool inArea = false;
    bool changed = false;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        const int lineIndex = parsedLine.lineNumber - 1;
        if (lineIndex < 0 || lineIndex >= lines->size()) {
            continue;
        }
        if (!inArea) {
            if (parsedLine.directive == QStringLiteral("area")) {
                inArea = true;
            }
            continue;
        }

        if (parsedLine.directive == QStringLiteral("endarea")) {
            inArea = false;
            continue;
        }

        QString rewrittenLine = lines->at(lineIndex);
        if (rewriteAreaReferenceLine(&rewrittenLine, parsedLine, originalLineId, splitLineId)) {
            (*lines)[lineIndex] = rewrittenLine;
            changed = true;
        }
    }
    return changed;
}

bool documentHasAreaReferencesForLineId(const QVector<TherionParsedLine> &parsedLines, const QString &lineId)
{
    if (lineId.isEmpty()) {
        return false;
    }

    bool inArea = false;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (!inArea) {
            if (parsedLine.directive == QStringLiteral("area")) {
                inArea = true;
            }
            continue;
        }

        if (parsedLine.directive == QStringLiteral("endarea")) {
            inArea = false;
            continue;
        }

        if (lineContainsAreaReference(parsedLine, lineId)) {
            return true;
        }
    }
    return false;
}
}

MapEditorLineSplitPlan MapEditorLineSplitPlanner::planSplit(const QString &text,
                                                            int lineNumber,
                                                            const QStringList &firstCoordinateRows,
                                                            const QStringList &secondCoordinateRows)
{
    MapEditorLineSplitPlan plan;
    if (lineNumber <= 0) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorLineSplitPlanner",
                                                        "Invalid line number.");
        return plan;
    }
    if (firstCoordinateRows.isEmpty() || secondCoordinateRows.isEmpty()) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorLineSplitPlanner",
                                                        "Split produced invalid line geometry.");
        return plan;
    }

    QStringList lines = splitLinesNormalizingLineEndings(text);
    if (lineNumber > lines.size()) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorLineSplitPlanner",
                                                        "Selected source line no longer exists.");
        return plan;
    }

    const int blockStartLineIndex = lineNumber - 1;
    const QVector<TherionParsedLine> parsedLines = TherionSourceDocument::fromText(text).tokenLines();
    const TherionParsedLine *startLine = parsedLineAt(parsedLines, lineNumber);
    if (startLine == nullptr || startLine->directive != QStringLiteral("line")) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorLineSplitPlanner",
                                                        "Selected source line is not a line block.");
        return plan;
    }

    int blockEndLineIndex = -1;
    for (int candidateIndex = blockStartLineIndex + 1; candidateIndex < lines.size(); ++candidateIndex) {
        const TherionParsedLine *candidateLine = parsedLineAt(parsedLines, candidateIndex + 1);
        if (candidateLine != nullptr && candidateLine->directive == QStringLiteral("endline")) {
            blockEndLineIndex = candidateIndex;
            break;
        }
    }
    if (blockEndLineIndex < 0) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorLineSplitPlanner",
                                                        "Selected line block is missing endline.");
        return plan;
    }

    const QString originalId = commandOptionValue(startLine->tokens, QStringLiteral("-id")).trimmed();
    plan.originalLineId = originalId;
    QString splitLineId;
    QString secondStartLine = lines.at(blockStartLineIndex);
    if (!originalId.isEmpty()) {
        QSet<QString> existingIds = lineIdentifiersInDocument(text);
        splitLineId = uniqueSplitLineIdentifier(originalId, existingIds);
        secondStartLine = lineStartWithReplacementIdentifier(secondStartLine, *startLine, splitLineId);
    }

    QStringList rewrittenBlock = lineBlockForRows(lines.at(blockStartLineIndex),
                                                  firstCoordinateRows,
                                                  lines.at(blockEndLineIndex));
    rewrittenBlock.append(lineBlockForRows(secondStartLine,
                                           secondCoordinateRows,
                                           lines.at(blockEndLineIndex)));

    const int replaceCount = (blockEndLineIndex - blockStartLineIndex) + 1;
    for (int index = 0; index < replaceCount; ++index) {
        lines.removeAt(blockStartLineIndex);
    }
    for (int index = rewrittenBlock.size() - 1; index >= 0; --index) {
        lines.insert(blockStartLineIndex, rewrittenBlock.at(index));
    }

    const QString lineEnding = TherionSourceText::detectedLineEnding(text);
    const QVector<TherionParsedLine> rewrittenParsedLines = TherionSourceDocument::fromText(lines.join(lineEnding)).tokenLines();
    if (!originalId.isEmpty()
        && !splitLineId.isEmpty()
        && documentHasAreaReferencesForLineId(rewrittenParsedLines, originalId)) {
        plan.areaReferencesUpdated = rewriteAreaBorderReferences(&lines, rewrittenParsedLines, originalId, splitLineId);
        plan.splitLineId = splitLineId;
    } else {
        plan.areaReferencesUpdated = false;
        plan.splitLineId = splitLineId;
    }

    plan.updatedText = lines.join(lineEnding);
    plan.resolved = true;
    plan.changed = plan.updatedText != text;
    return plan;
}
}
