#include "../../src/core/TherionCommandSyntax.h"
#include "../../src/core/TherionSourceLogicalDocument.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionSourceLogicalDocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void groupsContinuationLinesIntoOneLogicalCommand();
    void mapsLogicalTokenRangesBackToPhysicalLines();
    void exposesPhysicalRangesForParsedTokens();
    void exposesPositionalAndOptionEntryRanges();
    void findsLogicalCommandsAndTokensByPhysicalCursorPosition();
    void findsLogicalCommandsAndTokensByAbsoluteOffset();
    void attachesCatalogMetadataToLogicalCommands();
    void keepsBlockContentRowsAsNonCommandLogicalEntries();
    void normalizesCentrelineAliasOnLogicalCommands();
    void carriesSourceSnapshotMetadata();
    void attachesDocumentTypeMetadataToLogicalCommands();

private:
    static const TherionSourceLogicalCommand &commandAt(
        const TherionSourceLogicalDocument &document,
        int zeroBasedIndex);
};

const TherionSourceLogicalCommand &TherionSourceLogicalDocumentTest::commandAt(
    const TherionSourceLogicalDocument &document,
    int zeroBasedIndex)
{
    Q_ASSERT(zeroBasedIndex >= 0);
    Q_ASSERT(zeroBasedIndex < document.commands().size());
    return document.commands().at(zeroBasedIndex);
}

void TherionSourceLogicalDocumentTest::groupsContinuationLinesIntoOneLogicalCommand()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    QCOMPARE(document.commands().size(), 3);

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    QCOMPARE(survey.startLineNumber, 1);
    QCOMPARE(survey.endLineNumber, 2);
    QCOMPARE(survey.physicalLineNumbers, QVector<int>({1, 2}));
    QCOMPARE(survey.normalizedDirective, QStringLiteral("survey"));
    QVERIFY(survey.parsed.tokens.contains(QStringLiteral("-person-rename")));
    QVERIFY(survey.shouldValidateCommandCatalog());

    const TherionSourceLogicalCommand &close = commandAt(document, 1);
    QCOMPARE(close.normalizedDirective, QStringLiteral("endsurvey"));
    QVERIFY(!close.hasUnmatchedClose());
}

void TherionSourceLogicalDocumentTest::mapsLogicalTokenRangesBackToPhysicalLines()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    const int optionStart = survey.text.indexOf(QStringLiteral("-person-rename"));
    TherionSourcePhysicalRange range;
    QVERIFY(optionStart >= 0);
    QVERIFY(survey.physicalRangeForLogicalRange(optionStart,
                                                QStringLiteral("-person-rename").size(),
                                                &range));
    QCOMPARE(range.lineNumber, 2);
    QCOMPARE(range.columnNumber, 3);
    QCOMPARE(range.columnLength, QStringLiteral("-person-rename").size());
    QCOMPARE(range.lineText, QStringLiteral("  -person-rename \"Old Name\" \"New Name\""));
}

void TherionSourceLogicalDocumentTest::exposesPhysicalRangesForParsedTokens()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    QCOMPARE(survey.tokenRanges.size(), survey.parsed.tokenSpans.size());

    const int titleValueIndex = survey.parsed.tokens.indexOf(QStringLiteral("Cave"));
    QVERIFY(titleValueIndex >= 0);
    QCOMPARE(survey.tokenRanges.at(titleValueIndex).text, QStringLiteral("Cave"));
    QVERIFY(survey.tokenRanges.at(titleValueIndex).type == TherionTokenType::QuotedString);
    QCOMPARE(survey.tokenRanges.at(titleValueIndex).physicalRange.lineNumber, 1);
    QCOMPARE(survey.tokenRanges.at(titleValueIndex).physicalRange.columnNumber, 20);
    QCOMPARE(survey.tokenRanges.at(titleValueIndex).physicalRange.columnLength, 6);

    const int renameOptionIndex = survey.parsed.tokens.indexOf(QStringLiteral("-person-rename"));
    QVERIFY(renameOptionIndex >= 0);
    TherionSourcePhysicalRange range;
    QVERIFY(survey.physicalRangeForTokenIndex(renameOptionIndex, &range));
    QCOMPARE(range.lineNumber, 2);
    QCOMPARE(range.columnNumber, 3);
    QCOMPARE(range.columnLength, QStringLiteral("-person-rename").size());
}

