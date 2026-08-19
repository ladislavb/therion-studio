#include "TherionSourceValidator.h"

#include "TherionCommandLineModel.h"
#include "TherionCommandSyntax.h"
#include "TherionDocumentEditor.h"
#include "TherionDocumentParser.h"
#include "TherionSourceDocument.h"
#include "TherionSourceLogicalDocument.h"
#include "TherionSourceReferenceResolver.h"
#include "TherionSourceSnapshotCache.h"
#include "TherionTokenRules.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QSet>

#include <algorithm>
#include <optional>

namespace TherionStudio
{
namespace
{
struct LineCleanupResult
{
    QString text;
    bool changed = false;
    int columnNumber = 1;
    int columnLength = 0;
};

struct UnclosedBlockFixPlan
{
    bool valid = false;
    int startOffset = 0;
    int insertionLineNumber = 0;
    QString replacementText;
    QString suggestedText;
};

QByteArray sourceDigest(const QString &contents)
{
    return QCryptographicHash::hash(contents.toUtf8(), QCryptographicHash::Sha256);
}

QString leadingWhitespace(const QString &text)
{
    int index = 0;
    while (index < text.size() && text.at(index).isSpace()) {
        ++index;
    }
    return text.left(index);
}

bool isLineOrAreaCloseBoundary(const QString &directive)
{
    static const QSet<QString> boundaryDirectives{
        QStringLiteral("point"),
        QStringLiteral("line"),
        QStringLiteral("area"),
        QStringLiteral("endscrap"),
    };
    return boundaryDirectives.contains(directive);
}

bool isFixableUnclosedMapBlock(const QString &directive)
{
    return directive == QStringLiteral("scrap")
        || directive == QStringLiteral("line")
        || directive == QStringLiteral("area");
}

bool isUnclosedBlockFixBoundary(const QString &openDirective, const QString &candidateDirective)
{
    if (openDirective == QStringLiteral("scrap")) {
        return candidateDirective == QStringLiteral("scrap");
    }
    if (openDirective == QStringLiteral("line") || openDirective == QStringLiteral("area")) {
        return isLineOrAreaCloseBoundary(candidateDirective);
    }
    return false;
}

QString lineEndingForInsertion(const TherionSourceDocument &sourceDocument,
                               int insertionLineIndex,
                               const QString &contents)
{
    const QVector<TherionSourceDocumentLine> &lines = sourceDocument.lines();
    if (insertionLineIndex > 0 && insertionLineIndex - 1 < lines.size()) {
        const QString lineEnding = lines.at(insertionLineIndex - 1).sourceLine.lineEnding;
        if (!lineEnding.isEmpty()) {
            return lineEnding;
        }
    }
    if (insertionLineIndex >= 0 && insertionLineIndex < lines.size()) {
        const QString lineEnding = lines.at(insertionLineIndex).sourceLine.lineEnding;
        if (!lineEnding.isEmpty()) {
            return lineEnding;
        }
    }
    return contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
}

UnclosedBlockFixPlan unclosedBlockFixPlan(const TherionSourceDocument &sourceDocument,
                                          const TherionSourceBlockFrame &openBlock,
                                          const QString &closeDirective)
{
    UnclosedBlockFixPlan plan;
    if (closeDirective.isEmpty() || !isFixableUnclosedMapBlock(openBlock.directive)) {
        return plan;
    }

    const QString contents = sourceDocument.toText();
    const QVector<TherionSourceDocumentLine> &lines = sourceDocument.lines();
    const TherionSourceDocumentLine *openLine = sourceDocument.lineAtLineNumber(openBlock.lineNumber);
    const int openLineIndex = openLine != nullptr ? openLine->sourceLine.lineNumber - 1 : -1;
    if (openLineIndex < 0 || openLineIndex >= lines.size()) {
        return plan;
    }

    int insertionLineIndex = lines.size();
    for (int index = openLineIndex + 1; index < lines.size(); ++index) {
        const QString candidateDirective = lines.at(index).normalizedDirective;
        if (isUnclosedBlockFixBoundary(openBlock.directive, candidateDirective)) {
            insertionLineIndex = index;
            break;
        }
    }

    const QString indentation = leadingWhitespace(openLine->sourceLine.text);
    const QString insertedLine = indentation + closeDirective;
    const QString lineEnding = lineEndingForInsertion(sourceDocument, insertionLineIndex, contents);
    plan.valid = true;
    plan.startOffset = insertionLineIndex < lines.size()
        ? lines.at(insertionLineIndex).sourceLine.startOffset
        : contents.size();
    plan.insertionLineNumber = insertionLineIndex < lines.size()
        ? lines.at(insertionLineIndex).sourceLine.lineNumber
        : 0;
    plan.replacementText = insertedLine + lineEnding;
    if (insertionLineIndex >= lines.size() && !contents.isEmpty() && !contents.endsWith(QLatin1Char('\n'))) {
        plan.replacementText.prepend(lineEnding);
    }
    plan.suggestedText = plan.insertionLineNumber > 0
        ? QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Insert before line %1:\n%2")
              .arg(plan.insertionLineNumber)
              .arg(insertedLine)
        : QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Insert at end of file:\n%1")
              .arg(insertedLine);
    return plan;
}

std::optional<TherionParsedToken> tokenSpanForCommandTokenIndex(const TherionParsedLine &parsedLine,
                                                                int tokenIndex)
{
    if (tokenIndex < 0) {
        return std::nullopt;
    }

    int currentTokenIndex = 0;
    for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
        if (tokenSpan.type == TherionTokenType::Comment) {
            continue;
        }
        if (currentTokenIndex == tokenIndex) {
            return tokenSpan;
        }
        ++currentTokenIndex;
    }
    return std::nullopt;
}

QPair<int, int> sourceLineRemovalRangeForTokenRange(const QString &lineText,
                                                    const TherionParsedLine &parsedLine,
                                                    int firstTokenIndex,
                                                    int lastTokenIndex)
{
    if (firstTokenIndex < 0
        || lastTokenIndex < firstTokenIndex) {
        return qMakePair(-1, -1);
    }

    const std::optional<TherionParsedToken> firstToken = tokenSpanForCommandTokenIndex(parsedLine,
                                                                                       firstTokenIndex);
    const std::optional<TherionParsedToken> lastToken = tokenSpanForCommandTokenIndex(parsedLine,
                                                                                     lastTokenIndex);
    if (!firstToken.has_value() || !lastToken.has_value()) {
        return qMakePair(-1, -1);
    }

    int removeStart = firstToken->start;
    int removeEnd = lastToken->start + lastToken->length;
    if (removeStart < 0 || removeEnd < removeStart || removeEnd > lineText.size()) {
        return qMakePair(-1, -1);
    }

    if (removeStart > 0 && lineText.at(removeStart - 1).isSpace()) {
        --removeStart;
    } else if (removeEnd < lineText.size() && lineText.at(removeEnd).isSpace()) {
        ++removeEnd;
    }

    return qMakePair(removeStart, removeEnd);
}

QString lineWithRemovedTokenRange(const QString &lineText,
                                  const TherionParsedLine &parsedLine,
                                  int firstTokenIndex,
                                  int lastTokenIndex,
                                  bool *ok)
{
    if (ok != nullptr) {
        *ok = false;
    }

    const QPair<int, int> range =
        sourceLineRemovalRangeForTokenRange(lineText, parsedLine, firstTokenIndex, lastTokenIndex);
    if (range.first < 0 || range.second < range.first) {
        return lineText;
    }

    QString updated = lineText;
    updated.remove(range.first, range.second - range.first);
    if (ok != nullptr) {
        *ok = true;
    }
    return updated;
}

bool setRangeFromTokenRange(const QString &lineText,
                            const TherionParsedLine &parsedLine,
                            int firstTokenIndex,
                            int lastTokenIndex,
                            int *columnNumber,
                            int *columnLength)
{
    if (columnNumber != nullptr) {
        *columnNumber = 1;
    }
    if (columnLength != nullptr) {
        *columnLength = 0;
    }

    if (firstTokenIndex < 0
        || lastTokenIndex < firstTokenIndex) {
        return false;
    }

    const std::optional<TherionParsedToken> firstToken = tokenSpanForCommandTokenIndex(parsedLine,
                                                                                       firstTokenIndex);
    const std::optional<TherionParsedToken> lastToken = tokenSpanForCommandTokenIndex(parsedLine,
                                                                                     lastTokenIndex);
    if (!firstToken.has_value() || !lastToken.has_value()) {
        return false;
    }

    const int rangeStart = firstToken->start;
    const int rangeEnd = lastToken->start + lastToken->length;
    if (rangeStart < 0 || rangeEnd <= rangeStart || rangeEnd > lineText.size()) {
        return false;
    }

    if (columnNumber != nullptr) {
        *columnNumber = rangeStart + 1;
    }
    if (columnLength != nullptr) {
        *columnLength = rangeEnd - rangeStart;
    }
    return true;
}

bool setRangeFromTokenIndex(const TherionParsedLine &parsedLine,
                            int tokenIndex,
                            int *columnNumber,
                            int *columnLength)
{
    if (columnNumber != nullptr) {
        *columnNumber = 1;
    }
    if (columnLength != nullptr) {
        *columnLength = 0;
    }

    if (tokenIndex < 0) {
        return false;
    }

    const std::optional<TherionParsedToken> token = tokenSpanForCommandTokenIndex(parsedLine, tokenIndex);
    if (!token.has_value()) {
        return false;
    }

    if (token->start < 0 || token->length <= 0) {
        return false;
    }

    if (columnNumber != nullptr) {
        *columnNumber = token->start + 1;
    }
    if (columnLength != nullptr) {
        *columnLength = token->length;
    }
    return true;
}

bool looksLikeCommandDirective(const QString &token)
{
    const QString trimmed = token.trimmed();
    if (trimmed.isEmpty() || !trimmed.at(0).isLetter()) {
        return false;
    }
    for (const QChar character : trimmed) {
        if (character.isLetterOrNumber()
            || character == QLatin1Char('-')
            || character == QLatin1Char('_')) {
            continue;
        }
        return false;
    }
    return true;
}

bool layoutWildcardOptionMatches(const QString &knownOption,
                                 const QString &normalizedOption,
                                 const TherionSourceValidationCatalog &catalog)
{
    if (knownOption != QStringLiteral("-layout-xxx")) {
        return false;
    }

    static const QString layoutPrefix = QStringLiteral("-layout-");
    if (!normalizedOption.startsWith(layoutPrefix)
        || normalizedOption.size() <= layoutPrefix.size()) {
        return false;
    }

    const QString layoutOption = QStringLiteral("-") + normalizedOption.mid(layoutPrefix.size());
    return catalog.commandOptionNames.value(QStringLiteral("layout")).contains(layoutOption);
}

bool knownOptionMatches(const QString &knownOption,
                        const QString &normalizedOption,
                        const TherionSourceValidationCatalog &catalog)
{
    if (normalizedOption.endsWith(QStringLiteral("xxx"))) {
        return false;
    }

    return knownOption == normalizedOption
        || layoutWildcardOptionMatches(knownOption, normalizedOption, catalog);
}

bool isKnownCatalogOption(const QSet<QString> &knownOptions,
                          const QString &normalizedOption,
                          const TherionSourceValidationCatalog &catalog)
{
    if (!normalizedOption.endsWith(QStringLiteral("xxx"))
        && knownOptions.contains(normalizedOption)) {
        return true;
    }
    for (const QString &knownOption : knownOptions) {
        if (knownOptionMatches(knownOption, normalizedOption, catalog)) {
            return true;
        }
    }
    return false;
}

QString pathTokenWithForwardSlashes(QString pathToken)
{
    pathToken.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return pathToken;
}

TherionSourcePhysicalRange expandedPathTokenRange(TherionSourcePhysicalRange range)
{
    if (range.lineText.isEmpty() || range.columnNumber <= 0) {
        return range;
    }

    const int originalStart = range.columnNumber - 1;
    int start = originalStart;
    while (start > 0 && !range.lineText.at(start - 1).isSpace()) {
        --start;
    }

    int end = qMin(range.lineText.size(), originalStart + qMax(range.columnLength, 1));
    if (start < range.lineText.size() && range.lineText.at(start) == QLatin1Char('"')) {
        end = qMin(range.lineText.size(), qMax(end, start + 1));
        while (end < range.lineText.size()) {
            const QChar ch = range.lineText.at(end);
            ++end;
            if (ch == QLatin1Char('"')) {
                break;
            }
        }
    } else {
        while (end < range.lineText.size() && !range.lineText.at(end).isSpace()) {
            ++end;
        }
    }

    range.columnNumber = start + 1;
    range.columnLength = end - start;
    range.startOffset += start - originalStart;
    range.length = range.columnLength;
    return range;
}

TherionSourcePhysicalRange pathPhysicalRangeForTokenIndex(const TherionSourceLogicalCommand &command, int tokenIndex)
{
    TherionSourcePhysicalRange physicalRange;
    if (!command.physicalRangeForTokenIndex(tokenIndex, &physicalRange)) {
        return {};
    }
    return expandedPathTokenRange(physicalRange);
}

QString physicalRangeText(const TherionSourcePhysicalRange &physicalRange)
{
    if (physicalRange.columnNumber <= 0 || physicalRange.columnLength <= 0) {
        return {};
    }
    return physicalRange.lineText.mid(physicalRange.columnNumber - 1, physicalRange.columnLength);
}

QString physicalTokenText(const TherionSourceLogicalCommand &command, int tokenIndex)
{
    const TherionSourcePhysicalRange physicalRange = pathPhysicalRangeForTokenIndex(command, tokenIndex);
    if (physicalRange.columnLength > 0) {
        return physicalRangeText(physicalRange);
    }
    return tokenIndex >= 0 && tokenIndex < command.parsed.tokens.size()
        ? command.parsed.tokens.at(tokenIndex)
        : QString();
}

bool pathTokenUsesBackslashSeparator(const TherionSourceLogicalCommand &command, int tokenIndex)
{
    return physicalTokenText(command, tokenIndex).contains(QLatin1Char('\\'));
}

bool physicalRangeUsesBackslashSeparator(const TherionSourcePhysicalRange &physicalRange)
{
    return physicalRangeText(physicalRange).contains(QLatin1Char('\\'));
}

QString lineWithPhysicalRangeReplacement(const TherionSourcePhysicalRange &physicalRange,
                                         const QString &replacementText)
{
    if (physicalRange.columnNumber <= 0 || physicalRange.columnLength <= 0) {
        return replacementText;
    }

    QString lineText = physicalRange.lineText;
    lineText.replace(physicalRange.columnNumber - 1, physicalRange.columnLength, replacementText);
    return lineText;
}

QString optionDeduplicationKey(const QString &optionToken, const QStringList &values)
{
    return normalizedCommandOptionName(optionToken)
        + QLatin1Char('\n')
        + values.join(QLatin1Char('\n'));
}

TherionParsedLine parseValidatorSyntheticLine(const QString &lineText)
{
    // Validator fix previews and block-frame diagnostics intentionally reparse one synthetic line.
    return TherionDocumentParser::parseLine(lineText);
}

QString objectIdScopeKey(const QVector<TherionSourceBlockFrame> &blockStack)
{
    QStringList parts;
    parts.reserve(blockStack.size());
    for (const TherionSourceBlockFrame &frame : blockStack) {
        const TherionParsedLine parsedLine = parseValidatorSyntheticLine(frame.lineText);
        QString id;
        if (parsedLine.tokens.size() > 1) {
            id = parsedLine.tokens.at(1).trimmed();
        }
        parts.append(frame.directive + QLatin1Char(':') + id);
    }
    return parts.join(QLatin1Char('/'));
}

QString scrapObjectScopeKey(const QVector<TherionSourceBlockFrame> &blockStack)
{
    QStringList parts;
    for (const TherionSourceBlockFrame &frame : blockStack) {
        const TherionParsedLine parsedLine = parseValidatorSyntheticLine(frame.lineText);
        QString id;
        if (parsedLine.tokens.size() > 1) {
            id = parsedLine.tokens.at(1).trimmed();
        }
        parts.append(frame.directive + QLatin1Char(':') + id);
        if (frame.directive == QStringLiteral("scrap")) {
            break;
        }
    }
    return parts.join(QLatin1Char('/'));
}

QString duplicateObjectIdKey(const TherionSourceLogicalCommand &command,
                             const QString &objectId)
{
    return QStringLiteral("object\n")
        + objectIdScopeKey(command.blockStackBefore)
        + QLatin1Char('\n')
        + objectId.trimmed();
}

QString duplicateNamespaceNameKey(const TherionSourceLogicalCommand &command,
                                  const QString &name)
{
    return QStringLiteral("namespace\n")
        + objectIdScopeKey(command.blockStackBefore)
        + QLatin1Char('\n')
        + name.trimmed();
}

bool commandUsesScrapObjectIdNamespace(const QString &commandName)
{
    return commandName == QStringLiteral("line")
        || commandName == QStringLiteral("point")
        || commandName == QStringLiteral("area");
}

bool commandUsesParentNamespaceName(const QString &commandName)
{
    return commandName == QStringLiteral("survey")
        || commandName == QStringLiteral("map")
        || commandName == QStringLiteral("scrap");
}

bool isScrapObjectBlockDirective(const QString &directive)
{
    return directive == QStringLiteral("line")
        || directive == QStringLiteral("area");
}

bool lineContentRowHasCoordinates(const TherionParsedLine &parsedLine)
{
    int numericTokenCount = 0;
    for (int index = 0; index < parsedLine.tokens.size(); ++index) {
        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }
        if (!TherionTokenRules::isNumericToken(parsedLine.tokens.at(index))) {
            return numericTokenCount >= 2 && numericTokenCount % 2 == 0;
        }
        ++numericTokenCount;
    }
    return numericTokenCount >= 2 && numericTokenCount % 2 == 0;
}

