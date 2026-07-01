#include "../src/app/text_editor/raw_editor/RawEditorCompletionContextAnalyzer.h"
#include "../src/core/TherionSourceSnapshotCache.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QtTest/QtTest>

#include <utility>

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

QString completionPrefixForMarkedText(const QString &markedText)
{
    const MarkedText marked = textWithCursorMarker(markedText);
    if (marked.cursorOffset < 0) {
        return QStringLiteral("__missing_cursor_marker__");
    }

    QPlainTextEdit editor;
    editor.setPlainText(marked.text);

    QTextCursor cursor(editor.document());
    cursor.setPosition(marked.cursorOffset);
    editor.setTextCursor(cursor);

    TherionSourceSnapshotCache sourceSnapshotCache;
    RawEditorCompletionContext context;
    context.editor = &editor;
    context.sourceSnapshotCache = &sourceSnapshotCache;
    return RawEditorCompletionContextAnalyzer(std::move(context)).currentCompletionPrefix();
}
}

class RawEditorCompletionContextAnalyzerTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesPrefixFromLogicalTokenRanges_data();
    void resolvesPrefixFromLogicalTokenRanges();
};

void RawEditorCompletionContextAnalyzerTest::resolvesPrefixFromLogicalTokenRanges_data()
{
    QTest::addColumn<QString>("markedText");
    QTest::addColumn<QString>("expectedPrefix");

    QTest::newRow("quoted-token")
        << QStringLiteral("survey cave -title \"Ca|ve\"\n")
        << QStringLiteral("Cave");

    QTest::newRow("continued-option-token")
        << QStringLiteral("survey cave -title \"Cave\" \\\n  -person-|rename \"Old Name\" \"New Name\"\n")
        << QStringLiteral("-person-rename");

    QTest::newRow("input-path-keeps-word-prefix")
        << QStringLiteral("input ./ch|ild.th2\n")
        << QStringLiteral("child");

    QTest::newRow("input-activation-prefix-stays-empty")
        << QStringLiteral("input ./|\n")
        << QString();
}

void RawEditorCompletionContextAnalyzerTest::resolvesPrefixFromLogicalTokenRanges()
{
    QFETCH(QString, markedText);
    QFETCH(QString, expectedPrefix);

    QCOMPARE(completionPrefixForMarkedText(markedText), expectedPrefix);
}

int runRawEditorCompletionContextAnalyzerTest(int argc, char **argv)
{
    RawEditorCompletionContextAnalyzerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "RawEditorCompletionContextAnalyzerTest.moc"