void TherionSourceLogicalDocumentTest::exposesPositionalAndOptionEntryRanges()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    QCOMPARE(survey.positionalArgumentRanges.size(), 1);
    QCOMPARE(survey.positionalArgumentRanges.constFirst().text, QStringLiteral("cave"));
    QCOMPARE(survey.positionalArgumentRanges.constFirst().physicalRange.lineNumber, 1);
    QCOMPARE(survey.positionalArgumentRanges.constFirst().physicalRange.columnNumber, 8);
    QVERIFY(survey.positionalArgumentGroupRange.isValid());
    QCOMPARE(survey.positionalArgumentGroupRange.firstTokenIndex, 1);
    QCOMPARE(survey.positionalArgumentGroupRange.lastTokenIndex, 1);
    QCOMPARE(survey.positionalArgumentGroupRange.text, QStringLiteral("cave"));
    QCOMPARE(survey.metadata.commandName, QStringLiteral("survey"));
    QCOMPARE(survey.metadata.positionalArgumentCount, 1);

    QCOMPARE(survey.optionEntryRanges.size(), 2);
    const TherionSourceLogicalOptionEntryRange &title = survey.optionEntryRanges.at(0);
    QCOMPARE(title.key, QStringLiteral("-title"));
    QCOMPARE(title.optionTokenIndex, 2);
    QCOMPARE(title.firstValueTokenIndex, 3);
    QCOMPARE(title.lastValueTokenIndex, 3);
    QCOMPARE(title.nextTokenIndex, 4);
    QCOMPARE(title.rawValueTokens, QStringList({QStringLiteral("Cave")}));
    QCOMPARE(title.logicalValueCount, 1);
    QCOMPARE(title.optionRange.lineNumber, 1);
    QCOMPARE(title.optionRange.columnNumber, 13);
    QCOMPARE(title.valueRanges.size(), 1);
    QCOMPARE(title.valueRanges.constFirst().text, QStringLiteral("Cave"));
    QCOMPARE(title.valueRanges.constFirst().physicalRange.lineNumber, 1);
    QCOMPARE(title.valueRanges.constFirst().physicalRange.columnNumber, 20);
    QVERIFY(title.valueGroupRange.isValid());
    QCOMPARE(title.valueGroupRange.text, QStringLiteral("Cave"));
    QCOMPARE(title.valueGroupRange.firstTokenIndex, title.valueRanges.constFirst().tokenIndex);
    QCOMPARE(title.valueGroupRange.lastTokenIndex, title.valueRanges.constFirst().tokenIndex);
    QVERIFY(survey.metadata.normalizedOptionNames.contains(QStringLiteral("-title")));
    QCOMPARE(survey.metadata.optionEntryIndexesByNormalizedName.value(QStringLiteral("-title")),
             QVector<int>({0}));

    const TherionSourceLogicalOptionEntryRange &rename = survey.optionEntryRanges.at(1);
    QCOMPARE(rename.key, QStringLiteral("-person-rename"));
    QCOMPARE(rename.optionTokenIndex, 4);
    QCOMPARE(rename.firstValueTokenIndex, 5);
    QCOMPARE(rename.lastValueTokenIndex, 6);
    QCOMPARE(rename.nextTokenIndex, 7);
    QCOMPARE(rename.rawValueTokens, QStringList({QStringLiteral("Old Name"), QStringLiteral("New Name")}));
    QCOMPARE(rename.logicalValueCount, 2);
    QCOMPARE(rename.optionRange.lineNumber, 2);
    QCOMPARE(rename.optionRange.columnNumber, 3);
    QCOMPARE(rename.valueRanges.size(), 2);
    QCOMPARE(rename.valueRanges.at(0).text, QStringLiteral("Old Name"));
    QCOMPARE(rename.valueRanges.at(1).text, QStringLiteral("New Name"));
    QCOMPARE(rename.valueRanges.at(0).physicalRange.lineNumber, 2);
    QCOMPARE(rename.valueRanges.at(1).physicalRange.lineNumber, 2);
    QVERIFY(rename.valueGroupRange.isValid());
    QCOMPARE(rename.valueGroupRange.firstTokenIndex, rename.valueRanges.constFirst().tokenIndex);
    QCOMPARE(rename.valueGroupRange.lastTokenIndex, rename.valueRanges.constLast().tokenIndex);
    QCOMPARE(rename.valueGroupRange.text, QStringLiteral("Old Name New Name"));
    QCOMPARE(rename.valueGroupRange.argumentRanges.size(), 2);
    QVERIFY(survey.metadata.normalizedOptionNames.contains(QStringLiteral("-person-rename")));
    QCOMPARE(survey.metadata.optionEntryIndexesByNormalizedName.value(QStringLiteral("-person-rename")),
             QVector<int>({1}));
}

