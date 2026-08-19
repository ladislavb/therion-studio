#include "TherionCommandLineModel.h"

#include "TherionDocumentParser.h"
#include "TherionCommandSyntax.h"
#include "TherionTokenRules.h"

namespace TherionStudio
{
bool commandTokenActsAsOptionBoundary(const TherionParsedLine &parsedLine, int tokenIndex)
{
    if (tokenIndex < 0 || tokenIndex >= parsedLine.tokens.size()) {
        return false;
    }

    if (tokenIndex < parsedLine.tokenSpans.size()
        && parsedLine.tokenSpans.at(tokenIndex).type == TherionTokenType::QuotedString) {
        return false;
    }

    return commandTokenStartsNewOption(parsedLine.tokens.at(tokenIndex));
}

int nextCommandOptionIndex(const TherionParsedLine &parsedLine, int optionIndex)
{
    bool inBracketedValue = false;
    for (int scan = optionIndex + 1; scan < parsedLine.tokens.size(); ++scan) {
        const QString token = parsedLine.tokens.at(scan).trimmed();
        if (inBracketedValue) {
            if (token.contains(QLatin1Char(']'))) {
                inBracketedValue = false;
            }
            continue;
        }

        if (scan == optionIndex + 1 && token.contains(QLatin1Char('[')) && !token.contains(QLatin1Char(']'))) {
            inBracketedValue = true;
            continue;
        }

        if (commandTokenActsAsOptionBoundary(parsedLine, scan)) {
            return scan;
        }
    }

    return parsedLine.tokens.size();
}

QStringList commandLogicalOptionValueTokens(const TherionParsedLine &parsedLine, int optionIndex)
{
    if (optionIndex < 0 || optionIndex >= parsedLine.tokens.size()) {
        return {};
    }

    const QString optionToken = parsedLine.tokens.at(optionIndex);
    if (commandTokenEmbedsOptionValue(optionToken)) {
        return {commandEmbeddedOptionValue(optionToken)};
    }

    const int nextOptionIndex = nextCommandOptionIndex(parsedLine, optionIndex);
    return nextOptionIndex <= optionIndex + 1
        ? QStringList{}
        : parsedLine.tokens.mid(optionIndex + 1, nextOptionIndex - optionIndex - 1);
}

bool commandTokenStartsNewOption(const QString &token)
{
    if (!TherionTokenRules::tokenStartsOption(token)) {
        return false;
    }

    const QString embeddedName = commandEmbeddedOptionName(token);
    if (embeddedName != token.trimmed()) {
        return looksLikeOptionToken(embeddedName);
    }

    return true;
}

bool commandTokenActsAsOptionBoundary(const QStringList &tokens, int tokenIndex)
{
    if (tokenIndex < 0 || tokenIndex >= tokens.size()) {
        return false;
    }

    return commandTokenStartsNewOption(tokens.at(tokenIndex));
}

QString commandEmbeddedOptionName(const QString &token)
{
    const QString trimmed = token.trimmed();
    for (int index = 0; index < trimmed.size(); ++index) {
        if (trimmed.at(index).isSpace()) {
            return trimmed.left(index);
        }
    }

    return trimmed;
}

QString commandEmbeddedOptionValue(const QString &token)
{
    const QString trimmed = token.trimmed();
    for (int index = 0; index < trimmed.size(); ++index) {
        if (trimmed.at(index).isSpace()) {
            return trimmed.mid(index + 1).trimmed();
        }
    }

    return QString();
}

bool commandTokenEmbedsOptionValue(const QString &token)
{
    return commandTokenStartsNewOption(token)
        && !commandEmbeddedOptionValue(token).isEmpty();
}

QString normalizedCommandOptionName(const QString &optionName)
{
    QString normalized = optionName.trimmed().toLower();
    while (normalized.startsWith(QLatin1Char('-'))) {
        normalized.remove(0, 1);
    }
    return normalized;
}

bool commandOptionNameMatches(const QString &token, const QString &optionName)
{
    const QString normalizedToken = normalizedCommandOptionName(token);
    const QString normalizedOption = normalizedCommandOptionName(optionName);
    return !normalizedToken.isEmpty() && normalizedToken == normalizedOption;
}

int nextCommandOptionIndex(const QStringList &tokens, int optionIndex)
{
    bool inBracketedValue = false;
    for (int scan = optionIndex + 1; scan < tokens.size(); ++scan) {
        const QString token = tokens.at(scan).trimmed();
        if (inBracketedValue) {
            if (token.contains(QLatin1Char(']'))) {
                inBracketedValue = false;
            }
            continue;
        }

        if (scan == optionIndex + 1 && token.contains(QLatin1Char('[')) && !token.contains(QLatin1Char(']'))) {
            inBracketedValue = true;
            continue;
        }

        if (commandTokenActsAsOptionBoundary(tokens, scan)) {
            return scan;
        }
    }

    return tokens.size();
}

QStringList commandLogicalOptionValueTokens(const QStringList &tokens, int optionIndex)
{
    if (optionIndex < 0 || optionIndex >= tokens.size()) {
        return {};
    }

    const QString optionToken = tokens.at(optionIndex);
    if (commandTokenEmbedsOptionValue(optionToken)) {
        return {commandEmbeddedOptionValue(optionToken)};
    }

    const int nextOptionIndex = nextCommandOptionIndex(tokens, optionIndex);
    return nextOptionIndex <= optionIndex + 1
        ? QStringList{}
        : tokens.mid(optionIndex + 1, nextOptionIndex - optionIndex - 1);
}

int commandLogicalOptionValueCount(const QStringList &tokens, int optionIndex)
{
    const QStringList valueTokens = commandLogicalOptionValueTokens(tokens, optionIndex);
    int count = 0;
    for (int index = 0; index < valueTokens.size(); ++index) {
        const QString token = valueTokens.at(index).trimmed();
        if (token == QStringLiteral("\\")) {
            continue;
        }
        if (token.startsWith(QLatin1Char('['))) {
            ++count;
            while (index + 1 < valueTokens.size()
                   && !valueTokens.at(index).contains(QLatin1Char(']'))) {
                ++index;
            }
            continue;
        }
        ++count;
    }
    return count;
}

int commandLogicalOptionValueCount(const TherionParsedLine &parsedLine, int optionIndex)
{
    const QStringList valueTokens = commandLogicalOptionValueTokens(parsedLine, optionIndex);
    int count = 0;
    for (int index = 0; index < valueTokens.size(); ++index) {
        const QString token = valueTokens.at(index).trimmed();
        if (token == QStringLiteral("\\")) {
            continue;
        }
        if (token.startsWith(QLatin1Char('['))) {
            ++count;
            while (index + 1 < valueTokens.size()
                   && !valueTokens.at(index).contains(QLatin1Char(']'))) {
                ++index;
            }
            continue;
        }
        ++count;
    }
    return count;
}

QStringList commandOptionValueTokens(const QStringList &tokens, const QString &optionName)
{
    if (optionName.trimmed().isEmpty()) {
        return {};
    }

    for (int index = 0; index < tokens.size(); ++index) {
        if (!commandTokenActsAsOptionBoundary(tokens, index)) {
            continue;
        }
        if (!commandOptionNameMatches(commandEmbeddedOptionName(tokens.at(index)), optionName)) {
            continue;
        }

        return commandLogicalOptionValueTokens(tokens, index);
    }

    return {};
}

QString commandOptionValue(const QStringList &tokens, const QString &optionName)
{
    const QStringList values = commandOptionValueTokens(tokens, optionName);
    return values.isEmpty() ? QString() : values.constFirst().trimmed();
}

QHash<QString, QString> commandOptionValuesByName(const QStringList &tokens)
{
    QHash<QString, QString> values;
    for (int index = 0; index < tokens.size(); ++index) {
        const QString token = tokens.at(index).trimmed();
        if (!commandTokenActsAsOptionBoundary(tokens, index)) {
            continue;
        }

        const QString fieldName = normalizedCommandOptionName(commandEmbeddedOptionName(token));
        if (fieldName.isEmpty()) {
            continue;
        }

        const int nextOptionIndex = nextCommandOptionIndex(tokens, index);
        const QStringList rawOptionValues = commandLogicalOptionValueTokens(tokens, index);
        if (rawOptionValues.isEmpty()) {
            continue;
        }

        values.insert(fieldName, rawOptionValues.join(QLatin1Char(' ')).trimmed());
        index = nextOptionIndex - 1;
    }
    return values;
}

QString commandOptionValue(const TherionParsedLine &parsedLine, const QString &optionName)
{
    if (optionName.trimmed().isEmpty()) {
        return {};
    }

    for (int index = 0; index < parsedLine.tokens.size(); ++index) {
        if (!commandTokenActsAsOptionBoundary(parsedLine, index)) {
            continue;
        }
        if (!commandOptionNameMatches(commandEmbeddedOptionName(parsedLine.tokens.at(index)), optionName)) {
            continue;
        }

        const QStringList values = commandLogicalOptionValueTokens(parsedLine, index);
        return values.isEmpty() ? QString() : values.constFirst().trimmed();
    }

    return {};
}

QHash<QString, QString> commandOptionValuesByName(const TherionParsedLine &parsedLine)
{
    QHash<QString, QString> values;
    for (int index = 0; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (!commandTokenActsAsOptionBoundary(parsedLine, index)) {
            continue;
        }

        const QString fieldName = normalizedCommandOptionName(commandEmbeddedOptionName(token));
        if (fieldName.isEmpty()) {
            continue;
        }

        const int nextOptionIndex = nextCommandOptionIndex(parsedLine, index);
        const QStringList rawOptionValues = commandLogicalOptionValueTokens(parsedLine, index);
        if (rawOptionValues.isEmpty()) {
            continue;
        }

        values.insert(fieldName, rawOptionValues.join(QLatin1Char(' ')).trimmed());
        index = nextOptionIndex - 1;
    }
    return values;
}

namespace
{
CommandOptionEntry commandOptionEntry(const QString &commandName,
                                      const QStringList &tokens,
                                      int optionTokenIndex,
                                      const QString &token,
                                      const QStringList &rawOptionValues,
                                      int nextTokenIndex,
                                      const QHash<QString, int> &commandOptionFixedArityByKey)
{
    QString optionKey = token.trimmed();
    QString optionDisplayValue;
    const QString embeddedValue = commandEmbeddedOptionValue(optionKey);
    const bool embedded = !embeddedValue.isEmpty();
    if (!embeddedValue.isEmpty()) {
        optionDisplayValue = embeddedValue;
        optionKey = commandEmbeddedOptionName(optionKey);
    } else {
        optionDisplayValue = rawOptionValues.join(QLatin1Char(' '));
        const int fixedArity = commandOptionFixedArityByKey.value(
            commandOptionValueKey(commandName, optionKey.toLower().trimmed()), -1);
        if (fixedArity > 1 && !rawOptionValues.isEmpty()) {
            optionDisplayValue = serializeCommandArgumentValues(rawOptionValues);
        }
    }

    const int firstValueIndex = embedded || rawOptionValues.isEmpty() ? -1 : optionTokenIndex + 1;
    const int lastValueIndex = embedded || rawOptionValues.isEmpty() ? -1 : nextTokenIndex - 1;
    CommandOptionEntry entry;
    entry.key = optionKey;
    entry.value = optionDisplayValue;
    entry.optionTokenIndex = optionTokenIndex;
    entry.firstValueTokenIndex = firstValueIndex;
    entry.lastValueTokenIndex = lastValueIndex;
    entry.nextTokenIndex = nextTokenIndex;
    entry.rawValueTokens = rawOptionValues;
    entry.embeddedValue = embedded;
    entry.logicalValueCount = commandLogicalOptionValueCount(tokens, optionTokenIndex);
    return entry;
}

QString commandOptionEntryDeduplicationKey(const CommandOptionEntry &entry)
{
    return normalizedCommandOptionName(entry.key)
        + QLatin1Char('\n')
        + entry.value.trimmed();
}

std::optional<bool> parseCommandToggleToken(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized == QStringLiteral("on")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("1")) {
        return true;
    }
    if (normalized == QStringLiteral("off")
        || normalized == QStringLiteral("no")
        || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("0")) {
        return false;
    }

