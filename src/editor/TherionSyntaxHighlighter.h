#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QSet>
#include <QHash>
#include <QJsonObject>

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
    // While the text is not shown (for example a map drawn in the Visual workspace), skip validator and external
    // diagnostic formats, which revalidate the whole document on each edit; they are applied once when undeferred.
    void setValidationFormatsDeferred(bool deferred);

protected:
    void highlightBlock(const QString &text) override;

private:
    void loadPalette();
    void applyStyle(const QString &styleName, const QJsonObject &styleObject);
    void loadCommandCatalogKeywords();
    void applyValidatorInvalidTokenFormats(const QString &text);
    const TherionSourceValidationResult &cachedValidationResult();
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
    bool validationFormatsDeferred_ = false;
    bool validationFormatsPending_ = false;
    TherionSourceDocumentType sourceDocumentType_ = TherionSourceDocumentType::Unknown;
    CommandCatalogStore catalogStore_;
};
}
