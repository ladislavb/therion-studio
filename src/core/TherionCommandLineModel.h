#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace TherionStudio
{
struct TherionParsedLine;

struct CommandOptionEntry
{
    QString key;
    QString value;
    int optionTokenIndex = -1;
    int firstValueTokenIndex = -1;
    int lastValueTokenIndex = -1;
    int nextTokenIndex = -1;
    QStringList rawValueTokens;
    bool embeddedValue = false;
    int logicalValueCount = 0;
};

struct ParsedCommandOptions
{
    QString leadingValue;
    QStringList extraPositionalTokens;
    QVector<CommandOptionEntry> optionEntries;
    int optionsStartIndex = 1;
};

ParsedCommandOptions parseCommandOptions(const QString &commandName,
                                         const QStringList &tokens,
                                         const QHash<QString, int> &commandOptionFixedArityByKey,
                                         bool leadingValueAllowed,
                                         bool collapseDuplicateEntries = true);
ParsedCommandOptions parseCommandOptions(const QString &commandName,
                                         const TherionParsedLine &parsedLine,
                                         const QHash<QString, int> &commandOptionFixedArityByKey,
                                         bool leadingValueAllowed,
                                         bool collapseDuplicateEntries = true);

bool commandTokenStartsNewOption(const QString &token);
bool commandTokenActsAsOptionBoundary(const QStringList &tokens, int tokenIndex);
bool commandTokenActsAsOptionBoundary(const TherionParsedLine &parsedLine, int tokenIndex);
bool commandTokenEmbedsOptionValue(const QString &token);
QString commandEmbeddedOptionName(const QString &token);
QString commandEmbeddedOptionValue(const QString &token);
int nextCommandOptionIndex(const QStringList &tokens, int optionIndex);
int nextCommandOptionIndex(const TherionParsedLine &parsedLine, int optionIndex);
QStringList commandLogicalOptionValueTokens(const QStringList &tokens, int optionIndex);
QStringList commandLogicalOptionValueTokens(const TherionParsedLine &parsedLine, int optionIndex);
int commandLogicalOptionValueCount(const QStringList &tokens, int optionIndex);
int commandLogicalOptionValueCount(const TherionParsedLine &parsedLine, int optionIndex);
QString normalizedCommandOptionName(const QString &optionName);
bool commandOptionNameMatches(const QString &token, const QString &optionName);
QStringList commandOptionValueTokens(const QStringList &tokens, const QString &optionName);
QString commandOptionValue(const QStringList &tokens, const QString &optionName);
QHash<QString, QString> commandOptionValuesByName(const QStringList &tokens);
QString commandOptionValue(const TherionParsedLine &parsedLine, const QString &optionName);
QHash<QString, QString> commandOptionValuesByName(const TherionParsedLine &parsedLine);
std::optional<bool> commandOptionToggleValue(const QStringList &tokens, const QString &optionName);
QString serializeCommandArgumentValues(const QStringList &values);
QStringList serializeCommandOptionTokens(const QString &optionToken, const QStringList &values);
}
