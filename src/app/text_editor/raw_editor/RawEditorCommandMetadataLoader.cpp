#include "RawEditorCommandMetadataLoader.h"

#include "../TextEditorCommandMetadata.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringListModel>

#include "../../../core/TherionCommandSyntax.h"

#include <algorithm>

namespace
{
void appendUnique(QStringList &target, const QString &value)
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

void appendUniqueList(QStringList &target, const QStringList &values)
{
    for (const QString &value : values) {
        appendUnique(target, value);
    }
}

bool isRequiredArgumentSignature(const QString &signature)
{
    const QString trimmed = signature.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (trimmed.startsWith(QLatin1Char('['))) {
        return false;
    }
    return trimmed.contains(QLatin1Char('<')) && trimmed.contains(QLatin1Char('>'));
}

QString normalizedCompletionContextToken(const QString &token)
{
    QString normalized = token.trimmed().toLower();
    if (normalized == QStringLiteral("centreline")) {
        normalized = QStringLiteral("centerline");
    }
    if (normalized == QStringLiteral("endcentreline")) {
        normalized = QStringLiteral("endcenterline");
    }
    if (normalized == QStringLiteral("all")) {
        return QStringLiteral("all");
    }
    if (normalized == QStringLiteral("none")) {
        return QStringLiteral("none");
    }
    static const QRegularExpression contextTokenPattern(QStringLiteral("^[a-z][a-z0-9-]*$"));
    if (contextTokenPattern.match(normalized).hasMatch()) {
        return normalized;
    }
    return QString();
}

QStringList optionArgumentLabelsFromSignature(const QString &signature)
{
    QStringList labels;
    static const QRegularExpression placeholderPattern(QStringLiteral(R"(<([^>]+)>)"));
    QRegularExpressionMatchIterator iterator = placeholderPattern.globalMatch(signature);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QString placeholder = match.captured(1).trimmed();
        if (placeholder.isEmpty()) {
            continue;
        }
        QString label = placeholder.toLower();
        QStringList words = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (QString &word : words) {
            if (!word.isEmpty()) {
                word[0] = word.at(0).toUpper();
            }
        }
        label = words.join(QLatin1Char(' '));
        if (!label.isEmpty()) {
            labels.append(label);
        }
    }
    return labels;
}

QString normalizedDocumentTypeToken(const QString &token)
{
    QString normalized = token.trimmed().toLower();
    if (normalized.startsWith(QLatin1Char('.'))) {
        normalized.remove(0, 1);
    }
    if (normalized == QStringLiteral("therion-source")
        || normalized == QStringLiteral("therion source")
        || normalized == QStringLiteral("source")) {
        return QStringLiteral("th");
    }
    if (normalized == QStringLiteral("therion-map")
        || normalized == QStringLiteral("therion map")
        || normalized == QStringLiteral("map")) {
        return QStringLiteral("th2");
    }
    if (normalized == QStringLiteral("therion-config")
        || normalized == QStringLiteral("therion config")
        || normalized == QStringLiteral("config")) {
        return QStringLiteral("thconfig");
    }
    if (normalized == QStringLiteral("th")
        || normalized == QStringLiteral("th2")
        || normalized == QStringLiteral("thconfig")
        || normalized == QStringLiteral("all")) {
        return normalized;
    }
    return QString();
}

}