bool lineContentRowIsSmoothOff(const TherionParsedLine &parsedLine)
{
    return parsedLine.tokens.size() >= 2
        && parsedLine.tokens.at(0).compare(QStringLiteral("smooth"), Qt::CaseInsensitive) == 0
        && parsedLine.tokens.at(1).compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0;
}

bool blockIsInsideScrap(const TherionSourceBlockRange &blockRange)
{
    for (const TherionSourceBlockFrame &frame : blockRange.parentStack) {
        if (frame.directive == QStringLiteral("scrap")) {
            return true;
        }
    }
    return false;
}

bool blockHasOnlyBlankBody(const TherionSourceBlockRange &blockRange,
                           const TherionSourceDocument &sourceDocument)
{
    if (!blockRange.isClosed()) {
        return false;
    }

    for (int lineNumber = blockRange.openLineNumber + 1; lineNumber < blockRange.closeLineNumber; ++lineNumber) {
        const TherionSourceDocumentLine *line = sourceDocument.lineAtLineNumber(lineNumber);
        if (line == nullptr) {
            return false;
        }
        if (!line->sourceLine.isBlank()) {
            return false;
        }
    }
    return true;
}

TherionSourceDiagnostic diagnosticForEmptyScrapObject(const TherionSourceBlockRange &blockRange,
                                                     const TherionSourceDocument &sourceDocument)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("empty-scrap-object");
    diagnostic.severity = TherionSourceDiagnosticSeverity::Warning;
    diagnostic.lineNumber = blockRange.openLineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.columnLength = blockRange.directive.size();
    diagnostic.title = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Empty scrap object");
    diagnostic.message = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "This `%1` object in a scrap has no geometry rows.")
                             .arg(blockRange.directive);
    diagnostic.currentText = sourceDocument.toText().mid(blockRange.startOffset,
                                                         blockRange.endOffset - blockRange.startOffset);
    diagnostic.suggestedText = QString();
    diagnostic.hasFix = true;
    diagnostic.fix.startOffset = blockRange.startOffset;
    diagnostic.fix.length = blockRange.endOffset - blockRange.startOffset;
    diagnostic.fix.replacementText = QString();
    diagnostic.fix.description = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Remove empty %1 block").arg(blockRange.directive);
    return diagnostic;
}

