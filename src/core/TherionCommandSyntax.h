#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
struct TherionParsedLine;

QString commandOptionValueKey(const QString &commandName, const QString &optionToken);
QString commandOptionHelpKey(const QString &commandName, const QString &optionToken);
QString commandArgumentValueKey(const QString &commandName, int argumentIndex);
QString commandSubtypeValueKey(const QString &commandName, const QString &typeToken);

QStringList extractOptionKeys(const QString &optionKeyField);
QStringList extractOptionKeysFromSignatureAliases(const QString &signature);
bool looksLikeOptionToken(const QString &token);

QString normalizeSymbolTypeToken(const QString &token);
QString symbolTypeForSubtypeLookup(const QString &commandName, const TherionParsedLine &parsedLine);
void appendConcreteSubtypeValues(QStringList &target, const QStringList &source);
int positionalTokenCountBeforeCursor(const TherionParsedLine &parsedLine, int tokenIndexExclusive);

QString canonicalOptionArityToken(const QString &rawArityToken);
bool optionArityRequiresValue(const QString &rawArityToken);
bool optionArityForbidsValue(const QString &rawArityToken);
QStringList parseOptionValuesFromEditor(const QString &rawValue, const QString &rawArityToken, int fixedArity);

QString quoteTherionArgument(const QString &value);
bool shouldQuoteTherionArgument(const QString &value);
bool isBracketedTherionValueGroup(const QString &value);
// Text of a value as Therion reads it: a bracketed group without its outer brackets.
QString therionValueText(const QString &value);
// The text written as a bracketed group, or empty when its brackets do not balance.
QString bracketedTherionValue(const QString &text);
QString serializeTherionArgumentToken(const QString &value);
}
