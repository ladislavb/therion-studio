#include "../../src/core/TherionCommandLineModel.h"
#include "../../src/core/TherionCommandSyntax.h"
#include "../../src/core/TherionDocumentEditor.h"
#include "../../src/core/TherionDocumentParser.h"
#include "../../src/core/TherionSourceDocument.h"
#include "../../src/core/TherionSourceLogicalDocument.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
// One span as text@start, prefixed Q: for quoted values and C: for comments.
QString renderedSpans(const TherionParsedLine &parsed)
{
    QStringList parts;
    for (const TherionParsedToken &span : parsed.tokenSpans) {
        QString prefix;
        if (span.type == TherionTokenType::QuotedString) {
            prefix = QStringLiteral("Q:");
        } else if (span.type == TherionTokenType::Comment) {
            prefix = QStringLiteral("C:");
        }
        parts.append(QStringLiteral("%1%2@%3").arg(prefix, span.text).arg(span.start));
    }
    return parts.join(QStringLiteral(" | "));
}
}

// Characterises how lines are split into values (plans/VALIDATION_ACCURACY_PLAN.md, V1), so each change to the
// splitting rules shows exactly what it alters.
class TherionLineSplittingTest final : public QObject
{
    Q_OBJECT

private slots:
    void splitsLines_data();
    void splitsLines();
    void readsOptionValues_data();
    void readsOptionValues();
    void splitsTypedOptionValues_data();
    void splitsTypedOptionValues();
    void splitsBracketedValueParts();
    void readsTherionValueText();
    void keepsLabelFormWhenEdited_data();
    void keepsLabelFormWhenEdited();
    void writesQuotesTherionReads();
    void findsContinuationMarkers_data();
    void findsContinuationMarkers();
    void joinsCodeLineEndingInBackslash();
};

void TherionLineSplittingTest::splitsLines_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<QString>("expected");

    // Command lines.
    QTest::newRow("quoted")
        << QStringLiteral("point 1 2 label -text \"Main Chamber\"")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | Q:Main Chamber@22");
    QTest::newRow("bracket label")
        << QStringLiteral("point 1 2 label -text [Main<br>Chamber] -scale l")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | [Main<br>Chamber]@22 | -scale@40 | l@47");
    QTest::newRow("bracket numbers")
        << QStringLiteral("scrap s1 -scale [0 0 100 0 0 0 1 0 m]")
        << QStringLiteral("scrap@0 | s1@6 | -scale@9 | [0 0 100 0 0 0 1 0 m]@16");
    QTest::newRow("bracket nested")
        << QStringLiteral("point 1 2 label -text [a [b] c]")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | [a [b] c]@22");
    QTest::newRow("bracket unclosed")
        << QStringLiteral("point 1 2 label -text [Main Chamber")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | [Main Chamber@22");
    QTest::newRow("bracket inside word")
        << QStringLiteral("point 1 2 label -text abc[d e]")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | abc[d@22 | e]@28");
    QTest::newRow("bracket with hash")
        << QStringLiteral("point 1 2 label -text [Room #1] -scale l")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | [Room #1]@22 | -scale@32 | l@39");
    QTest::newRow("bracket with percent")
        << QStringLiteral("point 1 2 label -text [50% wet]")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | [50% wet]@22");
    QTest::newRow("bracket with quotes")
        << QStringLiteral("point 1 2 label -text [say \"hi\" twice]")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | [say \"hi\" twice]@22");
    QTest::newRow("bracket continued")
        << QStringLiteral("point 1 2 label -text [Main \\")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | [Main@22 | \\@28");
    QTest::newRow("bracket empty")
        << QStringLiteral("point 1 2 label -text []")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | []@22");
    QTest::newRow("map offset")
        << QStringLiteral("  s1@cave [10 20 m] above")
        << QStringLiteral("s1@cave@2 | [10 20 m]@10 | above@20");
    QTest::newRow("revise stations")
        << QStringLiteral("revise s@cave -stations [4@a 5@a]")
        << QStringLiteral("revise@0 | s@cave@7 | -stations@14 | [4@a 5@a]@24");
    QTest::newRow("percent comment")
        << QStringLiteral("point 1 2 label % note")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | %@16 | note@18");
    QTest::newRow("hash inside word")
        << QStringLiteral("point 1 2 label -text abc#def")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | abc#def@22");
    QTest::newRow("single quote")
        << QStringLiteral("point 1 2 label -text Dan's cave")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | Dan's@22 | cave@28");
    QTest::newRow("backslash inside line")
        << QStringLiteral("point 1 2 label -text a\\ b")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | a\\@22 | b@25");
    QTest::newRow("doubled quote")
        << QStringLiteral("point 1 2 label -text \"say \"\"hi\"\"\"")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | Q:say \"hi\"@22");

    QTest::newRow("quote inside word")
        << QStringLiteral("point 1 2 label -text abc\"def")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | abc\"def@22");
    QTest::newRow("backslash quote inside quotes")
        << QStringLiteral("point 1 2 label -text \"say \\\"hi\\\"\"")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | -text@16 | Q:say \\@22 | hi\\\"\"@29");
    QTest::newRow("hash starts word")
        << QStringLiteral("point 1 2 label #note")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | C:#note@16");
    QTest::newRow("double backslash at end")
        << QStringLiteral("point 1 2 label \\\\")
        << QStringLiteral("point@0 | 1@6 | 2@8 | label@10 | \\\\@16");

    // Lines inside code ... endcode (TeX and MetaPost), which Therion does not split.
    QTest::newRow("tex comment")
        << QStringLiteral("\\framethickness=0.5mm % frame")
        << QStringLiteral("\\framethickness=0.5mm@0 | %@22 | frame@24");
    QTest::newRow("tex macro argument")
        << QStringLiteral("\\def\\x#1{[#1]}")
        << QStringLiteral("\\def\\x#1{[#1]}@0");
    QTest::newRow("metapost comment")
        << QStringLiteral("draw (0,0)--(1,1) withpen pencircle; % pen")
        << QStringLiteral("draw@0 | (0,0)--(1,1)@5 | withpen@18 | pencircle;@26 | %@37 | pen@39");
    QTest::newRow("tex backslash endcode")
        << QStringLiteral("\\endcode")
        << QStringLiteral("\\endcode@0");
    QTest::newRow("tex apostrophe")
        << QStringLiteral("\\legend{Dan's cave} % note")
        << QStringLiteral("\\legend{Dan's@0 | cave}@14 | %@20 | note@22");
    QTest::newRow("metapost brackets")
        << QStringLiteral("p := z[1] -- z[2];")
        << QStringLiteral("p@0 | :=@2 | z[1]@5 | --@10 | z[2];@13");
}