namespace TherionStudio
{
RawEditorCommandMetadataLoader::RawEditorCommandMetadataLoader(RawEditorCommandMetadataContext context)
    : context_(std::move(context))
{
}

TextEditorCommandMetadata &RawEditorCommandMetadataLoader::metadata() const
{
    return *context_.metadata;
}

QString RawEditorCommandMetadataLoader::normalizeCompletionContext(const QString &contextToken) const
{
    return normalizedCompletionContextToken(contextToken);
}

void RawEditorCommandMetadataLoader::applyCommandArgumentMetadata(const QString &commandName,
                                                                  const QJsonObject &commandObject,
                                                                  TherionHelpEntry *entry,
                                                                  int *requiredPositionalCount,
                                                                  int *maxPositionalCount,
                                                                  bool *primaryValueIsPerson,
                                                                  QStringList *commandArgumentSignatures) const
{
    if (context_.metadata == nullptr || entry == nullptr || requiredPositionalCount == nullptr || maxPositionalCount == nullptr
        || primaryValueIsPerson == nullptr || commandArgumentSignatures == nullptr) {
        return;
    }

    const QJsonArray argumentsArray = commandObject.value(QStringLiteral("arguments")).toArray();
    bool hasVariadicPositionalArgument = false;
    for (int argumentIndex = 0; argumentIndex < argumentsArray.size(); ++argumentIndex) {
        const QJsonValue value = argumentsArray.at(argumentIndex);
        const QJsonObject argumentObject = value.toObject();
        const QString signature = argumentObject.value(QStringLiteral("signature")).toString().trimmed();
        if (!signature.isEmpty()) {
            commandArgumentSignatures->append(signature);
        }
        const QString description = argumentObject.value(QStringLiteral("description")).toString().trimmed();
        const QString argumentLine = description.isEmpty() ? signature : QStringLiteral("%1 = %2").arg(signature, description);
        appendUnique(entry->arguments, argumentLine);
        if (isRequiredArgumentSignature(signature)) {
            ++(*requiredPositionalCount);
        }
        if (argumentObject.value(QStringLiteral("value_arity")).toString().trimmed().toUpper() == QStringLiteral("N")) {
            hasVariadicPositionalArgument = true;
        }
        if (!(*primaryValueIsPerson)) {
            const QString normalizedSignature = signature.trimmed().toLower();
            if (normalizedSignature.contains(QStringLiteral("<person>"))) {
                *primaryValueIsPerson = true;
            }
        }

        QStringList argumentAllowedValues;
        const QJsonArray argumentAllowedValuesArray = argumentObject.value(QStringLiteral("allowed_values")).toArray();
        for (const QJsonValue &argumentAllowedValue : argumentAllowedValuesArray) {
            appendUnique(argumentAllowedValues, argumentAllowedValue.toString().trimmed().toLower());
        }
        if (!argumentAllowedValues.isEmpty()) {
            appendUniqueList(metadata().commandArgumentValueTokens[commandArgumentValueKey(commandName, argumentIndex)],
                             argumentAllowedValues);
        }
    }
    if (!hasVariadicPositionalArgument) {
        *maxPositionalCount = argumentsArray.size();
    }
}

void RawEditorCommandMetadataLoader::applyCommandContextMetadata(const QString &commandName,
                                                                 const QJsonObject &commandObject,
                                                                 QStringList *normalizedCommandContexts) const
{
    if (context_.metadata == nullptr || normalizedCommandContexts == nullptr) {
        return;
    }

    normalizedCommandContexts->clear();
    const QJsonArray contextsArray = commandObject.value(QStringLiteral("contexts")).toArray();
    for (const QJsonValue &contextValue : contextsArray) {
        const QString contextToken = normalizeCompletionContext(contextValue.toString());
        if (contextToken.isEmpty()) {
            continue;
        }
        appendUnique(*normalizedCommandContexts, contextToken);
        appendUnique(metadata().contextCommandTokens[contextToken], commandName);
    }
    appendUniqueList(metadata().blockCommandContextsByKind[context_.normalizedDirectiveToken(commandName)],
                     *normalizedCommandContexts);

    QStringList inlineCommands;
    const QJsonArray inlineCommandArray = commandObject.value(QStringLiteral("inline_commands")).toArray();
    for (const QJsonValue &inlineCommandValue : inlineCommandArray) {
        appendUnique(inlineCommands, inlineCommandValue.toString());
    }
    if (inlineCommands.isEmpty()) {
        return;
    }

    QStringList targetContexts = *normalizedCommandContexts;
    if (targetContexts.isEmpty()) {
        appendUnique(targetContexts, QStringLiteral("all"));
    }
    for (const QString &inlineCommand : inlineCommands) {
        const QString normalizedInlineCommand = context_.normalizedDirectiveToken(inlineCommand);
        if (normalizedInlineCommand.isEmpty()) {
            continue;
        }
        for (const QString &contextToken : targetContexts) {
            appendUnique(metadata().contextCommandTokens[contextToken], normalizedInlineCommand);
        }
        registerCompletionToken(inlineCommand);
    }
}

void RawEditorCommandMetadataLoader::applyCommandDocumentTypeMetadata(const QString &commandName,
                                                                      const QJsonObject &commandObject) const
{
    if (context_.metadata == nullptr) {
        return;
    }

    auto appendDocumentTypeArray = [&](const QJsonArray &documentTypesArray) {
        for (const QJsonValue &documentTypeValue : documentTypesArray) {
            const QString documentTypeToken = normalizedDocumentTypeToken(documentTypeValue.toString());
            if (!documentTypeToken.isEmpty()) {
                appendUnique(metadata().commandDocumentTypeTokens[commandName], documentTypeToken);
            }
        }
    };

    appendDocumentTypeArray(commandObject.value(QStringLiteral("document_types")).toArray());
    appendDocumentTypeArray(commandObject.value(QStringLiteral("source_types")).toArray());
}

void RawEditorCommandMetadataLoader::applyCommandOptionCatalogMetadata(const QString &commandName,
                                                                       const QJsonObject &commandObject,
                                                                       TherionHelpEntry *entry) const
{
    if (context_.metadata == nullptr || entry == nullptr) {
        return;
    }

    const QJsonArray optionsArray = commandObject.value(QStringLiteral("options")).toArray();
    for (const QJsonValue &value : optionsArray) {
        const QJsonObject optionObject = value.toObject();
        const QString signature = optionObject.value(QStringLiteral("signature")).toString().trimmed();
        // Options Therion accepts that the book does not describe anywhere carry no help text of their own.
        const QString description = optionObject.value(QStringLiteral("described_in_book")).toBool(true)
            ? optionObject.value(QStringLiteral("description")).toString().trimmed()
            : QCoreApplication::translate("TherionStudio::TextEditorTab", "Not described in the Therion book.");
        const QString optionKey = optionObject.value(QStringLiteral("option_key")).toString().trimmed();
        const QString valueArity = canonicalOptionArityToken(optionObject.value(QStringLiteral("value_arity")).toString());
        const QString optionLine = description.isEmpty() ? signature : QStringLiteral("%1 = %2").arg(signature, description);
        appendUnique(entry->options, optionLine);
        QStringList normalizedOptionKeys = extractOptionKeys(optionKey);
        appendUniqueList(normalizedOptionKeys, extractOptionKeysFromSignatureAliases(signature));
        const QStringList optionArgumentLabels = optionArgumentLabelsFromSignature(signature);
        const bool signatureHasEllipsis = signature.contains(QStringLiteral("..."));
        int fixedArity = -1;
        if (valueArity == QStringLiteral("NONE")) {
            fixedArity = 0;
        } else if (valueArity == QStringLiteral("EXACTLY_ONE")) {
            fixedArity = 1;
        } else if (valueArity == QStringLiteral("ONE_OR_MORE")
                   && !signatureHasEllipsis
                   && optionArgumentLabels.size() >= 2) {
            fixedArity = optionArgumentLabels.size();
        }
        QStringList normalizedOptionValues;
        for (const QString &normalizedOptionKey : normalizedOptionKeys) {
            appendUnique(entry->relatedKeywords, normalizedOptionKey);
            appendUnique(metadata().commandOptionTokens[commandName], normalizedOptionKey);
            if (!valueArity.isEmpty()) {
                metadata().commandOptionValueArityTokens.insert(commandOptionValueKey(commandName, normalizedOptionKey), valueArity);
            }
            if (!optionArgumentLabels.isEmpty()) {
                metadata().commandOptionArgumentLabelsByKey.insert(commandOptionValueKey(commandName, normalizedOptionKey),
                                                                   optionArgumentLabels);
            }
            if (fixedArity >= 0) {
                metadata().commandOptionFixedArityByKey.insert(commandOptionValueKey(commandName, normalizedOptionKey),
                                                               fixedArity);
            }
            registerCompletionToken(normalizedOptionKey);
        }
        const QJsonArray optionValues = optionObject.value(QStringLiteral("allowed_values")).toArray();
        for (const QJsonValue &optionValue : optionValues) {
            const QString normalizedValue = optionValue.toString().trimmed();
            appendUnique(normalizedOptionValues, normalizedValue);
        }
        QString optionHelpHtml;
        {
            QStringList html;
            html << QStringLiteral("<p><b>Option:</b><br><b><tt>%1</tt></b></p>")
                        .arg(signature.toHtmlEscaped());
            if (!description.isEmpty()) {
                html << QStringLiteral("<p><b>Description:</b> %1</p>").arg(description.toHtmlEscaped());
            }
            if (!valueArity.isEmpty()) {
                html << QStringLiteral("<p><b>Value Arity:</b> %1</p>").arg(valueArity.toHtmlEscaped());
            }
            if (!normalizedOptionValues.isEmpty()) {
                html << QStringLiteral("<p><b>Accepted Values:</b> %1</p>")
                            .arg(normalizedOptionValues.join(QStringLiteral(", ")).toHtmlEscaped());
            }
            optionHelpHtml = html.join(QString());
        }
        if (!normalizedOptionValues.isEmpty()) {
            for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                appendUniqueList(metadata().commandOptionValueTokens[commandOptionValueKey(commandName, normalizedOptionKey)],
                                 normalizedOptionValues);
            }
        }
        if (!optionHelpHtml.isEmpty()) {
            for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                metadata().commandOptionHelpHtmlByKey.insert(commandOptionHelpKey(commandName, normalizedOptionKey), optionHelpHtml);
            }
        }
    }

    const QJsonArray allowedValuesArray = commandObject.value(QStringLiteral("allowed_values")).toArray();
    for (const QJsonValue &value : allowedValuesArray) {
        appendUnique(entry->acceptedValues, value.toString());
    }

    const QJsonArray typeValuesArray = commandObject.value(QStringLiteral("type_values")).toArray();
    for (const QJsonValue &value : typeValuesArray) {
        appendUnique(metadata().commandTypeValueTokens[commandName], value.toString().trimmed().toLower());
    }

    const QJsonObject subtypeByTypeObject = commandObject.value(QStringLiteral("subtype_by_type")).toObject();
    for (auto subtypeIterator = subtypeByTypeObject.begin(); subtypeIterator != subtypeByTypeObject.end(); ++subtypeIterator) {
        const QString typeKey = subtypeIterator.key().trimmed().toLower();
        if (typeKey.isEmpty()) {
            continue;
        }

        const QJsonArray subtypeArray = subtypeIterator.value().toArray();
        for (const QJsonValue &subtypeValue : subtypeArray) {
            appendUnique(metadata().commandSubtypeByTypeTokens[commandName][typeKey],
                         subtypeValue.toString().trimmed().toLower());
        }
    }
}