    return std::nullopt;
}
}

std::optional<bool> commandOptionToggleValue(const QStringList &tokens, const QString &optionName)
{
    std::optional<bool> value;
    if (optionName.trimmed().isEmpty()) {
        return value;
    }

    for (int index = 0; index < tokens.size(); ++index) {
        if (!commandOptionNameMatches(commandEmbeddedOptionName(tokens.at(index)), optionName)) {
            continue;
        }

        const int nextOptionIndex = nextCommandOptionIndex(tokens, index);
        const QStringList values = commandLogicalOptionValueTokens(tokens, index);
        if (values.isEmpty()) {
            value = true;
        } else {
            value = parseCommandToggleToken(values.constFirst()).value_or(true);
        }
        index = nextOptionIndex - 1;
    }

    return value;
}

QString serializeCommandArgumentValues(const QStringList &values)
{
    QStringList serializedValues;
    serializedValues.reserve(values.size());
    for (const QString &value : values) {
        serializedValues.append(serializeTherionArgumentToken(value.trimmed()));
    }
    return serializedValues.join(QLatin1Char(' '));
}

QStringList serializeCommandOptionTokens(const QString &optionToken, const QStringList &values)
{
    QStringList tokens;
    const QString key = optionToken.trimmed();
    if (key.isEmpty()) {
        return tokens;
    }

    tokens.append(key);
    if (!values.isEmpty()) {
        tokens.append(serializeCommandArgumentValues(values));
    }
    return tokens;
}