void TherionLineSplittingTest::splitsLines()
{
    QFETCH(QString, line);
    QFETCH(QString, expected);

    QCOMPARE(renderedSpans(TherionDocumentParser::parseLine(line)), expected);
}

void TherionLineSplittingTest::readsOptionValues_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<QString>("option");
    QTest::addColumn<QString>("expectedValue");
    QTest::addColumn<int>("expectedCount");

    QTest::newRow("bracket label")
        << QStringLiteral("point 1 2 label -text [Main<br>Chamber] -scale l")
        << QStringLiteral("-text") << QStringLiteral("[Main<br>Chamber]") << 1;
    QTest::newRow("bracket label with space")
        << QStringLiteral("point 1 2 label -text [Main Chamber] -scale l")
        << QStringLiteral("-text") << QStringLiteral("[Main Chamber]") << 1;
    QTest::newRow("option after bracket label")
        << QStringLiteral("point 1 2 label -text [Main<br>Chamber] -scale l")
        << QStringLiteral("-scale") << QStringLiteral("l") << 1;
    QTest::newRow("bracket numbers")
        << QStringLiteral("scrap s1 -scale [0 0 100 0 0 0 1 0 m] -author 2024 x")
        << QStringLiteral("-scale") << QStringLiteral("[0 0 100 0 0 0 1 0 m]") << 1;
    QTest::newRow("bracket with dash")
        << QStringLiteral("point 1 2 label -text [Main -east] -scale l")
        << QStringLiteral("-text") << QStringLiteral("[Main -east]") << 1;
    QTest::newRow("bracket with hash")
        << QStringLiteral("point 1 2 label -text [Room #1] -scale l")
        << QStringLiteral("-text") << QStringLiteral("[Room #1]") << 1;
    QTest::newRow("quoted")
        << QStringLiteral("point 1 2 label -text \"Main Chamber\" -scale l")
        << QStringLiteral("-text") << QStringLiteral("Main Chamber") << 1;
}

