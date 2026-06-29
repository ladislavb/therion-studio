#include "TherionSourceDocument.h"

namespace TherionStudio
{
namespace
{
struct BlockPair
{
    QString openDirective;
    QString closeDirective;
};

struct ActiveBlockFrame
{
    int rangeIndex = -1;
};

QVector<BlockPair> knownBlockPairs()
{
    return {
        {QStringLiteral("scrap"), QStringLiteral("endscrap")},
        {QStringLiteral("line"), QStringLiteral("endline")},
        {QStringLiteral("area"), QStringLiteral("endarea")},
        {QStringLiteral("map"), QStringLiteral("endmap")},
        {QStringLiteral("survey"), QStringLiteral("endsurvey")},
        {QStringLiteral("centerline"), QStringLiteral("endcenterline")},
        {QStringLiteral("centreline"), QStringLiteral("endcentreline")},
        {QStringLiteral("layout"), QStringLiteral("endlayout")},
        {QStringLiteral("code"), QStringLiteral("endcode")},
        {QStringLiteral("group"), QStringLiteral("endgroup")},
    };
}
}

bool TherionSourceBlockRange::isClosed() const
{
    return closeLineNumber > 0;
}

bool TherionSourceDocumentLine::shouldValidateCommandCatalog() const
{
    return role == TherionSourceLineRole::Command;
}

bool TherionSourceDocumentLine::hasUnmatchedClose() const
{
    if (!closesBlock) {
        return false;
    }
    return blockStackBefore.isEmpty()
        || blockStackBefore.constLast().directive != closeMatchesOpenDirective;
}

QString normalizedTherionDirectiveToken(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized == QStringLiteral("centreline")) {
        return QStringLiteral("centerline");
    }
    if (normalized == QStringLiteral("endcentreline")) {
        return QStringLiteral("endcenterline");
    }
    return normalized;
}

QHash<QString, QString> therionOpenToCloseDirectiveMap()
{
    QHash<QString, QString> values;
    for (const BlockPair &pair : knownBlockPairs()) {
        values.insert(normalizedTherionDirectiveToken(pair.openDirective),
                      normalizedTherionDirectiveToken(pair.closeDirective));
    }
    return values;
}

QHash<QString, QString> therionCloseToOpenDirectiveMap()
{
    QHash<QString, QString> values;
    for (const BlockPair &pair : knownBlockPairs()) {
        values.insert(normalizedTherionDirectiveToken(pair.closeDirective),
                      normalizedTherionDirectiveToken(pair.openDirective));
    }
    return values;
}

bool therionDirectiveIsKnownBlockDirective(const QString &directive)
{
    const QString normalized = normalizedTherionDirectiveToken(directive);
    return therionOpenToCloseDirectiveMap().contains(normalized)
        || therionCloseToOpenDirectiveMap().contains(normalized);
}

bool therionBlockTreatsChildrenAsContent(const QString &directive)
{
    const QString normalized = normalizedTherionDirectiveToken(directive);
    return normalized == QStringLiteral("line")
        || normalized == QStringLiteral("area")
        || normalized == QStringLiteral("map")
        || normalized == QStringLiteral("centerline")
        || normalized == QStringLiteral("code");
}

