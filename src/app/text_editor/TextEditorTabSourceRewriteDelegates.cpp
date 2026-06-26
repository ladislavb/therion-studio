#include "TextEditorTab.h"

#include "TextEditorSourceRewriteController.h"

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
}