QString commandNameForDuplicateValidation(const TherionSourceLogicalCommand &command)
{
    return command.metadata.commandName.isEmpty()
        ? command.normalizedDirective
        : command.metadata.commandName;
}

std::optional<TherionSourceLogicalArgumentRange> commandObjectIdentityRange(
    const TherionSourceLogicalCommand &command)
{
    const QString commandName = commandNameForDuplicateValidation(command);
    if (commandUsesParentNamespaceName(commandName)) {
        if (command.positionalArgumentRanges.isEmpty()) {
            return std::nullopt;
        }
        return command.positionalArgumentRanges.constFirst();
    }

    if (!commandUsesScrapObjectIdNamespace(commandName)) {
        return std::nullopt;
    }

    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        if (normalizedCommandOptionName(optionEntry.key) != QStringLiteral("id")) {
            continue;
        }
        if (optionEntry.valueRanges.isEmpty()) {
            return std::nullopt;
        }
        return optionEntry.valueRanges.constFirst();
    }
    return std::nullopt;
}

std::optional<QString> commandDuplicateIdentityKey(const TherionSourceLogicalCommand &command,
                                                   const QString &identity)
{
    const QString commandName = commandNameForDuplicateValidation(command);
    if (commandUsesScrapObjectIdNamespace(commandName)) {
        return duplicateObjectIdKey(command, identity);
    }
    if (commandUsesParentNamespaceName(commandName)) {
        return duplicateNamespaceNameKey(command, identity);
    }
    return std::nullopt;
}

