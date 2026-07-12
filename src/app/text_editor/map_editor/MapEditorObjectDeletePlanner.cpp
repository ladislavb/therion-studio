#include "MapEditorObjectDeletePlanner.h"

#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/TherionSourceDocument.h"

#include <QCoreApplication>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace TherionStudio
{
namespace
{
struct SourceRange
{
    int startLine = 0;
    int endLine = 0;
};

struct SourceLine
{
    QString text;
    QString lineEnding;
};

struct AreaReference
{
    int startLine = 0;
    int endLine = 0;
    QSet<QString> lineIds;
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

const TherionParsedLine *parsedLineAt(const QVector<TherionParsedLine> &parsedLines, int lineNumber)
{
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.lineNumber == lineNumber) {
            return &parsedLine;
        }
    }
    return nullptr;
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

int matchingBlockEndLine(const QVector<TherionParsedLine> &parsedLines,
                         int lineCount,
                         int startLine,
                         const QString &openingDirective,
                         const QString &closingDirective)
{
    if (startLine <= 0 || startLine > lineCount || openingDirective.isEmpty() || closingDirective.isEmpty()) {
        return 0;
    }

    int depth = 0;
    for (int line = startLine; line <= lineCount; ++line) {
        const TherionParsedLine *parsedLine = parsedLineAt(parsedLines, line);
        if (parsedLine == nullptr) {
            continue;
        }
        if (parsedLine->directive == openingDirective) {
            ++depth;
            continue;
        }
        if (parsedLine->directive != closingDirective) {
            continue;
        }
        --depth;
        if (depth == 0) {
            return line;
        }
    }
    return 0;
}

SourceRange commandRangeAtLine(const QVector<TherionParsedLine> &parsedLines,
                               int lineCount,
                               int lineNumber,
                               QString *directive = nullptr)
{
    if (lineNumber <= 0 || lineNumber > lineCount) {
        return {};
    }

    const TherionParsedLine *parsedLine = parsedLineAt(parsedLines, lineNumber);
    if (parsedLine == nullptr) {
        return {};
    }
    if (directive != nullptr) {
        *directive = parsedLine->directive;
    }
    if (parsedLine->directive.isEmpty()) {
        return {};
    }

    const QString closingDirective = closingDirectiveFor(parsedLine->directive);
    if (closingDirective.isEmpty()) {
        return {lineNumber, lineNumber};
    }

    const int endLine = matchingBlockEndLine(parsedLines, lineCount, lineNumber, parsedLine->directive, closingDirective);
    return endLine >= lineNumber ? SourceRange{lineNumber, endLine} : SourceRange{};
}

QSet<QString> areaLineReferences(const QVector<TherionParsedLine> &parsedLines, int lineCount, const SourceRange &areaRange)
{
    QSet<QString> references;
    if (areaRange.startLine <= 0 || areaRange.endLine <= areaRange.startLine || areaRange.endLine > lineCount) {
        return references;
    }

    for (int line = areaRange.startLine + 1; line < areaRange.endLine; ++line) {
        const TherionParsedLine *parsedLine = parsedLineAt(parsedLines, line);
        if (parsedLine == nullptr) {
            continue;
        }
        for (const QString &token : parsedLine->tokens) {
            if (!token.startsWith(QLatin1Char('-'))) {
                references.insert(token);
            }
        }
    }
    return references;
}

bool rangeLessThan(const SourceRange &lhs, const SourceRange &rhs)
{
    return lhs.startLine < rhs.startLine || (lhs.startLine == rhs.startLine && lhs.endLine < rhs.endLine);
}

QVector<SourceRange> mergedRanges(QVector<SourceRange> ranges)
{
    ranges.erase(std::remove_if(ranges.begin(),
                                ranges.end(),
                                [](const SourceRange &range) {
                                    return range.startLine <= 0 || range.endLine < range.startLine;
                                }),
                 ranges.end());
    std::sort(ranges.begin(), ranges.end(), rangeLessThan);

    QVector<SourceRange> merged;
    for (const SourceRange &range : ranges) {
        if (merged.isEmpty() || range.startLine > merged.last().endLine + 1) {
            merged.append(range);
            continue;
        }
        merged.last().endLine = std::max(merged.last().endLine, range.endLine);
    }
    return merged;
}

QString removeRangesFromSourceLines(const QVector<SourceLine> &sourceLines,
                                    const QVector<SourceRange> &ranges,
                                    QVector<int> *removedLineNumbers)
{
    QVector<SourceLine> lines = sourceLines;
    QVector<SourceRange> descending = ranges;
    std::sort(descending.begin(), descending.end(), [](const SourceRange &lhs, const SourceRange &rhs) {
        return lhs.startLine > rhs.startLine;
    });

    for (const SourceRange &range : descending) {
        const int startIndex = range.startLine - 1;
        const int count = range.endLine - range.startLine + 1;
        if (startIndex < 0 || startIndex >= lines.size() || count <= 0) {
            continue;
        }
        const int removableCount = std::min(count, static_cast<int>(lines.size()) - startIndex);
        for (int line = range.startLine; line < range.startLine + removableCount; ++line) {
            if (removedLineNumbers != nullptr) {
                removedLineNumbers->append(line);
            }
        }
        for (int index = 0; index < removableCount; ++index) {
            lines.removeAt(startIndex);
        }
    }

    if (removedLineNumbers != nullptr) {
        std::sort(removedLineNumbers->begin(), removedLineNumbers->end());
    }
    return joinSourceLines(lines);
}
}

MapEditorObjectDeletePlan planMapEditorObjectDelete(const QString &text, int lineNumber)
{
    MapEditorObjectDeletePlan plan;
    if (lineNumber <= 0) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectDeletePlanner",
                                                        "Invalid object line.");
        return plan;
    }

    const QVector<SourceLine> sourceLines = splitSourceLines(text);
    const QVector<TherionParsedLine> parsedLines = TherionSourceDocument::fromText(text).tokenLines();
    const int lineCount = sourceLines.size();
    QString targetDirective;
    const SourceRange targetRange = commandRangeAtLine(parsedLines, lineCount, lineNumber, &targetDirective);
    if (targetRange.startLine <= 0) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectDeletePlanner",
                                                        "Unable to resolve object source block.");
        return plan;
    }

    QVector<AreaReference> areas;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive == QStringLiteral("area")) {
            QString directive;
            const SourceRange range = commandRangeAtLine(parsedLines, lineCount, parsedLine.lineNumber, &directive);
            if (range.startLine > 0) {
                areas.append(AreaReference{range.startLine, range.endLine, areaLineReferences(parsedLines, lineCount, range)});
            }
        }
    }

    if (targetDirective == QStringLiteral("line")) {
        const TherionParsedLine *parsedLine = parsedLineAt(parsedLines, lineNumber);
        const QString lineId = parsedLine != nullptr
            ? commandOptionValue(parsedLine->tokens, QStringLiteral("-id"))
            : QString();
        if (!lineId.isEmpty()) {
            for (const AreaReference &area : std::as_const(areas)) {
                if (area.lineIds.contains(lineId)) {
                    plan.errorMessage = QCoreApplication::translate(
                        "TherionStudio::MapEditorObjectDeletePlanner",
                        "Line `%1` is referenced by an area and cannot be deleted separately.")
                                            .arg(lineId);
                    return plan;
                }
            }
        }
    }

    const QVector<SourceRange> ranges = mergedRanges({targetRange});
    plan.updatedText = removeRangesFromSourceLines(sourceLines, ranges, &plan.removedLineNumbers);
    plan.changed = plan.updatedText != text;
    plan.resolved = true;
    plan.focusLineAfterDelete = ranges.isEmpty() ? std::max(1, targetRange.startLine) : std::max(1, ranges.first().startLine);
    if (!plan.changed) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorObjectDeletePlanner",
                                                        "Object deletion did not change the source.");
    }
    return plan;
}
}
