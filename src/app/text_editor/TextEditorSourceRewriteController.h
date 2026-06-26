#pragma once

#include <functional>

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "../../core/TherionDocumentEditor.h"
#include "TextEditorSourceTransactionController.h"

class QPlainTextEdit;

namespace TherionStudio
{
struct TextEditorSourceRewriteContext
{
    QPlainTextEdit *editor = nullptr;
    bool *loading = nullptr;
    int *currentLineNumber = nullptr;
    std::function<void()> applyDirtyStateFromCurrentState;
    std::function<void()> rebuildBlocksCanvasFromText;
    std::function<void()> documentTextChanged;
    std::function<void()> updateContextHelp;
};

class TextEditorSourceRewriteController final
{
public:
    explicit TextEditorSourceRewriteController(TextEditorSourceRewriteContext context);

    TextEditorSourceTransactionResult applyTransactionRequestWithEditorUndoResult(
        const TextEditorSourceTransactionRequest &request,
        QString *statusMessage = nullptr);
    bool applyTransactionRequestWithEditorUndo(const TextEditorSourceTransactionRequest &request,
                                               QString *statusMessage = nullptr);
    void applySourceSnapshotForTransaction(const QString &contents);
    void applySourceEditsForTransaction(QVector<TherionSourceTextEdit> edits, bool rebuildBlocksCanvas = true);
    TextEditorSourceTransactionResult replaceTextForSystemNormalizationResult(const QString &contents);
    bool replaceTextForSystemNormalization(const QString &contents);

private:
    void replaceEditorText(const QString &contents, bool recordUndoStep);
    void replaceTextPreservingCursor(const QString &contents,
                                     bool emitDocumentTextChanged,
                                     bool rebuildBlocksCanvas,
                                     bool applyDirtyState,
                                     bool recordUndoStep);
    void applyTextEditsPreservingCursor(QVector<TherionSourceTextEdit> edits,
                                        bool emitDocumentTextChanged,
                                        bool rebuildBlocksCanvas,
                                        bool applyDirtyState,
                                        bool recordUndoStep);
    void replaceTextSelectingLine(const QString &contents, int lineNumber, bool recordUndoStep);

    TextEditorSourceRewriteContext context_;
};
}
