#include "MapEditorObjectMovePlanner.h"

#include "../../../core/TherionSourceDocument.h"

#include <QCoreApplication>
#include <QStringList>
#include <QVector>

#include <algorithm>

namespace TherionStudio
{
namespace
{
struct SourceLine
{
    QString text;
    QString lineEnding;
};

QVector<SourceLine> splitSourceLines(const QString &text)
{
    QVector<SourceLine> lines;
    int lineStart = 0;
    for (int index = 0; index < text.size(); ++index) {
        if (text.at(index) != QLatin1Char('\n')) {
            continue;
        }

        int contentEnd = index;
        QString lineEnding = QStringLiteral("\n");
        if (contentEnd > lineStart && text.at(contentEnd - 1) == QLatin1Char('\r')) {
            --contentEnd;
            lineEnding = QStringLiteral("\r\n");
        }
        lines.append(SourceLine{text.mid(lineStart, contentEnd - lineStart), lineEnding});
        lineStart = index + 1;
    }

    if (lineStart < text.size()) {
        lines.append(SourceLine{text.mid(lineStart), QString()});
    }

    return lines;
}

QString joinSourceLines(const QVector<SourceLine> &lines)
{
    QString result;
    for (const SourceLine &line : lines) {
        result += line.text;
        result += line.lineEnding;
    }
    return result;
}

bool isMovableMapObjectDirective(const QString &directive)
{
    return directive == QStringLiteral("point")
        || directive == QStringLiteral("line")
        || directive == QStringLiteral("area");
}

QString closingDirectiveFor(const QString &directive)
{
    if (directive == QStringLiteral("line")) {
        return QStringLiteral("endline");
    }
    if (directive == QStringLiteral("area")) {
        return QStringLiteral("endarea");
    }
    if (directive == QStringLiteral("scrap")) {
        return QStringLiteral("endscrap");
    }
    return QString();
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

int objectEndLine(const QVector<TherionParsedLine> &parsedLines, const TherionParsedLine &startLine)
{
    if (startLine.directive == QStringLiteral("point")) {
        return startLine.lineNumber;
    }

    const QString closingDirective = closingDirectiveFor(startLine.directive);
    if (closingDirective.isEmpty()) {
        return 0;
    }

    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.lineNumber <= startLine.lineNumber) {
            continue;
        }
        if (parsedLine.directive == closingDirective) {
            return parsedLine.lineNumber;
        }
    }

    return 0;
}

bool lineRangeIsValid(int startLine, int endLine, int lineCount)
{
    return startLine > 0 && endLine >= startLine && endLine <= lineCount;
}
}

MapEditorObjectMovePlan MapEditorObjectMovePlanner::planMove(const QString &text,
                                                             int sourceLineNumber,
                                                             int targetLineNumber,
                                                             MapEditorObjectMovePosition position)
{
    MapEditorObjectMovePlan plan;
    const QVector<SourceLine> sourceLines = splitSourceLines(text);
    if (sourceLines.isEmpty()) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "Cannot move a map object in an empty document.");
        return plan;
    }

    const QVector<TherionParsedLine> parsedLines = TherionSourceDocument::fromText(text).tokenLines();
    const TherionParsedLine *sourceStart = parsedLineAt(parsedLines, sourceLineNumber);
    if (sourceStart == nullptr || !isMovableMapObjectDirective(sourceStart->directive)) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "The source line is not a movable point, line, or area object.");
        return plan;
    }

    const TherionParsedLine *targetStart = parsedLineAt(parsedLines, targetLineNumber);
    if (targetStart == nullptr) {
        plan.errorMessage = QCoreApplication::translate(
            "TherionStudio::MapEditorObjectMovePlanner",
            "The target line is not a movable point, line, area, or scrap object.");
        return plan;
    }
    if (position == MapEditorObjectMovePosition::IntoTargetScrap) {
        if (targetStart->directive != QStringLiteral("scrap")) {
            plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                            "The target line is not a scrap object.");
            return plan;
        }
    } else if (!isMovableMapObjectDirective(targetStart->directive)) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "The target line is not a movable point, line, or area object.");
        return plan;
    }

    const int sourceEndLine = objectEndLine(parsedLines, *sourceStart);
    const int targetEndLine = objectEndLine(parsedLines, *targetStart);
    const int lineCount = sourceLines.size();
    if (!lineRangeIsValid(sourceStart->lineNumber, sourceEndLine, lineCount)) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "The source object block is incomplete or malformed.");
        return plan;
    }
    if (!lineRangeIsValid(targetStart->lineNumber, targetEndLine, lineCount)) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "The target object block is incomplete or malformed.");
        return plan;
    }

    int insertBeforeLineOriginal = 0;
    switch (position) {
    case MapEditorObjectMovePosition::BeforeTarget:
        insertBeforeLineOriginal = targetStart->lineNumber;
        break;
    case MapEditorObjectMovePosition::AfterTarget:
        insertBeforeLineOriginal = targetEndLine + 1;
        break;
    case MapEditorObjectMovePosition::IntoTargetScrap:
        insertBeforeLineOriginal = targetEndLine;
        break;
    }

    plan.sourceStartLine = sourceStart->lineNumber;
    plan.sourceEndLine = sourceEndLine;
    plan.insertBeforeLineOriginal = insertBeforeLineOriginal;
    plan.resolved = true;

    if (insertBeforeLineOriginal >= sourceStart->lineNumber && insertBeforeLineOriginal <= sourceEndLine + 1) {
        plan.changed = false;
        plan.insertBeforeLineAfterRemoval = sourceStart->lineNumber;
        plan.movedText = text;
        return plan;
    }

    const int movedLineCount = sourceEndLine - sourceStart->lineNumber + 1;
    int insertBeforeLineAfterRemoval = insertBeforeLineOriginal;
    if (insertBeforeLineOriginal > sourceEndLine) {
        insertBeforeLineAfterRemoval -= movedLineCount;
    }
    plan.insertBeforeLineAfterRemoval = insertBeforeLineAfterRemoval;

    QVector<SourceLine> rewrittenLines = sourceLines;
    const auto firstMoved = rewrittenLines.begin() + (sourceStart->lineNumber - 1);
    const auto afterMoved = rewrittenLines.begin() + sourceEndLine;
    const QVector<SourceLine> movedLines(firstMoved, afterMoved);
    rewrittenLines.erase(firstMoved, afterMoved);

    qsizetype insertIndex = std::clamp<qsizetype>(insertBeforeLineAfterRemoval - 1, 0, rewrittenLines.size());
    for (const SourceLine &line : movedLines) {
        rewrittenLines.insert(insertIndex, line);
        ++insertIndex;
    }

    plan.movedText = joinSourceLines(rewrittenLines);
    plan.changed = plan.movedText != text;
    return plan;
}

