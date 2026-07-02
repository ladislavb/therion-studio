#include "TextEditorSourceTransactionController.h"

#include "TextEditorTab.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QPointer>
#include <QScopedValueRollback>
#include <QUndoCommand>
#include <QUndoStack>

#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
bool diagnosticSourceTransactionLoggingEnabled()
{
    static const bool enabled = [] {
        const QString value = QString::fromLocal8Bit(qgetenv("THERION_STUDIO_ENABLE_LOG")).trimmed().toLower();
        return value == QStringLiteral("1")
            || value == QStringLiteral("true")
            || value == QStringLiteral("yes")
            || value == QStringLiteral("on");
    }();
    return enabled;
}

QString undoStackDiagnosticSummary(const QUndoStack *undoStack)
{
    if (undoStack == nullptr) {
        return QStringLiteral("undo_count=-1 undo_index=-1 undo_limit=-1");
    }

    return QStringLiteral("undo_count=%1 undo_index=%2 undo_limit=%3")
        .arg(undoStack->count())
        .arg(undoStack->index())
        .arg(undoStack->undoLimit());
}

std::optional<QString> resolvedAfterText(const TextEditorSourceTransactionRequest &request)
{
    if (request.sourceEdits.isEmpty()) {
        return request.afterText;
    }

    QString afterText = request.beforeText;
    if (!TherionDocumentEditor::applySourceTextEdits(&afterText, request.sourceEdits)) {
        return std::nullopt;
    }
    return afterText;
}

QString staleSourceTransactionMessage()
{
    return QCoreApplication::translate("TherionStudio::TextEditorSourceTransactionController",
                                       "Source transaction skipped: document changed.");
}

bool sourceRevisionMatches(const TextEditorSourceTransactionContext &context,
                          const TextEditorSourceTransactionRequest &request)
{
    if (context.textEditor == nullptr || request.expectedSourceRevision <= 0) {
        return true;
    }
    return context.textEditor->documentRevision() == request.expectedSourceRevision;
}

bool currentTextMatches(const TextEditorSourceTransactionContext &context, const QString &expectedText)
{
    return context.textEditor != nullptr && context.textEditor->text() == expectedText;
}

struct PushSnapshotCommandTiming
{
    qint64 createUndoMs = 0;
    qint64 undoPushMs = 0;
    qint64 undoGuardMs = 0;

    qint64 totalMs() const
    {
        return createUndoMs + undoPushMs + undoGuardMs;
    }
};

void reportStaleRequest(const TextEditorSourceTransactionContext &context,
                        const TextEditorSourceTransactionRequest &request)
{
    if (context.statusCallback == nullptr) {
        return;
    }

    context.statusCallback(request.staleStatusMessage.isEmpty()
                               ? staleSourceTransactionMessage()
                               : request.staleStatusMessage);
}

class TextEditorSourceSnapshotCommand final : public QUndoCommand
{
public:
    TextEditorSourceSnapshotCommand(TextEditorTab *textEditor,
                                    QString label,
                                    QString beforeText,
                                    QString afterText,
                                    QString undoStatusMessage,
                                    QString redoStatusMessage,
                                    std::function<void()> initialRedoHook,
                                    std::function<void()> undoHook,
                                    std::function<void()> redoHook,
                                    std::function<void(const QString &)> statusCallback)
        : textEditor_(textEditor)
        , beforeText_(std::move(beforeText))
        , afterText_(std::move(afterText))
        , undoStatusMessage_(std::move(undoStatusMessage))
        , redoStatusMessage_(std::move(redoStatusMessage))
        , initialRedoHook_(std::move(initialRedoHook))
        , undoHook_(std::move(undoHook))
        , redoHook_(std::move(redoHook))
        , statusCallback_(std::move(statusCallback))
    {
        setText(std::move(label));
    }

    void undo() override
    {
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        applyTextEditorSourceSnapshot(textEditor_, beforeText_);
        if (undoHook_) {
            undoHook_();
        }
        if (statusCallback_ != nullptr && !undoStatusMessage_.isEmpty()) {
            statusCallback_(undoStatusMessage_);
        }
    }

    void redo() override
    {
        if (firstRedo_) {
            firstRedo_ = false;
            if (initialRedoHook_) {
                initialRedoHook_();
            }
            return;
        }
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        applyTextEditorSourceSnapshot(textEditor_, afterText_);
        if (redoHook_) {
            redoHook_();
        }
        if (statusCallback_ != nullptr && !redoStatusMessage_.isEmpty()) {
            statusCallback_(redoStatusMessage_);
        }
    }

private:
    QPointer<TextEditorTab> textEditor_;
    QString beforeText_;
    QString afterText_;
    QString undoStatusMessage_;
    QString redoStatusMessage_;
    std::function<void()> initialRedoHook_;
    std::function<void()> undoHook_;
    std::function<void()> redoHook_;
    std::function<void(const QString &)> statusCallback_;
    bool firstRedo_ = true;
};