void TherionSourceLogicalDocumentTest::findsLogicalCommandsAndTokensByPhysicalCursorPosition()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n"));

    const TherionSourceLogicalCommand *continuedCommand = document.commandAtPhysicalLine(2);
    QVERIFY(continuedCommand != nullptr);
    QCOMPARE(continuedCommand->startLineNumber, 1);
    QCOMPARE(continuedCommand->endLineNumber, 2);
    QCOMPARE(continuedCommand->normalizedDirective, QStringLiteral("survey"));

    const TherionSourceLogicalTokenRange *continuedOption = document.tokenAtPhysicalPosition(2, 5);
    QVERIFY(continuedOption != nullptr);
    QCOMPARE(continuedOption->text, QStringLiteral("-person-rename"));
    QCOMPARE(continuedOption->physicalRange.lineNumber, 2);

    const TherionSourceLogicalTokenRange *quotedTitle = document.tokenAtPhysicalPosition(1, 22);
    QVERIFY(quotedTitle != nullptr);
    QCOMPARE(quotedTitle->text, QStringLiteral("Cave"));
    QVERIFY(quotedTitle->type == TherionTokenType::QuotedString);

    QVERIFY(document.tokenAtPhysicalPosition(3, 20) == nullptr);
}

void TherionSourceLogicalDocumentTest::findsLogicalCommandsAndTokensByAbsoluteOffset()
{
    const QString contents = QStringLiteral(
        "survey cave -title \"Cave\" \\\n"
        "  -person-rename \"Old Name\" \"New Name\"\n"
        "endsurvey\n");
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(contents);

    const int continuationOffset = contents.indexOf(QStringLiteral("-person-rename"));
    QVERIFY(continuationOffset > 0);
    const TherionSourceLogicalCommand *continuedCommand = document.commandAtOffset(continuationOffset);
    QVERIFY(continuedCommand != nullptr);
    QCOMPARE(continuedCommand->startLineNumber, 1);
    QCOMPARE(continuedCommand->endLineNumber, 2);
    QCOMPARE(continuedCommand->normalizedDirective, QStringLiteral("survey"));

    const TherionSourceLogicalTokenRange *continuedOption = document.tokenAtOffset(continuationOffset + 2);
    QVERIFY(continuedOption != nullptr);
    QCOMPARE(continuedOption->text, QStringLiteral("-person-rename"));
    QCOMPARE(continuedOption->physicalRange.lineNumber, 2);

    const int titleOffset = contents.indexOf(QStringLiteral("\"Cave\""));
    QVERIFY(titleOffset > 0);
    const TherionSourceLogicalTokenRange *quotedTitle = document.tokenAtOffset(titleOffset + 1);
    QVERIFY(quotedTitle != nullptr);
    QCOMPARE(quotedTitle->text, QStringLiteral("Cave"));
    QVERIFY(quotedTitle->type == TherionTokenType::QuotedString);

    const int titleEndOffset = titleOffset + QStringLiteral("\"Cave\"").size();
    QVERIFY(document.tokenAtOffset(titleEndOffset) == nullptr);
    const TherionSourceLogicalTokenRange *boundaryToken = document.tokenAtOffset(titleEndOffset - 1);
    QVERIFY(boundaryToken != nullptr);
    QCOMPARE(boundaryToken->text, QStringLiteral("Cave"));

    const int lineEndingOffset = contents.indexOf(QLatin1Char('\n'));
    QVERIFY(document.tokenAtOffset(lineEndingOffset) == nullptr);
    QVERIFY(document.commandAtOffset(-1) == nullptr);
    QVERIFY(document.commandAtOffset(contents.size()) == nullptr);
}

