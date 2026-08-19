#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

QString createTestFile(QTemporaryDir &tempDir, const QByteArray &contents)
{
    const QString filePath = tempDir.path() + QStringLiteral("/validation-fixes-transaction.th2");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    file.write(contents);
    file.close();
    return filePath;
}

bool loadTestTab(TextEditorTab *tab, const QString &filePath)
{
    QString errorMessage;
    if (!tab->loadFile(filePath, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << '\n';
        return false;
    }
    tab->resize(640, 480);
    tab->show();
    pumpEvents();
    return true;
}

int runValidationFixApplyUndoRedoTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("point 1 2 station -foo bar\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create validation-fix transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load validation-fix transaction test tab.")) {
        return 1;
    }

    const int removeStart = beforeText.indexOf(QStringLiteral("-foo bar"));
    if (!expect(removeStart >= 0, "Failed to find removable token range in test source.")) {
        return 1;
    }

    TherionSourceDiagnosticFix fix;
    fix.startOffset = removeStart;
    fix.length = QStringLiteral("-foo bar").size();
    fix.replacementText = QString();
    fix.description = QStringLiteral("Drop unsupported option");
    fix.expectedSourceDigest = QCryptographicHash::hash(beforeText.toUtf8(), QCryptographicHash::Sha256);

    const bool applied = tab.applyValidationFixes({fix});
    pumpEvents();
    if (!expect(applied, "Validation fixes transaction should apply a valid fix set.")) {
        return 1;
    }

    const QString afterText = QStringLiteral("point 1 2 station \n");
    if (!expect(tab.text() == afterText,
                "Validation fixes transaction should rewrite source through transaction adapter.")) {
        return 1;
    }

    if (!expect(tab.canUndo(), "Validation fixes transaction should produce an undoable change.")) {
        return 1;
    }

    tab.triggerUndo();
    pumpEvents();
    if (!expect(tab.text() == beforeText,
                "Validation fixes undo should restore the original source text.")) {
        return 1;
    }

    tab.triggerRedo();
    pumpEvents();
    if (!expect(tab.text() == afterText,
                "Validation fixes redo should reapply the transaction result.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (runValidationFixApplyUndoRedoTest() != 0) {
        return 1;
    }

    return 0;
}