TherionSourceDiagnostic diagnosticForDuplicateObjectId(
    const TherionSourceLogicalArgumentRange &idRange)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("duplicate-object-id");
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.lineNumber = idRange.physicalRange.lineNumber;
    diagnostic.columnNumber = idRange.physicalRange.columnNumber;
    diagnostic.columnLength = idRange.physicalRange.columnLength;
    diagnostic.title = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Duplicate object id");
    diagnostic.message = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Object id `%1` is already used by another object in this namespace.")
                             .arg(idRange.text);
    diagnostic.currentText = idRange.physicalRange.lineText;
    diagnostic.suggestedText = QString();
    diagnostic.hasFix = false;
    return diagnostic;
}

std::optional<TherionSourceLogicalArgumentRange> lineObjectIdRange(
    const TherionSourceLogicalCommand &command)
{
    if (commandNameForDuplicateValidation(command) != QStringLiteral("line")) {
        return std::nullopt;
    }

    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        if (normalizedCommandOptionName(optionEntry.key) != QStringLiteral("id")) {
            continue;
        }
        if (optionEntry.valueRanges.isEmpty()) {
            return std::nullopt;
        }
        return optionEntry.valueRanges.constFirst();
    }
    return std::nullopt;
}

TherionSourceDiagnostic diagnosticForUnknownAreaLineReference(
    const TherionSourceLogicalTokenRange &referenceRange)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("unknown-area-line-reference");
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.lineNumber = referenceRange.physicalRange.lineNumber;
    diagnostic.columnNumber = referenceRange.physicalRange.columnNumber;
    diagnostic.columnLength = referenceRange.physicalRange.columnLength;
    diagnostic.title = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unknown area line reference");
    diagnostic.message = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Area references line `%1`, but no line with this id exists in the current scrap.")
                             .arg(referenceRange.text);
    diagnostic.currentText = referenceRange.physicalRange.lineText;
    diagnostic.suggestedText = QString();
    diagnostic.hasFix = false;
    return diagnostic;
}

TherionSourceDiagnostic diagnosticForDuplicateLinePointSmoothOff(
    const TherionSourceDocumentLine &line)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("duplicate-line-point-smooth-off");
    diagnostic.severity = TherionSourceDiagnosticSeverity::Warning;
    diagnostic.lineNumber = line.sourceLine.lineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.columnLength = line.sourceLine.textLength;
    setRangeFromTokenIndex(line.sourceLine.parsed,
                           0,
                           &diagnostic.columnNumber,
                           &diagnostic.columnLength);
    diagnostic.title = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Duplicate line-point smooth option");
    diagnostic.message = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "This line point already has a `smooth off` option before the next coordinate row.");
    diagnostic.currentText = line.sourceLine.text;
    diagnostic.suggestedText = QString();
    diagnostic.hasFix = true;
    diagnostic.fix.startOffset = line.sourceLine.startOffset;
    diagnostic.fix.length = line.sourceLine.textLength + line.sourceLine.lineEndingLength;
    diagnostic.fix.replacementText = QString();
    diagnostic.fix.description = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Remove duplicate smooth off on line %1").arg(line.sourceLine.lineNumber);
    return diagnostic;
}

QString cleanupInvalidSubtypeValues(const QString &lineText,
                                    bool *changed,
                                    int *firstChangedColumnNumber,
                                    int *firstChangedColumnLength)
{
    if (changed != nullptr) {
        *changed = false;
    }

    QString updated = lineText;
    for (;;) {
        const TherionParsedLine parsedLine = parseValidatorSyntheticLine(updated);
        const ParsedCommandOptions commandOptions = parseCommandOptions(parsedLine.directive,
                                                                        parsedLine.tokens,
                                                                        {},
                                                                        false,
                                                                        false);
        int removeStartTokenIndex = -1;
        int removeEndTokenIndex = -1;
        for (int entryIndex = 0; entryIndex < commandOptions.optionEntries.size(); ++entryIndex) {
            const CommandOptionEntry &entry = commandOptions.optionEntries.at(entryIndex);
            const QString optionToken = entry.key.trimmed().toLower();
            if (optionToken != QStringLiteral("-subtype")) {
                continue;
            }

            if (entry.firstValueTokenIndex >= 0 && entry.lastValueTokenIndex >= entry.firstValueTokenIndex) {
                for (int valueIndex = entry.firstValueTokenIndex; valueIndex <= entry.lastValueTokenIndex; ++valueIndex) {
                    if (valueIndex >= parsedLine.tokens.size()) {
                        break;
                    }
                    if (commandTokenEmbedsOptionValue(parsedLine.tokens.at(valueIndex))) {
                        removeStartTokenIndex = entry.optionTokenIndex;
                        removeEndTokenIndex = entry.lastValueTokenIndex;
                        break;
                    }
                }
            }

            if (removeStartTokenIndex < 0
                && entry.rawValueTokens.isEmpty()
                && entryIndex + 1 < commandOptions.optionEntries.size()) {
                const CommandOptionEntry &nextEntry = commandOptions.optionEntries.at(entryIndex + 1);
                if (nextEntry.embeddedValue) {
                    removeStartTokenIndex = entry.optionTokenIndex;
                    removeEndTokenIndex = nextEntry.optionTokenIndex;
                    break;
                }
            }

            if (removeStartTokenIndex >= 0) {
                break;
            }
        }

        if (removeStartTokenIndex < 0) {
            return updated;
        }

        bool removed = false;
        if (firstChangedColumnLength != nullptr
            && *firstChangedColumnLength <= 0) {
            setRangeFromTokenRange(updated,
                                   parsedLine,
                                   removeStartTokenIndex,
                                   removeEndTokenIndex,
                                   firstChangedColumnNumber,
                                   firstChangedColumnLength);
        }
        updated = lineWithRemovedTokenRange(updated,
                                            parsedLine,
                                            removeStartTokenIndex,
                                            removeEndTokenIndex,
                                            &removed);
        if (!removed) {
            return updated;
        }
        if (changed != nullptr) {
            *changed = true;
        }
    }
}

QString cleanupDuplicateOptions(const QString &lineText,
                                bool *changed,
                                int *firstChangedColumnNumber,
                                int *firstChangedColumnLength)
{
    if (changed != nullptr) {
        *changed = false;
    }

    QString updated = lineText;
    for (;;) {
        const TherionParsedLine parsedLine = parseValidatorSyntheticLine(updated);
        const ParsedCommandOptions commandOptions = parseCommandOptions(parsedLine.directive,
                                                                        parsedLine.tokens,
                                                                        {},
                                                                        false,
                                                                        false);
        QStringList seenOptions;
        int removeStartTokenIndex = -1;
        int removeEndTokenIndex = -1;

        for (const CommandOptionEntry &entry : commandOptions.optionEntries) {
            const QString optionToken = entry.key;
            const QStringList values = entry.rawValueTokens;
            const QString deduplicationKey = optionDeduplicationKey(optionToken, values);
            if (seenOptions.contains(deduplicationKey)) {
                removeStartTokenIndex = entry.optionTokenIndex;
                removeEndTokenIndex = entry.embeddedValue ? entry.optionTokenIndex : entry.lastValueTokenIndex;
                break;
            }

            seenOptions.append(deduplicationKey);
        }

        if (removeStartTokenIndex < 0) {
            return updated;
        }

        bool removed = false;
        if (firstChangedColumnLength != nullptr
            && *firstChangedColumnLength <= 0) {
            setRangeFromTokenRange(updated,
                                   parsedLine,
                                   removeStartTokenIndex,
                                   removeEndTokenIndex,
                                   firstChangedColumnNumber,
                                   firstChangedColumnLength);
        }
        updated = lineWithRemovedTokenRange(updated,
                                            parsedLine,
                                            removeStartTokenIndex,
                                            removeEndTokenIndex,
                                            &removed);
        if (!removed) {
            return updated;
        }
        if (changed != nullptr) {
            *changed = true;
        }
    }
}

