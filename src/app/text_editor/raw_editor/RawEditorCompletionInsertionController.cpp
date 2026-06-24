#include "RawEditorCompletionInsertionController.h"

#include "../../../core/TherionDocumentParser.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>

#include <utility>

namespace TherionStudio
{
namespace
{
struct CompletionReplacementRange
{
    int start = 0;
    int end = 0;
};

CompletionReplacementRange completionWordRange(const QString &blockText, int cursorColumn)
{
    CompletionReplacementRange range{cursorColumn, cursorColumn};

    auto isCompletionCharacter = [](QChar ch) {
        return ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_');
    };

    while (range.start > 0 && isCompletionCharacter(blockText.at(range.start - 1))) {
        --range.start;
    }
    while (range.end < blockText.length() && isCompletionCharacter(blockText.at(range.end))) {
        ++range.end;
    }

    return range;
}

CompletionReplacementRange inputPathReplacementRange(const QString &blockText, int cursorColumn)
{
    CompletionReplacementRange range{cursorColumn, cursorColumn};
    const int clampedColumn = qBound(0, cursorColumn, blockText.size());

    int quoteStart = -1;
    QChar quoteChar;
    for (int index = 0; index < clampedColumn; ++index) {
        const QChar ch = blockText.at(index);
        if (quoteStart >= 0) {
            if (ch == quoteChar) {
                quoteStart = -1;
                quoteChar = QChar();
            }
            continue;
        }
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
            quoteStart = index;
            quoteChar = ch;
        }
    }

    if (quoteStart >= 0) {
        range.start = quoteStart + 1;
        range.end = clampedColumn;
        for (int index = clampedColumn; index < blockText.size(); ++index) {
            const QChar ch = blockText.at(index);
            if (ch == quoteChar) {
                break;
            }
            range.end = index + 1;
        }
        return range;
    }

    while (range.start > 0) {
        const QChar ch = blockText.at(range.start - 1);
        if (ch.isSpace() || ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
            break;
        }
        --range.start;
    }
    while (range.end < blockText.length()) {
        const QChar ch = blockText.at(range.end);
        if (ch.isSpace() || ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
            break;
        }
        ++range.end;
    }

    return range;
}

bool isInputCommandLine(const QString &blockText)
{
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(blockText);
    return parsedLine.directive == QStringLiteral("input");
}
}

RawEditorCompletionInsertionController::RawEditorCompletionInsertionController(RawEditorCompletionInsertionContext context)
    : context_(std::move(context))
{
}

void RawEditorCompletionInsertionController::insertCompletionToken(const QString &completion)
{
    if (context_.editor == nullptr || !context_.normalizedDirectiveToken || !context_.closingDirectiveForOpeningToken
        || completion.trimmed().isEmpty()) {
        return;
    }

    QTextCursor cursor = context_.editor->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return;
    }

    const QString blockText = block.text();
    const CompletionReplacementRange replacementRange = isInputCommandLine(blockText)
        ? inputPathReplacementRange(blockText, cursor.positionInBlock())
        : completionWordRange(blockText, cursor.positionInBlock());
    const int start = replacementRange.start;
    const int end = replacementRange.end;

    const QString normalizedCompletion = context_.normalizedDirectiveToken(completion.toLower());
    const QString closingDirective = context_.closingDirectiveForOpeningToken(normalizedCompletion);
    const QString leftTrimmed = blockText.left(start).trimmed();
    const QString rightTrimmed = blockText.mid(end).trimmed();
    const bool firstTokenOnlyLine = leftTrimmed.isEmpty() && rightTrimmed.isEmpty();
    bool shouldInsertClosingPair = !closingDirective.isEmpty() && firstTokenOnlyLine;

    QString lineIndent;
    for (int index = 0; index < blockText.length(); ++index) {
        const QChar ch = blockText.at(index);
        if (!ch.isSpace() || ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
            break;
        }
        lineIndent.append(ch);
    }

    if (shouldInsertClosingPair) {
        const QTextBlock nextBlock = block.next();
        if (nextBlock.isValid()) {
            const QString nextText = nextBlock.text().trimmed().toLower();
            if (context_.normalizedDirectiveToken(nextText) == closingDirective) {
                shouldInsertClosingPair = false;
            }
        }
    }

    cursor.beginEditBlock();
    cursor.setPosition(block.position() + start);
    cursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
    cursor.insertText(completion);
    const int completionEndPos = cursor.position();

    if (shouldInsertClosingPair) {
        cursor.insertText(QStringLiteral("\n%1%2").arg(lineIndent, closingDirective));
        cursor.setPosition(completionEndPos);
    }

    cursor.endEditBlock();
    context_.editor->setTextCursor(cursor);
}
}
