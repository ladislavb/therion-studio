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

[[nodiscard]] RawEditorCompletionTokenContext rawEditorCompletionTokenContextAtOffset(
    const TherionSourceLogicalDocument &logicalDocument,
    int cursorOffset);
}