LineCleanupResult cleanupLine(const QString &lineText)
{
    LineCleanupResult result;
    result.text = lineText;

    bool subtypeChanged = false;
    result.text = cleanupInvalidSubtypeValues(result.text,
                                              &subtypeChanged,
                                              &result.columnNumber,
                                              &result.columnLength);

    bool duplicateChanged = false;
    result.text = cleanupDuplicateOptions(result.text,
                                          &duplicateChanged,
                                          &result.columnNumber,
                                          &result.columnLength);

    result.changed = subtypeChanged || duplicateChanged;
    return result;
}

TherionSourceDiagnostic diagnosticForLineCleanup(const TherionSourceLogicalCommand &command,
                                                 const LineCleanupResult &cleanup)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("malformed-option-token");
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.lineNumber = command.startLineNumber;
    diagnostic.columnNumber = cleanup.columnNumber;
    diagnostic.columnLength = cleanup.columnLength;
    diagnostic.title = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Malformed or duplicate option token");
    diagnostic.message = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "This command contains an option-like token or duplicate option/value pair that Therion may reject.");
    diagnostic.currentText = command.text;
    diagnostic.suggestedText = cleanup.text;
    diagnostic.hasFix = true;
    diagnostic.fix.startOffset = command.startOffset;
    diagnostic.fix.length = command.text.size();
    diagnostic.fix.replacementText = cleanup.text;
    diagnostic.fix.description = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Rewrite line %1").arg(command.startLineNumber);
    return diagnostic;
}

TherionSourceDiagnostic diagnosticForLine(const TherionParsedSourceLine &sourceLine,
                                          const QString &code,
                                          const QString &title,
                                          const QString &message,
                                          TherionSourceDiagnosticSeverity severity = TherionSourceDiagnosticSeverity::Warning)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = severity;
    diagnostic.lineNumber = sourceLine.lineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.title = title;
    diagnostic.message = message;
    diagnostic.currentText = sourceLine.text;
    diagnostic.suggestedText = QString();
    diagnostic.hasFix = false;
    return diagnostic;
}

TherionSourceDiagnostic diagnosticForLine(const TherionSourceLogicalCommand &command,
                                          const QString &code,
                                          const QString &title,
                                          const QString &message,
                                          TherionSourceDiagnosticSeverity severity = TherionSourceDiagnosticSeverity::Warning)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = severity;
    diagnostic.lineNumber = command.startLineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.title = title;
    diagnostic.message = message;
    diagnostic.currentText = command.text;
    diagnostic.suggestedText = QString();
    diagnostic.hasFix = false;
    return diagnostic;
}

TherionSourceDiagnostic diagnosticForToken(const TherionSourceLogicalCommand &command,
                                           int tokenIndex,
                                           const QString &code,
                                           const QString &title,
                                           const QString &message,
                                           TherionSourceDiagnosticSeverity severity = TherionSourceDiagnosticSeverity::Warning)
{
    TherionSourceDiagnostic diagnostic = diagnosticForLine(command,
                                                          code,
                                                          title,
                                                          message,
                                                          severity);
    TherionSourcePhysicalRange physicalRange;
    if (command.physicalRangeForTokenIndex(tokenIndex, &physicalRange)) {
        diagnostic.lineNumber = physicalRange.lineNumber;
        diagnostic.columnNumber = physicalRange.columnNumber;
        diagnostic.columnLength = physicalRange.columnLength;
        diagnostic.currentText = physicalRange.lineText;
        return diagnostic;
    }

    setRangeFromTokenIndex(command.parsed,
                           tokenIndex,
                           &diagnostic.columnNumber,
                           &diagnostic.columnLength);
    return diagnostic;
}

TherionSourceDiagnostic diagnosticForBackslashPathSeparator(const TherionSourceLogicalCommand &command,
                                                           const TherionSourcePhysicalRange &physicalRange)
{
    const QString pathToken = physicalRangeText(physicalRange);
    TherionSourceDiagnostic diagnostic = diagnosticForToken(
        command,
        0,
        QStringLiteral("windows-path-separator"),
        QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Portable path separator"),
        QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Path `%1` uses backslash separators. Use `/` separators for portable Therion projects.")
            .arg(pathToken));

    if (physicalRange.columnLength <= 0) {
        return diagnostic;
    }

    diagnostic.lineNumber = physicalRange.lineNumber;
    diagnostic.columnNumber = physicalRange.columnNumber;
    diagnostic.columnLength = physicalRange.columnLength;
    diagnostic.currentText = physicalRange.lineText;
    const QString replacementText = pathTokenWithForwardSlashes(pathToken);
    diagnostic.suggestedText = lineWithPhysicalRangeReplacement(physicalRange, replacementText);
    diagnostic.hasFix = true;
    diagnostic.fix.startOffset = physicalRange.startOffset;
    diagnostic.fix.length = physicalRange.length;
    diagnostic.fix.replacementText = replacementText;
    diagnostic.fix.description = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Convert selected path to /");
    return diagnostic;
}

TherionSourceDiagnostic diagnosticForBackslashPathSeparator(const TherionSourceLogicalCommand &command,
                                                           int tokenIndex)
{
    return diagnosticForBackslashPathSeparator(command, pathPhysicalRangeForTokenIndex(command, tokenIndex));
}

void appendPathSeparatorDiagnostics(TherionSourceValidationResult *result,
                                    const TherionSourceLogicalCommand &command)
{
    if (result == nullptr) {
        return;
    }

    const QString commandName = command.metadata.commandName.isEmpty()
        ? command.normalizedDirective
        : command.metadata.commandName;
    if (commandName == QStringLiteral("input") || commandName == QStringLiteral("source")) {
        const TherionSourcePhysicalRange pathRange = therionSourceReferencePathRange(command);
        if (physicalRangeUsesBackslashSeparator(pathRange)) {
            result->diagnostics.append(diagnosticForBackslashPathSeparator(command, pathRange));
        }
    }

    if (commandName != QStringLiteral("export")) {
        return;
    }

    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        const QString normalizedOption = QStringLiteral("-") + normalizedCommandOptionName(optionEntry.key);
        if (normalizedOption != QStringLiteral("-output")
            && normalizedOption != QStringLiteral("-o")) {
            continue;
        }
        if (optionEntry.firstValueTokenIndex < 0
            || optionEntry.firstValueTokenIndex >= command.parsed.tokens.size()) {
            continue;
        }
        if (pathTokenUsesBackslashSeparator(command, optionEntry.firstValueTokenIndex)) {
            result->diagnostics.append(diagnosticForBackslashPathSeparator(command,
                                                                          optionEntry.firstValueTokenIndex));
        }
    }
}

