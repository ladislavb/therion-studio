#pragma once

#include "../../../core/TherionDocumentParser.h"

namespace TherionStudio
{
class TherionSourceLogicalDocument;

struct RawEditorCompletionTokenContext
{
    TherionParsedLine parsedLine;
    int tokenIndexAtCursor = 0;
    bool cursorInsideToken = false;
};

[[nodiscard]] RawEditorCompletionTokenContext rawEditorCompletionTokenContextAtPosition(
    const TherionSourceLogicalDocument &logicalDocument,
    int lineNumber,
    int columnNumber);

[[nodiscard]] RawEditorCompletionTokenContext rawEditorCompletionTokenContextAtOffset(
    const TherionSourceLogicalDocument &logicalDocument,
    int cursorOffset);
}
