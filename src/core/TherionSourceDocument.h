#pragma once

#include "TherionDocumentParser.h"
#include "TherionSourceText.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace TherionStudio
{
enum class TherionSourceDocumentType
{
    Unknown,
    TherionSource,
    TherionMap,
    TherionConfig,
};

struct TherionSourceDocumentMetadata
{
    TherionSourceDocumentType sourceType = TherionSourceDocumentType::Unknown;
    QString encodingName;
    int revisionId = 0;
};

enum class TherionSourceLineRole
{
    Empty,
    Comment,
    Command,
    BlockContent,
};

struct TherionSourceBlockFrame
{
    QString directive;
    int lineNumber = 0;
    QString lineText;
};

struct TherionSourceBlockRange
{
    QString directive;
    int openLineNumber = 0;
    int closeLineNumber = 0;
    int startOffset = 0;
    int endOffset = 0;
    QString openLineText;
    QString closeLineText;
    QVector<TherionSourceBlockFrame> parentStack;

    [[nodiscard]] bool isClosed() const;
};

struct TherionSourceDocumentLine
{
    TherionParsedSourceLine sourceLine;
    QString normalizedDirective;
    QString currentBlockDirective;
    TherionSourceLineRole role = TherionSourceLineRole::Empty;
    bool opensBlock = false;
    bool closesBlock = false;
    QString closeMatchesOpenDirective;
    QVector<TherionSourceBlockFrame> blockStackBefore;

    [[nodiscard]] bool shouldValidateCommandCatalog() const;
    [[nodiscard]] bool hasUnmatchedClose() const;
};

class TherionSourceDocument final
{
public:
    [[nodiscard]] static TherionSourceDocument fromText(
        const QString &contents,
        const TherionSourceDocumentMetadata &metadata = {});

    [[nodiscard]] const TherionSourceDocumentMetadata &metadata() const;
    [[nodiscard]] TherionSourceDocumentType sourceType() const;
    [[nodiscard]] QString encodingName() const;
    [[nodiscard]] int revisionId() const;
    [[nodiscard]] const TherionSourceText &sourceText() const;
    [[nodiscard]] const TherionParsedSourceDocument &parsedDocument() const;
    [[nodiscard]] const QVector<TherionSourceDocumentLine> &lines() const;
    [[nodiscard]] const QVector<TherionSourceBlockRange> &blockRanges() const;
    [[nodiscard]] const QVector<TherionSourceBlockFrame> &openBlocksAtEnd() const;
    [[nodiscard]] QString toText() const;
    [[nodiscard]] QVector<TherionParsedLine> tokenLines() const;

private:
    TherionSourceDocumentMetadata metadata_;
    TherionSourceText sourceText_;
    TherionParsedSourceDocument parsedDocument_;
    QVector<TherionSourceDocumentLine> lines_;
    QVector<TherionSourceBlockRange> blockRanges_;
    QVector<TherionSourceBlockFrame> openBlocksAtEnd_;
};

[[nodiscard]] QString normalizedTherionDirectiveToken(const QString &token);
[[nodiscard]] QHash<QString, QString> therionOpenToCloseDirectiveMap();
[[nodiscard]] QHash<QString, QString> therionCloseToOpenDirectiveMap();
[[nodiscard]] bool therionDirectiveIsKnownBlockDirective(const QString &directive);
[[nodiscard]] bool therionBlockTreatsChildrenAsContent(const QString &directive);
}