void appendCommandCatalogDiagnostics(TherionSourceValidationResult *result,
                                     const TherionSourceLogicalCommand &command,
                                     const TherionSourceValidationCatalog &catalog)
{
    if (result == nullptr || command.parsed.tokens.isEmpty()) {
        return;
    }

    const QString commandName = command.metadata.commandName;
    const bool commandKnown = command.metadata.catalogCommandKnown;
    if (!commandKnown) {
        if (!therionDirectiveIsKnownBlockDirective(commandName)
            && looksLikeCommandDirective(command.parsed.directive)) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          0,
                                                          QStringLiteral("unknown-command"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unknown command"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Command `%1` is not present in the Therion command catalog.").arg(command.parsed.directive)));
        }
        return;
    }

    if (!command.metadata.catalogContexts.isEmpty()
        && !command.metadata.catalogContextAllowed) {
        result->diagnostics.append(diagnosticForToken(command,
                                                      0,
                                                      QStringLiteral("invalid-command-context"),
                                                      QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unexpected command context"),
                                                      QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Command `%1` is not listed for context `%2`. Expected context: %3.")
                                                          .arg(commandName,
                                                               command.metadata.catalogCurrentContext,
                                                               command.metadata.catalogContexts.join(QStringLiteral(", ")))));
    }

    if (!command.metadata.catalogDocumentTypes.isEmpty()
        && !command.metadata.catalogDocumentTypeAllowed) {
        QStringList expectedDocumentTypes = command.metadata.catalogDocumentTypes.values();
        std::sort(expectedDocumentTypes.begin(), expectedDocumentTypes.end());
        result->diagnostics.append(diagnosticForToken(command,
                                                      0,
                                                      QStringLiteral("invalid-document-type"),
                                                      QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unexpected document type"),
                                                      QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Command `%1` is not listed for document type `%2`. Expected document type: %3.")
                                                          .arg(commandName,
                                                               command.metadata.catalogCurrentDocumentType,
                                                               expectedDocumentTypes.join(QStringLiteral(", ")))));
    }

    const int requiredPositionalCount = command.metadata.catalogRequiredPositionalCount;
    const int providedPositionalCount = command.metadata.positionalArgumentCount;
    if (requiredPositionalCount > 0 && providedPositionalCount < requiredPositionalCount) {
        result->diagnostics.append(diagnosticForLine(command,
                                                     QStringLiteral("missing-argument"),
                                                     QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Missing argument"),
                                                     QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Command `%1` expects at least %2 positional argument(s), but %3 provided.")
                                                         .arg(commandName)
                                                         .arg(requiredPositionalCount)
                                                         .arg(providedPositionalCount),
                                                     TherionSourceDiagnosticSeverity::Error));
    }

    const int maxPositionalCount = command.metadata.catalogMaxPositionalCount;
    if (maxPositionalCount >= 0 && providedPositionalCount > maxPositionalCount) {
        const TherionSourceLogicalArgumentRange extraArgument =
            command.positionalArgumentRanges.at(maxPositionalCount);
        result->diagnostics.append(diagnosticForToken(command,
                                                     extraArgument.tokenIndex,
                                                     QStringLiteral("extra-argument"),
                                                     QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Extra argument"),
                                                     QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Command `%1` declares %2 positional argument(s), but %3 provided.")
                                                         .arg(commandName)
                                                         .arg(maxPositionalCount)
                                                         .arg(providedPositionalCount)));
    }

    for (int argumentIndex = 0; argumentIndex < providedPositionalCount; ++argumentIndex) {
        const QStringList allowedValues =
            command.metadata.catalogArgumentAllowedValuesByIndex.value(argumentIndex);
        const int tokenIndex = argumentIndex + 1;
        if (allowedValues.isEmpty() || tokenIndex >= command.parsed.tokens.size()) {
            continue;
        }

        const QString value = command.parsed.tokens.at(tokenIndex).trimmed();
        if (!value.isEmpty() && !allowedValues.contains(value, Qt::CaseInsensitive)) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          tokenIndex,
                                                          QStringLiteral("unknown-argument-value"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unknown argument value"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Command `%1` does not list `%2` as a known value. Known values: %3.")
                                                              .arg(commandName, value, allowedValues.join(QStringLiteral(", ")))));
        }
    }

    const QSet<QString> knownOptions = command.metadata.catalogOptionNames;
    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        const QString optionToken = optionEntry.key;
        const QString normalizedOption = QStringLiteral("-") + normalizedCommandOptionName(optionToken);
        if (!knownOptions.isEmpty() && !isKnownCatalogOption(knownOptions, normalizedOption, catalog)) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          optionEntry.optionTokenIndex,
                                                          QStringLiteral("unknown-option"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unknown option"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Command `%1` does not list option `%2` in the Therion command catalog.")
                                                              .arg(commandName, optionToken)));
        }

        const QString arity = command.metadata.catalogOptionValueArityTokens.value(normalizedOption);
        const int fixedArity = command.metadata.catalogOptionFixedArityByName.value(normalizedOption, -1);
        const int providedValueCount = optionEntry.logicalValueCount;
        if ((optionArityRequiresValue(arity) || fixedArity > 0) && providedValueCount == 0) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          optionEntry.optionTokenIndex,
                                                          QStringLiteral("missing-option-value"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Missing option value"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Option `%1` on command `%2` expects a value.")
                                                              .arg(optionToken, commandName),
                                                          TherionSourceDiagnosticSeverity::Error));
        } else if (fixedArity > 0 && providedValueCount > 0 && providedValueCount != fixedArity) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          optionEntry.optionTokenIndex,
                                                          QStringLiteral("wrong-option-value-count"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unexpected option value count"),
                                                          QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Option `%1` on command `%2` expects exactly %3 value(s), but %4 provided.")
                                                              .arg(optionToken)
                                                              .arg(commandName)
                                                              .arg(fixedArity)
                                                              .arg(providedValueCount),
                                                          TherionSourceDiagnosticSeverity::Error));
        }

        const QStringList allowedOptionValues =
            command.metadata.catalogOptionAllowedValuesByName.value(normalizedOption);
        if (!allowedOptionValues.isEmpty()
            && optionEntry.logicalValueCount == 1
            && optionEntry.rawValueTokens.size() == 1) {
            const QString valueToken = optionEntry.rawValueTokens.constFirst().trimmed();
            if (!valueToken.isEmpty()
                && !allowedOptionValues.contains(valueToken, Qt::CaseInsensitive)) {
                const int diagnosticTokenIndex = optionEntry.embeddedValue
                    ? optionEntry.optionTokenIndex
                    : optionEntry.firstValueTokenIndex;
                result->diagnostics.append(diagnosticForToken(command,
                                                              diagnosticTokenIndex,
                                                              QStringLiteral("unknown-option-value"),
                                                              QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unknown option value"),
                                                              QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Option `%1` on command `%2` does not list `%3` as a known value. Known values: %4.")
                                                                  .arg(normalizedOption, commandName, valueToken, allowedOptionValues.join(QStringLiteral(", ")))));
            }
        }
    }
}