TherionSourceDocument TherionSourceDocument::fromText(
    const QString &contents,
    const TherionSourceDocumentMetadata &metadata)
{
    TherionSourceDocument document;
    document.metadata_ = metadata;
    document.sourceText_ = TherionSourceText::fromText(contents);
    document.parsedDocument_ = TherionDocumentParser::parseSourceDocument(document.sourceText_);
    document.lines_.reserve(document.parsedDocument_.lines.size());

    const QHash<QString, QString> openToClose = therionOpenToCloseDirectiveMap();
    const QHash<QString, QString> closeToOpen = therionCloseToOpenDirectiveMap();
    QVector<TherionSourceBlockFrame> stack;
    QVector<ActiveBlockFrame> activeStack;

    for (const TherionParsedSourceLine &sourceLine : document.parsedDocument_.lines) {
        TherionSourceDocumentLine semanticLine;
        semanticLine.sourceLine = sourceLine;
        semanticLine.blockStackBefore = stack;
        semanticLine.currentBlockDirective = stack.isEmpty() ? QString() : stack.constLast().directive;

        if (sourceLine.parsed.tokens.isEmpty()) {
            semanticLine.role = sourceLine.isCommentOnly()
                ? TherionSourceLineRole::Comment
                : TherionSourceLineRole::Empty;
            document.lines_.append(semanticLine);
            continue;
        }

        semanticLine.normalizedDirective = normalizedTherionDirectiveToken(sourceLine.parsed.directive);
        semanticLine.opensBlock = openToClose.contains(semanticLine.normalizedDirective);
        semanticLine.closesBlock = closeToOpen.contains(semanticLine.normalizedDirective);
        semanticLine.closeMatchesOpenDirective = closeToOpen.value(semanticLine.normalizedDirective);
        semanticLine.role = therionBlockTreatsChildrenAsContent(semanticLine.currentBlockDirective)
                && !semanticLine.opensBlock
                && !semanticLine.closesBlock
            ? TherionSourceLineRole::BlockContent
            : TherionSourceLineRole::Command;

        if (semanticLine.opensBlock) {
            TherionSourceBlockRange range;
            range.directive = semanticLine.normalizedDirective;
            range.openLineNumber = sourceLine.lineNumber;
            range.startOffset = sourceLine.startOffset;
            range.endOffset = sourceLine.endOffset;
            range.openLineText = sourceLine.text;
            range.parentStack = stack;
            document.blockRanges_.append(range);

            const TherionSourceBlockFrame frame{semanticLine.normalizedDirective,
                                                sourceLine.lineNumber,
                                                sourceLine.text};
            stack.append(frame);
            activeStack.append(ActiveBlockFrame{static_cast<int>(document.blockRanges_.size() - 1)});
        } else if (semanticLine.closesBlock
                   && !semanticLine.hasUnmatchedClose()) {
            if (!activeStack.isEmpty()) {
                const int rangeIndex = activeStack.constLast().rangeIndex;
                if (rangeIndex >= 0 && rangeIndex < document.blockRanges_.size()) {
                    TherionSourceBlockRange &range = document.blockRanges_[rangeIndex];
                    range.closeLineNumber = sourceLine.lineNumber;
                    range.endOffset = sourceLine.endOffset;
                    range.closeLineText = sourceLine.text;
                }
                activeStack.removeLast();
            }
            stack.removeLast();
        }

        document.lines_.append(semanticLine);
    }

    document.openBlocksAtEnd_ = stack;
    return document;
}

const TherionSourceDocumentMetadata &TherionSourceDocument::metadata() const
{
    return metadata_;
}

TherionSourceDocumentType TherionSourceDocument::sourceType() const
{
    return metadata_.sourceType;
}

QString TherionSourceDocument::encodingName() const
{
    return metadata_.encodingName;
}

int TherionSourceDocument::revisionId() const
{
    return metadata_.revisionId;
}

const TherionSourceText &TherionSourceDocument::sourceText() const
{
    return sourceText_;
}

const TherionParsedSourceDocument &TherionSourceDocument::parsedDocument() const
{
    return parsedDocument_;
}

const QVector<TherionSourceDocumentLine> &TherionSourceDocument::lines() const
{
    return lines_;
}

const QVector<TherionSourceBlockRange> &TherionSourceDocument::blockRanges() const
{
    return blockRanges_;
}

const QVector<TherionSourceBlockFrame> &TherionSourceDocument::openBlocksAtEnd() const
{
    return openBlocksAtEnd_;
}

QString TherionSourceDocument::toText() const
{
    return parsedDocument_.toText();
}

QVector<TherionParsedLine> TherionSourceDocument::tokenLines() const
{
    return parsedDocument_.tokenLines();
}
}
