#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QSet>
#include <QHash>
#include <QJsonObject>
#include <QFutureWatcher>

#include "../core/CommandCatalogStore.h"
#include "../core/TherionSourceDocument.h"
#include "../core/TherionSourceSnapshotCache.h"
#include "../core/TherionSourceValidator.h"

class QTextDocument;

namespace TherionStudio
{
class TherionSyntaxHighlighter final : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit TherionSyntaxHighlighter(CommandCatalogStore catalogStore, QTextDocument *parent = nullptr);
    void reloadPaletteForApplicationAppearance();
    void setSourceDocumentType(TherionSourceDocumentType sourceType);
    void setExternalDiagnostics(const QVector<TherionSourceDiagnostic> &diagnostics);
    // QSyntaxHighlighter re-formatting emits QTextDocument::contentsChanged even though the
    // text is unchanged; listeners use this to tell such notifications from real edits.
    [[nodiscard]] bool applyingFormatsOnly() const { return formatOnlyDepth_ > 0; }

protected:
    void highlightBlock(const QString &text) override;

private:
    void loadPalette();
    void applyStyle(const QString &styleName, const QJsonObject &styleObject);
    void loadCommandCatalogKeywords();
    void applyValidatorInvalidTokenFormats(const QString &text);
    const TherionSourceValidationResult &cachedValidationResult();
    void rehighlightFormatsOnly();
    void startAsyncValidation(int revision);
    void handleAsyncValidationFinished();
    const QTextCharFormat &validationFormatForSeverity(TherionSourceDiagnosticSeverity severity) const;

    QTextCharFormat baseTextFormat_;
    QTextCharFormat keywordFormat_;
    QTextCharFormat optionFormat_;
    QTextCharFormat identifierFormat_;
    QTextCharFormat validationWarningFormat_;
    QTextCharFormat validationErrorFormat_;
    QTextCharFormat stringFormat_;
    QTextCharFormat numberFormat_;
    QTextCharFormat commentFormat_;
    QSet<QString> keywordTokens_;
    QSet<QString> commandTokens_;
    QHash<QString, QSet<QString>> commandOptionTokens_;
    QHash<QString, QSet<QString>> commandAllowedValues_;
    QHash<QString, QHash<QString, QString>> commandOptionValueArity_;
    QHash<QString, QSet<int>> commandPositionalIdTokenIndexes_;
    QHash<QString, QSet<QString>> commandOptionIdValueTokens_;
    QSet<QString> closingDirectiveIdTokens_;
    TherionSourceValidationCatalog validationCatalog_;
    TherionSourceSnapshotCache sourceSnapshotCache_;
    TherionSourceValidationResult cachedValidationResult_;
    QVector<TherionSourceDiagnostic> externalDiagnostics_;
    int cachedValidationRevision_ = -1;
    QFutureWatcher<TherionSourceValidationResult> asyncValidationWatcher_;
    int asyncValidationRevision_ = -1;
    int asyncValidationGeneration_ = 0;
    int validationGeneration_ = 0;
    bool applyingAsyncValidation_ = false;
    int formatOnlyDepth_ = 0;
    TherionSourceDocumentType sourceDocumentType_ = TherionSourceDocumentType::Unknown;
    CommandCatalogStore catalogStore_;
};
}