void TherionSourceLogicalDocumentTest::attachesCatalogMetadataToLogicalCommands()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {QStringLiteral("survey"), QStringLiteral("line")};
    catalog.commandContexts.insert(QStringLiteral("survey"), {QStringLiteral("none"), QStringLiteral("survey")});
    catalog.commandContexts.insert(QStringLiteral("line"), {QStringLiteral("scrap")});
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("survey"), 1);
    catalog.commandOptionNames.insert(QStringLiteral("survey"),
                                      {QStringLiteral("-title"), QStringLiteral("-person-rename")});
    catalog.commandOptionNames.insert(QStringLiteral("line"),
                                      {QStringLiteral("-close"), QStringLiteral("-subtype")});
    catalog.commandArgumentAllowedValuesByKey.insert(commandArgumentValueKey(QStringLiteral("line"), 0),
                                                     {QStringLiteral("wall"), QStringLiteral("border")});
    catalog.commandOptionValueArityTokens.insert(commandOptionValueKey(QStringLiteral("line"),
                                                                       QStringLiteral("-close")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionAllowedValuesByKey.insert(commandOptionValueKey(QStringLiteral("line"),
                                                                         QStringLiteral("-close")),
                                                   {QStringLiteral("on"), QStringLiteral("off")});
    catalog.commandSubtypeValuesByTypeKey.insert(commandSubtypeValueKey(QStringLiteral("line"),
                                                                        QStringLiteral("wall")),
                                                 {QStringLiteral("bedrock"), QStringLiteral("blocks")});

    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(
        QStringLiteral(
            "survey cave -title \"Cave\" \\\n"
            "  -person-rename \"Old Name\" \"New Name\"\n"
            "endsurvey\n"
            "scrap s1\n"
            "line wall -close maybe -subtype bedrock\n"
            "endline\n"
            "endscrap\n"),
        catalog);

    const TherionSourceLogicalCommand &survey = commandAt(document, 0);
    QVERIFY(survey.metadata.catalogCommandKnown);
    QCOMPARE(survey.metadata.catalogRequiredPositionalCount, 1);
    QCOMPARE(survey.metadata.catalogContexts, QStringList({QStringLiteral("none"), QStringLiteral("survey")}));
    QCOMPARE(survey.metadata.catalogCurrentContext, QStringLiteral("none"));
    QVERIFY(survey.metadata.catalogContextAllowed);
    QVERIFY(survey.metadata.catalogOptionNames.contains(QStringLiteral("-title")));
    QVERIFY(survey.metadata.catalogOptionNames.contains(QStringLiteral("-person-rename")));

    const TherionSourceLogicalCommand &close = commandAt(document, 1);
    QVERIFY(!close.metadata.catalogCommandKnown);

    const TherionSourceLogicalCommand &line = commandAt(document, 3);
    QVERIFY(line.metadata.catalogCommandKnown);
    QCOMPARE(line.metadata.catalogContexts, QStringList({QStringLiteral("scrap")}));
    QCOMPARE(line.metadata.catalogCurrentContext, QStringLiteral("scrap"));
    QVERIFY(line.metadata.catalogContextAllowed);
    QCOMPARE(line.metadata.catalogArgumentAllowedValuesByIndex.value(0),
             QStringList({QStringLiteral("wall"), QStringLiteral("border")}));
    QCOMPARE(line.metadata.catalogOptionValueArityTokens.value(QStringLiteral("-close")),
             QStringLiteral("EXACTLY_ONE"));
    QCOMPARE(line.metadata.catalogOptionAllowedValuesByName.value(QStringLiteral("-close")),
             QStringList({QStringLiteral("on"), QStringLiteral("off")}));
    QCOMPARE(line.metadata.catalogOptionAllowedValuesByName.value(QStringLiteral("-subtype")),
             QStringList({QStringLiteral("bedrock"), QStringLiteral("blocks")}));
}

void TherionSourceLogicalDocumentTest::keepsBlockContentRowsAsNonCommandLogicalEntries()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "scrap s1\n"
        "line wall\n"
        "  0 0\n"
        "  smooth off\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n"));

    const TherionSourceLogicalCommand &coordinate = commandAt(document, 2);
    QVERIFY(coordinate.role == TherionSourceLineRole::BlockContent);
    QVERIFY(!coordinate.shouldValidateCommandCatalog());

    const TherionSourceLogicalCommand &areaReference = commandAt(document, 6);
    QVERIFY(areaReference.role == TherionSourceLineRole::BlockContent);
    QVERIFY(!areaReference.shouldValidateCommandCatalog());
}

