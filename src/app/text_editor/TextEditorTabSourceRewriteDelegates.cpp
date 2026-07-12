#include "TextEditorTab.h"

#include "TextEditorSourceRewriteController.h"
#include "../../core/TherionSourceFormatter.h"

#include <QPlainTextEdit>
#include <QTextDocument>

#include <utility>

namespace TherionStudio
{
void TextEditorTab::applySourceSnapshotForTransaction(const QString &contents)
{
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->applySourceSnapshotForTransaction(contents);
    }
}

void TextEditorTab::applySourceEditsForTransaction(QVector<TherionSourceTextEdit> edits, bool rebuildBlocksCanvas)
{
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->applySourceEditsForTransaction(std::move(edits), rebuildBlocksCanvas);
    }
}

bool TextEditorTab::replaceTextForSystemNormalization(const QString &contents)
{
    if (sourceRewriteController_ == nullptr) {
        return false;
    }

    return sourceRewriteController_->replaceTextForSystemNormalization(contents);
}

bool TextEditorTab::formatDocument()
{
    if (editor_ == nullptr || sourceRewriteController_ == nullptr || editor_->document() == nullptr) {
        return false;
    }

    const QString beforeText = editor_->toPlainText();
    TextEditorSourceTransactionRequest request;
    request.label = tr("Format Document");
    request.beforeText = beforeText;
    request.afterText = TherionSourceFormatter::formatIndentation(beforeText);
    request.expectedSourceRevision = editor_->document()->revision();
    request.projectionInvalidationPolicy = TextEditorSourceProjectionInvalidationPolicy::FlushPendingRefresh;
    request.selectionRestorePolicy = TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection;
    request.staleStatusMessage = tr("Formatting skipped: document changed.");

    return sourceRewriteController_->applyTransactionRequestWithEditorUndoResult(request)
        == TextEditorSourceTransactionResult::Applied;
}
}