ParsedCommandOptions parseCommandOptions(
    const QString &commandName,
    const QStringList &tokens,
    const QHash<QString, int> &commandOptionFixedArityByKey,
    bool leadingValueAllowed,
    bool collapseDuplicateEntries)
{
    ParsedCommandOptions parsed;
    QStringList seenOptionEntries;
    if (leadingValueAllowed
        && tokens.size() > 1
        && !tokens.at(1).trimmed().startsWith(QLatin1Char('-'))) {
        parsed.leadingValue = tokens.at(1).trimmed();
        parsed.optionsStartIndex = 2;
    }

    for (int index = parsed.optionsStartIndex; index < tokens.size();) {
        const QString token = tokens.at(index).trimmed();
        if (commandTokenActsAsOptionBoundary(tokens, index)) {
            const int nextOptionIndex = nextCommandOptionIndex(tokens, index);
            const QStringList rawOptionValues = commandLogicalOptionValueTokens(tokens, index);
            const CommandOptionEntry entry = commandOptionEntry(commandName,
                                                               tokens,
                                                               index,
                                                               token,
                                                               rawOptionValues,
                                                               nextOptionIndex,
                                                               commandOptionFixedArityByKey);
            const QString deduplicationKey = commandOptionEntryDeduplicationKey(entry);
            if (!collapseDuplicateEntries || !seenOptionEntries.contains(deduplicationKey)) {
                parsed.optionEntries.append(entry);
                seenOptionEntries.append(deduplicationKey);
            }
            index = nextOptionIndex;
            continue;
        }

        parsed.extraPositionalTokens.append(token);
        ++index;
    }

    return parsed;
}