void TherionSourceLogicalDocumentTest::normalizesCentrelineAliasOnLogicalCommands()
{
    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(QStringLiteral(
        "centreline\n"
        "  data normal from to length compass clino\n"
        "endcentreline\n"));

    const TherionSourceLogicalCommand &open = commandAt(document, 0);
    QCOMPARE(open.normalizedDirective, QStringLiteral("centerline"));
    QVERIFY(open.opensBlock);

    const TherionSourceLogicalCommand &body = commandAt(document, 1);
    QVERIFY(body.role == TherionSourceLineRole::BlockContent);

    const TherionSourceLogicalCommand &close = commandAt(document, 2);
    QCOMPARE(close.normalizedDirective, QStringLiteral("endcenterline"));
    QVERIFY(!close.hasUnmatchedClose());
}

void TherionSourceLogicalDocumentTest::carriesSourceSnapshotMetadata()
{
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionConfig;
    metadata.encodingName = QStringLiteral("UTF-8");
    metadata.revisionId = 7;

    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(
        QStringLiteral("source index.th\n"),
        metadata);

    QVERIFY(document.metadata().sourceType == TherionSourceDocumentType::TherionConfig);
    QCOMPARE(document.metadata().encodingName, QStringLiteral("UTF-8"));
    QCOMPARE(document.metadata().revisionId, 7);
}

void TherionSourceLogicalDocumentTest::attachesDocumentTypeMetadataToLogicalCommands()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {QStringLiteral("source"), QStringLiteral("line")};
    catalog.commandDocumentTypes.insert(QStringLiteral("source"), {QStringLiteral("thconfig")});
    catalog.commandDocumentTypes.insert(QStringLiteral("line"), {QStringLiteral("th2")});

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionConfig;

    const TherionSourceLogicalDocument document = TherionSourceLogicalDocument::fromText(
        QStringLiteral("source index.th\n"
                       "line wall\n"),
        catalog,
        metadata);

    const TherionSourceLogicalCommand &source = commandAt(document, 0);
    QCOMPARE(source.metadata.catalogCurrentDocumentType, QStringLiteral("thconfig"));
    QVERIFY(source.metadata.catalogDocumentTypes.contains(QStringLiteral("thconfig")));
    QVERIFY(source.metadata.catalogDocumentTypeAllowed);

    const TherionSourceLogicalCommand &line = commandAt(document, 1);
    QVERIFY(line.metadata.catalogDocumentTypes.contains(QStringLiteral("th2")));
    QVERIFY(!line.metadata.catalogDocumentTypeAllowed);
}
}

int runTherionSourceLogicalDocumentTest(int argc, char **argv)
{
    TherionSourceLogicalDocumentTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceLogicalDocumentTest.moc"
