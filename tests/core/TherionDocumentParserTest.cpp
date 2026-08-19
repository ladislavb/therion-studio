#include "../../src/core/TherionDocumentParser.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class TherionDocumentParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesQuotedStringsAndComments();
    void parsesCommentOnlyLinesWithoutDirectiveTokens();
    void parsesTextWithPhysicalLineNumbers();
    void parsesLosslessSourceDocument();
    void parsesLinePointRowsAndReviseStations();
};

void TherionDocumentParserTest::parsesQuotedStringsAndComments()
{
    const TherionParsedLine parsed = TherionDocumentParser::parseLine(
        QStringLiteral("point 1 2 label -text \"Entrance #1\" # visible label"), 7);

    QCOMPARE(parsed.lineNumber, 7);
    QCOMPARE(parsed.rawText, QStringLiteral("point 1 2 label -text \"Entrance #1\" # visible label"));
    QCOMPARE(parsed.directive, QStringLiteral("point"));
    QCOMPARE(parsed.tokens, QStringList({QStringLiteral("point"),
                                         QStringLiteral("1"),
                                         QStringLiteral("2"),
                                         QStringLiteral("label"),
                                         QStringLiteral("-text"),
                                         QStringLiteral("Entrance #1")}));
    QCOMPARE(parsed.commentStart, 36);
    QCOMPARE(parsed.commentText, QStringLiteral(" visible label"));
    QCOMPARE(parsed.tokenSpans.size(), 7);
    QCOMPARE(parsed.tokenSpans.at(5).type, TherionTokenType::QuotedString);
    QCOMPARE(parsed.tokenSpans.at(5).start, 22);
    QCOMPARE(parsed.tokenSpans.at(5).length, 13);
    QCOMPARE(parsed.tokenSpans.at(5).text, QStringLiteral("Entrance #1"));
    QCOMPARE(parsed.tokenSpans.at(6).type, TherionTokenType::Comment);
    QCOMPARE(parsed.tokenSpans.at(6).text, QStringLiteral("# visible label"));
}

void TherionDocumentParserTest::parsesCommentOnlyLinesWithoutDirectiveTokens()
{
    const TherionParsedLine hashComment = TherionDocumentParser::parseLine(QStringLiteral("   # survey note"), 12);
    QVERIFY(hashComment.tokens.isEmpty());
    QVERIFY(hashComment.directive.isEmpty());
    QCOMPARE(hashComment.commentStart, 3);
    QCOMPARE(hashComment.commentText, QStringLiteral(" survey note"));
    QCOMPARE(hashComment.tokenSpans.size(), 1);
    QCOMPARE(hashComment.tokenSpans.at(0).type, TherionTokenType::Comment);

    const TherionParsedLine percentComment = TherionDocumentParser::parseLine(QStringLiteral("% temporary branch"), 13);
    QVERIFY(percentComment.tokens.isEmpty());
    QCOMPARE(percentComment.commentStart, 0);
    QCOMPARE(percentComment.commentText, QStringLiteral(" temporary branch"));
    QCOMPARE(percentComment.tokenSpans.size(), 1);
    QCOMPARE(percentComment.tokenSpans.at(0).text, QStringLiteral("% temporary branch"));
}

void TherionDocumentParserTest::parsesTextWithPhysicalLineNumbers()
{
    const QVector<TherionParsedLine> parsed = TherionDocumentParser::parseText(QStringLiteral(
        "# file header\r\n"
        "\r\n"
        "survey cave\r\n"
        "% local note\r\n"
        "endsurvey\r\n"));

    QCOMPARE(parsed.size(), 2);
    QCOMPARE(parsed.at(0).lineNumber, 3);
    QCOMPARE(parsed.at(0).directive, QStringLiteral("survey"));
    QCOMPARE(parsed.at(1).lineNumber, 5);
    QCOMPARE(parsed.at(1).directive, QStringLiteral("endsurvey"));
    QCOMPARE(parsed.at(0).rawText, QStringLiteral("survey cave"));
}

