#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"

#include <QApplication>
#include <QFrame>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
QLineEdit *findEditFor(TextEditorTab *tab)
{
    if (tab == nullptr) {
        return nullptr;
    }
    const QList<QLineEdit *> edits = tab->findChildren<QLineEdit *>();
    for (QLineEdit *edit : edits) {
        if (edit != nullptr && edit->placeholderText() == QStringLiteral("Find")) {
            return edit;
        }
    }
    return nullptr;
}

QFrame *findBarFor(TextEditorTab *tab)
{
    QLineEdit *findEdit = findEditFor(tab);
    return findEdit != nullptr ? qobject_cast<QFrame *>(findEdit->parentWidget()) : nullptr;
}

QPlainTextEdit *rawEditorFor(TextEditorTab *tab)
{
    return tab != nullptr ? tab->findChild<QPlainTextEdit *>(QStringLiteral("rawTextEditorCanvas")) : nullptr;
}
}

class RawEditorFindShortcutTest : public QObject
{
    Q_OBJECT

private slots:
    void escapeClosesFindBarFromFindPanel();
    void escapeClosesFindBarFromRawEditor();
};

void RawEditorFindShortcutTest::escapeClosesFindBarFromFindPanel()
{
    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    tab.initializeNewDocument(QStringLiteral("test.th"), QStringLiteral("survey test\nendsurvey\n"));
    tab.resize(800, 600);
    tab.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tab));

    tab.showFindBar(false);

    QLineEdit *findEdit = findEditFor(&tab);
    QFrame *findBar = findBarFor(&tab);
    QVERIFY(findEdit != nullptr);
    QVERIFY(findBar != nullptr);
    QVERIFY(findBar->isVisible());
    QTRY_COMPARE(QApplication::focusWidget(), findEdit);

    QTest::keyClick(findEdit, Qt::Key_Escape);

    QTRY_VERIFY(!findBar->isVisible());
}

void RawEditorFindShortcutTest::escapeClosesFindBarFromRawEditor()
{
    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    tab.initializeNewDocument(QStringLiteral("test.th"), QStringLiteral("survey test\nendsurvey\n"));
    tab.resize(800, 600);
    tab.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tab));

    tab.showFindBar(true);

    QFrame *findBar = findBarFor(&tab);
    QPlainTextEdit *editor = rawEditorFor(&tab);
    QVERIFY(findBar != nullptr);
    QVERIFY(editor != nullptr);
    QVERIFY(findBar->isVisible());

    editor->setFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(QApplication::focusWidget(), editor);

    QTest::keyClick(editor, Qt::Key_Escape);

    QTRY_VERIFY(!findBar->isVisible());
}

int runRawEditorFindShortcutTest(int argc, char **argv)
{
    RawEditorFindShortcutTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "RawEditorFindShortcutTest.moc"
