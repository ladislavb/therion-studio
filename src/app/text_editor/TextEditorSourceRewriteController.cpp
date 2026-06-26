#include "TextEditorSourceRewriteController.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtGlobal>

#include <algorithm>
#include <utility>

#include "../../core/TherionDocumentEditor.h"

namespace TherionStudio
{
namespace
{
QString staleSourceTransactionMessage(const TextEditorSourceTransactionRequest &request)
{
    return request.staleStatusMessage.isEmpty()
        ? QStringLiteral("Source transaction skipped: document changed.")
        : request.staleStatusMessage;
}
}

TextEditorSourceRewriteController::TextEditorSourceRewriteController(TextEditorSourceRewriteContext context)
    : context_(std::move(context))
{
}

bool TextEditorSourceRewriteController::applyTransactionRequestWithEditorUndo(const TextEditorSourceTransactionRequest &request,
                                                                               QString *statusMessage)
{
    return applyTransactionRequestWithEditorUndoResult(request, statusMessage)
        == TextEditorSourceTransactionResult::Applied;
}

TextEditorSourceTransactionResult TextEditorSourceRewriteController::applyTransactionRequestWithEditorUndoResult(
    const TextEditorSourceTransactionRequest &request,
    QString *statusMessage)
{
    if (context_.editor == nullptr) {
        return TextEditorSourceTransactionResult::Unavailable;
    }

    const QString currentText = context_.editor->toPlainText();
    if (request.expectedSourceRevision > 0) {
        QTextDocument *document = context_.editor->document();
        if (document == nullptr || document->revision() != request.expectedSourceRevision) {
            if (statusMessage != nullptr) {
                *statusMessage = staleSourceTransactionMessage(request);
            }
            return TextEditorSourceTransactionResult::Stale;
        }
    }
    if (currentText != request.beforeText) {
        if (statusMessage != nullptr) {
            *statusMessage = staleSourceTransactionMessage(request);
        }
        return TextEditorSourceTransactionResult::Stale;
    }

    QString afterText = request.afterText;
    if (!request.sourceEdits.isEmpty()) {
        afterText = request.beforeText;
        if (!TherionDocumentEditor::applySourceTextEdits(&afterText, request.sourceEdits)) {
            return TextEditorSourceTransactionResult::InvalidEdit;
        }
    }
    if (afterText == request.beforeText) {
        return TextEditorSourceTransactionResult::NoChange;
    }

    const bool rebuildBlocksCanvas = request.projectionInvalidationPolicy
        != TextEditorSourceProjectionInvalidationPolicy::None;
    if (!request.sourceEdits.isEmpty()) {
        applyTextEditsPreservingCursor(request.sourceEdits,
                                       true,
                                       rebuildBlocksCanvas,
                                       true,
                                       true);
    } else {
        replaceTextPreservingCursor(afterText,
                                    true,
                                    rebuildBlocksCanvas,
                                    true,
                                    true);
    }

    if (request.projectionInvalidationPolicy == TextEditorSourceProjectionInvalidationPolicy::CustomHook
        && request.projectionInvalidationHook) {
        request.projectionInvalidationHook();
    }
    if (request.selectionRestorePolicy == TextEditorSourceSelectionRestorePolicy::CustomHook
        && request.selectionRestoreHook) {
        request.selectionRestoreHook();
    }
    return TextEditorSourceTransactionResult::Applied;
}

void TextEditorSourceRewriteController::applySourceSnapshotForTransaction(const QString &contents)
{
    if (context_.editor == nullptr) {
        return;
    }

    replaceTextPreservingCursor(contents, true, true, true, false);
}

void TextEditorSourceRewriteController::applySourceEditsForTransaction(QVector<TherionSourceTextEdit> edits,
                                                                       bool rebuildBlocksCanvas)
{
    if (context_.editor == nullptr || edits.isEmpty()) {
        return;
    }

    applyTextEditsPreservingCursor(std::move(edits), true, rebuildBlocksCanvas, true, false);
}

bool TextEditorSourceRewriteController::replaceTextForSystemNormalization(const QString &contents)
{
    return replaceTextForSystemNormalizationResult(contents) == TextEditorSourceTransactionResult::Applied;
}

TextEditorSourceTransactionResult TextEditorSourceRewriteController::replaceTextForSystemNormalizationResult(
    const QString &contents)
{
    if (context_.editor == nullptr) {
        return TextEditorSourceTransactionResult::Unavailable;
    }

    TextEditorSourceTransactionRequest request;
    request.beforeText = context_.editor->toPlainText();
    request.afterText = contents;
    QTextDocument *document = context_.editor->document();
    request.expectedSourceRevision = document != nullptr ? document->revision() : 0;
    request.projectionInvalidationPolicy = TextEditorSourceProjectionInvalidationPolicy::FlushPendingRefresh;
    request.selectionRestorePolicy = TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection;
    request.staleStatusMessage = QStringLiteral("System normalization skipped: document changed.");
    return applyTransactionRequestWithEditorUndoResult(request);
}

void TextEditorSourceRewriteController::replaceEditorText(const QString &contents, bool recordUndoStep)
{
    if (context_.editor == nullptr) {
        return;
    }

    if (context_.editor->toPlainText() == contents) {
        return;
    }

    if (!recordUndoStep) {
        context_.editor->setPlainText(contents);
        return;
    }

    QTextDocument *document = context_.editor->document();
    if (document == nullptr) {
        context_.editor->setPlainText(contents);
        return;
    }

    QTextCursor rewriteCursor(document);
    rewriteCursor.beginEditBlock();
    rewriteCursor.select(QTextCursor::Document);
    rewriteCursor.insertText(contents);
    rewriteCursor.endEditBlock();
}

void TextEditorSourceRewriteController::replaceTextPreservingCursor(const QString &contents,
                                                                    bool emitDocumentTextChanged,
                                                                    bool rebuildBlocksCanvas,
                                                                    bool applyDirtyState,
                                                                    bool recordUndoStep)
{
    if (context_.editor == nullptr) {
        return;
    }

    const QTextCursor previousCursor = context_.editor->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    if (context_.loading != nullptr) {
        *context_.loading = true;
    }
    replaceEditorText(contents, recordUndoStep);

    const int targetLine = qBound(0, previousLine, qMax(0, context_.editor->document()->blockCount() - 1));
    const QTextBlock targetBlock = context_.editor->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        context_.editor->setTextCursor(restoredCursor);
    }

