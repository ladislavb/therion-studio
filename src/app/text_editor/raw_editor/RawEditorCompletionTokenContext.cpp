#include "RawEditorCompletionTokenContext.h"

#include "../../../core/TherionSourceLogicalDocument.h"

namespace TherionStudio
{
namespace
{
const TherionSourceLogicalTokenRange *tokenAtCompletionCursorOffset(
    const TherionSourceLogicalDocument &logicalDocument,
    int cursorOffset)
{
    if (const TherionSourceLogicalTokenRange *tokenRange = logicalDocument.tokenAtOffset(cursorOffset)) {
        return tokenRange;
    }

    if (cursorOffset <= 0) {
        return nullptr;
    }

    const TherionSourceLogicalTokenRange *previousTokenRange = logicalDocument.tokenAtOffset(cursorOffset - 1);
    if (previousTokenRange == nullptr) {
        return nullptr;
    }

    const TherionSourcePhysicalRange &range = previousTokenRange->physicalRange;
    return range.startOffset + range.length == cursorOffset ? previousTokenRange : nullptr;
}

const TherionSourceLogicalCommand *commandAtCompletionCursorOffset(
    const TherionSourceLogicalDocument &logicalDocument,
    int cursorOffset)
{
    if (const TherionSourceLogicalCommand *command = logicalDocument.commandAtOffset(cursorOffset)) {
        return command;
    }

    if (cursorOffset <= 0) {
        return nullptr;
    }

    const TherionSourceLogicalCommand *previousCommand = logicalDocument.commandAtOffset(cursorOffset - 1);
    return previousCommand != nullptr && previousCommand->endOffset == cursorOffset ? previousCommand : nullptr;
}

RawEditorCompletionTokenContext rawEditorCompletionTokenContextForCommand(
    const TherionSourceLogicalCommand &command,
    const TherionSourceLogicalTokenRange *cursorTokenRange,
    int cursorOffset)
{
    RawEditorCompletionTokenContext context;
    context.parsedLine = command.parsed;
    context.tokenIndexAtCursor = context.parsedLine.tokens.size();

    if (cursorTokenRange != nullptr) {
        context.tokenIndexAtCursor = cursorTokenRange->tokenIndex;
        context.cursorInsideToken = true;
        return context;
    }

    for (const TherionSourceLogicalTokenRange &tokenRange : command.tokenRanges) {
        if (tokenRange.type == TherionTokenType::Comment) {
            continue;
        }
        const TherionSourcePhysicalRange &range = tokenRange.physicalRange;
        if (cursorOffset > range.startOffset + range.length) {
            context.tokenIndexAtCursor = tokenRange.tokenIndex + 1;
        }
    }

    return context;
}
}

RawEditorCompletionTokenContext rawEditorCompletionTokenContextAtPosition(
    const TherionSourceLogicalDocument &logicalDocument,
    int lineNumber,
    int columnNumber)
{
    RawEditorCompletionTokenContext context;
    const TherionSourceLogicalCommand *command = logicalDocument.commandAtPhysicalLine(lineNumber);
    if (command == nullptr) {
        return context;
    }

    context.parsedLine = command->parsed;
    context.tokenIndexAtCursor = context.parsedLine.tokens.size();
    if (const TherionSourceLogicalTokenRange *tokenRange =
            logicalDocument.tokenAtPhysicalPosition(lineNumber, columnNumber)) {
        context.tokenIndexAtCursor = tokenRange->tokenIndex;
        context.cursorInsideToken = true;
        return context;
    }

    for (const TherionSourceLogicalTokenRange &tokenRange : command->tokenRanges) {
        if (tokenRange.type == TherionTokenType::Comment) {
            continue;
        }
        const TherionSourcePhysicalRange &range = tokenRange.physicalRange;
        if (range.lineNumber < lineNumber
            || (range.lineNumber == lineNumber
                && columnNumber > range.columnNumber + range.columnLength)) {
            context.tokenIndexAtCursor = tokenRange.tokenIndex + 1;
        }
    }

    return context;
}

RawEditorCompletionTokenContext rawEditorCompletionTokenContextAtOffset(
    const TherionSourceLogicalDocument &logicalDocument,
    int cursorOffset)
{
    const TherionSourceLogicalCommand *command = commandAtCompletionCursorOffset(logicalDocument, cursorOffset);
    if (command == nullptr) {
        return {};
    }

    return rawEditorCompletionTokenContextForCommand(*command,
                                                     tokenAtCompletionCursorOffset(logicalDocument, cursorOffset),
                                                     cursorOffset);
}
}
