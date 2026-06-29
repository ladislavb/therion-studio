#include "../../src/core/TherionSourceText.h"
#include "../../src/core/TherionStringUtils.h"

#include <QString>
#include <QStringList>
#include <QtTest/QtTest>

using TherionStudio::TherionSourceText;

namespace
{
class TherionSourceTextTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsMixedPhysicalLineEndings();
    void exposesPhysicalLineSpans();
    void mapsOffsetsToLineLocations();
    void matchesLegacySplitAndJoinBehavior();
    void replacesLineRangeWithoutMergingFollowingLines();
    void rejectsInvalidRanges();
};

void TherionSourceTextTest::roundTripsMixedPhysicalLineEndings()
{
    const QString text = QStringLiteral("encoding utf-8\r\n# comment\nline wall\rpoint 1 2 station\n");
    const TherionSourceText source = TherionSourceText::fromText(text);

    QCOMPARE(source.toText(), text);
    QCOMPARE(source.physicalLines().size(), 5);
    QCOMPARE(source.textLines(), QStringList({QStringLiteral("encoding utf-8"),
                                              QStringLiteral("# comment"),
                                              QStringLiteral("line wall"),
                                              QStringLiteral("point 1 2 station"),
                                              QString()}));
}

void TherionSourceTextTest::exposesPhysicalLineSpans()
{
    const TherionSourceText source = TherionSourceText::fromText(QStringLiteral("a\r\nbb\nccc"));
    const QVector<TherionStudio::TherionSourceLineSpan> spans = source.lineSpans();

    QCOMPARE(spans.size(), 3);
    QCOMPARE(spans.at(0).lineNumber, 1);
    QCOMPARE(spans.at(0).startOffset, 0);
    QCOMPARE(spans.at(0).textLength, 1);
    QCOMPARE(spans.at(0).lineEndingLength, 2);
    QCOMPARE(spans.at(0).textEndOffset(), 1);
    QCOMPARE(spans.at(0).endOffset, 3);

    QCOMPARE(spans.at(1).lineNumber, 2);
    QCOMPARE(spans.at(1).startOffset, 3);
    QCOMPARE(spans.at(1).textLength, 2);
    QCOMPARE(spans.at(1).lineEndingLength, 1);
    QCOMPARE(spans.at(1).endOffset, 6);

    QCOMPARE(spans.at(2).lineNumber, 3);
    QCOMPARE(spans.at(2).startOffset, 6);
    QCOMPARE(spans.at(2).textLength, 3);
    QCOMPARE(spans.at(2).lineEndingLength, 0);
    QCOMPARE(spans.at(2).endOffset, 9);

    const std::optional<TherionStudio::TherionSourceLineSpan> secondLine = source.lineSpanForLineNumber(2);
    QVERIFY(secondLine.has_value());
    QCOMPARE(secondLine->startOffset, 3);
    QVERIFY(!source.lineSpanForLineNumber(0).has_value());
    QVERIFY(!source.lineSpanForLineNumber(4).has_value());
}

void TherionSourceTextTest::mapsOffsetsToLineLocations()
{
    const TherionSourceText source = TherionSourceText::fromText(QStringLiteral("a\r\nbb\nccc"));

    const std::optional<TherionStudio::TherionSourceTextLocation> atStart = source.locationForOffset(0);
    QVERIFY(atStart.has_value());
    QCOMPARE(atStart->lineNumber, 1);
    QCOMPARE(atStart->columnOffset, 0);

    const std::optional<TherionStudio::TherionSourceTextLocation> insideCrLf = source.locationForOffset(2);
    QVERIFY(insideCrLf.has_value());
    QCOMPARE(insideCrLf->lineNumber, 1);
    QCOMPARE(insideCrLf->columnOffset, 1);

    const std::optional<TherionStudio::TherionSourceTextLocation> secondLine = source.locationForOffset(4);
    QVERIFY(secondLine.has_value());
    QCOMPARE(secondLine->lineNumber, 2);
    QCOMPARE(secondLine->columnOffset, 1);

    const std::optional<TherionStudio::TherionSourceTextLocation> endOfDocument = source.locationForOffset(9);
    QVERIFY(endOfDocument.has_value());
    QCOMPARE(endOfDocument->lineNumber, 3);
    QCOMPARE(endOfDocument->columnOffset, 3);

    QVERIFY(!source.locationForOffset(-1).has_value());
    QVERIFY(!source.locationForOffset(10).has_value());
}

void TherionSourceTextTest::matchesLegacySplitAndJoinBehavior()
{
    QCOMPARE(TherionSourceText::splitTextLines(QStringLiteral("a\r\nb\n")),
             QStringList({QStringLiteral("a"), QStringLiteral("b"), QString()}));
    QCOMPARE(TherionStudio::splitLinesNormalizingLineEndings(QStringLiteral("a\rb\r\nc\n")),
             QStringList({QStringLiteral("a"),
                          QStringLiteral("b"),
                          QStringLiteral("c"),
                          QString()}));

    QCOMPARE(TherionSourceText::joinTextLines(QStringLiteral("a\r\n"), {QStringLiteral("x"),
                                                                         QStringLiteral("y"),
                                                                         QString()}),
             QStringLiteral("x\r\ny\r\n"));
}

void TherionSourceTextTest::replacesLineRangeWithoutMergingFollowingLines()
{
    TherionSourceText source = TherionSourceText::fromText(QStringLiteral("a\r\nb\r\nc\r\n"));

    QVERIFY(source.replaceLineRange(1, 1, {QStringLiteral("B1"), QStringLiteral("B2")}));
    QCOMPARE(source.toText(), QStringLiteral("a\r\nB1\r\nB2\r\nc\r\n"));
}

void TherionSourceTextTest::rejectsInvalidRanges()
{
    TherionSourceText source = TherionSourceText::fromText(QStringLiteral("a\nb\n"));

    QVERIFY(!source.replaceLineRange(-1, 1, {}));
    QVERIFY(!source.replaceLineRange(1, -1, {}));
    QVERIFY(!source.replaceLineRange(4, 0, {}));
    QVERIFY(!source.replaceLineRange(2, 2, {}));
    QCOMPARE(source.toText(), QStringLiteral("a\nb\n"));
}
}

int runTherionSourceTextTest(int argc, char **argv)
{
    TherionSourceTextTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceTextTest.moc"