void RawEditorCommandMetadataLoader::applyCommandRegistrationMetadata(const QString &commandName,
                                                                      const TherionHelpEntry &entry,
                                                                      int requiredPositionalCount,
                                                                      int maxPositionalCount,
                                                                      bool primaryValueIsPerson,
                                                                      const QStringList &commandArgumentSignatures,
                                                                      const QString &sourceFile) const
{
    if (context_.metadata == nullptr) {
        return;
    }

    appendUnique(metadata().commandCompletionTokens, commandName);
    metadata().commandRequiredPositionalCount.insert(commandName, requiredPositionalCount);
    if (maxPositionalCount >= 0) {
        metadata().commandMaxPositionalCount.insert(commandName, maxPositionalCount);
    }
    if (!commandArgumentSignatures.isEmpty()) {
        metadata().commandArgumentSignaturesByToken.insert(commandName, commandArgumentSignatures);
    }
    metadata().commandPrimaryValueIsPerson.insert(commandName, primaryValueIsPerson);
    if (!sourceFile.trimmed().isEmpty()) {
        metadata().commandSourceFileByToken.insert(commandName, sourceFile.trimmed());
    }
    registerCompletionToken(commandName);
    for (const QString &keyword : entry.relatedKeywords) {
        registerCompletionToken(keyword);
    }
    for (const QString &acceptedValue : entry.acceptedValues) {
        registerCompletionToken(acceptedValue);
        appendUnique(metadata().commandValueTokens[commandName], acceptedValue);
    }

    mergeHelpEntry(commandName, entry);
}

