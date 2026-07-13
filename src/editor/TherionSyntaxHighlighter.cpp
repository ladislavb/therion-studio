#include "TherionSyntaxHighlighter.h"

#include "ValidationSeverityStyle.h"
#include "../core/TherionCommandLineModel.h"
#include "../core/TherionCommandSyntax.h"
#include "../core/TherionTokenRules.h"

#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextDocument>

#include <utility>

namespace TherionStudio
{
namespace
{
QTextCharFormat makeFormat(const QColor &foreground, bool bold = false, bool italic = false)
{
    QTextCharFormat format;
    format.setForeground(foreground);
    format.setFontWeight(bold ? QFont::Bold : QFont::Normal);
    format.setFontItalic(italic);
    return format;
}

QTextCharFormat makeValidationFormat(TherionSourceDiagnosticSeverity severity, const QPalette &palette)
{
    QTextCharFormat format = makeFormat(validationSeverityForeground(severity, palette), true);
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    format.setUnderlineColor(validationSeverityAccent(severity, palette));
    format.setBackground(validationSeverityBackground(severity, palette, 0.18));
    return format;
}

bool applicationUsesDarkSyntaxPalette()
{
    const QColor baseColor = QGuiApplication::palette().color(QPalette::Base);
    return baseColor.isValid() && baseColor.lightnessF() < 0.5;
}

QSet<QString> extractOptionKeys(const QString &optionKeyField)
{
    QSet<QString> keys;
    const QStringList segments = optionKeyField.split(QRegularExpression(QStringLiteral("[,/]")),
                                                      Qt::SkipEmptyParts);
    static const QRegularExpression keyPattern(QStringLiteral("^-[A-Za-z][A-Za-z0-9-]*$"));
    for (QString segment : segments) {
        segment = segment.trimmed().toLower();
        if (segment.isEmpty() || !segment.startsWith(QLatin1Char('-'))) {
            continue;
        }
        if (segment.contains(QLatin1Char('<')) || segment.contains(QLatin1Char('>'))) {
            continue;
        }
        if (!keyPattern.match(segment).hasMatch()) {
            continue;
        }
        keys.insert(segment);
    }
    return keys;
}

QSet<QString> extractOptionKeys(const QString &optionKeyField, const QString &signature)
{
    QSet<QString> keys = extractOptionKeys(optionKeyField);
    for (const QString &aliasKey : extractOptionKeysFromSignatureAliases(signature)) {
        keys.insert(aliasKey.trimmed().toLower());
    }
    return keys;
}

QSet<QString> lowerSetFromArray(const QJsonArray &values)
{
    QSet<QString> lowered;
    for (const QJsonValue &value : values) {
        const QString normalized = value.toString().trimmed().toLower();
        if (!normalized.isEmpty()) {
            lowered.insert(normalized);
        }
    }
    return lowered;
}

QSet<QString> catalogTokenSetFromCommandObject(const QJsonObject &commandObject, const QString &fieldName)
{
    QSet<QString> tokens = lowerSetFromArray(commandObject.value(fieldName).toArray());
    if (tokens.contains(QStringLiteral("source"))) {
        tokens.remove(QStringLiteral("source"));
        tokens.insert(QStringLiteral("th"));
    }
    if (tokens.contains(QStringLiteral("map"))) {
        tokens.remove(QStringLiteral("map"));
        tokens.insert(QStringLiteral("th2"));
    }
    if (tokens.contains(QStringLiteral("config"))) {
        tokens.remove(QStringLiteral("config"));
        tokens.insert(QStringLiteral("thconfig"));
    }
    return tokens;
}

QSet<QString> documentTypesFromCommandObject(const QJsonObject &commandObject)
{
    QSet<QString> documentTypes = catalogTokenSetFromCommandObject(commandObject, QStringLiteral("document_types"));
    documentTypes.unite(catalogTokenSetFromCommandObject(commandObject, QStringLiteral("source_types")));
    return documentTypes;
}

QStringList contextsFromCommandObject(const QJsonObject &commandObject)
{
    QStringList contexts;
    const QSet<QString> values = lowerSetFromArray(commandObject.value(QStringLiteral("contexts")).toArray());
    contexts.reserve(values.size());
    for (const QString &value : values) {
        contexts.append(value);
    }
    contexts.sort(Qt::CaseInsensitive);
    return contexts;
}

QStringList sortedStringListFromSet(const QSet<QString> &values)
{
    QStringList sorted;
    sorted.reserve(values.size());
    for (const QString &value : values) {
        sorted.append(value);
    }
    sorted.sort(Qt::CaseInsensitive);
    return sorted;
}

QSet<QString> extractClosingDirectiveTokens(const QJsonArray &syntaxArray)
{
    QSet<QString> closingTokens;
    static const QRegularExpression closingPattern(QStringLiteral("\\bend[a-z][a-z0-9-]*\\b"),
                                                   QRegularExpression::CaseInsensitiveOption);
    for (const QJsonValue &syntaxValue : syntaxArray) {
        const QString syntaxText = syntaxValue.toString();
        auto matchIterator = closingPattern.globalMatch(syntaxText);
        while (matchIterator.hasNext()) {
            const QRegularExpressionMatch match = matchIterator.next();
            const QString token = match.captured(0).trimmed().toLower();
            if (!token.isEmpty()) {
                closingTokens.insert(token);
            }
        }
    }
    return closingTokens;
}

QSet<QString> extractClosingDirectiveIdTokens(const QJsonArray &syntaxArray)
{
    QSet<QString> closingIdTokens;
    static const QRegularExpression closingWithIdPattern(
        QStringLiteral("\\b(end[a-z][a-z0-9-]*)\\s+\\[<[^>]*\\bid\\b[^>]*>\\]"),
        QRegularExpression::CaseInsensitiveOption);
    for (const QJsonValue &syntaxValue : syntaxArray) {
        const QString syntaxText = syntaxValue.toString();
        auto matchIterator = closingWithIdPattern.globalMatch(syntaxText);
        while (matchIterator.hasNext()) {
            const QRegularExpressionMatch match = matchIterator.next();
            const QString token = match.captured(1).trimmed().toLower();
            if (!token.isEmpty()) {
                closingIdTokens.insert(token);
            }
        }
    }
    return closingIdTokens;
}

bool textContainsIdPlaceholder(const QString &text)
{
    static const QRegularExpression idPlaceholderPattern(
        QStringLiteral("<[^>]*\\bid\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    return idPlaceholderPattern.match(text).hasMatch();
}
}

TherionSyntaxHighlighter::TherionSyntaxHighlighter(CommandCatalogStore catalogStore, QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , catalogStore_(std::move(catalogStore))
{
    loadPalette();
}

void TherionSyntaxHighlighter::reloadPaletteForApplicationAppearance()
{
    loadPalette();
    rehighlight();
}

void TherionSyntaxHighlighter::setSourceDocumentType(TherionSourceDocumentType sourceType)
{
    if (sourceDocumentType_ == sourceType) {
        return;
    }

    sourceDocumentType_ = sourceType;
    cachedValidationResult_ = {};
    cachedValidationRevision_ = -1;
    sourceSnapshotCache_.clear();
    rehighlight();
}

void TherionSyntaxHighlighter::loadPalette()
{
    keywordTokens_.clear();
    commandTokens_.clear();
    commandOptionTokens_.clear();
    commandAllowedValues_.clear();
    commandOptionValueArity_.clear();
    commandPositionalIdTokenIndexes_.clear();
    commandOptionIdValueTokens_.clear();
    closingDirectiveIdTokens_.clear();
    validationCatalog_ = {};
    cachedValidationResult_ = {};
    cachedValidationRevision_ = -1;
    sourceSnapshotCache_.clear();

    const bool darkPalette = applicationUsesDarkSyntaxPalette();
    baseTextFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#D4D4D4")) : QColor(QStringLiteral("#24292f")));
    keywordFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#569CD6")) : QColor(QStringLiteral("#0550ae")), true);
    optionFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#9CDCFE")) : QColor(QStringLiteral("#0969da")));
    identifierFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#DCDCAA")) : QColor(QStringLiteral("#8250df")));
    validationWarningFormat_ =
        makeValidationFormat(TherionSourceDiagnosticSeverity::Warning, QGuiApplication::palette());
    validationErrorFormat_ =
        makeValidationFormat(TherionSourceDiagnosticSeverity::Error, QGuiApplication::palette());
    stringFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#CE9178")) : QColor(QStringLiteral("#0a3069")));
    numberFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#B5CEA8")) : QColor(QStringLiteral("#116329")));
    commentFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#6A9955")) : QColor(QStringLiteral("#6e7781")), false, true);

    QFile paletteFile(QStringLiteral(":/resources/therion_syntax_palette.json"));
    if (!paletteFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(paletteFile.readAll());
    if (!document.isObject()) {
        return;
    }

    const QJsonObject rootObject = document.object();
    QJsonObject stylesObject = rootObject.value(darkPalette ? QStringLiteral("darkStyles") : QStringLiteral("styles")).toObject();
    if (stylesObject.isEmpty()) {
        stylesObject = rootObject.value(QStringLiteral("styles")).toObject();
    }

    applyStyle(QStringLiteral("baseText"), stylesObject.value(QStringLiteral("baseText")).toObject());
    applyStyle(QStringLiteral("keyword"), stylesObject.value(QStringLiteral("keyword")).toObject());
    applyStyle(QStringLiteral("option"), stylesObject.value(QStringLiteral("option")).toObject());
    applyStyle(QStringLiteral("identifier"), stylesObject.value(QStringLiteral("identifier")).toObject());
    applyStyle(QStringLiteral("string"), stylesObject.value(QStringLiteral("string")).toObject());
    applyStyle(QStringLiteral("number"), stylesObject.value(QStringLiteral("number")).toObject());
    applyStyle(QStringLiteral("comment"), stylesObject.value(QStringLiteral("comment")).toObject());

    const QJsonArray keywordsArray = rootObject.value(QStringLiteral("additionalKeywords")).toArray();
    for (const QJsonValue &value : keywordsArray) {
        const QString keyword = value.toString().trimmed();
        if (keyword.isEmpty()) {
            continue;
        }

        keywordTokens_.insert(keyword.toLower());
    }

    loadCommandCatalogKeywords();
}

void TherionSyntaxHighlighter::applyStyle(const QString &styleName, const QJsonObject &styleObject)
{
    const QColor foreground(styleObject.value(QStringLiteral("foreground")).toString());
    const bool bold = styleObject.value(QStringLiteral("bold")).toBool(false);
    const bool italic = styleObject.value(QStringLiteral("italic")).toBool(false);
    const QTextCharFormat format = makeFormat(foreground.isValid() ? foreground : QColor(QStringLiteral("#D4D4D4")), bold, italic);

    if (styleName == QStringLiteral("baseText")) {
        baseTextFormat_ = format;
    } else if (styleName == QStringLiteral("keyword")) {
        keywordFormat_ = format;
    } else if (styleName == QStringLiteral("option")) {
        optionFormat_ = format;
    } else if (styleName == QStringLiteral("identifier")) {
        identifierFormat_ = format;
    } else if (styleName == QStringLiteral("string")) {
        stringFormat_ = format;
    } else if (styleName == QStringLiteral("number")) {
        numberFormat_ = format;
    } else if (styleName == QStringLiteral("comment")) {
        commentFormat_ = format;
    }
}

void TherionSyntaxHighlighter::highlightBlock(const QString &text)
{
    setFormat(0, text.length(), baseTextFormat_);

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(text);
    const TherionParsedLine parsedLine = sourceDocument.lines().isEmpty()
        ? TherionParsedLine{}
        : sourceDocument.lines().first().sourceLine.parsed;
    const QString directive = parsedLine.directive.trimmed().toLower();
    const bool knownCommand = commandTokens_.contains(directive);
    const QSet<QString> commandAllowedValues = commandAllowedValues_.value(directive);
    const QSet<QString> commandOptions = commandOptionTokens_.value(directive);
    const QHash<QString, QString> optionValueArity = commandOptionValueArity_.value(directive);
    const QSet<int> idPositionalIndexes = commandPositionalIdTokenIndexes_.value(directive);
    const QSet<QString> idOptionTokens = commandOptionIdValueTokens_.value(directive);
    const bool closingDirectiveAllowsId = closingDirectiveIdTokens_.contains(directive);

    QString activeOptionToken;
    QString activeOptionArity;
    bool activeOptionExpectsId = false;

    int tokenIndex = 0;
    for (int index = 0; index < parsedLine.tokenSpans.size(); ++index) {
        const TherionParsedToken &tokenSpan = parsedLine.tokenSpans.at(index);
        if (tokenSpan.type == TherionTokenType::Comment) {
            setFormat(tokenSpan.start, tokenSpan.length, commentFormat_);
            continue;
        }

        const QString normalizedToken = tokenSpan.text.trimmed().toLower();
        const bool optionLikeToken = looksLikeOptionToken(normalizedToken);
        bool identifierToken = false;
        if (knownCommand && tokenIndex > 0 && !optionLikeToken && idPositionalIndexes.contains(tokenIndex)) {
            identifierToken = true;
        }
        if (!knownCommand && closingDirectiveAllowsId && tokenIndex == 1 && !optionLikeToken) {
            identifierToken = true;
        }
        bool formatted = false;

        if (tokenIndex == 0 && keywordTokens_.contains(normalizedToken)) {
            setFormat(tokenSpan.start, tokenSpan.length, keywordFormat_);
            formatted = true;
        } else if (knownCommand && tokenIndex > 0 && optionLikeToken) {
            if (commandOptions.contains(normalizedToken)) {
                setFormat(tokenSpan.start, tokenSpan.length, optionFormat_);
                formatted = true;

                activeOptionToken = normalizedToken;
                activeOptionArity = optionValueArity.value(normalizedToken).trimmed().toUpper();
                activeOptionExpectsId = idOptionTokens.contains(normalizedToken);

                if (activeOptionArity == QStringLiteral("0")) {
                    activeOptionToken.clear();
                    activeOptionArity.clear();
                    activeOptionExpectsId = false;
                }
            } else {
                activeOptionToken.clear();
                activeOptionArity.clear();
                activeOptionExpectsId = false;
            }
        } else {
            if (knownCommand && !activeOptionToken.isEmpty()) {
                if (activeOptionExpectsId) {
                    identifierToken = true;
                }

                if (activeOptionArity != QStringLiteral("N")) {
                    activeOptionToken.clear();
                    activeOptionArity.clear();
                    activeOptionExpectsId = false;
                }
            }
        }

        if (!formatted) {
            if (tokenSpan.type == TherionTokenType::QuotedString) {
                setFormat(tokenSpan.start, tokenSpan.length, stringFormat_);
            } else if (identifierToken) {
                setFormat(tokenSpan.start, tokenSpan.length, identifierFormat_);
            } else if (knownCommand && tokenIndex > 0 && commandAllowedValues.contains(normalizedToken)) {
                setFormat(tokenSpan.start, tokenSpan.length, optionFormat_);
            } else if (keywordTokens_.contains(normalizedToken)) {
                setFormat(tokenSpan.start, tokenSpan.length, keywordFormat_);
            } else if (TherionTokenRules::isNumericToken(tokenSpan.text,
                                                         TherionTokenRules::NumericTokenContext::SyntaxToken)) {
                setFormat(tokenSpan.start, tokenSpan.length, numberFormat_);
            }
        }

        ++tokenIndex;
    }

    applyValidatorInvalidTokenFormats(text);
}

void TherionSyntaxHighlighter::loadCommandCatalogKeywords()
{
    const QJsonObject catalogObject = catalogStore_.catalogObject();
    if (catalogObject.isEmpty()) {
        return;
    }

    const QJsonArray commandsArray = catalogObject.value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &commandValue : commandsArray) {
        const QJsonObject commandObject = commandValue.toObject();
        const QString commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        const QSet<QString> commandAllowedValues = lowerSetFromArray(commandObject.value(QStringLiteral("allowed_values")).toArray());
        const QSet<QString> commandDocumentTypes = documentTypesFromCommandObject(commandObject);
        const QStringList commandContexts = contextsFromCommandObject(commandObject);
        if (!commandName.isEmpty()) {
            commandTokens_.insert(commandName);
            keywordTokens_.insert(commandName);
            validationCatalog_.commandNames.insert(commandName);
            if (!commandDocumentTypes.isEmpty()) {
                validationCatalog_.commandDocumentTypes.insert(commandName, commandDocumentTypes);
            }
            if (!commandContexts.isEmpty()) {
                validationCatalog_.commandContexts.insert(commandName, commandContexts);
            }
            if (!commandAllowedValues.isEmpty()) {
                commandAllowedValues_.insert(commandName, commandAllowedValues);
                validationCatalog_.commandArgumentAllowedValuesByKey.insert(commandArgumentValueKey(commandName, 0),
                                                                            sortedStringListFromSet(commandAllowedValues));
            }
        }

        const QSet<QString> closingDirectiveTokens = extractClosingDirectiveTokens(commandObject.value(QStringLiteral("syntax")).toArray());
        const QSet<QString> closingDirectiveIdTokens = extractClosingDirectiveIdTokens(commandObject.value(QStringLiteral("syntax")).toArray());
        keywordTokens_.unite(closingDirectiveTokens);
        closingDirectiveIdTokens_.unite(closingDirectiveIdTokens);

        QSet<QString> optionTokens;
        QHash<QString, QString> optionArities;
        QHash<QString, QSet<QString>> optionEnumValues;
        QSet<QString> optionIdTokens;
        const QJsonArray optionsArray = commandObject.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &optionValue : optionsArray) {
            const QJsonObject optionObject = optionValue.toObject();
            const QString optionKeyField = optionObject.value(QStringLiteral("option_key")).toString().trimmed();
            const QString signature = optionObject.value(QStringLiteral("signature")).toString().trimmed();
            const QSet<QString> optionKeys = extractOptionKeys(optionKeyField, signature);
            if (optionKeys.isEmpty()) {
                continue;
            }

            optionTokens.unite(optionKeys);
            const QString arity = optionObject.value(QStringLiteral("value_arity")).toString().trimmed().toUpper();
            const QSet<QString> allowedValues = lowerSetFromArray(optionObject.value(QStringLiteral("allowed_values")).toArray());
            const bool optionExpectsId = optionKeys.contains(QStringLiteral("-id"))
                || textContainsIdPlaceholder(optionObject.value(QStringLiteral("signature")).toString())
                || textContainsIdPlaceholder(optionObject.value(QStringLiteral("name")).toString())
                || textContainsIdPlaceholder(optionObject.value(QStringLiteral("raw")).toString());
            for (const QString &optionKey : optionKeys) {
                if (!arity.isEmpty()) {
                    optionArities.insert(optionKey, arity);
                }
                if (!allowedValues.isEmpty()) {
                    optionEnumValues.insert(optionKey, allowedValues);
                }
                if (optionExpectsId) {
                    optionIdTokens.insert(optionKey);
                }
            }
        }

        QSet<int> positionalIdIndexes;
        QHash<int, QSet<QString>> argumentEnumValues;
        int positionalIndex = 1;
        int argumentValueIndex = 0;
        const QJsonArray argumentsArray = commandObject.value(QStringLiteral("arguments")).toArray();
        bool hasVariadicPositionalArgument = false;
        for (const QJsonValue &argumentValue : argumentsArray) {
            const QJsonObject argumentObject = argumentValue.toObject();
            if (textContainsIdPlaceholder(argumentObject.value(QStringLiteral("signature")).toString())
                || textContainsIdPlaceholder(argumentObject.value(QStringLiteral("name")).toString())
                || textContainsIdPlaceholder(argumentObject.value(QStringLiteral("raw")).toString())) {
                positionalIdIndexes.insert(positionalIndex);
            }
            if (argumentObject.value(QStringLiteral("value_arity")).toString().trimmed().toUpper() == QStringLiteral("N")) {
                hasVariadicPositionalArgument = true;
            }
            const QSet<QString> argumentAllowedValues = lowerSetFromArray(argumentObject.value(QStringLiteral("allowed_values")).toArray());
            if (!argumentAllowedValues.isEmpty() && !commandName.isEmpty()) {
                argumentEnumValues.insert(argumentValueIndex, argumentAllowedValues);
                validationCatalog_.commandArgumentAllowedValuesByKey.insert(
                    commandArgumentValueKey(commandName, argumentValueIndex),
                    sortedStringListFromSet(argumentAllowedValues));
            }
            ++positionalIndex;
            ++argumentValueIndex;
        }

        const QJsonObject subtypeByTypeObject = commandObject.value(QStringLiteral("subtype_by_type")).toObject();
        for (auto subtypeIterator = subtypeByTypeObject.begin(); subtypeIterator != subtypeByTypeObject.end(); ++subtypeIterator) {
            const QString typeToken = subtypeIterator.key().trimmed().toLower();
            if (typeToken.isEmpty()) {
                continue;
            }
            validationCatalog_.commandSubtypeValuesByTypeKey.insert(commandSubtypeValueKey(commandName, typeToken),
                                                                    sortedStringListFromSet(lowerSetFromArray(subtypeIterator.value().toArray())));
        }

        if (!commandName.isEmpty()) {
            commandOptionTokens_.insert(commandName, optionTokens);
            commandOptionValueArity_.insert(commandName, optionArities);
            commandPositionalIdTokenIndexes_.insert(commandName, positionalIdIndexes);
            commandOptionIdValueTokens_.insert(commandName, optionIdTokens);
            validationCatalog_.commandOptionNames.insert(commandName, optionTokens);
            for (auto arityIt = optionArities.cbegin(); arityIt != optionArities.cend(); ++arityIt) {
                validationCatalog_.commandOptionValueArityTokens.insert(commandOptionValueKey(commandName, arityIt.key()),
                                                                        arityIt.value());
            }
            for (auto enumIt = optionEnumValues.cbegin(); enumIt != optionEnumValues.cend(); ++enumIt) {
                validationCatalog_.commandOptionAllowedValuesByKey.insert(commandOptionValueKey(commandName, enumIt.key()),
                                                                          sortedStringListFromSet(enumIt.value()));
            }
            if (!hasVariadicPositionalArgument) {
                validationCatalog_.commandMaxPositionalCount.insert(commandName, argumentsArray.size());
            }
        }

        const QJsonArray aliasesArray = commandObject.value(QStringLiteral("aliases")).toArray();
        for (const QJsonValue &aliasValue : aliasesArray) {
            const QString alias = aliasValue.toString().trimmed().toLower();
            if (!alias.isEmpty()) {
                commandTokens_.insert(alias);
                keywordTokens_.insert(alias);
                validationCatalog_.commandNames.insert(alias);
                if (!commandDocumentTypes.isEmpty()) {
                    validationCatalog_.commandDocumentTypes.insert(alias, commandDocumentTypes);
                }
                if (!commandContexts.isEmpty()) {
                    validationCatalog_.commandContexts.insert(alias, commandContexts);
                }
                for (const QString &closingToken : closingDirectiveTokens) {
                    keywordTokens_.insert(closingToken);
                    if (!commandName.isEmpty()) {
                        const QString commandClosingPrefix = QStringLiteral("end") + commandName;
                        if (closingToken == commandClosingPrefix) {
                            const QString aliasClosingToken = QStringLiteral("end") + alias;
                            keywordTokens_.insert(aliasClosingToken);
                            if (closingDirectiveIdTokens.contains(closingToken)) {
                                closingDirectiveIdTokens_.insert(aliasClosingToken);
                            }
                        }
                    }
                }
                if (!commandAllowedValues.isEmpty()) {
                    commandAllowedValues_.insert(alias, commandAllowedValues);
                    validationCatalog_.commandArgumentAllowedValuesByKey.insert(commandArgumentValueKey(alias, 0),
                                                                                sortedStringListFromSet(commandAllowedValues));
                }
                for (auto argumentEnumIt = argumentEnumValues.cbegin(); argumentEnumIt != argumentEnumValues.cend(); ++argumentEnumIt) {
                    validationCatalog_.commandArgumentAllowedValuesByKey.insert(
                        commandArgumentValueKey(alias, argumentEnumIt.key()),
                        sortedStringListFromSet(argumentEnumIt.value()));
                }
                if (!hasVariadicPositionalArgument) {
                    validationCatalog_.commandMaxPositionalCount.insert(alias, argumentsArray.size());
                }
                commandOptionTokens_.insert(alias, optionTokens);
                commandOptionValueArity_.insert(alias, optionArities);
                commandPositionalIdTokenIndexes_.insert(alias, positionalIdIndexes);
                commandOptionIdValueTokens_.insert(alias, optionIdTokens);
                validationCatalog_.commandOptionNames.insert(alias, optionTokens);
                for (auto subtypeIterator = subtypeByTypeObject.begin(); subtypeIterator != subtypeByTypeObject.end(); ++subtypeIterator) {
                    const QString typeToken = subtypeIterator.key().trimmed().toLower();
                    if (typeToken.isEmpty()) {
                        continue;
                    }
                    validationCatalog_.commandSubtypeValuesByTypeKey.insert(commandSubtypeValueKey(alias, typeToken),
                                                                            sortedStringListFromSet(lowerSetFromArray(subtypeIterator.value().toArray())));
                }
                for (auto arityIt = optionArities.cbegin(); arityIt != optionArities.cend(); ++arityIt) {
                    validationCatalog_.commandOptionValueArityTokens.insert(commandOptionValueKey(alias, arityIt.key()),
                                                                            arityIt.value());
                }
                for (auto enumIt = optionEnumValues.cbegin(); enumIt != optionEnumValues.cend(); ++enumIt) {
                    validationCatalog_.commandOptionAllowedValuesByKey.insert(commandOptionValueKey(alias, enumIt.key()),
                                                                              sortedStringListFromSet(enumIt.value()));
                }
            }
        }
    }
}

void TherionSyntaxHighlighter::applyValidatorInvalidTokenFormats(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return;
    }

    const QTextBlock block = currentBlock();
    if (!block.isValid()) {
        return;
    }

    const int lineNumber = block.blockNumber() + 1;
    auto applyDiagnostic = [this, &text, lineNumber](const TherionSourceDiagnostic &diagnostic) {
        if (diagnostic.lineNumber != lineNumber) {
            return;
        }
        if (diagnostic.columnLength <= 0) {
            return;
        }

        const int start = qMax(0, diagnostic.columnNumber - 1);
        if (start >= text.size()) {
            return;
        }

        const int length = qMin(diagnostic.columnLength, text.size() - start);
        if (length <= 0) {
            return;
        }

        setFormat(start, length, validationFormatForSeverity(diagnostic.severity));
    };

    const TherionSourceValidationResult &validation = cachedValidationResult();
    for (const TherionSourceDiagnostic &diagnostic : validation.diagnostics) {
        const QString code = diagnostic.code;
        if (code != QStringLiteral("malformed-option-token")
            && code != QStringLiteral("unknown-option")
            && code != QStringLiteral("missing-option-value")
            && code != QStringLiteral("wrong-option-value-count")
            && code != QStringLiteral("unknown-option-value")
            && code != QStringLiteral("unknown-argument-value")
            && code != QStringLiteral("extra-argument")
            && code != QStringLiteral("invalid-command-context")
            && code != QStringLiteral("invalid-document-type")
            && code != QStringLiteral("duplicate-object-id")
            && code != QStringLiteral("unclosed-block")
            && code != QStringLiteral("unknown-command")
            && code != QStringLiteral("unknown-area-line-reference")) {
            continue;
        }

        applyDiagnostic(diagnostic);
    }

    for (const TherionSourceDiagnostic &diagnostic : externalDiagnostics_) {
        applyDiagnostic(diagnostic);
    }
}

void TherionSyntaxHighlighter::setExternalDiagnostics(const QVector<TherionSourceDiagnostic> &diagnostics)
{
    externalDiagnostics_ = diagnostics;
    rehighlight();
}

const QTextCharFormat &TherionSyntaxHighlighter::validationFormatForSeverity(
    TherionSourceDiagnosticSeverity severity) const
{
    switch (severity) {
    case TherionSourceDiagnosticSeverity::Error:
        return validationErrorFormat_;
    case TherionSourceDiagnosticSeverity::Warning:
        return validationWarningFormat_;
    }
    return validationWarningFormat_;
}

const TherionSourceValidationResult &TherionSyntaxHighlighter::cachedValidationResult()
{
    if (document() == nullptr) {
        static const TherionSourceValidationResult emptyResult;
        return emptyResult;
    }

    const int revision = document()->revision();
    if (revision != cachedValidationRevision_) {
        const QString text = document()->toPlainText();
        TherionSourceDocumentMetadata metadata;
        metadata.sourceType = sourceDocumentType_;
        metadata.revisionId = revision;
        const TherionSourceDocument &sourceDocument =
            sourceSnapshotCache_.sourceDocument(text, metadata);
        const TherionSourceLogicalDocument &logicalDocument =
            sourceSnapshotCache_.logicalDocument(text,
                                                 validationCatalog_,
                                                 metadata,
                                                 TherionSourceSnapshotCatalogKey::fromRevision(revision));
        cachedValidationResult_ = TherionSourceValidator::validate(sourceDocument,
                                                                   logicalDocument,
                                                                   validationCatalog_);
        cachedValidationRevision_ = revision;
    }
    return cachedValidationResult_;
}
}
