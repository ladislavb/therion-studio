#include "TherionSourceLogicalDocument.h"

#include "TherionCommandLineModel.h"
#include "TherionCommandSyntax.h"
#include "TherionFileTypes.h"

#include <utility>

namespace TherionStudio
{
namespace
{
QString rightTrimmed(QString line)
{
    while (!line.isEmpty() && line.back().isSpace()) {
        line.chop(1);
    }
    return line;
}

bool endsWithContinuationMarker(const QString &line)
{
    const QString trimmed = rightTrimmed(line);
    if (trimmed.isEmpty()) {
        return false;
    }

    int trailingBackslashCount = 0;
    int index = trimmed.size() - 1;
    while (index >= 0 && trimmed.at(index) == QLatin1Char('\\')) {
        ++trailingBackslashCount;
        --index;
    }

    return trailingBackslashCount % 2 == 1;
}

int firstNonSpaceIndex(const QString &line)
{
    for (int index = 0; index < line.size(); ++index) {
        if (!line.at(index).isSpace()) {
            return index;
        }
    }
    return line.size();
}

QString stripTrailingContinuationMarker(const QString &line)
{
    QString trimmed = rightTrimmed(line);
    if (!trimmed.isEmpty() && trimmed.back() == QLatin1Char('\\')) {
        trimmed.chop(1);
    }
    return trimmed;
}

struct LogicalCommandPart
{
    QString text;
    int sourceColumnNumber = 1;
    int sourceStartOffsetDelta = 0;
};

LogicalCommandPart logicalCommandPartForLine(const QString &sourceLineText, bool previousLineContinued)
{
    const bool lineContinues = endsWithContinuationMarker(sourceLineText);
    const QString currentPart = lineContinues
        ? stripTrailingContinuationMarker(sourceLineText)
        : sourceLineText;
    if (!previousLineContinued) {
        return {currentPart, 1, 0};
    }

    const int firstContentIndex = firstNonSpaceIndex(currentPart);
    const QString normalizedPart = currentPart.mid(firstContentIndex).trimmed();
    if (normalizedPart.isEmpty()) {
        return {};
    }
    return {normalizedPart, firstContentIndex + 1, firstContentIndex};
}

TherionSourceLogicalArgumentGroupRange argumentGroupRange(const QVector<TherionSourceLogicalArgumentRange> &arguments)
{
    TherionSourceLogicalArgumentGroupRange group;
    if (arguments.isEmpty()) {
        return group;
    }

    QStringList values;
    values.reserve(arguments.size());
    for (const TherionSourceLogicalArgumentRange &argument : arguments) {
        values.append(argument.text);
    }
    group.firstTokenIndex = arguments.constFirst().tokenIndex;
    group.lastTokenIndex = arguments.constLast().tokenIndex;
    group.text = values.join(QLatin1Char(' '));
    group.argumentRanges = arguments;
    return group;
}

TherionSourcePhysicalRange tokenPhysicalRange(const TherionSourceLogicalCommand &command, int tokenIndex)
{
    TherionSourcePhysicalRange range;
    if (!command.physicalRangeForTokenIndex(tokenIndex, &range)) {
        return {};
    }
    return range;
}

TherionSourceLogicalArgumentRange argumentRangeForToken(const TherionSourceLogicalCommand &command, int tokenIndex)
{
    return TherionSourceLogicalArgumentRange{
        tokenIndex,
        tokenIndex >= 0 && tokenIndex < command.parsed.tokens.size()
            ? command.parsed.tokens.at(tokenIndex)
            : QString(),
        tokenPhysicalRange(command, tokenIndex)};
}

void populateArgumentAndOptionRanges(TherionSourceLogicalCommand *command)
{
    if (command == nullptr || command->parsed.tokens.size() <= 1) {
        return;
    }

    int tokenIndex = 1;
    while (tokenIndex < command->parsed.tokens.size()) {
        const QString token = command->parsed.tokens.at(tokenIndex);
        if (!commandTokenActsAsOptionBoundary(command->parsed, tokenIndex)) {
            command->positionalArgumentRanges.append(argumentRangeForToken(*command, tokenIndex));
            ++tokenIndex;
            continue;
        }

        TherionSourceLogicalOptionEntryRange entry;
        entry.key = commandEmbeddedOptionName(token);
        entry.optionTokenIndex = tokenIndex;
        entry.nextTokenIndex = tokenIndex + 1;
        entry.optionRange = tokenPhysicalRange(*command, tokenIndex);
        entry.embeddedValue = commandTokenEmbedsOptionValue(token);
        if (entry.embeddedValue) {
            entry.rawValueTokens = {commandEmbeddedOptionValue(token)};
            entry.logicalValueCount = commandLogicalOptionValueCount(command->parsed.tokens, tokenIndex);
            entry.valueRanges.append(TherionSourceLogicalArgumentRange{
                tokenIndex,
                commandEmbeddedOptionValue(token),
                entry.optionRange});
            entry.valueGroupRange = argumentGroupRange(entry.valueRanges);
            command->optionEntryRanges.append(entry);
            ++tokenIndex;
            continue;
        }

        const int nextOptionIndex = nextCommandOptionIndex(command->parsed, tokenIndex);
        entry.nextTokenIndex = nextOptionIndex;
        entry.rawValueTokens = commandLogicalOptionValueTokens(command->parsed, tokenIndex);
        entry.logicalValueCount = commandLogicalOptionValueCount(command->parsed, tokenIndex);
        if (!entry.rawValueTokens.isEmpty()) {
            entry.firstValueTokenIndex = tokenIndex + 1;
            entry.lastValueTokenIndex = nextOptionIndex - 1;
        }
        for (int valueIndex = tokenIndex + 1;
             valueIndex < nextOptionIndex && valueIndex < command->parsed.tokens.size();
             ++valueIndex) {
            entry.valueRanges.append(argumentRangeForToken(*command, valueIndex));
        }
        entry.valueGroupRange = argumentGroupRange(entry.valueRanges);
        command->optionEntryRanges.append(entry);
        tokenIndex = qMax(nextOptionIndex, tokenIndex + 1);
    }

    command->positionalArgumentGroupRange = argumentGroupRange(command->positionalArgumentRanges);
}

QString catalogContextForCommand(const TherionSourceLogicalCommand &command)
{
    if (command.blockStackBefore.isEmpty()) {
        return QStringLiteral("none");
    }
    if (command.closesBlock
        && !command.hasUnmatchedClose()
        && command.blockStackBefore.size() > 1) {
        return command.blockStackBefore.at(command.blockStackBefore.size() - 2).directive;
    }
    if (command.closesBlock
        && !command.hasUnmatchedClose()) {
        return QStringLiteral("none");
    }
    return command.blockStackBefore.constLast().directive;
}

void populateCommandMetadata(TherionSourceLogicalCommand *command,
                             TherionSourceDocumentType sourceType,
                             const TherionSourceValidationCatalog *catalog)
{
    if (command == nullptr) {
        return;
    }

    command->metadata.commandName = command->normalizedDirective;
    command->metadata.positionalArgumentCount = command->positionalArgumentRanges.size();
    command->metadata.catalogCurrentContext = catalogContextForCommand(*command);
    command->metadata.catalogCurrentDocumentType = therionSourceDocumentTypeCatalogToken(sourceType);
    command->metadata.normalizedOptionNames.clear();
    command->metadata.optionEntryIndexesByNormalizedName.clear();

    for (int entryIndex = 0; entryIndex < command->optionEntryRanges.size(); ++entryIndex) {
        const TherionSourceLogicalOptionEntryRange &entry = command->optionEntryRanges.at(entryIndex);
        const QString normalizedOptionName =
            QStringLiteral("-") + normalizedCommandOptionName(entry.key);
        if (normalizedOptionName == QStringLiteral("-")) {
            continue;
        }

        command->metadata.normalizedOptionNames.insert(normalizedOptionName);
        command->metadata.optionEntryIndexesByNormalizedName[normalizedOptionName].append(entryIndex);
    }

    if (catalog == nullptr || catalog->commandNames.isEmpty()) {
        return;
    }

    command->metadata.catalogCommandKnown = catalog->commandNames.contains(command->metadata.commandName);
    command->metadata.catalogContexts =
        catalog->commandContexts.value(command->metadata.commandName);
    command->metadata.catalogContextAllowed =
        command->metadata.catalogContexts.contains(QStringLiteral("all"), Qt::CaseInsensitive)
        || command->metadata.catalogContexts.contains(command->metadata.catalogCurrentContext,
                                                      Qt::CaseInsensitive);
    command->metadata.catalogDocumentTypes =
        catalog->commandDocumentTypes.value(command->metadata.commandName);
    command->metadata.catalogDocumentTypeAllowed =
        command->metadata.catalogDocumentTypes.isEmpty()
        || command->metadata.catalogDocumentTypes.contains(QStringLiteral("all"))
        || command->metadata.catalogCurrentDocumentType.isEmpty()
        || command->metadata.catalogDocumentTypes.contains(command->metadata.catalogCurrentDocumentType);
    command->metadata.catalogRequiredPositionalCount =
        qMax(0, catalog->commandRequiredPositionalCount.value(command->metadata.commandName, 0));
    command->metadata.catalogMaxPositionalCount =
        catalog->commandMaxPositionalCount.value(command->metadata.commandName, -1);
    command->metadata.catalogArgumentAllowedValuesByIndex.clear();
    for (int argumentIndex = 0;
         argumentIndex < command->metadata.positionalArgumentCount;
         ++argumentIndex) {
        const QStringList allowedValues = catalog->commandArgumentAllowedValuesByKey.value(
            commandArgumentValueKey(command->metadata.commandName, argumentIndex));
        if (!allowedValues.isEmpty()) {
            command->metadata.catalogArgumentAllowedValuesByIndex.insert(argumentIndex,
                                                                         allowedValues);
        }
    }
    command->metadata.catalogOptionNames =
        catalog->commandOptionNames.value(command->metadata.commandName);

    for (const QString &normalizedOptionName : std::as_const(command->metadata.normalizedOptionNames)) {
        const QString optionKey = commandOptionValueKey(command->metadata.commandName,
                                                        normalizedOptionName);
        command->metadata.catalogOptionValueArityTokens.insert(
            normalizedOptionName,
            catalog->commandOptionValueArityTokens.value(optionKey));
        command->metadata.catalogOptionFixedArityByName.insert(
            normalizedOptionName,
            catalog->commandOptionFixedArityByKey.value(optionKey, -1));

        QStringList allowedValues;
        if (normalizedOptionName == QStringLiteral("-subtype")) {
            const QString symbolTypeToken = symbolTypeForSubtypeLookup(command->metadata.commandName,
                                                                       command->parsed);
            const QStringList subtypeValues = catalog->commandSubtypeValuesByTypeKey.value(
                commandSubtypeValueKey(command->metadata.commandName, symbolTypeToken));
            if (!subtypeValues.contains(QStringLiteral("*"))) {
                appendConcreteSubtypeValues(allowedValues, subtypeValues);
            }
        }
        if (allowedValues.isEmpty()) {
            allowedValues = catalog->commandOptionAllowedValuesByKey.value(optionKey);
        }
        command->metadata.catalogOptionAllowedValuesByName.insert(normalizedOptionName,
                                                                  allowedValues);
    }
}
}

bool TherionSourceLogicalArgumentGroupRange::isValid() const
{
    return firstTokenIndex >= 0 && lastTokenIndex >= firstTokenIndex;
}

bool TherionSourceLogicalCommand::shouldValidateCommandCatalog() const
{
    return role == TherionSourceLineRole::Command;
}

bool TherionSourceLogicalCommand::hasUnmatchedClose() const
{
    if (!closesBlock) {
        return false;
    }
    return blockStackBefore.isEmpty()
        || blockStackBefore.constLast().directive != closeMatchesOpenDirective;
}

bool TherionSourceLogicalCommand::physicalRangeForLogicalRange(int logicalStart,
                                                               int logicalLength,
                                                               TherionSourcePhysicalRange *range) const
{
    if (range != nullptr) {
        *range = {};
    }
    if (logicalStart < 0 || logicalLength <= 0) {
        return false;
    }

    for (const TherionSourceLogicalTextPart &part : textParts) {
        const int partStart = part.logicalStart;
        const int partEnd = part.logicalStart + part.logicalLength;
        if (logicalStart < partStart || logicalStart >= partEnd) {
            continue;
        }

        const int delta = logicalStart - partStart;
        const int mappedLength = qMin(logicalLength, partEnd - logicalStart);
        if (range != nullptr) {
            range->lineNumber = part.lineNumber;
            range->columnNumber = part.columnNumber + delta;
            range->columnLength = mappedLength;
            range->startOffset = part.startOffset + delta;
            range->length = mappedLength;
            range->lineText = part.lineText;
        }
        return true;
    }

    return false;
}

bool TherionSourceLogicalCommand::physicalRangeForTokenIndex(int tokenIndex, TherionSourcePhysicalRange *range) const
{
    if (range != nullptr) {
        *range = {};
    }
    if (tokenIndex < 0) {
        return false;
    }

    for (const TherionSourceLogicalTokenRange &tokenRange : tokenRanges) {
        if (tokenRange.tokenIndex != tokenIndex) {
            continue;
        }
        if (range != nullptr) {
            *range = tokenRange.physicalRange;
        }
        return true;
    }

    return false;
}

TherionSourceLogicalDocument TherionSourceLogicalDocument::fromText(
    const QString &contents,
    const TherionSourceDocumentMetadata &metadata)
{
    return fromSourceDocument(TherionSourceDocument::fromText(contents, metadata));
}

TherionSourceLogicalDocument TherionSourceLogicalDocument::fromText(
    const QString &contents,
    const TherionSourceValidationCatalog &catalog,
    const TherionSourceDocumentMetadata &metadata)
{
    return fromSourceDocument(TherionSourceDocument::fromText(contents, metadata), catalog);
}

TherionSourceLogicalDocument TherionSourceLogicalDocument::fromSourceDocument(const TherionSourceDocument &sourceDocument)
{
    return fromSourceDocument(sourceDocument, nullptr);
}

TherionSourceLogicalDocument TherionSourceLogicalDocument::fromSourceDocument(
    const TherionSourceDocument &sourceDocument,
    const TherionSourceValidationCatalog &catalog)
{
    return fromSourceDocument(sourceDocument, &catalog);
}

TherionSourceLogicalDocument TherionSourceLogicalDocument::fromSourceDocument(
    const TherionSourceDocument &sourceDocument,
    const TherionSourceValidationCatalog *catalog)
{
    TherionSourceLogicalDocument logicalDocument;
    logicalDocument.metadata_ = sourceDocument.metadata();
    const QVector<TherionSourceDocumentLine> &lines = sourceDocument.lines();
    logicalDocument.commands_.reserve(lines.size());

    int lineIndex = 0;
    while (lineIndex < lines.size()) {
        const TherionSourceDocumentLine &firstLine = lines.at(lineIndex);
        TherionSourceLogicalCommand command;
        command.startLineNumber = firstLine.sourceLine.lineNumber;
        command.endLineNumber = firstLine.sourceLine.lineNumber;
        command.startOffset = firstLine.sourceLine.startOffset;
        command.endOffset = firstLine.sourceLine.endOffset;
        command.currentBlockDirective = firstLine.currentBlockDirective;
        command.role = firstLine.role;
        command.opensBlock = firstLine.opensBlock;
        command.closesBlock = firstLine.closesBlock;
        command.closeMatchesOpenDirective = firstLine.closeMatchesOpenDirective;
        command.blockStackBefore = firstLine.blockStackBefore;

        bool previousLineContinued = false;
        int currentIndex = lineIndex;
        while (currentIndex < lines.size()) {
            const TherionSourceDocumentLine &currentLine = lines.at(currentIndex);
            const LogicalCommandPart part = logicalCommandPartForLine(currentLine.sourceLine.text,
                                                                      previousLineContinued);
            if (!part.text.isEmpty()) {
                if (!command.text.isEmpty()) {
                    command.text.append(QLatin1Char(' '));
                }

                TherionSourceLogicalTextPart textPart;
                textPart.logicalStart = command.text.size();
                textPart.logicalLength = part.text.size();
                textPart.lineNumber = currentLine.sourceLine.lineNumber;
                textPart.columnNumber = part.sourceColumnNumber;
                textPart.startOffset = currentLine.sourceLine.startOffset + part.sourceStartOffsetDelta;
                textPart.lineText = currentLine.sourceLine.text;
                command.textParts.append(textPart);

                command.text.append(part.text);
            }
            command.endLineNumber = currentLine.sourceLine.lineNumber;
            command.endOffset = currentLine.sourceLine.endOffset;
            command.physicalLineNumbers.append(currentLine.sourceLine.lineNumber);

            const bool lineContinues = endsWithContinuationMarker(currentLine.sourceLine.text);
            previousLineContinued = lineContinues;
            ++currentIndex;
            if (!lineContinues) {
                break;
            }
        }

        // Logical source construction owns the canonical parseLine call for merged command text.
        command.parsed = TherionDocumentParser::parseLine(command.text, command.startLineNumber);
        command.normalizedDirective = normalizedTherionDirectiveToken(command.parsed.directive);
        if (command.parsed.tokens.isEmpty()) {
            command.normalizedDirective.clear();
        }

        command.tokenRanges.reserve(command.parsed.tokenSpans.size());
        for (int tokenIndex = 0; tokenIndex < command.parsed.tokenSpans.size(); ++tokenIndex) {
            const TherionParsedToken &tokenSpan = command.parsed.tokenSpans.at(tokenIndex);
            TherionSourcePhysicalRange physicalRange;
            if (!command.physicalRangeForLogicalRange(tokenSpan.start, tokenSpan.length, &physicalRange)) {
                continue;
            }

            command.tokenRanges.append(TherionSourceLogicalTokenRange{
                tokenIndex,
                tokenSpan.text,
                tokenSpan.type,
                tokenSpan.start,
                tokenSpan.length,
                physicalRange});
        }
        populateArgumentAndOptionRanges(&command);
        populateCommandMetadata(&command, sourceDocument.sourceType(), catalog);
        logicalDocument.commands_.append(command);
        lineIndex = currentIndex;
    }

    return logicalDocument;
}

const TherionSourceDocumentMetadata &TherionSourceLogicalDocument::metadata() const
{
    return metadata_;
}

const QVector<TherionSourceLogicalCommand> &TherionSourceLogicalDocument::commands() const
{
    return commands_;
}

const TherionSourceLogicalCommand *TherionSourceLogicalDocument::commandAtPhysicalLine(int lineNumber) const
{
    if (lineNumber <= 0) {
        return nullptr;
    }

    for (const TherionSourceLogicalCommand &command : commands_) {
        if (command.startLineNumber <= lineNumber && command.endLineNumber >= lineNumber) {
            return &command;
        }
    }
    return nullptr;
}

const TherionSourceLogicalCommand *TherionSourceLogicalDocument::commandAtOffset(int offset) const
{
    if (offset < 0) {
        return nullptr;
    }

    for (const TherionSourceLogicalCommand &command : commands_) {
        if (offset >= command.startOffset && offset < command.endOffset) {
            return &command;
        }
    }
    return nullptr;
}

const TherionSourceLogicalTokenRange *TherionSourceLogicalDocument::tokenAtPhysicalPosition(int lineNumber,
                                                                                           int columnNumber) const
{
    const TherionSourceLogicalCommand *command = commandAtPhysicalLine(lineNumber);
    if (command == nullptr || columnNumber <= 0) {
        return nullptr;
    }

    for (const TherionSourceLogicalTokenRange &tokenRange : command->tokenRanges) {
        if (tokenRange.type == TherionTokenType::Comment) {
            continue;
        }
        const TherionSourcePhysicalRange &range = tokenRange.physicalRange;
        if (range.lineNumber != lineNumber) {
            continue;
        }
        const int startColumn = range.columnNumber;
        const int endColumn = startColumn + range.columnLength;
        if (columnNumber >= startColumn && columnNumber <= endColumn) {
            return &tokenRange;
        }
    }
    return nullptr;
}

const TherionSourceLogicalTokenRange *TherionSourceLogicalDocument::tokenAtOffset(int offset) const
{
    const TherionSourceLogicalCommand *command = commandAtOffset(offset);
    if (command == nullptr) {
        return nullptr;
    }

    for (const TherionSourceLogicalTokenRange &tokenRange : command->tokenRanges) {
        if (tokenRange.type == TherionTokenType::Comment) {
            continue;
        }
        const TherionSourcePhysicalRange &range = tokenRange.physicalRange;
        if (offset >= range.startOffset && offset < range.startOffset + range.length) {
            return &tokenRange;
        }
    }
    return nullptr;
}
}