void appendBlockDiagnostics(TherionSourceValidationResult *result,
                            const TherionSourceDocument &sourceDocument)
{
    if (result == nullptr) {
        return;
    }

    const QHash<QString, QString> openToClose = therionOpenToCloseDirectiveMap();
    for (const TherionSourceDocumentLine &line : sourceDocument.lines()) {
        if (!line.hasUnmatchedClose()) {
            continue;
        }
        result->diagnostics.append(diagnosticForLine(line.sourceLine,
                                                     QStringLiteral("unmatched-block-close"),
                                                     QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unmatched block close"),
                                                     QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Closing directive `%1` does not match the currently open block.").arg(line.sourceLine.parsed.directive),
                                                     TherionSourceDiagnosticSeverity::Error));
    }

    for (const TherionSourceBlockFrame &openBlock : sourceDocument.openBlocksAtEnd()) {
        const QString closeDirective = openToClose.value(openBlock.directive);
        TherionSourceDiagnostic diagnostic;
        diagnostic.code = QStringLiteral("unclosed-block");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
        diagnostic.lineNumber = openBlock.lineNumber;
        diagnostic.columnNumber = 1;
        diagnostic.columnLength = openBlock.directive.size();
        diagnostic.title = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unclosed block");
        diagnostic.message = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Block `%1` is not closed before the end of the document. Expected `%2`.")
                                 .arg(openBlock.directive, closeDirective);
        diagnostic.currentText = openBlock.lineText;
        const UnclosedBlockFixPlan fixPlan = unclosedBlockFixPlan(sourceDocument, openBlock, closeDirective);
        if (fixPlan.valid) {
            diagnostic.suggestedText = fixPlan.suggestedText;
            diagnostic.hasFix = true;
            diagnostic.fix.startOffset = fixPlan.startOffset;
            diagnostic.fix.length = 0;
            diagnostic.fix.replacementText = fixPlan.replacementText;
            diagnostic.fix.description = fixPlan.insertionLineNumber > 0
                ? QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Insert %1 before line %2")
                      .arg(closeDirective)
                      .arg(fixPlan.insertionLineNumber)
                : QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Insert %1 at end of file")
                      .arg(closeDirective);
        }
        result->diagnostics.append(diagnostic);
    }
}

bool catalogCommandAllowedInContext(const TherionSourceValidationCatalog &catalog,
                                    const QString &commandName,
                                    const QString &context)
{
    if (!catalog.commandNames.contains(commandName)) {
        return false;
    }

    const QStringList contexts = catalog.commandContexts.value(commandName);
    return contexts.contains(QStringLiteral("all"), Qt::CaseInsensitive)
        || contexts.contains(context, Qt::CaseInsensitive);
}

TherionSourceDiagnostic diagnosticForUnclosedCodeBeforeParentCommand(const TherionSourceDocument &sourceDocument,
                                                                     const TherionSourceBlockFrame &codeBlock,
                                                                     const QString &parentCommand,
                                                                     const QString &parentContext)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("unclosed-block");
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.lineNumber = codeBlock.lineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.columnLength = codeBlock.directive.size();
    diagnostic.title = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Unclosed block");
    diagnostic.message = QCoreApplication::translate("TherionStudio::TherionSourceValidator", "Block `%1` should be closed with `%2` before command `%3` in `%4` context.")
                             .arg(QStringLiteral("code"),
                                  QStringLiteral("endcode"),
                                  parentCommand,
                                  parentContext);
    diagnostic.currentText = codeBlock.lineText;

    const TherionSourceDocumentLine *codeLine = sourceDocument.lineAtLineNumber(codeBlock.lineNumber);
    if (codeLine != nullptr && !codeLine->sourceLine.parsed.tokenSpans.isEmpty()) {
        const TherionParsedToken &token = codeLine->sourceLine.parsed.tokenSpans.constFirst();
        diagnostic.columnNumber = token.start + 1;
        diagnostic.columnLength = token.length;
        diagnostic.currentText = codeLine->sourceLine.text;
    }
    return diagnostic;
}

void appendCodeBoundaryDiagnostics(TherionSourceValidationResult *result,
                                   const TherionSourceDocument &sourceDocument,
                                   const TherionSourceValidationCatalog &catalog)
{
    if (result == nullptr) {
        return;
    }

    QSet<int> reportedCodeOpenLines;
    for (const TherionSourceDocumentLine &line : sourceDocument.lines()) {
        if (line.blockStackBefore.size() < 2
            || line.normalizedDirective.isEmpty()
            || line.normalizedDirective == QStringLiteral("endcode")
            || line.blockStackBefore.constLast().directive != QStringLiteral("code")) {
            continue;
        }

        const QString parentContext = line.blockStackBefore.at(line.blockStackBefore.size() - 2).directive;
        if (!catalogCommandAllowedInContext(catalog, line.normalizedDirective, parentContext)) {
            continue;
        }

        const TherionSourceBlockFrame codeBlock = line.blockStackBefore.constLast();
        if (reportedCodeOpenLines.contains(codeBlock.lineNumber)) {
            continue;
        }
        reportedCodeOpenLines.insert(codeBlock.lineNumber);

        result->diagnostics.append(diagnosticForUnclosedCodeBeforeParentCommand(sourceDocument,
                                                                                codeBlock,
                                                                                line.normalizedDirective,
                                                                                parentContext));
    }
}

void appendEmptyScrapObjectDiagnostics(TherionSourceValidationResult *result,
                                       const TherionSourceDocument &sourceDocument)
{
    if (result == nullptr) {
        return;
    }

    for (const TherionSourceBlockRange &blockRange : sourceDocument.blockRanges()) {
        if (!isScrapObjectBlockDirective(blockRange.directive)
            || !blockRange.isClosed()
            || !blockIsInsideScrap(blockRange)
            || !blockHasOnlyBlankBody(blockRange, sourceDocument)) {
            continue;
        }
        result->diagnostics.append(diagnosticForEmptyScrapObject(blockRange, sourceDocument));
    }
}

void appendDuplicateLinePointSmoothOffDiagnostics(TherionSourceValidationResult *result,
                                                  const TherionSourceDocument &sourceDocument)
{
    if (result == nullptr) {
        return;
    }

    bool inLineBlock = false;
    bool hasCurrentLinePoint = false;
    bool currentLinePointHasSmoothOff = false;
    for (const TherionSourceDocumentLine &line : sourceDocument.lines()) {
        if (line.opensBlock && line.normalizedDirective == QStringLiteral("line")) {
            inLineBlock = true;
            hasCurrentLinePoint = false;
            currentLinePointHasSmoothOff = false;
            continue;
        }
        if (!inLineBlock) {
            continue;
        }
        if (line.closesBlock && line.closeMatchesOpenDirective == QStringLiteral("line")) {
            inLineBlock = false;
            hasCurrentLinePoint = false;
            currentLinePointHasSmoothOff = false;
            continue;
        }
        if (line.role != TherionSourceLineRole::BlockContent || line.sourceLine.parsed.tokens.isEmpty()) {
            continue;
        }

        if (lineContentRowHasCoordinates(line.sourceLine.parsed)) {
            hasCurrentLinePoint = true;
            currentLinePointHasSmoothOff = false;
            continue;
        }

        if (!lineContentRowIsSmoothOff(line.sourceLine.parsed) || !hasCurrentLinePoint) {
            continue;
        }

        if (currentLinePointHasSmoothOff) {
            result->diagnostics.append(diagnosticForDuplicateLinePointSmoothOff(line));
            continue;
        }
        currentLinePointHasSmoothOff = true;
    }
}

