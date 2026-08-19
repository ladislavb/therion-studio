#include "../src/core/TherionCommandLineModel.h"
#include "../src/core/TherionCommandSyntax.h"
#include "../src/core/TherionDocumentParser.h"

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <QStringList>

#include <cstdio>
#include <cstdlib>

using TherionStudio::ParsedCommandOptions;
using TherionStudio::commandOptionNameMatches;
using TherionStudio::commandOptionToggleValue;
using TherionStudio::commandOptionValue;
using TherionStudio::commandOptionValuesByName;
using TherionStudio::commandOptionValueKey;
using TherionStudio::commandEmbeddedOptionName;
using TherionStudio::commandEmbeddedOptionValue;
using TherionStudio::commandTokenEmbedsOptionValue;
using TherionStudio::commandTokenStartsNewOption;
using TherionStudio::normalizedCommandOptionName;
using TherionStudio::parseCommandOptions;
using TherionStudio::serializeCommandArgumentValues;
using TherionStudio::serializeCommandOptionTokens;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "CommandOptionParserTest failed: %s\n", message);
        std::exit(1);
    }
}

void parsesMapObjectAttributesAndOptions()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-orientation")), 1);
    arity.insert(commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-text")), 1);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("point"),
                                                            {QStringLiteral("point"),
                                                             QStringLiteral("10.5"),
                                                             QStringLiteral("20.0"),
                                                             QStringLiteral("label"),
                                                             QStringLiteral("-text"),
                                                             QStringLiteral("Entrance"),
                                                             QStringLiteral("-orientation"),
                                                             QStringLiteral("45")},
                                                            arity,
                                                            false);

    require(parsed.leadingValue.isEmpty(), "map object parser must not consume the first point argument as an id");
    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("10.5"), QStringLiteral("20.0"), QStringLiteral("label")}),
            "point positional attributes should be preserved before options");
    require(parsed.optionEntries.size() == 2, "point options should be parsed as option rows");
    require(parsed.optionEntries.at(0).key == QStringLiteral("-text")
                && parsed.optionEntries.at(0).value == QStringLiteral("Entrance"),
            "single-value option should preserve its value");
    require(parsed.optionEntries.at(1).key == QStringLiteral("-orientation")
                && parsed.optionEntries.at(1).value == QStringLiteral("45"),
            "numeric option value should be preserved");
}

void keepsNegativeNumbersAsValues()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-orientation")), 1);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("point"),
                                                            {QStringLiteral("point"),
                                                             QStringLiteral("1"),
                                                             QStringLiteral("2"),
                                                             QStringLiteral("label"),
                                                             QStringLiteral("-orientation"),
                                                             QStringLiteral("-45")},
                                                            arity,
                                                            false);

    require(!commandTokenStartsNewOption(QStringLiteral("-45")), "negative numeric values must not start a new option");
    require(parsed.optionEntries.size() == 1, "negative numeric option value should stay with its option");
    require(parsed.optionEntries.at(0).value == QStringLiteral("-45"), "negative numeric value should be preserved");
}

void keepsBracketedValuesTogether()
{
    QHash<QString, int> arity;

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("revise"),
                                                            {QStringLiteral("revise"),
                                                             QStringLiteral("dd.s@dur.dur_dom"),
                                                             QStringLiteral("-stations"),
                                                             QStringLiteral("[4@monum.dur_dom"),
                                                             QStringLiteral("5@monum.dur_dom"),
                                                             QStringLiteral("6@monum.dur_dom]")},
                                                            arity,
                                                            false);

    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("dd.s@dur.dur_dom")}),
            "revise id should stay as positional argument");
    require(parsed.optionEntries.size() == 1, "bracketed option value should stay in one option row");
    require(parsed.optionEntries.at(0).key == QStringLiteral("-stations"), "revise stations option should be detected");
    require(parsed.optionEntries.at(0).value == QStringLiteral("[4@monum.dur_dom 5@monum.dur_dom 6@monum.dur_dom]"),
            "bracketed stations value should preserve inner spaces");
}

void serializesFixedArityOptionValues()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-clip")), 2);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("line"),
                                                            {QStringLiteral("line"),
                                                             QStringLiteral("wall"),
                                                             QStringLiteral("-clip"),
                                                             QStringLiteral("left wall"),
                                                             QStringLiteral("right")},
                                                            arity,
                                                            false);

    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("wall")}),
            "line type should stay as a positional argument");
    require(parsed.optionEntries.size() == 1, "fixed-arity option should be parsed as one option row");
    require(parsed.optionEntries.at(0).key == QStringLiteral("-clip"), "fixed-arity option key should be preserved");
    require(parsed.optionEntries.at(0).value == QStringLiteral("\"left wall\" right"),
            "fixed-arity values should be serialized with Therion quoting rules");
}

