#pragma once

#include "TherionSourceDocument.h"
#include "TherionSourceValidationCatalog.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
struct TherionSourceLogicalTextPart
{
    int logicalStart = 0;
    int logicalLength = 0;
    int lineNumber = 0;
    int columnNumber = 1;
    int startOffset = 0;
    QString lineText;
};

struct TherionSourcePhysicalRange
{
    int lineNumber = 0;
    int columnNumber = 0;
    int columnLength = 0;
    int startOffset = 0;
    int length = 0;
    QString lineText;
};

struct TherionSourceLogicalTokenRange
{
    int tokenIndex = -1;
    QString text;
    TherionTokenType type = TherionTokenType::Other;
    int logicalStart = 0;
    int logicalLength = 0;
    TherionSourcePhysicalRange physicalRange;
};

struct TherionSourceLogicalArgumentRange
{
    int tokenIndex = -1;
    QString text;
    TherionSourcePhysicalRange physicalRange;
};

struct TherionSourceLogicalArgumentGroupRange
{
    int firstTokenIndex = -1;
    int lastTokenIndex = -1;
    QString text;
    QVector<TherionSourceLogicalArgumentRange> argumentRanges;

    [[nodiscard]] bool isValid() const;
};

struct TherionSourceLogicalOptionEntryRange
{
    QString key;
    int optionTokenIndex = -1;
    int firstValueTokenIndex = -1;
    int lastValueTokenIndex = -1;
    int nextTokenIndex = -1;
    TherionSourcePhysicalRange optionRange;
    QStringList rawValueTokens;
    int logicalValueCount = 0;
    QVector<TherionSourceLogicalArgumentRange> valueRanges;
    TherionSourceLogicalArgumentGroupRange valueGroupRange;
    bool embeddedValue = false;
};

struct TherionSourceLogicalCommandMetadata
{
    QString commandName;
    int positionalArgumentCount = 0;
    QSet<QString> normalizedOptionNames;
    QHash<QString, QVector<int>> optionEntryIndexesByNormalizedName;
    bool catalogCommandKnown = false;
    QStringList catalogContexts;
    QString catalogCurrentContext;
    bool catalogContextAllowed = false;
    QSet<QString> catalogDocumentTypes;
    QString catalogCurrentDocumentType;
    bool catalogDocumentTypeAllowed = false;
    int catalogRequiredPositionalCount = 0;
    int catalogMaxPositionalCount = -1;
    QHash<int, QStringList> catalogArgumentAllowedValuesByIndex;
    QSet<QString> catalogOptionNames;
    QHash<QString, QString> catalogOptionValueArityTokens;
    QHash<QString, int> catalogOptionFixedArityByName;
    QHash<QString, QStringList> catalogOptionAllowedValuesByName;
};

struct TherionSourceLogicalCommand
{
    int startLineNumber = 0;
    int endLineNumber = 0;
    int startOffset = 0;
    int endOffset = 0;
    QString text;
    TherionParsedLine parsed;
    QString normalizedDirective;
    QString currentBlockDirective;
    TherionSourceLineRole role = TherionSourceLineRole::Empty;
    bool opensBlock = false;
    bool closesBlock = false;
    QString closeMatchesOpenDirective;
    QVector<TherionSourceBlockFrame> blockStackBefore;
    QVector<int> physicalLineNumbers;
    QVector<TherionSourceLogicalTextPart> textParts;
    QVector<TherionSourceLogicalTokenRange> tokenRanges;
    QVector<TherionSourceLogicalArgumentRange> positionalArgumentRanges;
    TherionSourceLogicalArgumentGroupRange positionalArgumentGroupRange;
    QVector<TherionSourceLogicalOptionEntryRange> optionEntryRanges;
    TherionSourceLogicalCommandMetadata metadata;

    [[nodiscard]] bool shouldValidateCommandCatalog() const;
    [[nodiscard]] bool hasUnmatchedClose() const;
    [[nodiscard]] bool physicalRangeForLogicalRange(int logicalStart,
                                                    int logicalLength,
                                                    TherionSourcePhysicalRange *range) const;
    [[nodiscard]] bool physicalRangeForTokenIndex(int tokenIndex, TherionSourcePhysicalRange *range) const;
};

class TherionSourceLogicalDocument final
{
public:
    [[nodiscard]] static TherionSourceLogicalDocument fromText(
        const QString &contents,
        const TherionSourceDocumentMetadata &metadata = {});
    [[nodiscard]] static TherionSourceLogicalDocument fromText(
        const QString &contents,
        const TherionSourceValidationCatalog &catalog,
        const TherionSourceDocumentMetadata &metadata = {});
    [[nodiscard]] static TherionSourceLogicalDocument fromSourceDocument(const TherionSourceDocument &sourceDocument);
    [[nodiscard]] static TherionSourceLogicalDocument fromSourceDocument(
        const TherionSourceDocument &sourceDocument,
        const TherionSourceValidationCatalog &catalog);

    [[nodiscard]] const TherionSourceDocumentMetadata &metadata() const;
    [[nodiscard]] const QVector<TherionSourceLogicalCommand> &commands() const;
    [[nodiscard]] const TherionSourceLogicalCommand *commandAtPhysicalLine(int lineNumber) const;
    [[nodiscard]] const TherionSourceLogicalCommand *commandAtOffset(int offset) const;
    [[nodiscard]] const TherionSourceLogicalTokenRange *tokenAtPhysicalPosition(int lineNumber,
                                                                               int columnNumber) const;
    [[nodiscard]] const TherionSourceLogicalTokenRange *tokenAtOffset(int offset) const;

private:
    [[nodiscard]] static TherionSourceLogicalDocument fromSourceDocument(
        const TherionSourceDocument &sourceDocument,
        const TherionSourceValidationCatalog *catalog);

    TherionSourceDocumentMetadata metadata_;
    QVector<TherionSourceLogicalCommand> commands_;
};
}