    if (context_.loading != nullptr) {
        *context_.loading = false;
    }
    if (context_.currentLineNumber != nullptr) {
        *context_.currentLineNumber = context_.editor->textCursor().blockNumber() + 1;
    }
    if (applyDirtyState && context_.applyDirtyStateFromCurrentState) {
        context_.applyDirtyStateFromCurrentState();
    }
    if (rebuildBlocksCanvas && context_.rebuildBlocksCanvasFromText) {
        context_.rebuildBlocksCanvasFromText();
    }
    if (emitDocumentTextChanged && context_.documentTextChanged) {
        context_.documentTextChanged();
    }
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
}

void TextEditorSourceRewriteController::applyTextEditsPreservingCursor(QVector<TherionSourceTextEdit> edits,
                                                                       bool emitDocumentTextChanged,
                                                                       bool rebuildBlocksCanvas,
                                                                       bool applyDirtyState,
                                                                       bool recordUndoStep)
{
    if (context_.editor == nullptr || edits.isEmpty()) {
        return;
    }

    QTextDocument *document = context_.editor->document();
    if (document == nullptr) {
        return;
    }

    const int documentLength = context_.editor->toPlainText().size();
    for (const TherionSourceTextEdit &edit : edits) {
        if (edit.startOffset < 0 || edit.length < 0 || edit.startOffset + edit.length > documentLength) {
            return;
        }
    }

    const QTextCursor previousCursor = context_.editor->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    if (context_.loading != nullptr) {
        *context_.loading = true;
    }

    std::sort(edits.begin(), edits.end(), [](const TherionSourceTextEdit &left, const TherionSourceTextEdit &right) {
        return left.startOffset > right.startOffset;
    });

    const bool undoWasEnabled = document->isUndoRedoEnabled();
    if (!recordUndoStep && undoWasEnabled) {
        document->setUndoRedoEnabled(false);
    }

    QTextCursor editCursor(document);
    if (recordUndoStep) {
        editCursor.beginEditBlock();
    }
    for (const TherionSourceTextEdit &edit : edits) {
        editCursor.setPosition(edit.startOffset);
        editCursor.setPosition(edit.startOffset + edit.length, QTextCursor::KeepAnchor);
        editCursor.insertText(edit.replacementText);
    }
    if (recordUndoStep) {
        editCursor.endEditBlock();
    }
    if (!recordUndoStep && undoWasEnabled) {
        document->setUndoRedoEnabled(true);
    }

    const int targetLine = qBound(0, previousLine, qMax(0, context_.editor->document()->blockCount() - 1));
    const QTextBlock targetBlock = context_.editor->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        context_.editor->setTextCursor(restoredCursor);
    }

    if (context_.loading != nullptr) {
        *context_.loading = false;
    }
    if (context_.currentLineNumber != nullptr) {
        *context_.currentLineNumber = context_.editor->textCursor().blockNumber() + 1;
    }
    if (applyDirtyState && context_.applyDirtyStateFromCurrentState) {
        context_.applyDirtyStateFromCurrentState();
    }
    if (rebuildBlocksCanvas && context_.rebuildBlocksCanvasFromText) {
        context_.rebuildBlocksCanvasFromText();
    }
    if (emitDocumentTextChanged && context_.documentTextChanged) {
        context_.documentTextChanged();
    }
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
}

void TextEditorSourceRewriteController::replaceTextSelectingLine(const QString &contents,
                                                                 int lineNumber,
                                                                 bool recordUndoStep)
{
    if (context_.editor == nullptr) {
        return;
    }

    if (context_.loading != nullptr) {
        *context_.loading = true;
    }
    replaceEditorText(contents, recordUndoStep);

    if (lineNumber > 0) {
        const QTextBlock targetBlock = context_.editor->document()->findBlockByLineNumber(lineNumber - 1);
        if (targetBlock.isValid()) {
            QTextCursor cursor(targetBlock);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            context_.editor->setTextCursor(cursor);
            context_.editor->centerCursor();
        }
    }

    if (context_.loading != nullptr) {
        *context_.loading = false;
    }
    if (context_.currentLineNumber != nullptr) {
        *context_.currentLineNumber = context_.editor->textCursor().blockNumber() + 1;
    }
    if (context_.applyDirtyStateFromCurrentState) {
        context_.applyDirtyStateFromCurrentState();
    }
    if (context_.documentTextChanged) {
        context_.documentTextChanged();
    }
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
}
}
