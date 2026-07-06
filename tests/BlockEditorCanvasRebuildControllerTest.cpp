#include "../src/app/text_editor/TextEditorCommandMetadata.h"
#include "../src/app/text_editor/block_editor/BlockEditorCanvasItem.h"
#include "../src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.h"
#include "../src/app/text_editor/block_editor/BlockEditorDirectiveRules.h"
#include "../src/app/text_editor/block_editor/BlockEditorSourceText.h"
#include "../src/core/TherionDocumentParser.h"
#include "../src/core/TherionSourceSnapshotCache.h"

#include <QApplication>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QPlainTextEdit>

#include <iostream>

using namespace TherionStudio;
using namespace TherionStudio::BlockEditorDirectiveRules;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

TextEditorCommandMetadata commandMetadata()
{
    TextEditorCommandMetadata metadata;
    metadata.contextCommandTokens.insert(QStringLiteral("none"),
                                         {QStringLiteral("encoding"), QStringLiteral("survey")});
    metadata.contextCommandTokens.insert(QStringLiteral("survey"),
                                         {QStringLiteral("centerline")});
    metadata.contextCommandTokens.insert(QStringLiteral("centerline"),
                                         {QStringLiteral("data"),
                                          QStringLiteral("date"),
                                          QStringLiteral("extend"),
                                          QStringLiteral("team")});
    return metadata;
}

bool commandInScope(const TextEditorCommandMetadata &metadata, const QString &directive, const QString &scope)
{
    QStringList candidates = metadata.contextCommandTokens.value(scope);
    candidates.append(metadata.contextCommandTokens.value(QStringLiteral("all")));
    if (scope == QStringLiteral("none")) {
        candidates.append(metadata.contextCommandTokens.value(QStringLiteral("none")));
    }
    return candidates.contains(normalizeDirective(directive), Qt::CaseInsensitive);
}