void detectsSingleTokenOptionsWithEmbeddedValues()
{
    require(commandTokenEmbedsOptionValue(QStringLiteral("-clip off")),
            "single token option/value pairs should be detectable");
    require(commandEmbeddedOptionName(QStringLiteral("-clip off")) == QStringLiteral("-clip"),
            "embedded option name should stop before whitespace");
    require(commandEmbeddedOptionValue(QStringLiteral("-clip off")) == QStringLiteral("off"),
            "embedded option value should contain the remainder after whitespace");
    require(!commandTokenEmbedsOptionValue(QStringLiteral("-clip")),
            "plain option tokens should not be reported as embedded option/value pairs");
    require(!commandTokenEmbedsOptionValue(QStringLiteral("left wall")),
            "ordinary spaced values should not be reported as embedded option/value pairs");
    require(!commandTokenStartsNewOption(QStringLiteral("-21 m")),
            "dash-prefixed free-text values should not start a new option when the embedded name is not option-like");
}

void keepsDashPrefixedQuotedTextValuesAsOptionValues()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-text")), 1);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("point"),
                                                            {QStringLiteral("point"),
                                                             QStringLiteral("4505.0"),
                                                             QStringLiteral("-1446.0"),
                                                             QStringLiteral("label"),
                                                             QStringLiteral("-text"),
                                                             QStringLiteral("-21 m")},
                                                            arity,
                                                            false);

    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("4505.0"),
                                                        QStringLiteral("-1446.0"),
                                                        QStringLiteral("label")}),
            "point positional values should allow negative coordinates before text options");
    require(parsed.optionEntries.size() == 1,
            "dash-prefixed text value should stay with the preceding option");
    require(parsed.optionEntries.first().key == QStringLiteral("-text")
                && parsed.optionEntries.first().value == QStringLiteral("-21 m"),
            "dash-prefixed text value should be preserved as the -text option value");
}

void keepsQuotedSingleTokenDashTextValuesAsOptionValues()
{
    const TherionStudio::TherionParsedLine parsed = TherionStudio::TherionDocumentParser::parseLine(
        QStringLiteral("point 4505.0 -1446.0 label -text \"-sump\""));

    require(commandOptionValue(parsed, QStringLiteral("-text")) == QStringLiteral("-sump"),
            "quoted single-token dash-prefixed text should stay with the preceding option");
    require(commandOptionValuesByName(parsed).value(QStringLiteral("text")) == QStringLiteral("-sump"),
            "quoted single-token dash-prefixed text should be available to source projections");
}

void deduplicatesLegacySingleTokenOptionRows()
{
    QHash<QString, int> arity;

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("line"),
                                                            {QStringLiteral("line"),
                                                             QStringLiteral("rock-border"),
                                                             QStringLiteral("-close"),
                                                             QStringLiteral("on"),
                                                             QStringLiteral("-clip"),
                                                             QStringLiteral("off"),
                                                             QStringLiteral("-clip off"),
                                                             QStringLiteral("-clip off")},
                                                            arity,
                                                            false);

    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("rock-border")}),
            "line type should stay as a positional argument when legacy option tokens are present");
    require(parsed.optionEntries.size() == 2,
            "duplicate legacy single-token options should collapse into existing option rows");
    require(parsed.optionEntries.at(0).key == QStringLiteral("-close")
                && parsed.optionEntries.at(0).value == QStringLiteral("on"),
            "close option should be preserved");
    require(parsed.optionEntries.at(1).key == QStringLiteral("-clip")
                && parsed.optionEntries.at(1).value == QStringLiteral("off"),
            "legacy quoted -clip off tokens should normalize to one -clip option row");

    const ParsedCommandOptions rawParsed = parseCommandOptions(QStringLiteral("line"),
                                                               {QStringLiteral("line"),
                                                                QStringLiteral("rock-border"),
                                                                QStringLiteral("-close"),
                                                                QStringLiteral("on"),
                                                                QStringLiteral("-clip"),
                                                                QStringLiteral("off"),
                                                                QStringLiteral("-clip off"),
                                                                QStringLiteral("-clip off")},
                                                               arity,
                                                               false,
                                                               false);
    require(rawParsed.optionEntries.size() == 4,
            "raw option parsing should preserve duplicate legacy single-token option rows for validators");
    require(rawParsed.optionEntries.at(2).key == QStringLiteral("-clip")
                && rawParsed.optionEntries.at(2).value == QStringLiteral("off")
                && rawParsed.optionEntries.at(2).embeddedValue
                && rawParsed.optionEntries.at(2).optionTokenIndex == 6,
            "raw parsing should expose the first duplicate legacy token source range");
    require(rawParsed.optionEntries.at(3).key == QStringLiteral("-clip")
                && rawParsed.optionEntries.at(3).value == QStringLiteral("off")
                && rawParsed.optionEntries.at(3).embeddedValue
                && rawParsed.optionEntries.at(3).optionTokenIndex == 7,
            "raw parsing should expose the second duplicate legacy token source range");
}

