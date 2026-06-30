#include "../src/app/text_editor/raw_editor/RawEditorCompletionTokenContext.h"
#include "../src/core/TherionSourceLogicalDocument.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
struct MarkedText
{
    QString text;
    int cursorOffset = -1;
};

MarkedText textWithCursorMarker(QString markedText)
{
    const int markerIndex = markedText.indexOf(QLatin1Char('|'));
    if (markerIndex >= 0) {
        markedText.remove(markerIndex, 1);
    }
    return {markedText, markerIndex};
}
}

class RawEditorCompletionTokenContextTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesCursorTokenByAbsoluteOffset_data();
    void resolvesCursorTokenByAbsoluteOffset();
};

void RawEditorCompletionTokenContextTest::resolvesCursorTokenByAbsoluteOffset_data()
{
    QTest::addColumn<QString>("markedText");
    QTest::addColumn<int>("expectedTokenIndex");
    QTest::addColumn<bool>("expectedInsideToken");
    QTest::addColumn<QString>("expectedToken");

    QTest::newRow("quoted-token")
        << QStringLiteral("survey cave -title \"Ca|ve\"\n")
        << 3
        << true
        << QStringLiteral("Cave");

    QTest::newRow("continued-option-token")
        << QStringLiteral("survey cave -title \"Cave\" \\\n  -person-|rename \"Old Name\" \"New Name\"\n")
        << 4
        << true
        << QStringLiteral("-person-rename");

    QTest::newRow("between-command-and-argument")
        << QStringLiteral("survey | cave -title \"Cave\"\n")
        << 1
        << false
        << QString();

    QTest::newRow("inside-comment")
        << QStringLiteral("survey cave # com|ment\n")
        << 2
        << false
        << QString();

    QTest::newRow("end-of-token")
        << QStringLiteral("endsurvey|\n")
        << 0
        << true
        << QStringLiteral("endsurvey");

    QTest::newRow("end-of-line-after-space")
        << QStringLiteral("endsurvey |\n")
        << 1
        << false
        << QString();
}

void RawEditorCompletionTokenContextTest::resolvesCursorTokenByAbsoluteOffset()
{
    QFETCH(QString, markedText);
    QFETCH(int, expectedTokenIndex);
    QFETCH(bool, expectedInsideToken);
    QFETCH(QString, expectedToken);

    const MarkedText marked = textWithCursorMarker(markedText);
    QVERIFY(marked.cursorOffset >= 0);

    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromText(marked.text);
    const RawEditorCompletionTokenContext context =
        rawEditorCompletionTokenContextAtOffset(logicalDocument, marked.cursorOffset);

    QCOMPARE(context.tokenIndexAtCursor, expectedTokenIndex);
    QCOMPARE(context.cursorInsideToken, expectedInsideToken);
    if (!expectedToken.isEmpty()) {
        QVERIFY(context.tokenIndexAtCursor >= 0);
        QVERIFY(context.tokenIndexAtCursor < context.parsedLine.tokens.size());
        QCOMPARE(context.parsedLine.tokens.at(context.tokenIndexAtCursor), expectedToken);
    }
}

int runRawEditorCompletionTokenContextTest(int argc, char **argv)
{
    RawEditorCompletionTokenContextTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "RawEditorCompletionTokenContextTest.moc"