PushSnapshotCommandTiming pushSnapshotCommand(const TextEditorSourceTransactionContext &context,
                                              const TextEditorSourceTransactionRequest &request,
                                              const QString &afterText)
{
    PushSnapshotCommandTiming timing;
    if (context.textEditor == nullptr || context.undoStack == nullptr || request.beforeText == afterText) {
        return timing;
    }

    QElapsedTimer stageTimer;
    stageTimer.start();
    auto *command = new TextEditorSourceSnapshotCommand(context.textEditor,
                                                       request.label,
                                                       request.beforeText,
                                                       afterText,
                                                       request.undoStatusMessage,
                                                       request.redoStatusMessage,
                                                       request.initialRedoHook,
                                                       request.undoHook,
                                                       request.redoHook,
                                                       context.statusCallback);
    timing.createUndoMs = stageTimer.restart();

    const auto pushCommand = [&context, command, &timing, &stageTimer]() {
        context.undoStack->push(command);
        timing.undoPushMs = stageTimer.restart();
    };

    if (context.commandApplyInProgress != nullptr) {
        QElapsedTimer guardTimer;
        guardTimer.start();
        stageTimer.restart();
        {
            const QScopedValueRollback<bool> commandGuard((*context.commandApplyInProgress), true);
            pushCommand();
        }
        const qint64 guardElapsedMs = guardTimer.elapsed();
        timing.undoGuardMs = guardElapsedMs > timing.undoPushMs ? guardElapsedMs - timing.undoPushMs : 0;
    } else {
        stageTimer.restart();
        pushCommand();
    }
    return timing;
}

void applyRequestPolicies(const TextEditorSourceTransactionContext &context,
                          const TextEditorSourceTransactionRequest &request)
{
    switch (request.projectionInvalidationPolicy) {
    case TextEditorSourceProjectionInvalidationPolicy::FlushPendingRefresh:
        if (context.flushPendingRefresh != nullptr) {
            context.flushPendingRefresh();
        }
        break;
    case TextEditorSourceProjectionInvalidationPolicy::CustomHook:
        if (request.projectionInvalidationHook) {
            request.projectionInvalidationHook();
        }
        break;
    case TextEditorSourceProjectionInvalidationPolicy::None:
        break;
    }

    switch (request.selectionRestorePolicy) {
    case TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection:
        break;
    case TextEditorSourceSelectionRestorePolicy::CustomHook:
        if (request.selectionRestoreHook) {
            request.selectionRestoreHook();
        }
        break;
    }
}
}

TextEditorSourceTransactionController::TextEditorSourceTransactionController(TextEditorSourceTransactionContext context)
    : context_(std::move(context))
{
}

void applyTextEditorSourceSnapshot(TextEditorTab *textEditor, const QString &contents)
{
    if (textEditor == nullptr) {
        return;
    }

    textEditor->applySourceSnapshotForTransaction(contents);
}

void applyTextEditorSourceChange(TextEditorTab *textEditor,
                                 const QString &contents,
                                 const QVector<TherionSourceTextEdit> &sourceEdits,
                                 bool rebuildBlocksCanvas)
{
    if (textEditor == nullptr) {
        return;
    }

    if (sourceEdits.isEmpty()) {
        textEditor->applySourceSnapshotForTransaction(contents);
        return;
    }

    textEditor->applySourceEditsForTransaction(sourceEdits, rebuildBlocksCanvas);
}

TextEditorSourceTransactionResult TextEditorSourceTransactionController::recordSnapshot(
    const TextEditorSourceTransactionRequest &request)
{
    const std::optional<QString> afterText = resolvedAfterText(request);
    if (!afterText.has_value()) {
        return TextEditorSourceTransactionResult::InvalidEdit;
    }
    if (request.beforeText == afterText.value()) {
        return TextEditorSourceTransactionResult::NoChange;
    }
    if (context_.textEditor == nullptr) {
        return TextEditorSourceTransactionResult::Unavailable;
    }

    if (!sourceRevisionMatches(context_, request) || !currentTextMatches(context_, afterText.value())) {
        reportStaleRequest(context_, request);
        return TextEditorSourceTransactionResult::Stale;
    }

    pushSnapshotCommand(context_, request, afterText.value());
    applyRequestPolicies(context_, request);
    return TextEditorSourceTransactionResult::Applied;
}

