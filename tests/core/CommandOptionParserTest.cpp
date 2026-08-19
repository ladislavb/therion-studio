#include "../../src/core/TherionCommandLineModel.h"
#include "../../src/core/TherionCommandSyntax.h"
#include "../../src/core/TherionDocumentParser.h"

#include <QtTest/QtTest>

using TherionStudio::ParsedCommandOptions;
using TherionStudio::commandEmbeddedOptionName;
using TherionStudio::commandEmbeddedOptionValue;
using TherionStudio::commandOptionNameMatches;
using TherionStudio::commandOptionToggleValue;
using TherionStudio::commandOptionValue;
using TherionStudio::commandOptionValueKey;
using TherionStudio::commandOptionValuesByName;
using TherionStudio::commandTokenEmbedsOptionValue;
using TherionStudio::commandTokenStartsNewOption;
using TherionStudio::normalizedCommandOptionName;
using TherionStudio::parseCommandOptions;
using TherionStudio::serializeCommandArgumentValues;
using TherionStudio::serializeCommandOptionTokens;

class CommandOptionParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesMapObjectAttributesAndOptions();
    void keepsNegativeNumbersAsValues();
    void keepsBracketedValuesTogether();
    void serializesFixedArityOptionValues();
    void detectsSingleTokenOptionsWithEmbeddedValues();
    void keepsDashPrefixedQuotedTextValuesAsOptionValues();
    void keepsQuotedSingleTokenDashTextValuesAsOptionValues();
    void deduplicatesLegacySingleTokenOptionRows();
    void keepsLeadingValueSeparateWhenAllowed();
    void doesNotTreatDashPrefixedTokenAsLeadingValue();
    void serializesCommandArguments();
    void serializesCommandOptionTokens();
    void readsCommandOptionsWithSharedBoundaries();
};

void CommandOptionParserTest::parsesMapObjectAttributesAndOptions()
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

    QVERIFY(parsed.leadingValue.isEmpty());
    QCOMPARE(parsed.extraPositionalTokens,
             QStringList({QStringLiteral("10.5"), QStringLiteral("20.0"), QStringLiteral("label")}));
    QCOMPARE(parsed.optionEntries.size(), 2);
    QCOMPARE(parsed.optionEntries.at(0).key, QStringLiteral("-text"));
    QCOMPARE(parsed.optionEntries.at(0).value, QStringLiteral("Entrance"));
    QCOMPARE(parsed.optionEntries.at(1).key, QStringLiteral("-orientation"));
    QCOMPARE(parsed.optionEntries.at(1).value, QStringLiteral("45"));
}

void CommandOptionParserTest::keepsNegativeNumbersAsValues()
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

    QVERIFY(!commandTokenStartsNewOption(QStringLiteral("-45")));
    QCOMPARE(parsed.optionEntries.size(), 1);
    QCOMPARE(parsed.optionEntries.at(0).value, QStringLiteral("-45"));
}

void CommandOptionParserTest::keepsBracketedValuesTogether()
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

    QCOMPARE(parsed.extraPositionalTokens, QStringList({QStringLiteral("dd.s@dur.dur_dom")}));
    QCOMPARE(parsed.optionEntries.size(), 1);
    QCOMPARE(parsed.optionEntries.at(0).key, QStringLiteral("-stations"));
    QCOMPARE(parsed.optionEntries.at(0).value,
             QStringLiteral("[4@monum.dur_dom 5@monum.dur_dom 6@monum.dur_dom]"));
}

void CommandOptionParserTest::serializesFixedArityOptionValues()
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

    QCOMPARE(parsed.extraPositionalTokens, QStringList({QStringLiteral("wall")}));
    QCOMPARE(parsed.optionEntries.size(), 1);
    QCOMPARE(parsed.optionEntries.at(0).key, QStringLiteral("-clip"));
    QCOMPARE(parsed.optionEntries.at(0).value, QStringLiteral("\"left wall\" right"));
}

void CommandOptionParserTest::detectsSingleTokenOptionsWithEmbeddedValues()
{
    QVERIFY(commandTokenEmbedsOptionValue(QStringLiteral("-clip off")));
    QCOMPARE(commandEmbeddedOptionName(QStringLiteral("-clip off")), QStringLiteral("-clip"));
    QCOMPARE(commandEmbeddedOptionValue(QStringLiteral("-clip off")), QStringLiteral("off"));
    QVERIFY(!commandTokenEmbedsOptionValue(QStringLiteral("-clip")));
    QVERIFY(!commandTokenEmbedsOptionValue(QStringLiteral("left wall")));
    QVERIFY(!commandTokenStartsNewOption(QStringLiteral("-21 m")));
}

void CommandOptionParserTest::keepsDashPrefixedQuotedTextValuesAsOptionValues()
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

    QCOMPARE(parsed.extraPositionalTokens,
             QStringList({QStringLiteral("4505.0"), QStringLiteral("-1446.0"), QStringLiteral("label")}));
    QCOMPARE(parsed.optionEntries.size(), 1);
    QCOMPARE(parsed.optionEntries.first().key, QStringLiteral("-text"));
    QCOMPARE(parsed.optionEntries.first().value, QStringLiteral("-21 m"));
}

void CommandOptionParserTest::keepsQuotedSingleTokenDashTextValuesAsOptionValues()
{
    const TherionStudio::TherionParsedLine parsed = TherionStudio::TherionDocumentParser::parseLine(
        QStringLiteral("point 4505.0 -1446.0 label -text \"-sump\""));

    QCOMPARE(commandOptionValue(parsed, QStringLiteral("-text")), QStringLiteral("-sump"));
    QCOMPARE(commandOptionValuesByName(parsed).value(QStringLiteral("text")), QStringLiteral("-sump"));
}