ParsedCommandOptions parseCommandOptions(
    const QString &commandName,
    const TherionParsedLine &parsedLine,
    const QHash<QString, int> &commandOptionFixedArityByKey,
    bool leadingValueAllowed,
    bool collapseDuplicateEntries)
{
    ParsedCommandOptions parsed;
    QStringList seenOptionEntries;
    const QStringList &tokens = parsedLine.tokens;
    if (leadingValueAllowed
        && tokens.size() > 1
        && !commandTokenActsAsOptionBoundary(parsedLine, 1)) {
        parsed.leadingValue = tokens.at(1).trimmed();
        parsed.optionsStartIndex = 2;
    }

    for (int index = parsed.optionsStartIndex; index < tokens.size();) {
        const QString token = tokens.at(index).trimmed();
        if (commandTokenActsAsOptionBoundary(parsedLine, index)) {
            const int nextOptionIndex = nextCommandOptionIndex(parsedLine, index);
            const QStringList rawOptionValues = commandLogicalOptionValueTokens(parsedLine, index);
            CommandOptionEntry entry = commandOptionEntry(commandName,
                                                          tokens,
                                                          index,
                                                          token,
                                                          rawOptionValues,
                                                          nextOptionIndex,
                                                          commandOptionFixedArityByKey);
            entry.logicalValueCount = commandLogicalOptionValueCount(parsedLine, index);
            const QString deduplicationKey = commandOptionEntryDeduplicationKey(entry);
            if (!collapseDuplicateEntries || !seenOptionEntries.contains(deduplicationKey)) {
                parsed.optionEntries.append(entry);
                seenOptionEntries.append(deduplicationKey);
            }
            index = nextOptionIndex;
            continue;
        }

        parsed.extraPositionalTokens.append(token);
        ++index;
    }

    return parsed;
}
}
