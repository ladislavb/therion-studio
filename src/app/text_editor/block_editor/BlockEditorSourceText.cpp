#include "BlockEditorSourceText.h"

#include "../../../core/TherionDocumentEditor.h"
#include "../../../core/TherionSourceDocument.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionSourceText.h"

#include <QtGlobal>

namespace TherionStudio
{
namespace
{
QString rightTrimmed(const QString &line)
{
    QString trimmed = line;
    while (!trimmed.isEmpty() && trimmed.back().isSpace()) {
        trimmed.chop(1);
    }
    return trimmed;
}

bool endsWithContinuationMarker(const QString &line)
{
    const QString trimmed = rightTrimmed(line);
    if (trimmed.isEmpty()) {
        return false;
    }

    int trailingBackslashCount = 0;
    int index = trimmed.size() - 1;
    while (index >= 0 && trimmed.at(index) == QLatin1Char('\\')) {
        ++trailingBackslashCount;
        --index;
    }

    return trailingBackslashCount % 2 == 1;
}

QString stripTrailingContinuationMarker(const QString &line)
{
    QString trimmed = rightTrimmed(line);
    if (!trimmed.isEmpty() && trimmed.back() == QLatin1Char('\\')) {
        trimmed.chop(1);
    }
    return trimmed;
}
}

QString blockEditorSourceLineEnding(const QString &contents)
{
    return TherionSourceText::detectedLineEnding(contents);
}

QStringList blockEditorNormalizedSourceLines(const QString &contents)
{
    return TherionSourceText::splitTextLines(contents);
}

QString blockEditorJoinSourceLines(const QString &originalContents, const QStringList &lines)
{
    return TherionSourceText::joinTextLines(originalContents, lines);
}

QVector<BlockEditorLogicalLine> blockEditorBuildLogicalLines(const QStringList &lines)
{
    QVector<BlockEditorLogicalLine> logicalLines;
    logicalLines.reserve(lines.size());

    int lineIndex = 0;
    while (lineIndex < lines.size()) {
        const int startLine = lineIndex + 1;
        int endLine = startLine;
        QString logicalText;
        bool hasContinuation = false;

        int currentIndex = lineIndex;
        while (currentIndex < lines.size()) {
            const QString currentLine = lines.at(currentIndex);
            const bool lineContinues = endsWithContinuationMarker(currentLine);
            const QString currentPart = lineContinues
                ? stripTrailingContinuationMarker(currentLine)
                : currentLine;
            const QString normalizedPart = hasContinuation
                ? currentPart.trimmed()
                : currentPart;
            if (!normalizedPart.isEmpty()) {
                if (!logicalText.isEmpty()) {
                    logicalText.append(QLatin1Char(' '));
                }
                logicalText.append(normalizedPart);
            }

            hasContinuation = lineContinues;
            endLine = currentIndex + 1;
            ++currentIndex;
            if (!lineContinues) {
                break;
            }
        }

        logicalLines.append(BlockEditorLogicalLine{
            startLine,
            endLine,
            logicalText
        });
        lineIndex = currentIndex;
    }

    return logicalLines;
}

bool blockEditorResolveLogicalLineAtLine(const QStringList &lines,
                                         int lineNumber,
                                         BlockEditorLogicalLine *logicalLine)
{
    if (lineNumber <= 0 || lineNumber > lines.size() || logicalLine == nullptr) {
        return false;
    }

    const QVector<BlockEditorLogicalLine> logicalLines = blockEditorBuildLogicalLines(lines);
    for (const BlockEditorLogicalLine &candidate : logicalLines) {
        if (lineNumber < candidate.startLine || lineNumber > candidate.endLine) {
            continue;
        }
        *logicalLine = candidate;
        return true;
    }

    return false;
}

TherionParsedLine blockEditorParsedLineForLogicalLine(const BlockEditorLogicalLine &logicalLine,
                                                      const TherionSourceDocument *sourceDocument,
                                                      const TherionSourceLogicalDocument *logicalDocument)
{
    if (logicalDocument != nullptr) {
        const TherionSourceLogicalCommand *logicalCommand =
            logicalDocument->commandAtPhysicalLine(logicalLine.startLine);
        if (logicalCommand != nullptr && logicalCommand->startLineNumber == logicalLine.startLine) {
            return logicalCommand->parsed;
        }
    }

    if (sourceDocument != nullptr) {
        if (const TherionSourceDocumentLine *sourceLine =
                sourceDocument->lineAtLineNumber(logicalLine.startLine);
            sourceLine != nullptr) {
            return sourceLine->sourceLine.parsed;
        }
    }

    return TherionDocumentParser::parseLine(logicalLine.text, logicalLine.startLine);
}

bool blockEditorSourceLineRangeReplacementEdit(const QString &contents,
                                               int startLine,
                                               int endLine,
                                               const QStringList &replacementLines,
                                               TherionSourceTextEdit *edit)
{
    if (edit == nullptr || startLine <= 0 || endLine < startLine - 1) {
        return false;
    }

    const TherionSourceText sourceText = TherionSourceText::fromText(contents);
    const QVector<TherionSourceLine> &sourceLines = sourceText.physicalLines();
    const int removeStartIndex = startLine - 1;
    const int removeCount = endLine >= startLine ? endLine - startLine + 1 : 0;
    if (removeStartIndex < 0 || removeStartIndex > sourceLines.size() || removeStartIndex + removeCount > sourceLines.size()) {
        return false;
    }

    QString replacementLineEnding;
    if (removeCount > 0 && removeStartIndex < sourceLines.size()) {
        replacementLineEnding = sourceLines.at(removeStartIndex).lineEnding;
    }
    if (replacementLineEnding.isEmpty() && removeStartIndex > 0) {
        replacementLineEnding = sourceLines.at(removeStartIndex - 1).lineEnding;
    }
    if (replacementLineEnding.isEmpty() && removeStartIndex < sourceLines.size()) {
        replacementLineEnding = sourceLines.at(removeStartIndex).lineEnding;
    }
    if (replacementLineEnding.isEmpty()) {
        replacementLineEnding = sourceText.preferredLineEnding();
    }

    QString updatedContents;
    for (int index = 0; index < removeStartIndex; ++index) {
        updatedContents += sourceLines.at(index).text;
        updatedContents += sourceLines.at(index).lineEnding;
    }

    const bool followedByExistingLine = removeStartIndex + removeCount < sourceLines.size();
    for (int index = 0; index < replacementLines.size(); ++index) {
        const bool lastReplacementLine = index == replacementLines.size() - 1;
        updatedContents += replacementLines.at(index);
        if (!lastReplacementLine || followedByExistingLine) {
            updatedContents += replacementLineEnding;
        }
    }

    for (int index = removeStartIndex + removeCount; index < sourceLines.size(); ++index) {
        updatedContents += sourceLines.at(index).text;
        updatedContents += sourceLines.at(index).lineEnding;
    }

    return blockEditorSourceReplacementEdit(contents, updatedContents, edit);
}

bool blockEditorSourceReplacementEdit(const QString &contents,
                                      const QString &updatedContents,
                                      TherionSourceTextEdit *edit)
{
    if (edit == nullptr) {
        return false;
    }

    int commonPrefix = 0;
    const int prefixLimit = qMin(contents.size(), updatedContents.size());
    while (commonPrefix < prefixLimit && contents.at(commonPrefix) == updatedContents.at(commonPrefix)) {
        ++commonPrefix;
    }

    int commonSuffix = 0;
    while (commonSuffix < contents.size() - commonPrefix
           && commonSuffix < updatedContents.size() - commonPrefix
           && contents.at(contents.size() - 1 - commonSuffix) == updatedContents.at(updatedContents.size() - 1 - commonSuffix)) {
        ++commonSuffix;
    }

    *edit = TherionSourceTextEdit{
        commonPrefix,
        static_cast<int>(contents.size() - commonPrefix - commonSuffix),
        updatedContents.mid(commonPrefix, updatedContents.size() - commonPrefix - commonSuffix),
    };
    return true;
}
}