void CommandOptionParserTest::deduplicatesLegacySingleTokenOptionRows()
{
    QHash<QString, int> arity;
    const QStringList tokens({QStringLiteral("line"),
                              QStringLiteral("rock-border"),
                              QStringLiteral("-close"),
                              QStringLiteral("on"),
                              QStringLiteral("-clip"),
                              QStringLiteral("off"),
                              QStringLiteral("-clip off"),
                              QStringLiteral("-clip off")});

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("line"), tokens, arity, false);
    QCOMPARE(parsed.extraPositionalTokens, QStringList({QStringLiteral("rock-border")}));
    QCOMPARE(parsed.optionEntries.size(), 2);
    QCOMPARE(parsed.optionEntries.at(0).key, QStringLiteral("-close"));
    QCOMPARE(parsed.optionEntries.at(0).value, QStringLiteral("on"));
    QCOMPARE(parsed.optionEntries.at(1).key, QStringLiteral("-clip"));
    QCOMPARE(parsed.optionEntries.at(1).value, QStringLiteral("off"));

    const ParsedCommandOptions rawParsed = parseCommandOptions(QStringLiteral("line"), tokens, arity, false, false);
    QCOMPARE(rawParsed.optionEntries.size(), 4);
    QCOMPARE(rawParsed.optionEntries.at(2).key, QStringLiteral("-clip"));
    QCOMPARE(rawParsed.optionEntries.at(2).value, QStringLiteral("off"));
    QVERIFY(rawParsed.optionEntries.at(2).embeddedValue);
    QCOMPARE(rawParsed.optionEntries.at(2).optionTokenIndex, 6);
    QCOMPARE(rawParsed.optionEntries.at(3).key, QStringLiteral("-clip"));
    QCOMPARE(rawParsed.optionEntries.at(3).value, QStringLiteral("off"));
    QVERIFY(rawParsed.optionEntries.at(3).embeddedValue);
    QCOMPARE(rawParsed.optionEntries.at(3).optionTokenIndex, 7);
}

void CommandOptionParserTest::keepsLeadingValueSeparateWhenAllowed()
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

    QCOMPARE(parsed.leadingValue, QStringLiteral("s1"));
    QCOMPARE(parsed.optionsStartIndex, 2);
    QVERIFY(parsed.extraPositionalTokens.isEmpty());
    QCOMPARE(parsed.optionEntries.size(), 1);
    QCOMPARE(parsed.optionEntries.at(0).key, QStringLiteral("-projection"));
    QCOMPARE(parsed.optionEntries.at(0).value, QStringLiteral("plan"));
}

void CommandOptionParserTest::doesNotTreatDashPrefixedTokenAsLeadingValue()
{
    QHash<QString, int> arity;

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("point"),
                                                            {QStringLiteral("point"),
                                                             QStringLiteral("-45"),
                                                             QStringLiteral("12")},
                                                            arity,
                                                            true);

    QVERIFY(parsed.leadingValue.isEmpty());
    QCOMPARE(parsed.optionsStartIndex, 1);
    QCOMPARE(parsed.extraPositionalTokens, QStringList({QStringLiteral("-45"), QStringLiteral("12")}));
}

void CommandOptionParserTest::serializesCommandArguments()
{
    QCOMPARE(serializeCommandArgumentValues({QStringLiteral("plain"),
                                             QStringLiteral("left wall"),
                                             QStringLiteral("[a b]")}),
             QStringLiteral("plain \"left wall\" [a b]"));
}

void CommandOptionParserTest::serializesCommandOptionTokens()
{
    QCOMPARE(serializeCommandOptionTokens(QStringLiteral("-clip"),
                                          {QStringLiteral("left wall"), QStringLiteral("right")}),
             QStringList({QStringLiteral("-clip"), QStringLiteral("\"left wall\" right")}));
    QVERIFY(serializeCommandOptionTokens(QStringLiteral(""), {QStringLiteral("ignored")}).isEmpty());
}

void CommandOptionParserTest::readsCommandOptionsWithSharedBoundaries()
{
    const QStringList lineTokens({QStringLiteral("line"),
                                  QStringLiteral("rock-border"),
                                  QStringLiteral("-close"),
                                  QStringLiteral("on"),
                                  QStringLiteral("-clip"),
                                  QStringLiteral("off"),
                                  QStringLiteral("-id"),
                                  QStringLiteral("rb-1")});

    QCOMPARE(normalizedCommandOptionName(QStringLiteral("-clip")), QStringLiteral("clip"));
    QVERIFY(commandOptionNameMatches(QStringLiteral("-clip"), QStringLiteral("clip")));
    QCOMPARE(commandOptionValue(lineTokens, QStringLiteral("-clip")), QStringLiteral("off"));
    QCOMPARE(commandOptionToggleValue(lineTokens, QStringLiteral("-clip")).value_or(true), false);
    QCOMPARE(commandOptionToggleValue(lineTokens, QStringLiteral("close")).value_or(false), true);

    const QHash<QString, QString> values = commandOptionValuesByName(lineTokens);
    QCOMPARE(values.value(QStringLiteral("clip")), QStringLiteral("off"));
    QCOMPARE(values.value(QStringLiteral("id")), QStringLiteral("rb-1"));

    const QStringList corruptedSubtypeTokens({QStringLiteral("line"),
                                              QStringLiteral("rock-border"),
                                              QStringLiteral("-clip"),
                                              QStringLiteral("off"),
                                              QStringLiteral("-subtype"),
                                              QStringLiteral("-clip off")});
    QVERIFY(commandOptionValue(corruptedSubtypeTokens, QStringLiteral("-subtype")).isEmpty());
}

int runCommandOptionParserTest(int argc, char **argv)
{
    CommandOptionParserTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "CommandOptionParserTest.moc"