MapEditorObjectMovePlan MapEditorObjectMovePlanner::planMoveIntoScrap(const QString &text,
                                                                      const QVector<int> &objectLineNumbers,
                                                                      int focusLineNumber,
                                                                      int targetScrapLineNumber)
{
    MapEditorObjectMovePlan plan;
    const QVector<SourceLine> sourceLines = splitSourceLines(text);
    const QVector<TherionParsedLine> parsedLines = TherionSourceDocument::fromText(text).tokenLines();
    const TherionParsedLine *targetStart = parsedLineAt(parsedLines, targetScrapLineNumber);
    if (targetStart == nullptr || targetStart->directive != QStringLiteral("scrap")) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "The target line is not a scrap object.");
        return plan;
    }
    const int targetEndLine = objectEndLine(parsedLines, *targetStart);
    if (!lineRangeIsValid(targetStart->lineNumber, targetEndLine, sourceLines.size())) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "The target object block is incomplete or malformed.");
        return plan;
    }

    struct Block
    {
        int startLine = 0;
        int endLine = 0;
    };
    QVector<Block> blocks;
    for (const int lineNumber : objectLineNumbers) {
        const TherionParsedLine *start = parsedLineAt(parsedLines, lineNumber);
        if (start == nullptr || !isMovableMapObjectDirective(start->directive)) {
            plan.errorMessage = QCoreApplication::translate(
                "TherionStudio::MapEditorObjectMovePlanner",
                "The source line is not a movable point, line, or area object.");
            return plan;
        }
        const int endLine = objectEndLine(parsedLines, *start);
        if (!lineRangeIsValid(start->lineNumber, endLine, sourceLines.size())) {
            plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                            "The source object block is incomplete or malformed.");
            return plan;
        }
        if (start->lineNumber <= targetEndLine && endLine >= targetStart->lineNumber) {
            plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                            "The object is already in the target scrap.");
            return plan;
        }
        blocks.append(Block{start->lineNumber, endLine});
    }
    if (blocks.isEmpty()) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "The source line is not a movable point, line, or area object.");
        return plan;
    }
    std::sort(blocks.begin(), blocks.end(), [](const Block &left, const Block &right) {
        return left.startLine < right.startLine;
    });

    QVector<SourceLine> movedLines;
    int focusOffset = 0;
    int removedAboveTarget = 0;
    for (const Block &block : blocks) {
        if (block.startLine == focusLineNumber) {
            focusOffset = movedLines.size();
        }
        movedLines += sourceLines.mid(block.startLine - 1, block.endLine - block.startLine + 1);
        if (block.endLine < targetStart->lineNumber) {
            removedAboveTarget += block.endLine - block.startLine + 1;
        }
    }

    QVector<SourceLine> rewrittenLines;
    for (int index = 0; index < sourceLines.size(); ++index) {
        const int lineNumber = index + 1;
        const bool moved = std::any_of(blocks.cbegin(), blocks.cend(), [lineNumber](const Block &block) {
            return lineNumber >= block.startLine && lineNumber <= block.endLine;
        });
        if (!moved) {
            rewrittenLines.append(sourceLines.at(index));
        }
    }

    // Insert before the target's endscrap, which moved up by the blocks removed above it.
    const int insertBeforeLine = targetEndLine - removedAboveTarget;
    qsizetype insertIndex = std::clamp<qsizetype>(insertBeforeLine - 1, 0, rewrittenLines.size());
    for (const SourceLine &line : std::as_const(movedLines)) {
        rewrittenLines.insert(insertIndex, line);
        ++insertIndex;
    }

    plan.resolved = true;
    plan.sourceStartLine = blocks.constFirst().startLine;
    plan.sourceEndLine = blocks.constLast().endLine;
    plan.insertBeforeLineOriginal = targetEndLine;
    plan.insertBeforeLineAfterRemoval = insertBeforeLine + focusOffset;
    plan.movedText = joinSourceLines(rewrittenLines);
    plan.changed = plan.movedText != text;
    return plan;
}

bool MapEditorObjectMovePlanner::applyMove(QString *text,
                                           int sourceLineNumber,
                                           int targetLineNumber,
                                           MapEditorObjectMovePosition position,
                                           QString *errorMessage)
{
    if (text == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectMovePlanner",
                                                        "Cannot move a map object without document text.");
        }
        return false;
    }

    const MapEditorObjectMovePlan plan = planMove(*text, sourceLineNumber, targetLineNumber, position);
    if (!plan.resolved) {
        if (errorMessage != nullptr) {
            *errorMessage = plan.errorMessage;
        }
        return false;
    }

    *text = plan.movedText;
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}
}