void RawEditorCommandMetadataLoader::applyCommandAliasMetadata(const QString &commandName,
                                                               const QJsonObject &commandObject,
                                                               const TherionHelpEntry &entry,
                                                               const QStringList &normalizedCommandContexts) const
{
    if (context_.metadata == nullptr) {
        return;
    }

    const QJsonArray aliasesArray = commandObject.value(QStringLiteral("aliases")).toArray();
    for (const QJsonValue &aliasValue : aliasesArray) {
        const QString alias = aliasValue.toString().trimmed().toLower();
        if (alias.isEmpty()) {
            continue;
        }
        appendUnique(metadata().commandCompletionTokens, alias);
        metadata().commandRequiredPositionalCount.insert(alias, metadata().commandRequiredPositionalCount.value(commandName));
        if (metadata().commandMaxPositionalCount.contains(commandName)) {
            metadata().commandMaxPositionalCount.insert(alias, metadata().commandMaxPositionalCount.value(commandName));
        }
        appendUniqueList(metadata().commandArgumentSignaturesByToken[alias],
                         metadata().commandArgumentSignaturesByToken.value(commandName));
        metadata().commandPrimaryValueIsPerson.insert(alias, metadata().commandPrimaryValueIsPerson.value(commandName));
        if (metadata().commandSourceFileByToken.contains(commandName)) {
            metadata().commandSourceFileByToken.insert(alias, metadata().commandSourceFileByToken.value(commandName));
        }
        appendUniqueList(metadata().commandDocumentTypeTokens[alias],
                         metadata().commandDocumentTypeTokens.value(commandName));
        registerCompletionToken(alias);
        TherionHelpEntry aliasEntry = entry;
        appendUnique(aliasEntry.relatedKeywords, commandName);
        mergeHelpEntry(alias, aliasEntry);
        appendUniqueList(metadata().commandOptionTokens[alias], metadata().commandOptionTokens.value(commandName));
        appendUniqueList(metadata().commandValueTokens[alias], metadata().commandValueTokens.value(commandName));
        QString argumentPrefix = commandArgumentValueKey(commandName, 0);
        argumentPrefix.chop(1);
        QStringList commandArgumentKeys;
        for (auto argumentIterator = metadata().commandArgumentValueTokens.cbegin();
             argumentIterator != metadata().commandArgumentValueTokens.cend();
             ++argumentIterator) {
            if (argumentIterator.key().startsWith(argumentPrefix)) {
                commandArgumentKeys.append(argumentIterator.key());
            }
        }
        for (const QString &key : commandArgumentKeys) {
            const QString suffix = key.mid(commandName.size());
            appendUniqueList(metadata().commandArgumentValueTokens[alias + suffix],
                             metadata().commandArgumentValueTokens.value(key));
        }
        appendUniqueList(metadata().commandTypeValueTokens[alias], metadata().commandTypeValueTokens.value(commandName));
        for (const QString &optionKey : metadata().commandOptionTokens.value(commandName)) {
            appendUniqueList(metadata().commandOptionValueTokens[commandOptionValueKey(alias, optionKey)],
                             metadata().commandOptionValueTokens.value(commandOptionValueKey(commandName, optionKey)));
            const QString key = commandOptionValueKey(commandName, optionKey);
            const QString aliasKey = commandOptionValueKey(alias, optionKey);
            const QString valueArity = metadata().commandOptionValueArityTokens.value(key);
            if (!valueArity.isEmpty()) {
                metadata().commandOptionValueArityTokens.insert(aliasKey, valueArity);
            }
            const QStringList optionArgumentLabels = metadata().commandOptionArgumentLabelsByKey.value(key);
            if (!optionArgumentLabels.isEmpty()) {
                metadata().commandOptionArgumentLabelsByKey.insert(aliasKey, optionArgumentLabels);
            }
            const int fixedArity = metadata().commandOptionFixedArityByKey.value(key, -1);
            if (fixedArity >= 0) {
                metadata().commandOptionFixedArityByKey.insert(aliasKey, fixedArity);
            }
            const QString optionHelpHtml = metadata().commandOptionHelpHtmlByKey.value(commandOptionHelpKey(commandName, optionKey));
            if (!optionHelpHtml.isEmpty()) {
                metadata().commandOptionHelpHtmlByKey.insert(commandOptionHelpKey(alias, optionKey), optionHelpHtml);
            }
        }
        const QHash<QString, QStringList> subtypeByType = metadata().commandSubtypeByTypeTokens.value(commandName);
        for (auto subtypeIterator = subtypeByType.begin(); subtypeIterator != subtypeByType.end(); ++subtypeIterator) {
            appendUniqueList(metadata().commandSubtypeByTypeTokens[alias][subtypeIterator.key()], subtypeIterator.value());
        }
        for (auto contextIterator = metadata().contextCommandTokens.begin();
             contextIterator != metadata().contextCommandTokens.end();
             ++contextIterator) {
            if (contextIterator.value().contains(commandName, Qt::CaseInsensitive)) {
                appendUnique(contextIterator.value(), alias);
            }
        }
        appendUniqueList(metadata().blockCommandContextsByKind[context_.normalizedDirectiveToken(alias)],
                         normalizedCommandContexts);
    }
}

