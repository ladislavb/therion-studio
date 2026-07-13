#include "BlockEditorDocumentOutlineBuilder.h"

#include "BlockEditorDirectiveRules.h"
#include "BlockEditorSourceText.h"

#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionSourceSnapshotCache.h"

#include <utility>

namespace
{
struct OpenContainer
{
    QString directive;
    int lineNumber = 0;
};

int nearestOpenContainerLine(const QVector<OpenContainer> &openStack, const QString &directive)
{
    for (int index = openStack.size() - 1; index >= 0; --index) {
        if (openStack.at(index).directive == directive) {
            return openStack.at(index).lineNumber;
        }
    }
    return 0;
}
}

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

BlockEditorDocumentOutlineBuilder::BlockEditorDocumentOutlineBuilder(BlockEditorDocumentOutlineContext context)
    : context_(std::move(context))
{
}

BlockEditorDocumentOutline BlockEditorDocumentOutlineBuilder::buildFromContents(const QString &contents) const
{
    BlockEditorDocumentOutline outline;
    outline.lines = blockEditorNormalizedSourceLines(contents);

    if (!context_.resolveScopeForCommandAtLine
        || !context_.isContainerDirectiveInstanceForParsedLine
        || !context_.isCommandDirectiveInScope) {
        return outline;
    }

    TherionSourceSnapshotCache sourceSnapshotCache;
    TherionSourceDocumentMetadata sourceMetadata;
    sourceMetadata.revisionId = 1;
    const TherionSourceLogicalDocument &logicalDocument =
        sourceSnapshotCache.logicalDocument(contents, sourceMetadata);

    outline.entries.reserve(logicalDocument.commands().size());
    QVector<OpenContainer> openStack;
    int activeDataEntryEndLine = 0;

    for (int parsedIndex = 0; parsedIndex < logicalDocument.commands().size(); ++parsedIndex) {
        const TherionSourceLogicalCommand &logicalCommand = logicalDocument.commands().at(parsedIndex);
        const TherionParsedLine &parsedLine = logicalCommand.parsed;
        const int currentLineNumber = parsedLine.lineNumber;
        const BlockEditorLogicalLine currentLogicalLine{
            logicalCommand.startLineNumber,
            logicalCommand.endLineNumber,
            logicalCommand.text,
        };
        if (currentLineNumber <= activeDataEntryEndLine) {
            continue;
        }
        if (isFullLineComment(parsedLine)) {
            BlockEditorDocumentEntry entry;
            entry.kind = QStringLiteral("comment");
            entry.startLine = currentLogicalLine.startLine;
            entry.endLine = currentLogicalLine.endLine;
            entry.parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            outline.entryIndexByStartLine.insert(entry.startLine, outline.entries.size());
            outline.entries.append(entry);
            continue;
        }

        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            const QString openingDirective = completionOpeningDirectiveForClosing(directive);
            if (openingDirective.isEmpty()) {
                continue;
            }
            for (int index = openStack.size() - 1; index >= 0; --index) {
                if (openStack.at(index).directive == openingDirective) {
                    openStack.remove(index, openStack.size() - index);
                    break;
                }
            }
            continue;
        }

        if (isBlockOpeningDirective(directive)) {
            int parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            if (directive == QStringLiteral("data")) {
                const QString dataScope = context_.resolveScopeForCommandAtLine(QStringLiteral("data"),
                                                                                 outline.lines,
                                                                                 currentLogicalLine.startLine);
                const int dataScopeParentLine = nearestOpenContainerLine(openStack, dataScope);
                if (dataScopeParentLine > 0) {
                    parentLine = dataScopeParentLine;
                }
            }

            BlockEditorDocumentEntry entry;
            entry.kind = directive;
            entry.startLine = currentLogicalLine.startLine;
            entry.endLine = currentLogicalLine.endLine;
            entry.parentLine = parentLine;

            if (context_.isContainerDirectiveInstanceForParsedLine(directive, parsedLine)) {
                const QString closingDirective = closingDirectiveFor(directive);
                const int endLine = findMatchingBlockEndLine(outline.lines,
                                                             currentLineNumber,
                                                             directive,
                                                             closingDirective);
                if (endLine > currentLineNumber) {
                    entry.endLine = endLine;
                }
                openStack.append(OpenContainer{directive, currentLineNumber});
            } else if (directive == QStringLiteral("data")) {
                const QString dataScope = context_.resolveScopeForCommandAtLine(QStringLiteral("data"),
                                                                                 outline.lines,
                                                                                 currentLogicalLine.startLine);
                const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);
                const int dataScopeParentLine = nearestOpenContainerLine(openStack, dataScope);
                int dataScopeEndLine = outline.lines.size() + 1;
                if (dataScopeParentLine > 0) {
                    const int resolvedEndLine = findMatchingBlockEndLine(outline.lines,
                                                                         dataScopeParentLine,
                                                                         dataScope,
                                                                         dataScopeClosing);
                    if (resolvedEndLine > dataScopeParentLine) {
                        dataScopeEndLine = resolvedEndLine;
                    }
                }

                int dataBodyLastLine = dataScopeEndLine - 1;
                for (int scanLine = currentLineNumber + 1; scanLine <= dataScopeEndLine - 1; ++scanLine) {
                    const TherionSourceLogicalCommand *scanCommand = logicalDocument.commandAtPhysicalLine(scanLine);
                    const QString scanDirective = scanCommand != nullptr
                        ? normalizeDirective(scanCommand->parsed.directive)
                        : QString();
                    if (scanDirective.isEmpty() || scanDirective == QStringLiteral("extend")) {
                        continue;
                    }
                    if (scanDirective == QStringLiteral("data")
                        || (!dataScopeClosing.isEmpty() && scanDirective == dataScopeClosing)
                        || (!dataScope.isEmpty() && context_.isCommandDirectiveInScope(scanDirective, dataScope))) {
                        dataBodyLastLine = scanLine - 1;
                        break;
                    }
                }
                if (dataBodyLastLine < currentLineNumber) {
                    dataBodyLastLine = currentLineNumber;
                }
                entry.endLine = qMax(dataBodyLastLine, currentLogicalLine.endLine);
                activeDataEntryEndLine = entry.endLine;
            }

            outline.entryIndexByStartLine.insert(entry.startLine, outline.entries.size());
            outline.entries.append(entry);
            continue;
        }

        QString activeScope = QStringLiteral("none");
        if (!openStack.isEmpty()) {
            activeScope = normalizeDirective(openStack.last().directive);
        }

        const bool commandDirective = !isBlockOpeningDirective(directive)
            && context_.isCommandDirectiveInScope(directive, activeScope);
        if (commandDirective || isMapObjectReferenceCandidateLine(activeScope, parsedLine, commandDirective)) {
            BlockEditorDocumentEntry entry;
            entry.kind = commandDirective ? directive : mapObjectReferenceKind();
            entry.startLine = currentLogicalLine.startLine;
            entry.endLine = currentLogicalLine.endLine;
            entry.parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            outline.entryIndexByStartLine.insert(entry.startLine, outline.entries.size());
            outline.entries.append(entry);
        }
    }

    return outline;
}
}