void TherionLineSplittingTest::readsOptionValues()
{
    QFETCH(QString, line);
    QFETCH(QString, option);
    QFETCH(QString, expectedValue);
    QFETCH(int, expectedCount);

    const TherionParsedLine parsed = TherionDocumentParser::parseLine(line);
    const int optionIndex = parsed.tokens.indexOf(option);
    QVERIFY(optionIndex > 0);
    QCOMPARE(commandOptionValue(parsed, option), expectedValue);
    QCOMPARE(commandLogicalOptionValueCount(parsed, optionIndex), expectedCount);
}

void TherionLineSplittingTest::splitsTypedOptionValues_data()
{
    QTest::addColumn<QString>("typed");
    QTest::addColumn<QString>("arity");
    QTest::addColumn<QString>("expected");

    QTest::newRow("bracket single")
        << QStringLiteral("[Main<br>Chamber]") << QStringLiteral("1") << QStringLiteral("[Main<br>Chamber]");
    QTest::newRow("bracket several")
        << QStringLiteral("[0 0 100 0 0 0 1 0 m]") << QStringLiteral("N") << QStringLiteral("[0 0 100 0 0 0 1 0 m]");
    QTest::newRow("words several")
        << QStringLiteral("2024 \"A Person\"") << QStringLiteral("N") << QStringLiteral("2024 | A Person");
    QTest::newRow("single quote")
        << QStringLiteral("Dan's cave") << QStringLiteral("1") << QStringLiteral("Dan's cave");
}

void TherionLineSplittingTest::splitsTypedOptionValues()
{
    QFETCH(QString, typed);
    QFETCH(QString, arity);
    QFETCH(QString, expected);

    QCOMPARE(parseOptionValuesFromEditor(typed, arity, -1).join(QStringLiteral(" | ")), expected);
}