void TherionDocumentParserTest::parsesLosslessSourceDocument()
{
    const QString text = QStringLiteral(
        "# file header\r\n"
        "\r"
        "survey cave\r\n"
        "  # local note\n"
        "  centerline\n"
        "    data normal from to tape compass clino\n"
        "  endcenterline\r"
        "endsurvey\r\n");

    const TherionParsedSourceDocument document = TherionDocumentParser::parseSourceDocument(text);
    QCOMPARE(document.toText(), text);
    QCOMPARE(document.lines.size(), 9);

    const TherionParsedSourceLine &firstLine = document.lines.at(0);
    QCOMPARE(firstLine.lineNumber, 1);
    QCOMPARE(firstLine.startOffset, 0);
    QCOMPARE(firstLine.textLength, 13);
    QCOMPARE(firstLine.lineEndingLength, 2);
    QCOMPARE(firstLine.endOffset, 15);
    QVERIFY(firstLine.isCommentOnly());
    QCOMPARE(firstLine.lineEnding, QStringLiteral("\r\n"));

    const TherionParsedSourceLine &blankLine = document.lines.at(1);
    QCOMPARE(blankLine.lineNumber, 2);
    QCOMPARE(blankLine.startOffset, 15);
    QCOMPARE(blankLine.textLength, 0);
    QCOMPARE(blankLine.lineEndingLength, 1);
    QCOMPARE(blankLine.endOffset, 16);
    QVERIFY(blankLine.isBlank());
    QCOMPARE(blankLine.lineEnding, QStringLiteral("\r"));

    const TherionParsedSourceLine &surveyLine = document.lines.at(2);
    QCOMPARE(surveyLine.lineNumber, 3);
    QCOMPARE(surveyLine.startOffset, 16);
    QVERIFY(surveyLine.hasTokens());
    QCOMPARE(surveyLine.parsed.directive, QStringLiteral("survey"));

    const TherionParsedSourceLine &commentLine = document.lines.at(3);
    QCOMPARE(commentLine.lineNumber, 4);
    QVERIFY(commentLine.isCommentOnly());
    QCOMPARE(commentLine.parsed.commentStart, 2);

    const TherionParsedSourceLine &lastLine = document.lines.last();
    QCOMPARE(lastLine.lineNumber, 9);
    QCOMPARE(lastLine.startOffset, text.size());
    QCOMPARE(lastLine.endOffset, text.size());
    QVERIFY(lastLine.isBlank());
    QVERIFY(lastLine.lineEnding.isEmpty());

    const TherionParsedSourceLine &dataLine = document.lines.at(5);
    QCOMPARE(dataLine.text, QStringLiteral("    data normal from to tape compass clino"));
    QVERIFY(dataLine.parsed.tokenSpans.size() >= 2);
    QCOMPARE(dataLine.absoluteTokenStart(dataLine.parsed.tokenSpans.at(0)), dataLine.startOffset + 4);
    QCOMPARE(dataLine.absoluteTokenEnd(dataLine.parsed.tokenSpans.at(1)), dataLine.startOffset + 15);

    const QVector<TherionParsedLine> tokenLines = document.tokenLines();
    QCOMPARE(tokenLines.size(), 5);
    QCOMPARE(tokenLines.at(0).lineNumber, 3);
    QCOMPARE(tokenLines.at(0).directive, QStringLiteral("survey"));
    QCOMPARE(tokenLines.last().lineNumber, 8);
    QCOMPARE(tokenLines.last().directive, QStringLiteral("endsurvey"));
}

void TherionDocumentParserTest::parsesLinePointRowsAndReviseStations()
{
    const TherionParsedLine linePointRow = TherionDocumentParser::parseLine(
        QStringLiteral("  altitude . # keep auto altitude"), 21);
    QCOMPARE(linePointRow.directive, QStringLiteral("altitude"));
    QCOMPARE(linePointRow.tokens, QStringList({QStringLiteral("altitude"), QStringLiteral(".")}));
    QCOMPARE(linePointRow.commentStart, 13);
    QCOMPARE(linePointRow.commentText, QStringLiteral(" keep auto altitude"));

    const TherionParsedLine revise = TherionDocumentParser::parseLine(
        QStringLiteral("revise dd.s@dur.dur_dom -stations [4@monum.dur_dom 5@monum.dur_dom 6@monum.dur_dom]"),
        31);
    QCOMPARE(revise.directive, QStringLiteral("revise"));
    QCOMPARE(revise.tokens, QStringList({QStringLiteral("revise"),
                                         QStringLiteral("dd.s@dur.dur_dom"),
                                         QStringLiteral("-stations"),
                                         QStringLiteral("[4@monum.dur_dom"),
                                         QStringLiteral("5@monum.dur_dom"),
                                         QStringLiteral("6@monum.dur_dom]")}));
}

int runTherionDocumentParserTest(int argc, char **argv)
{
    TherionDocumentParserTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionDocumentParserTest.moc"