void keepsLeadingValueSeparateWhenAllowed()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("scrap"), QStringLiteral("-projection")), 1);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("scrap"),
                                                            {QStringLiteral("scrap"),
                                                             QStringLiteral("s1"),
                                                             QStringLiteral("-projection"),
                                                             QStringLiteral("plan")},
                                                            arity,
                                                            true);

    require(parsed.leadingValue == QStringLiteral("s1"), "leading id value should be separated when allowed");
    require(parsed.optionsStartIndex == 2, "options should start after the leading value");
    require(parsed.extraPositionalTokens.isEmpty(), "leading id should not also appear as a positional token");
    require(parsed.optionEntries.size() == 1
                && parsed.optionEntries.at(0).key == QStringLiteral("-projection")
                && parsed.optionEntries.at(0).value == QStringLiteral("plan"),
            "options after a leading id should still parse normally");
}

void doesNotTreatDashPrefixedTokenAsLeadingValue()
{
    QHash<QString, int> arity;

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("point"),
                                                            {QStringLiteral("point"),
                                                             QStringLiteral("-45"),
                                                             QStringLiteral("12")},
                                                            arity,
                                                            true);

    require(parsed.leadingValue.isEmpty(), "dash-prefixed first argument should not be consumed as a leading id");
    require(parsed.optionsStartIndex == 1, "options should still start after the command token");
    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("-45"), QStringLiteral("12")}),
            "dash-prefixed numeric positional tokens should stay in positional arguments");
}

void serializesCommandArguments()
{
    require(serializeCommandArgumentValues({QStringLiteral("plain"),
                                            QStringLiteral("left wall"),
                                            QStringLiteral("[a b]")})
                == QStringLiteral("plain \"left wall\" [a b]"),
            "command argument serialization should quote values and preserve bracketed groups");
}

void serializesCommandOptionTokens()
{
    require(serializeCommandOptionTokens(QStringLiteral("-clip"),
                                         {QStringLiteral("left wall"), QStringLiteral("right")})
                == QStringList({QStringLiteral("-clip"), QStringLiteral("\"left wall\" right")}),
            "option token serialization should keep the option key and join serialized values");
    require(serializeCommandOptionTokens(QStringLiteral(""), {QStringLiteral("ignored")}).isEmpty(),
            "empty option keys should not emit serialized option tokens");
}

void readsCommandOptionsWithSharedBoundaries()
{
    const QStringList lineTokens({QStringLiteral("line"),
                                  QStringLiteral("rock-border"),
                                  QStringLiteral("-close"),
                                  QStringLiteral("on"),
                                  QStringLiteral("-clip"),
                                  QStringLiteral("off"),
                                  QStringLiteral("-id"),
                                  QStringLiteral("rb-1")});

    require(normalizedCommandOptionName(QStringLiteral("-clip")) == QStringLiteral("clip"),
            "normalized option names should strip leading dashes");
    require(commandOptionNameMatches(QStringLiteral("-clip"), QStringLiteral("clip")),
            "option name matching should ignore leading dashes");
    require(commandOptionValue(lineTokens, QStringLiteral("-clip")) == QStringLiteral("off"),
            "single-value lookup should read -clip off as a normal option value");
    require(commandOptionToggleValue(lineTokens, QStringLiteral("-clip")).value_or(true) == false,
            "toggle lookup should parse -clip off as false");
    require(commandOptionToggleValue(lineTokens, QStringLiteral("close")).value_or(false) == true,
            "toggle lookup should parse -close on as true and accept names without dashes");

    const QHash<QString, QString> values = commandOptionValuesByName(lineTokens);
    require(values.value(QStringLiteral("clip")) == QStringLiteral("off"),
            "option value map should store normalized field names");
    require(values.value(QStringLiteral("id")) == QStringLiteral("rb-1"),
            "option value map should preserve ordinary option values");

    const QStringList corruptedSubtypeTokens({QStringLiteral("line"),
                                              QStringLiteral("rock-border"),
                                              QStringLiteral("-clip"),
                                              QStringLiteral("off"),
                                              QStringLiteral("-subtype"),
                                              QStringLiteral("-clip off")});
    require(commandOptionValue(corruptedSubtypeTokens, QStringLiteral("-subtype")).isEmpty(),
            "option-like quoted subtype values should be treated as a new option boundary");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    parsesMapObjectAttributesAndOptions();
    keepsNegativeNumbersAsValues();
    keepsBracketedValuesTogether();
    serializesFixedArityOptionValues();
    detectsSingleTokenOptionsWithEmbeddedValues();
    keepsDashPrefixedQuotedTextValuesAsOptionValues();
    keepsQuotedSingleTokenDashTextValuesAsOptionValues();
    deduplicatesLegacySingleTokenOptionRows();
    keepsLeadingValueSeparateWhenAllowed();
    doesNotTreatDashPrefixedTokenAsLeadingValue();
    serializesCommandArguments();
    serializesCommandOptionTokens();
    readsCommandOptionsWithSharedBoundaries();
    return 0;
}