void TherionLineSplittingTest::splitsBracketedValueParts()
{
    const TherionParsedLine parsed = TherionDocumentParser::parseLine(QStringLiteral("scrap s1 -scale [0 0  100 m]"));
    const QVector<TherionParsedToken> parts = TherionDocumentParser::bracketedValueSpans(parsed.tokenSpans.at(3));
    QCOMPARE(parts.size(), 4);
    QCOMPARE(parts.at(2).text, QStringLiteral("100"));
    QCOMPARE(parts.at(2).start, 22);
    QCOMPARE(parts.at(3).text, QStringLiteral("m"));
    QCOMPARE(parts.at(3).start, 26);

    // Inside brackets Therion splits on whitespace only, so # is not a comment.
    QCOMPARE(TherionDocumentParser::bracketedValueParts(QStringLiteral("[Room #1]")),
             QStringList({QStringLiteral("Room"), QStringLiteral("#1")}));
    QCOMPARE(TherionDocumentParser::bracketedValueParts(QStringLiteral("[0 0 1")),
             QStringList({QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")}));
    QCOMPARE(TherionDocumentParser::bracketedValueParts(QStringLiteral("l")), QStringList({QStringLiteral("l")}));
}

void TherionLineSplittingTest::readsTherionValueText()
{
    QCOMPARE(therionValueText(QStringLiteral("[Main<br>Chamber]")), QStringLiteral("Main<br>Chamber"));
    QCOMPARE(therionValueText(QStringLiteral("Main")), QStringLiteral("Main"));
    QCOMPARE(therionValueText(QStringLiteral("[unclosed")), QStringLiteral("[unclosed"));
    QCOMPARE(bracketedTherionValue(QStringLiteral("a [b] c")), QStringLiteral("[a [b] c]"));
    QVERIFY(bracketedTherionValue(QStringLiteral("a ] b")).isEmpty());
}

void TherionLineSplittingTest::keepsLabelFormWhenEdited_data()
{
    QTest::addColumn<QString>("pointLine");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expectedLine");

    QTest::newRow("bracketed stays bracketed")
        << QStringLiteral("point 1 2 label -text [Main<br>Chamber] -scale l")
        << QStringLiteral("Main<br>Big Chamber")
        << QStringLiteral("point 1 2 label -text [Main<br>Big Chamber] -scale l");
    QTest::newRow("quoted stays quoted")
        << QStringLiteral("point 1 2 label -text \"Main Chamber\" -scale l")
        << QStringLiteral("Big Chamber")
        << QStringLiteral("point 1 2 label -text \"Big Chamber\" -scale l");
    QTest::newRow("unbalanced text falls back to quotes")
        << QStringLiteral("point 1 2 label -text [Main Chamber] -scale l")
        << QStringLiteral("a ] b")
        << QStringLiteral("point 1 2 label -text \"a ] b\" -scale l");
}

void TherionLineSplittingTest::keepsLabelFormWhenEdited()
{
    QFETCH(QString, pointLine);
    QFETCH(QString, text);
    QFETCH(QString, expectedLine);

    QString contents = QStringLiteral("scrap s1\n%1\nendscrap\n").arg(pointLine);
    QVector<TherionSourceTextEdit> edits;
    QString error;
    QVERIFY2(TherionDocumentEditor::mapObjectTextOptionRewriteEdits(contents, 2, text, &edits, &error),
             qPrintable(error));
    QCOMPARE(edits.size(), 1);
    contents.replace(edits.first().startOffset, edits.first().length, edits.first().replacementText);
    QCOMPARE(contents.split(QLatin1Char('\n')).at(1), expectedLine);
}

void TherionLineSplittingTest::writesQuotesTherionReads()
{
    const QString text = QStringLiteral("say \"hi\" to Dan's a\\b");
    const QString quoted = quoteTherionArgument(text);
    QCOMPARE(quoted, QStringLiteral("\"say \"\"hi\"\" to Dan's a\\b\""));
    const TherionParsedLine parsed = TherionDocumentParser::parseLine(QStringLiteral("point 1 2 label -text %1").arg(quoted));
    QCOMPARE(parsed.tokens.last(), text);
}

void TherionLineSplittingTest::findsContinuationMarkers_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expectedIndex");

    // Therion joins a line whose last non-space character is a backslash (thinput.cxx); it does not count them.
    QTest::newRow("single") << QStringLiteral("survey a \\") << 9;
    QTest::newRow("trailing spaces") << QStringLiteral("survey a \\  ") << 9;
    QTest::newRow("tex line break") << QStringLiteral("  \\def\\x{A}\\\\") << 12;
    QTest::newRow("inside line") << QStringLiteral("a\\ b") << -1;
    QTest::newRow("none") << QStringLiteral("survey a") << -1;
}

void TherionLineSplittingTest::findsContinuationMarkers()
{
    QFETCH(QString, line);
    QFETCH(int, expectedIndex);

    QCOMPARE(TherionDocumentParser::continuationMarkerIndex(line), expectedIndex);
}

void TherionLineSplittingTest::joinsCodeLineEndingInBackslash()
{
    const QString text = QStringLiteral(
        "layout test\n"
        "  code tex-map\n"
        "    \\def\\x{A}\\\\\n"
        "  endcode\n"
        "  scale 1 200\n"
        "endlayout\n");
    // Therion 6.4.0 joins the TeX line ending in \\\\ with the endcode line and passes
    // "\\def\\x{A}\\  endcode" to TeX; it still reads the following layout lines normally.
    const TherionSourceLogicalDocument logical = TherionSourceLogicalDocument::fromText(text);
    QVERIFY(logical.commands().size() >= 5);
    const TherionSourceLogicalCommand &codeLine = logical.commands().at(2);
    QCOMPARE(codeLine.physicalLineNumbers, QVector<int>({3, 4}));
    QCOMPARE(codeLine.text, QStringLiteral("    \\def\\x{A}\\ endcode"));
    QCOMPARE(codeLine.currentBlockDirective, QStringLiteral("code"));
    QCOMPARE(codeLine.role, TherionSourceLineRole::BlockContent);

    const TherionSourceLogicalCommand &scale = logical.commands().at(3);
    QCOMPARE(scale.normalizedDirective, QStringLiteral("scale"));
    QCOMPARE(scale.currentBlockDirective, QStringLiteral("layout"));
    QVERIFY(logical.commands().at(4).closesBlock);

    const TherionSourceDocument source = TherionSourceDocument::fromText(text);
    QCOMPARE(source.blockRanges().size(), 2);
    QCOMPARE(source.blockRanges().at(0).closeLineNumber, 6);
    QCOMPARE(source.blockRanges().at(1).directive, QStringLiteral("code"));
    QCOMPARE(source.blockRanges().at(1).closeLineNumber, 4);
}

int runTherionLineSplittingTest(int argc, char **argv)
{
    TherionLineSplittingTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionLineSplittingTest.moc"
