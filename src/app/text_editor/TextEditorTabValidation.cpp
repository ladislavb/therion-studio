#include "TextEditorTab.h"

#include "TextEditorContextHelpController.h"
#include "TextEditorSourceRewriteController.h"
#include "TextEditorValidationCatalog.h"
#include "../../editor/TherionSyntaxHighlighter.h"
#include "../../core/TherionDocumentEditor.h"
#include "../../core/TherionFileTypes.h"
#include "../../core/TherionSourceValidator.h"

#include <QPlainTextEdit>

#include <utility>

namespace TherionStudio
{
TherionSourceValidationResult TextEditorTab::validateDocument() const
{
    if (editor_ == nullptr) {
        return {};
    }

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = therionSourceDocumentTypeForFilePath(filePath_);
    metadata.revisionId = documentRevision();

    const QString text = editor_->toPlainText();
    const TherionSourceValidationCatalog catalog = validationCatalogFromCommandMetadata(commandMetadata_);
    const TherionSourceDocument &sourceDocument =
        validationSourceSnapshotCache_.sourceDocument(text, metadata);
    const TherionSourceLogicalDocument &logicalDocument =
        validationSourceSnapshotCache_.logicalDocument(text,
                                                       catalog,
                                                       metadata,
                                                       TherionSourceSnapshotCatalogKey::fromRevision(metadata.revisionId));
    TherionSourceValidationResult result =
        TherionSourceValidator::validate(sourceDocument, logicalDocument, catalog);
    if (!isTherionConfigFilePath(filePath_)) {
        return result;
    }

    QVector<TherionSourceDiagnostic> filteredDiagnostics;
    filteredDiagnostics.reserve(result.diagnostics.size());
    for (const TherionSourceDiagnostic &diagnostic : std::as_const(result.diagnostics)) {
        if (diagnostic.code != QStringLiteral("unknown-command")) {
            filteredDiagnostics.append(diagnostic);
        }
    }
    result.diagnostics = filteredDiagnostics;
    return result;
}

void TextEditorTab::setProjectValidationDiagnostics(const QVector<TherionSourceDiagnostic> &diagnostics)
{
    projectValidationDiagnostics_ = diagnostics;
    if (highlighter_ != nullptr) {
        highlighter_->setExternalDiagnostics(projectValidationDiagnostics_);
    }
    if (contextHelpController_ != nullptr) {
        contextHelpController_->invalidateValidationCache();
    }
}

void TextEditorTab::setSyntaxValidationDeferred(bool deferred)
{
    if (highlighter_ != nullptr) {
        highlighter_->setValidationFormatsDeferred(deferred);
    }
}

bool TextEditorTab::applyValidationFixes(const QVector<TherionSourceDiagnosticFix> &fixes)
{
    if (editor_ == nullptr || sourceRewriteController_ == nullptr || fixes.isEmpty()) {
        return false;
    }

    const QString beforeText = editor_->toPlainText();
    const int expectedRevision = documentRevision();
    const QVector<TherionSourceTextEdit> edits = TherionSourceValidator::validationFixEdits(beforeText, fixes);
    if (edits.isEmpty()) {
        return false;
    }

    QString afterText = beforeText;
    if (!TherionDocumentEditor::applySourceTextEdits(&afterText, edits)) {
        return false;
    }
    if (afterText == beforeText) {
        return false;
    }

    TextEditorSourceTransactionRequest request;
    request.label = tr("Apply Validation Fixes");
    request.beforeText = beforeText;
    request.sourceEdits = edits;
    request.expectedSourceRevision = expectedRevision;
    request.staleStatusMessage = tr("Validation fixes skipped: document changed.");
    return sourceRewriteController_->applyTransactionRequestWithEditorUndo(request);
}
}
