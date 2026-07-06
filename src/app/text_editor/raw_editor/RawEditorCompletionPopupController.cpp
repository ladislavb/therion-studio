#include "RawEditorCompletionPopupController.h"

#include "RawEditorCompletionContextAnalyzer.h"
#include "RawEditorCompletionSuggestionBuilder.h"
#include "RawEditorCompletionTokenContext.h"
#include "../TextEditorCommandMetadata.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QCoreApplication>
#include <QDir>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolTip>

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionFileTypes.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../TextEditorSourceSnapshotContext.h"

#include <utility>

namespace TherionStudio
{
RawEditorCompletionPopupController::RawEditorCompletionPopupController(RawEditorCompletionPopupContext context)
    : context_(std::move(context))
{
}

RawEditorCompletionContext RawEditorCompletionPopupController::completionContext() const
{
    RawEditorCompletionContext context;
    context.editor = context_.editor;
    context.metadata = context_.metadata;
    context.sourceSnapshotCache = context_.sourceSnapshotCache;
    context.normalizedDirectiveToken = context_.normalizedDirectiveToken;
    context.openingDirectiveForClosingToken = context_.openingDirectiveForClosingToken;
    context.isContainerDirectiveInstance = context_.isContainerDirectiveInstance;
    return context;
}

RawEditorCompletionSuggestionContext RawEditorCompletionPopupController::suggestionContext() const
{
    RawEditorCompletionSuggestionContext context;
    context.editor = context_.editor;
    context.metadata = context_.metadata;
    context.sourceSnapshotCache = context_.sourceSnapshotCache;
    context.projectRootPath = context_.projectRootPath;
    context.filePath = context_.filePath;
    context.normalizedDirectiveToken = context_.normalizedDirectiveToken;
    context.openingDirectiveForClosingToken = context_.openingDirectiveForClosingToken;
    context.isContainerDirectiveInstance = context_.isContainerDirectiveInstance;
    return context;
}

void RawEditorCompletionPopupController::triggerCompletionPopup()
{
    if (context_.editor == nullptr
        || context_.completionCompleter == nullptr
        || context_.completionModel == nullptr
        || context_.metadata == nullptr) {
        return;
    }

    const RawEditorCompletionContextAnalyzer contextAnalyzer(completionContext());
    const RawEditorCompletionSuggestionBuilder suggestionBuilder(suggestionContext());
    const QString command = contextAnalyzer.currentCompletionCommand();

    QString rawPathPrefix;
    if (command == QStringLiteral("input")) {
        const QTextCursor cursor = context_.editor->textCursor();
        const QTextBlock block = cursor.block();
        if (block.isValid()) {
            const QString blockText = block.text();
            int start = cursor.positionInBlock();
            int end = cursor.positionInBlock();
            while (start > 0 && !blockText.at(start - 1).isSpace()) {
                --start;
            }
            while (end < blockText.length() && !blockText.at(end).isSpace()) {
                ++end;
            }
            rawPathPrefix = QDir::fromNativeSeparators(blockText.mid(start, end - start).trimmed());
        }
    }

    const QString prefix = contextAnalyzer.currentCompletionPrefix();
    QStringList suggestions = suggestionBuilder.buildCompletionSuggestionsForCursor(prefix);
    if (command == QStringLiteral("input") && rawPathPrefix.startsWith(QStringLiteral("./"))) {
        QStringList sameDirectorySuggestions;
        for (const QString &candidate : suggestions) {
            if (!candidate.startsWith(QStringLiteral("../"))) {
                if (!sameDirectorySuggestions.contains(candidate, Qt::CaseInsensitive)) {
                    sameDirectorySuggestions.append(candidate);
                }
            }
        }
        suggestions = sameDirectorySuggestions;
    }

    if (suggestions.isEmpty()) {
        if (context_.completionCompleter->popup() != nullptr) {
            context_.completionCompleter->popup()->hide();
        }

        const int requiredPositionalCount = qMax(0, context_.metadata->commandRequiredPositionalCount.value(command));
        if (!command.isEmpty() && requiredPositionalCount > 0) {
            const QTextCursor cursor = context_.editor->textCursor();
            const QTextBlock block = cursor.block();
            if (block.isValid()) {
                const int cursorOffset = cursor.position();
                const TherionStudio::TextEditorSourceSnapshotContext snapshotContext =
                    TherionStudio::TextEditorSourceSnapshotContext::fromEditor(
                    context_.editor,
                    therionSourceDocumentTypeForFilePath(context_.filePath));
                TherionSourceSnapshotCache fallbackSourceSnapshotCache;
                const TherionSourceLogicalDocument *logicalDocument = context_.sourceSnapshotCache != nullptr
                    ? &snapshotContext.logicalDocument(*context_.sourceSnapshotCache)
                    : &snapshotContext.logicalDocument(fallbackSourceSnapshotCache);
                const TherionSourceLogicalCommand *logicalCommand = logicalDocument->commandAtOffset(cursorOffset);
                if (logicalCommand == nullptr && cursorOffset > 0) {
                    const TherionSourceLogicalCommand *previousCommand = logicalDocument->commandAtOffset(cursorOffset - 1);
                    if (previousCommand != nullptr && previousCommand->endOffset == cursorOffset) {
                        logicalCommand = previousCommand;
                    }
                }
                if (logicalCommand == nullptr) {
                    return;
                }
                const RawEditorCompletionTokenContext tokenContext =
                    rawEditorCompletionTokenContextAtOffset(*logicalDocument, cursorOffset);
                const int positionalCountBeforeCursor = positionalTokenCountBeforeCursor(logicalCommand->parsed,
                                                                                         tokenContext.tokenIndexAtCursor);
                if (positionalCountBeforeCursor < requiredPositionalCount) {
                    const int nextArgumentIndex = positionalCountBeforeCursor + 1;
                    QToolTip::showText(context_.editor->mapToGlobal(context_.editor->cursorRect().bottomLeft()),
                                       QCoreApplication::translate("TherionStudio::TextEditorTab",
                                                                   "Enter required argument %1 before options.")
                                           .arg(nextArgumentIndex),
                                       context_.editor,
                                       QRect(),
                                       1500);
                }
            }
        }
        return;
    }

    context_.completionModel->setStringList(suggestions);
    QString popupPrefix = prefix;
    if (command == QStringLiteral("input")) {
        popupPrefix = suggestionBuilder.normalizeInputCompletionPrefix(rawPathPrefix);
    }
    context_.completionCompleter->setCompletionPrefix(popupPrefix);
    if (context_.completionCompleter->completionCount() <= 0) {
        return;
    }

    QRect cursorRect = context_.editor->cursorRect();
    if (context_.completionCompleter->popup() != nullptr) {
        const int popupWidth = context_.completionCompleter->popup()->sizeHintForColumn(0)
            + context_.completionCompleter->popup()->verticalScrollBar()->sizeHint().width() + 12;
        cursorRect.setWidth(qMax(cursorRect.width(), popupWidth));
    }
    context_.completionCompleter->complete(cursorRect);

    const QString scopeLabel = contextAnalyzer.currentCompletionScopeLabel();
    if (!scopeLabel.isEmpty()) {
        QToolTip::showText(context_.editor->mapToGlobal(cursorRect.bottomLeft()),
                           QCoreApplication::translate("TherionStudio::TextEditorTab", "Completion scope: %1")
                               .arg(scopeLabel),
                           context_.editor,
                           QRect(),
                           1500);
    }
}
}
