#include "TherionCommandSyntax.h"

#include "TherionDocumentParser.h"

#include <QRegularExpression>

namespace
{
QString metadataKeySeparator()
{
    return QString(QChar(0x1f));
}

void appendUniqueCaseInsensitive(QStringList &target, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (target.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    target.append(trimmed);
}
}

namespace TherionStudio
{
QString commandOptionValueKey(const QString &commandName, const QString &optionToken)
{
    return commandName.trimmed().toLower() + metadataKeySeparator()
        + optionToken.trimmed().toLower();
}

QString commandOptionHelpKey(const QString &commandName, const QString &optionToken)
{
    return commandName.trimmed().toLower() + metadataKeySeparator() + QStringLiteral("help")
        + metadataKeySeparator()
        + optionToken.trimmed().toLower();
}

QString commandArgumentValueKey(const QString &commandName, int argumentIndex)
{
    return commandName.trimmed().toLower() + metadataKeySeparator() + QStringLiteral("arg")
        + metadataKeySeparator()
        + QString::number(qMax(argumentIndex, 0));
}

QStringList extractOptionKeys(const QString &optionKeyField)
{
    QStringList keys;
    const QStringList segments = optionKeyField.split(QRegularExpression(QStringLiteral("[,/]")),
                                                      Qt::SkipEmptyParts);
    for (QString segment : segments) {
        segment = segment.trimmed();
        if (!segment.startsWith(QLatin1Char('-'))) {
            continue;
        }
        if (segment.contains(QLatin1Char('<')) || segment.contains(QLatin1Char('>'))) {
            continue;
        }

        static const QRegularExpression keyPattern(QStringLiteral("^-[A-Za-z][A-Za-z0-9-]*$"));
        if (!keyPattern.match(segment).hasMatch()) {
            continue;
        }
        appendUniqueCaseInsensitive(keys, segment);
    }
    return keys;
}

QStringList extractOptionKeysFromSignatureAliases(const QString &signature)
{
    QStringList keys;
    const QStringList signatureTokens = signature.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const QString aliasPart = signatureTokens.isEmpty() ? QString() : signatureTokens.constFirst().trimmed();
    if (!aliasPart.contains(QLatin1Char('/'))) {
        return keys;
    }

    const QStringList aliases = aliasPart.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (QString alias : aliases) {
        alias = alias.trimmed();
        if (alias.isEmpty() || alias.contains(QLatin1Char('<')) || alias.contains(QLatin1Char('>'))) {
            continue;
        }
        if (!alias.startsWith(QLatin1Char('-'))) {
            alias.prepend(QLatin1Char('-'));
        }
        if (looksLikeOptionToken(alias)) {
            appendUniqueCaseInsensitive(keys, alias.toLower());
        }
    }
    return keys;
}

bool looksLikeOptionToken(const QString &token)
{
    static const QRegularExpression keyPattern(QStringLiteral("^-[A-Za-z][A-Za-z0-9-]*$"));
    return keyPattern.match(token.trimmed()).hasMatch();
}

QString normalizeSymbolTypeToken(const QString &token)
{
    QString normalized = token.trimmed().toLower();
    if (normalized.isEmpty()) {
        return QString();
    }
    if (normalized.startsWith(QLatin1Char('-'))) {
        return QString();
    }

    const int separatorIndex = normalized.indexOf(QLatin1Char(':'));
    if (separatorIndex > 0) {
        normalized = normalized.left(separatorIndex).trimmed();
    }
    return normalized;
}

QString commandSubtypeValueKey(const QString &commandName, const QString &typeToken)
{
    return commandName.trimmed().toLower() + metadataKeySeparator() + QStringLiteral("subtype")
        + metadataKeySeparator()
        + normalizeSymbolTypeToken(typeToken);
}

QString symbolTypeForSubtypeLookup(const QString &commandName, const TherionParsedLine &parsedLine)
{
    const QString normalizedCommand = commandName.trimmed().toLower();

    if (normalizedCommand == QStringLiteral("line")
        || normalizedCommand == QStringLiteral("area")) {
        for (int tokenIndex = 1; tokenIndex < parsedLine.tokens.size(); ++tokenIndex) {
            const QString token = parsedLine.tokens.at(tokenIndex).trimmed();
            if (token.startsWith(QLatin1Char('-'))) {
                break;
            }

            const QString normalizedType = normalizeSymbolTypeToken(token);
            if (!normalizedType.isEmpty()) {
                return normalizedType;
            }
        }
        return QString();
    }

    if (normalizedCommand == QStringLiteral("point")) {
        int positionalToken = 0;
        for (int tokenIndex = 1; tokenIndex < parsedLine.tokens.size(); ++tokenIndex) {
            const QString token = parsedLine.tokens.at(tokenIndex).trimmed();
            if (token.isEmpty()) {
                continue;
            }
            if (token.startsWith(QLatin1Char('-'))) {
                break;
            }

            ++positionalToken;
            if (positionalToken == 3) {
                return normalizeSymbolTypeToken(token);
            }
        }
    }

    return QString();
}

void appendConcreteSubtypeValues(QStringList &target, const QStringList &source)
{
    for (const QString &value : source) {
        const QString normalized = value.trimmed();
        if (normalized.isEmpty()) {
            continue;
        }
        if (normalized == QStringLiteral("*")) {
            continue;
        }
        if (normalized.startsWith(QLatin1Char('<')) && normalized.endsWith(QLatin1Char('>'))) {
            continue;
        }

        appendUniqueCaseInsensitive(target, normalized);
    }
}

int positionalTokenCountBeforeCursor(const TherionParsedLine &parsedLine, int tokenIndexExclusive)
{
    int positionalCount = 0;
    for (int index = 1; index < tokenIndexExclusive && index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (token.startsWith(QLatin1Char('-'))) {
            break;
        }
        ++positionalCount;
    }
    return positionalCount;
}

QString canonicalOptionArityToken(const QString &rawArityToken)
{
    const QString normalized = rawArityToken.trimmed().toUpper();
    if (normalized == QStringLiteral("0") || normalized == QStringLiteral("NONE")) {
        return QStringLiteral("NONE");
    }
    if (normalized == QStringLiteral("1") || normalized == QStringLiteral("EXACTLY_ONE")) {
        return QStringLiteral("EXACTLY_ONE");
    }
    if (normalized == QStringLiteral("N") || normalized == QStringLiteral("ONE_OR_MORE")) {
        return QStringLiteral("ONE_OR_MORE");
    }
    if (normalized == QStringLiteral("*") || normalized == QStringLiteral("ZERO_OR_MORE")) {
        return QStringLiteral("ZERO_OR_MORE");
    }
    return normalized;
}

bool optionArityRequiresValue(const QString &rawArityToken)
{
    const QString arity = canonicalOptionArityToken(rawArityToken);
    return arity == QStringLiteral("EXACTLY_ONE") || arity == QStringLiteral("ONE_OR_MORE");
}

bool optionArityForbidsValue(const QString &rawArityToken)
{
    return canonicalOptionArityToken(rawArityToken) == QStringLiteral("NONE");
}

QStringList parseOptionValuesFromEditor(const QString &rawValue, const QString &rawArityToken, int fixedArity)
{
    // Core command syntax tokenizes editor-provided option-value snippets without reparsing documents.
    const QString value = rawValue.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    const QString arity = canonicalOptionArityToken(rawArityToken);
    const bool singleValueExpected = fixedArity == 1
        || (fixedArity < 0 && arity == QStringLiteral("EXACTLY_ONE"));

    if (singleValueExpected) {
        const QStringList tokenized = TherionDocumentParser::tokenizeLine(value);
        if (tokenized.size() == 1 && tokenized.first() != value) {
            return tokenized;
        }
        return {value};
    }

    QStringList values = TherionDocumentParser::tokenizeLine(value);
    if (values.isEmpty()) {
        values.append(value);
    }
    return values;
}

QString quoteTherionArgument(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

bool shouldQuoteTherionArgument(const QString &value)
{
    if (value.isEmpty()) {
        return true;
    }

    static const QRegularExpression requiresQuotePattern(
        QStringLiteral(R"([\s#%"\[\]\\])"));
    return requiresQuotePattern.match(value).hasMatch();
}

bool isBracketedTherionValueGroup(const QString &value)
{
    const QString trimmed = value.trimmed();
    return trimmed.size() >= 2
        && trimmed.startsWith(QLatin1Char('['))
        && trimmed.endsWith(QLatin1Char(']'));
}

QString serializeTherionArgumentToken(const QString &value)
{
    if (isBracketedTherionValueGroup(value)) {
        return value.trimmed();
    }
    if (shouldQuoteTherionArgument(value)) {
        return quoteTherionArgument(value);
    }
    return value;
}
}