void RawEditorCommandMetadataLoader::applyCatalogCommandsMetadata(const QJsonObject &catalogObject) const
{
    if (context_.metadata == nullptr || catalogObject.isEmpty() || !context_.normalizedDirectiveToken) {
        return;
    }

    const QJsonArray commands = catalogObject.value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &commandValue : commands) {
        const QJsonObject commandObject = commandValue.toObject();
        const QString commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (commandName.isEmpty()) {
            continue;
        }

        TherionHelpEntry entry;
        entry.summary = commandObject.value(QStringLiteral("summary")).toString().trimmed();
        int requiredPositionalCount = 0;
        int maxPositionalCount = -1;
        QStringList commandArgumentSignatures;
        bool primaryValueIsPerson = false;
        const QString sourceFile = commandObject.value(QStringLiteral("source"))
                                       .toObject()
                                       .value(QStringLiteral("file"))
                                       .toString()
                                       .trimmed();

        const QJsonArray syntaxArray = commandObject.value(QStringLiteral("syntax")).toArray();
        QStringList syntaxRows;
        for (const QJsonValue &value : syntaxArray) {
            appendUnique(syntaxRows, value.toString());
        }
        entry.syntax = syntaxRows.join(QStringLiteral("\n"));

        applyCommandArgumentMetadata(commandName,
                                     commandObject,
                                     &entry,
                                     &requiredPositionalCount,
                                     &maxPositionalCount,
                                     &primaryValueIsPerson,
                                     &commandArgumentSignatures);

        applyCommandOptionCatalogMetadata(commandName, commandObject, &entry);
        appendUnique(entry.relatedKeywords, commandName);

        QStringList normalizedCommandContexts;
        applyCommandContextMetadata(commandName, commandObject, &normalizedCommandContexts);
        applyCommandDocumentTypeMetadata(commandName, commandObject);
        applyCommandRegistrationMetadata(commandName,
                                         entry,
                                         requiredPositionalCount,
                                         maxPositionalCount,
                                         primaryValueIsPerson,
                                         commandArgumentSignatures,
                                         sourceFile);
        applyCommandAliasMetadata(commandName, commandObject, entry, normalizedCommandContexts);
    }
}

