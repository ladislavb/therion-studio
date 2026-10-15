#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextLayout>
#include <QVBoxLayout>

using namespace TherionStudio;

namespace
{
bool tokenHasWaveUnderline(const QTextBlock &block, const QString &token)
{
    const int start = block.text().indexOf(token);
    if (start < 0 || block.layout() == nullptr) {
        return false;
    }
    for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
        if (range.start <= start && range.start + range.length > start
            && range.format.underlineStyle() == QTextCharFormat::WaveUnderline) {
            return true;
        }
    }
    return false;
}
}

class MapEditorRawValidationDeferralTest final : public QObject
{
    Q_OBJECT

private slots:
    void validatesHiddenRawTextOnlyWhenRawIsShown()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = QDir(dir.path()).filePath(QStringLiteral("deferral.th2"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("encoding utf-8\nscrap s1 -projection plan -bogusoption\npoint 0 0 station -name 1\nendscrap\n");
        file.close();

        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        QMainWindow host;
        auto *central = new QWidget(&host);
        auto *layout = new QVBoxLayout(central);
        auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
        layout->addWidget(mapTab);
        host.setCentralWidget(central);
        host.show();
        QString error;
        QVERIFY2(mapTab->loadFile(path, &error), qPrintable(error));
        QCoreApplication::processEvents();

        auto *editor = mapTab->findChild<QPlainTextEdit *>(QStringLiteral("rawTextEditorCanvas"));
        QVERIFY(editor != nullptr);
        const QTextBlock scrapLine = editor->document()->findBlockByNumber(1);
        QVERIFY(scrapLine.text().contains(QStringLiteral("-bogusoption")));

        // The Visual workspace hides the raw text, so it is not validated on each edit.
        QCOMPARE(mapTab->workspaceMode(), MapEditorTab::WorkspaceMode::Visual);
        QVERIFY(!tokenHasWaveUnderline(scrapLine, QStringLiteral("-bogusoption")));

        // Showing Raw applies the pending validation highlighting once.
        mapTab->setWorkspaceMode(MapEditorTab::WorkspaceMode::Raw);
        QCoreApplication::processEvents();
        QVERIFY(tokenHasWaveUnderline(editor->document()->findBlockByNumber(1), QStringLiteral("-bogusoption")));

        // Back in Visual, already-applied formats stay; new edits are again not validated until Raw is shown.
        mapTab->setWorkspaceMode(MapEditorTab::WorkspaceMode::Visual);
        QCoreApplication::processEvents();
        QTextCursor cursor(editor->document()->findBlockByNumber(2));
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText(QStringLiteral(" -othertypo"));
        QCoreApplication::processEvents();
        QVERIFY(!tokenHasWaveUnderline(editor->document()->findBlockByNumber(2), QStringLiteral("-othertypo")));
        mapTab->setWorkspaceMode(MapEditorTab::WorkspaceMode::Raw);
        QCoreApplication::processEvents();
        QVERIFY(tokenHasWaveUnderline(editor->document()->findBlockByNumber(2), QStringLiteral("-othertypo")));
    }
};

int runMapEditorRawValidationDeferralTest(int argc, char **argv)
{
    MapEditorRawValidationDeferralTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorRawValidationDeferralTest.moc"