QString resolveScopeForCommandAtLine(const TextEditorCommandMetadata &metadata,
                                     const QString &command,
                                     const QStringList &lines,
                                     int lineNumber)
{
    const QString normalizedCommand = normalizeDirective(command);
    const int lastLine = qBound(0, lineNumber - 1, lines.size());
    QStringList scopeStack;
    for (int lineIndex = 0; lineIndex < lastLine; ++lineIndex) {
        const TherionParsedLine parsedLine =
            TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }
        const QString openingDirective = completionOpeningDirectiveForClosing(directive);
        if (!openingDirective.isEmpty()) {
            for (int stackIndex = scopeStack.size() - 1; stackIndex >= 0; --stackIndex) {
                if (scopeStack.at(stackIndex) == openingDirective) {
                    scopeStack.removeAt(stackIndex);
                    break;
                }
            }
            continue;
        }
        if (isContainerDirectiveInstance(directive, parsedLine)) {
            scopeStack.append(directive);
        }
    }

    for (int stackIndex = scopeStack.size() - 1; stackIndex >= 0; --stackIndex) {
        const QString scope = scopeStack.at(stackIndex);
        if (commandInScope(metadata, normalizedCommand, scope)) {
            return scope;
        }
    }
    return commandInScope(metadata, normalizedCommand, QStringLiteral("none"))
        ? QStringLiteral("none")
        : QString();
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const TextEditorCommandMetadata metadata = commandMetadata();
    const QString sourceText = QStringLiteral(
        "encoding utf-8\n"
        "#\n"
        "survey test\n"
        "centreline\n"
        "  date 2006.08.12\n"
        "  data normal from to compass clino tape\n"
        "  extend right\n"
        "  1 2 12.3 45.0 3.0\n"
        "  extend left\n"
        "  2 3 4.0 120.0 0.5\n"
        "  team \\\n"
        "    Alice\n"
        "endcentreline\n"
        "endsurvey\n");
    QPlainTextEdit editor;
    editor.setPlainText(sourceText);

    const QStringList sourceLines = blockEditorNormalizedSourceLines(sourceText);
    const QVector<BlockEditorLogicalLine> sourceLogicalLines = blockEditorBuildLogicalLines(sourceLines);
    TherionSourceSnapshotCache sourceSnapshotCache;
    TherionSourceDocumentMetadata sourceMetadata;
    const TherionSourceDocument &sourceDocument =
        sourceSnapshotCache.sourceDocument(sourceText, sourceMetadata);
    const TherionSourceLogicalDocument &logicalDocument =
        sourceSnapshotCache.logicalDocument(sourceText, sourceMetadata);

    auto logicalLineAt = [&sourceLogicalLines](int startLine) {
        for (const BlockEditorLogicalLine &logicalLine : sourceLogicalLines) {
            if (logicalLine.startLine == startLine) {
                return logicalLine;
            }
        }
        return BlockEditorLogicalLine();
    };

    bool ok = true;
    const TherionParsedLine commentParsedLine =
        blockEditorParsedLineForLogicalLine(logicalLineAt(2), &sourceDocument, &logicalDocument);
    ok &= expect(isFullLineComment(commentParsedLine),
                 "Block editor logical parsing should preserve physical full-line comments from the source DOM.");
    const TherionParsedLine teamParsedLine =
        blockEditorParsedLineForLogicalLine(logicalLineAt(11), &sourceDocument, &logicalDocument);
    ok &= expect(teamParsedLine.directive == QStringLiteral("team")
                     && teamParsedLine.tokens == QStringList({QStringLiteral("team"), QStringLiteral("Alice")}),
                 "Block editor logical parsing should prefer DOM-parsed continuation commands.");

    QGraphicsScene scene;
    QGraphicsLineItem *movePreviewLine = nullptr;
    QList<QGraphicsLineItem *> guideItems;
    QHash<int, qreal> boundaryEndYByLine;
    bool tearingDown = false;

    BlockEditorCanvasRebuildContext context;
    context.scene = &scene;
    context.movePreviewLine = &movePreviewLine;
    context.containerBoundaryGuideItems = &guideItems;
    context.containerBoundaryEndYByLine = &boundaryEndYByLine;
    context.tearingDown = &tearingDown;
    context.commandMetadata = &metadata;
    context.sourceContext = [&editor]() {
        BlockEditorSourceContext sourceContext;
        sourceContext.editor = &editor;
        return sourceContext;
    };
    context.isBlocksModeSupportedForCurrentFile = []() {
        return true;
    };
    context.resolveScopeForCommandAtLine = [&metadata](const QString &command, const QStringList &lines, int lineNumber) {
        return resolveScopeForCommandAtLine(metadata, command, lines, lineNumber);
    };
    context.handleBlockDeleteRequest = [](int) {};
    context.handleBlockMoveRequest = [](int, const QPointF &) {};
    context.updateBlockMovePreview = [](int, const QPointF &) {};
    context.clearBlockMovePreview = []() {};
    context.refreshBlockDetailsSelectionFromScene = []() {};

    BlockEditorCanvasRebuildController(context).rebuildBlocksCanvasFromText();

    bool foundEmptyComment = false;
    bool foundData = false;
    bool foundTeam = false;
    for (QGraphicsItem *graphicsItem : scene.items()) {
        auto *blockItem = dynamic_cast<BlockCanvasItem *>(graphicsItem);
        if (blockItem == nullptr) {
            continue;
        }
        const QString kind = normalizeDirective(blockItem->kind());
        if (kind == QStringLiteral("comment") && blockItem->lineNumber() == 2) {
            foundEmptyComment = true;
        }
        if (kind == QStringLiteral("data")) {
            foundData = true;
        }
        if (kind == QStringLiteral("team")) {
            foundTeam = true;
        }
        ok &= expect(kind != QStringLiteral("extend"),
                     "Canvas rebuild should not create standalone extend cards inside data bodies.");
        ok &= expect(kind != QStringLiteral("alice"),
                     "Canvas rebuild should not create standalone cards from continuation rows.");
    }
    ok &= expect(foundEmptyComment,
                 "Canvas rebuild should create a card for an empty full-line comment.");
    ok &= expect(foundData, "Canvas rebuild should still create the data card.");
    ok &= expect(foundTeam, "Canvas rebuild should create a card for the continued team command after data rows.");

    return ok ? 0 : 1;
}