TextEditorSourceTransactionResult TextEditorSourceTransactionController::applyChangeWithSnapshot(
    const TextEditorSourceTransactionRequest &request)
{
    const bool logTiming = diagnosticSourceTransactionLoggingEnabled();
    QElapsedTimer totalTimer;
    QElapsedTimer stageTimer;
    if (logTiming) {
        totalTimer.start();
        stageTimer.start();
    }

    const std::optional<QString> afterText = resolvedAfterText(request);
    const qint64 resolveMs = logTiming ? stageTimer.elapsed() : 0;
    if (!afterText.has_value()) {
        if (logTiming) {
            qInfo().noquote()
                << QStringLiteral("source-transaction label=\"%1\" result=invalid-edit resolve_ms=%2 total_ms=%3")
                       .arg(request.label)
                       .arg(resolveMs)
                       .arg(totalTimer.elapsed());
        }
        return TextEditorSourceTransactionResult::InvalidEdit;
    }
    if (request.beforeText == afterText.value()) {
        if (logTiming) {
            qInfo().noquote()
                << QStringLiteral("source-transaction label=\"%1\" result=no-change resolve_ms=%2 total_ms=%3")
                       .arg(request.label)
                       .arg(resolveMs)
                       .arg(totalTimer.elapsed());
        }
        return TextEditorSourceTransactionResult::NoChange;
    }
    if (context_.textEditor == nullptr) {
        if (logTiming) {
            qInfo().noquote()
                << QStringLiteral("source-transaction label=\"%1\" result=unavailable resolve_ms=%2 total_ms=%3")
                       .arg(request.label)
                       .arg(resolveMs)
                       .arg(totalTimer.elapsed());
        }
        return TextEditorSourceTransactionResult::Unavailable;
    }

    if (!sourceRevisionMatches(context_, request) || !currentTextMatches(context_, request.beforeText)) {
        reportStaleRequest(context_, request);
        if (logTiming) {
            qInfo().noquote()
                << QStringLiteral("source-transaction label=\"%1\" result=stale resolve_ms=%2 total_ms=%3")
                       .arg(request.label)
                       .arg(resolveMs)
                       .arg(totalTimer.elapsed());
        }
        return TextEditorSourceTransactionResult::Stale;
    }

    auto applySnapshot = [&]() -> qint64 {
        stageTimer.restart();
        applyTextEditorSourceChange(context_.textEditor,
                                    afterText.value(),
                                    request.sourceEdits,
                                    request.rebuildBlocksCanvasOnApply);
        return stageTimer.elapsed();
    };
    auto markOrigin = [&]() -> qint64 {
        stageTimer.restart();
        if (context_.markSourceChangeOriginatedFromTransaction) {
            context_.markSourceChangeOriginatedFromTransaction();
        }
        return stageTimer.elapsed();
    };
    PushSnapshotCommandTiming pushUndoTiming;
    auto pushUndo = [&]() {
        pushUndoTiming = pushSnapshotCommand(context_, request, afterText.value());
        return pushUndoTiming.totalMs();
    };
    auto applyPolicies = [&]() -> qint64 {
        stageTimer.restart();
        applyRequestPolicies(context_, request);
        return stageTimer.elapsed();
    };

    if (context_.commandApplyInProgress != nullptr) {
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        const qint64 applySnapshotMs = applySnapshot();
        const qint64 markMs = markOrigin();
        const qint64 pushUndoMs = pushUndo();
        const qint64 policiesMs = applyPolicies();
        if (logTiming) {
            qInfo().noquote()
                << QStringLiteral(
                       "source-transaction label=\"%1\" result=applied guarded=1 before_chars=%2 after_chars=%3 resolve_ms=%4 "
                       "apply_snapshot_ms=%5 mark_ms=%6 push_undo_ms=%7 create_undo_ms=%8 undo_push_ms=%9 "
                       "undo_guard_ms=%10 policies_ms=%11 total_ms=%12 snapshot_chars=%13 %14")
                       .arg(request.label)
                       .arg(request.beforeText.size())
                       .arg(afterText->size())
                       .arg(resolveMs)
                       .arg(applySnapshotMs)
                       .arg(markMs)
                       .arg(pushUndoMs)
                       .arg(pushUndoTiming.createUndoMs)
                       .arg(pushUndoTiming.undoPushMs)
                       .arg(pushUndoTiming.undoGuardMs)
                       .arg(policiesMs)
                       .arg(totalTimer.elapsed())
                       .arg(request.beforeText.size() + afterText->size())
                       .arg(undoStackDiagnosticSummary(context_.undoStack));
        }
        return TextEditorSourceTransactionResult::Applied;
    }

    const qint64 applySnapshotMs = applySnapshot();
    const qint64 markMs = markOrigin();
    const qint64 pushUndoMs = pushUndo();
    const qint64 policiesMs = applyPolicies();
    if (logTiming) {
        qInfo().noquote()
            << QStringLiteral(
                   "source-transaction label=\"%1\" result=applied guarded=0 before_chars=%2 after_chars=%3 resolve_ms=%4 "
                   "apply_snapshot_ms=%5 mark_ms=%6 push_undo_ms=%7 create_undo_ms=%8 undo_push_ms=%9 "
                   "undo_guard_ms=%10 policies_ms=%11 total_ms=%12 snapshot_chars=%13 %14")
                   .arg(request.label)
                   .arg(request.beforeText.size())
                   .arg(afterText->size())
                   .arg(resolveMs)
                   .arg(applySnapshotMs)
                   .arg(markMs)
                   .arg(pushUndoMs)
                   .arg(pushUndoTiming.createUndoMs)
                   .arg(pushUndoTiming.undoPushMs)
                   .arg(pushUndoTiming.undoGuardMs)
                   .arg(policiesMs)
                   .arg(totalTimer.elapsed())
                   .arg(request.beforeText.size() + afterText->size())
                   .arg(undoStackDiagnosticSummary(context_.undoStack));
    }
    return TextEditorSourceTransactionResult::Applied;
}
}
