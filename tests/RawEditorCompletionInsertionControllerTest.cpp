#include "../src/app/text_editor/raw_editor/RawEditorCompletionInsertionController.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QtTest/QtTest>

#include <utility>

using namespace TherionStudio;

namespace
{
struct CompletionCase
{
    QString markedText;
    QString completion;
    QString expectedText;
};

QString applyCompletion(const CompletionCase &testCase)
{
    const QString marker = QStringLiteral("|");
    const int markerIndex = testCase.markedText.indexOf(marker);
    if (markerIndex < 0) {
        return QStringLiteral("__missing_completion_marker__");
    }

    QString text = testCase.markedText;
    text.remove(markerIndex, marker.size());

    QPlainTextEdit editor;
    editor.setPlainText(text);

    QTextCursor cursor(editor.document());
    cursor.setPosition(markerIndex);
    editor.setTextCursor(cursor);

    RawEditorCompletionInsertionContext context;
    context.editor = &editor;
    context.normalizedDirectiveToken = [](const QString &token) {
        return token.trimmed().toLower();
    };
    context.closingDirectiveForOpeningToken = [](const QString &) {
        return QString();
    };
    RawEditorCompletionInsertionController(std::move(context)).insertCompletionToken(testCase.completion);

    return editor.toPlainText();
}
}

class RawEditorCompletionInsertionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void replacesInputPathToken_data();
    void replacesInputPathToken();
    void keepsGenericCompletionWordRange();
};

void RawEditorCompletionInsertionControllerTest::replacesInputPathToken_data()
{
    QTest::addColumn<QString>("markedText");
    QTest::addColumn<QString>("completion");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("relative-dot-slash")
        << QStringLiteral("input ./|child.th2\n")
        << QStringLiteral("child.th2")
        << QStringLiteral("input child.th2\n");

    QTest::newRow("middle-of-path-token")
        << QStringLiteral("input sub|dir/child.th2\n")
        << QStringLiteral("subdir/child.th2")
        << QStringLiteral("input subdir/child.th2\n");

    QTest::newRow("windows-separators")
        << QStringLiteral("input .\\|subdir\\child.th2\n")
        << QStringLiteral("subdir/child.th2")
        << QStringLiteral("input subdir/child.th2\n");

    QTest::newRow("quoted-path")
        << QStringLiteral("input \"./|child.th2\"\n")
        << QStringLiteral("child.th2")
        << QStringLiteral("input \"child.th2\"\n");

    QTest::newRow("quoted-windows-separators")
        << QStringLiteral("input \".\\|subdir\\child.th2\"\n")
        << QStringLiteral("subdir/child.th2")
        << QStringLiteral("input \"subdir/child.th2\"\n");
}

void RawEditorCompletionInsertionControllerTest::replacesInputPathToken()
{
    QFETCH(QString, markedText);
    QFETCH(QString, completion);
    QFETCH(QString, expectedText);

    const CompletionCase testCase{
        markedText,
        completion,
        expectedText,
    };
    QCOMPARE(applyCompletion(testCase), testCase.expectedText);
}

void RawEditorCompletionInsertionControllerTest::keepsGenericCompletionWordRange()
{
    const CompletionCase testCase{
        QStringLiteral("scrap pre|fix/suffix\n"),
        QStringLiteral("prefix-completed"),
        QStringLiteral("scrap prefix-completed/suffix\n"),
    };

    QCOMPARE(applyCompletion(testCase), testCase.expectedText);
}

int runRawEditorCompletionInsertionControllerTest(int argc, char **argv)
{
    RawEditorCompletionInsertionControllerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "RawEditorCompletionInsertionControllerTest.moc"