TherionSourceValidationResult validateSourceDocuments(const TherionSourceDocument &sourceDocument,
                                                      const TherionSourceLogicalDocument &logicalDocument,
                                                      const TherionSourceValidationCatalog &validationCatalog,
                                                      bool validateCatalog)
{
    TherionSourceValidationResult result;
    appendEmptyScrapObjectDiagnostics(&result, sourceDocument);
    appendDuplicateLinePointSmoothOffDiagnostics(&result, sourceDocument);

    QHash<QString, QSet<QString>> lineIdsByScrapScope;
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        const std::optional<TherionSourceLogicalArgumentRange> idRange = lineObjectIdRange(command);
        if (!idRange.has_value() || idRange->text.trimmed().isEmpty()) {
            continue;
        }
        const QString scopeKey = scrapObjectScopeKey(command.blockStackBefore);
        if (!scopeKey.isEmpty()) {
            lineIdsByScrapScope[scopeKey].insert(idRange->text.trimmed());
        }
    }

    QHash<QString, TherionSourceLogicalArgumentRange> firstExplicitObjectIdByKey;
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        if (command.startLineNumber == command.endLineNumber) {
            const LineCleanupResult cleanup = cleanupLine(command.text);
            if (cleanup.changed && cleanup.text != command.text) {
                result.diagnostics.append(diagnosticForLineCleanup(command, cleanup));
            }
        }

        const std::optional<TherionSourceLogicalArgumentRange> idRange =
            commandObjectIdentityRange(command);
        if (idRange.has_value() && !idRange->text.trimmed().isEmpty()) {
            const std::optional<QString> key = commandDuplicateIdentityKey(command, idRange->text);
            if (key.has_value()) {
                if (firstExplicitObjectIdByKey.contains(*key)) {
                    result.diagnostics.append(diagnosticForDuplicateObjectId(*idRange));
                } else {
                    firstExplicitObjectIdByKey.insert(*key, *idRange);
                }
            }
        }

        if (command.role == TherionSourceLineRole::BlockContent
            && command.currentBlockDirective == QStringLiteral("area")) {
            const QString scopeKey = scrapObjectScopeKey(command.blockStackBefore);
            const QSet<QString> knownLineIds = lineIdsByScrapScope.value(scopeKey);
            for (const TherionSourceLogicalTokenRange &tokenRange : command.tokenRanges) {
                if (tokenRange.type == TherionTokenType::Comment) {
                    continue;
                }
                const QString reference = tokenRange.text.trimmed();
                if (reference.isEmpty() || knownLineIds.contains(reference)) {
                    continue;
                }
                result.diagnostics.append(diagnosticForUnknownAreaLineReference(tokenRange));
            }
        }

        if (validateCatalog && command.role != TherionSourceLineRole::BlockContent) {
            appendPathSeparatorDiagnostics(&result, command);
        }

        if (validateCatalog && command.shouldValidateCommandCatalog()) {
            appendCommandCatalogDiagnostics(&result, command, validationCatalog);
        }
    }

    if (validateCatalog) {
        appendBlockDiagnostics(&result, sourceDocument);
        appendCodeBoundaryDiagnostics(&result, sourceDocument, validationCatalog);
    }

    const QByteArray expectedSourceDigest = sourceDigest(sourceDocument.toText());
    for (TherionSourceDiagnostic &diagnostic : result.diagnostics) {
        if (diagnostic.hasFix) {
            diagnostic.fix.expectedSourceDigest = expectedSourceDigest;
        }
    }
    return result;
}
}

TherionSourceValidationResult TherionSourceValidator::validate(const QString &contents)
{
    return validate(contents, {});
}

TherionSourceValidationResult TherionSourceValidator::validate(const QString &contents,
                                                               const TherionSourceValidationCatalog &catalog)
{
    return validate(contents, catalog, {});
}

TherionSourceValidationResult TherionSourceValidator::validate(const QString &contents,
                                                               const TherionSourceValidationCatalog &catalog,
                                                               const TherionSourceDocumentMetadata &metadata)
{
    TherionSourceSnapshotCache sourceSnapshotCache;
    const TherionSourceDocument &sourceDocument = sourceSnapshotCache.sourceDocument(contents, metadata);
    const TherionSourceLogicalDocument &logicalDocument =
        catalog.commandNames.isEmpty()
            ? sourceSnapshotCache.logicalDocument(contents, metadata)
            : sourceSnapshotCache.logicalDocument(contents,
                                                  catalog,
                                                  metadata,
                                                  TherionSourceSnapshotCatalogKey::fromRevision(metadata.revisionId));
    return validateSourceDocuments(sourceDocument, logicalDocument, catalog, !catalog.commandNames.isEmpty());
}

TherionSourceValidationResult TherionSourceValidator::validate(
    const TherionSourceDocument &sourceDocument,
    const TherionSourceLogicalDocument &logicalDocument,
    const TherionSourceValidationCatalog &catalog)
{
    return validateSourceDocuments(sourceDocument, logicalDocument, catalog, !catalog.commandNames.isEmpty());
}

QVector<TherionSourceTextEdit> TherionSourceValidator::validationFixEdits(
    const QString &contents,
    const QVector<TherionSourceDiagnosticFix> &fixes)
{
    QVector<TherionSourceDiagnosticFix> sortedFixes = fixes;
    std::sort(sortedFixes.begin(),
              sortedFixes.end(),
              [](const TherionSourceDiagnosticFix &left, const TherionSourceDiagnosticFix &right) {
                  return left.startOffset > right.startOffset;
              });

    QVector<TherionSourceTextEdit> edits;
    edits.reserve(sortedFixes.size());
    const QByteArray currentSourceDigest = sourceDigest(contents);
    int nextStartOffset = contents.size() + 1;
    for (const TherionSourceDiagnosticFix &fix : std::as_const(sortedFixes)) {
        if (fix.expectedSourceDigest.isEmpty()
            || fix.expectedSourceDigest != currentSourceDigest
            || fix.startOffset < 0
            || fix.length < 0
            || fix.startOffset + fix.length > contents.size()
            || fix.startOffset + fix.length > nextStartOffset
            || fix.startOffset == nextStartOffset) {
            return {};
        }
        edits.append(TherionSourceTextEdit{fix.startOffset, fix.length, fix.replacementText});
        nextStartOffset = fix.startOffset;
    }
    return edits;
}

QString TherionSourceValidator::applyFixes(const QString &contents,
                                           const QVector<TherionSourceDiagnosticFix> &fixes)
{
    const QVector<TherionSourceTextEdit> edits = validationFixEdits(contents, fixes);

    QString updated = contents;
    for (const TherionSourceTextEdit &edit : edits) {
        updated.replace(edit.startOffset, edit.length, edit.replacementText);
    }
    return updated;
}
}