void RawEditorCommandMetadataLoader::mergeHelpEntry(const QString &token, const TherionHelpEntry &entry) const
{
    if (context_.metadata == nullptr || token.trimmed().isEmpty()) {
        return;
    }

    const QString normalizedToken = token.toLower();
    TherionHelpEntry merged = metadata().helpEntries.value(normalizedToken);
    if (merged.summary.trimmed().isEmpty() && !entry.summary.trimmed().isEmpty()) {
        merged.summary = entry.summary.trimmed();
    }
    if (merged.syntax.trimmed().isEmpty() && !entry.syntax.trimmed().isEmpty()) {
        merged.syntax = entry.syntax.trimmed();
    }
    appendUniqueList(merged.arguments, entry.arguments);
    appendUniqueList(merged.acceptedValues, entry.acceptedValues);
    appendUniqueList(merged.options, entry.options);
    appendUniqueList(merged.relatedKeywords, entry.relatedKeywords);
    metadata().helpEntries.insert(normalizedToken, merged);
}

void RawEditorCommandMetadataLoader::registerCompletionToken(const QString &token) const
{
    if (context_.metadata == nullptr) {
        return;
    }
    const QString normalized = token.trimmed();
    if (normalized.isEmpty()) {
        return;
    }
    if (normalized.startsWith(QLatin1Char('<')) && normalized.endsWith(QLatin1Char('>'))) {
        return;
    }
    if (normalized.contains(QLatin1Char(' '))) {
        return;
    }
    if (metadata().completionTokens.contains(normalized, Qt::CaseInsensitive)) {
        return;
    }
    metadata().completionTokens.append(normalized);
}

void RawEditorCommandMetadataLoader::rebuildCompletionModel() const
{
    if (context_.metadata == nullptr || context_.completionModel == nullptr) {
        return;
    }

    QStringList sortedTokens = metadata().completionTokens;
    std::sort(sortedTokens.begin(),
              sortedTokens.end(),
              [](const QString &a, const QString &b) {
                  return QString::compare(a, b, Qt::CaseInsensitive) < 0;
              });
    context_.completionModel->setStringList(sortedTokens);
}
}
