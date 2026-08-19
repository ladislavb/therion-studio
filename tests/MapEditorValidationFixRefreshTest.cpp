#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsView>
#include <QLineEdit>
#include <QMainWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

using namespace TherionStudio;

namespace
{
void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

QString createTestFile(QTemporaryDir &tempDir, const QByteArray &contents)
{
    const QString filePath = tempDir.path() + QStringLiteral("/validation-fix-refresh.th2");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    file.write(contents);
    file.close();
    return filePath;
}
}

class MapEditorValidationFixRefreshTest final : public QObject
{
    Q_OBJECT

private slots:
    void applyValidationFixesRefreshesMapScene();
    void keepsQuotedDashPrefixedPointLabelVisibleAndEditable();
};

void MapEditorValidationFixRefreshTest::applyValidationFixesRefreshesMapScene()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory.");

    const QString beforeText = QStringLiteral("encoding utf-8\n"
                                              "scrap s1 -projection plan\n"
                                              "point 10 20 label -text visible\n"
                                              "endscrap\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    QVERIFY2(!filePath.isEmpty(), "Failed to create validation fix refresh test file.");

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(800, 600);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    const bool loaded = mapTab->loadFile(filePath, &errorMessage);
    const QByteArray loadFailureMessage =
        errorMessage.isEmpty()
            ? QByteArray("Failed to load validation fix refresh fixture.")
            : QByteArray("Failed to load validation fix refresh fixture: ") + errorMessage.toUtf8();
    QVERIFY2(loaded, loadFailureMessage.constData());
    pumpEvents();

    QVERIFY2(mapTab->testHasMapSceneItemForLine(3),
             "The test fixture point should be present in the initial map scene.");

    TherionSourceDiagnosticFix fix;
    fix.startOffset = beforeText.indexOf(QStringLiteral("point 10 20 label"));
    QVERIFY2(fix.startOffset >= 0, "Failed to locate removable source line in the test fixture.");
    fix.length = QStringLiteral("point 10 20 label -text visible\n").size();
    fix.replacementText = QString();
    fix.description = QStringLiteral("Remove test point");
    fix.expectedSourceDigest = QCryptographicHash::hash(beforeText.toUtf8(), QCryptographicHash::Sha256);

    QVERIFY2(mapTab->applyValidationFixes({fix}),
             "Map editor validation fix application should report success.");
    pumpEvents();

    QVERIFY2(!mapTab->text().contains(QStringLiteral("point 10 20 label")),
             "Validation fix should remove the target source line.");
    QVERIFY2(!mapTab->testHasMapSceneItemForLine(3),
             "Validation fix application should refresh the map scene immediately.");
    QVERIFY2(mapTab->canUndo(), "Validation fix application should remain undoable.");
}

void MapEditorValidationFixRefreshTest::keepsQuotedDashPrefixedPointLabelVisibleAndEditable()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory.");

    const QString contents = QStringLiteral("encoding utf-8\n"
                                            "scrap s1 -projection plan\n"
                                            "point 10 20 label -text \"-sump\"\n"
                                            "endscrap\n");
    const QString filePath = createTestFile(tempDir, contents.toUtf8());
    QVERIFY2(!filePath.isEmpty(), "Failed to create dash-prefixed label fixture.");

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(800, 600);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();

    QString errorMessage;
    QVERIFY2(mapTab->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    pumpEvents();

    auto *mapView = mapTab->findChild<QGraphicsView *>(QStringLiteral("mapCanvasView"));
    QVERIFY2(mapView != nullptr && mapView->scene() != nullptr, "Map graphics view was not found.");
    mapTab->goToLine(3);
    pumpEvents();

    const QList<QGraphicsItem *> selectedItems = mapView->scene()->selectedItems();
    QVERIFY2(!selectedItems.isEmpty(), "Dash-prefixed label point should remain selectable in the map scene.");
    QVERIFY2(selectedItems.constFirst()->boundingRect().width() > 40.0,
             "Dash-prefixed label text should contribute to the rendered point bounds.");

    auto *textEditor = mapTab->findChild<QWidget *>(QStringLiteral("mapObjectQuickTextEditor"));
    QVERIFY2(textEditor != nullptr && textEditor->isVisible(), "Selection inspector text field was not shown.");
    auto *textEdit = textEditor->findChild<QLineEdit *>();
    QVERIFY2(textEdit != nullptr, "Selection inspector text editor was not found.");
    QCOMPARE(textEdit->text(), QStringLiteral("-sump"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MapEditorValidationFixRefreshTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorValidationFixRefreshTest.moc"
