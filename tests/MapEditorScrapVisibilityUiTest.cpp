// Fading and hiding scraps in a loaded map: scene item opacity, visibility and
// picking, the Objects view eye icon cycle, and moving an object to another
// scrap from the same menus. The pure rules are covered by
// MapEditorScrapVisibilityTest and MapEditorMoveToScrapPlannerTest.

#include "../src/app/text_editor/map_editor/MapEditorScrapVisibility.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMenu>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
#include <QVBoxLayout>

using namespace TherionStudio;

namespace
{
constexpr int kInspectorSourceLineRoleForTest = Qt::UserRole + 700;
constexpr int kInspectorObjectVisibilityColumnForTest = 2;
constexpr int kScrapALine = 2;
constexpr int kScrapAPointLine = 3;
constexpr int kScrapBLine = 6;
constexpr int kScrapBPointLine = 7;

void pumpEvents()
{
    for (int index = 0; index < 8; ++index) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

// Scene items drawn for a source line, leaving out selection-only items such as handles.
QList<QGraphicsItem *> objectItems(QGraphicsScene *scene, int lineNumber)
{
    QList<QGraphicsItem *> items;
    for (QGraphicsItem *item : scene->items()) {
        if (item->data(kMapSceneLineNumberRole).toInt() == lineNumber
            && !item->data(kMapSceneSelectionGatedRole).toBool()) {
            items.append(item);
        }
    }
    return items;
}

QMenu *openPopupMenu()
{
    pumpEvents();
    return qobject_cast<QMenu *>(QApplication::activePopupWidget());
}

QAction *actionWithText(const QMenu *menu, const QString &text)
{
    for (QAction *action : menu->actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

QModelIndex visibilityIndexForLine(const QStandardItemModel *model, int lineNumber, const QModelIndex &parent = {})
{
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const QModelIndex nameIndex = model->index(row, 0, parent);
        if (nameIndex.data(kInspectorSourceLineRoleForTest).toInt() == lineNumber) {
            return model->index(row, kInspectorObjectVisibilityColumnForTest, parent);
        }
        const QModelIndex child = visibilityIndexForLine(model, lineNumber, nameIndex);
        if (child.isValid()) {
            return child;
        }
    }
    return {};
}
}

class MapEditorScrapVisibilityUiTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void fadesAndHidesOtherScraps();
    void eyeIconCyclesShownFadedHidden();
    void emptyCanvasMenuShowsAllScrapsOnlyWhenNeeded();
    void objectsViewMenuOffersScrapSubmenus();
    void movesObjectToScrapWithUndo();

private:
    QTemporaryDir *temporaryDirectory_ = nullptr;
    QtFileSystem *fileSystem_ = nullptr;
    FakeSessionStore *sessionStore_ = nullptr;
    QMainWindow *hostWindow_ = nullptr;
    MapEditorTab *mapTab_ = nullptr;
    QGraphicsScene *scene_ = nullptr;
};

void MapEditorScrapVisibilityUiTest::init()
{
    temporaryDirectory_ = new QTemporaryDir;
    QVERIFY(temporaryDirectory_->isValid());
    const QString filePath = QDir(temporaryDirectory_->path()).filePath(QStringLiteral("scraps.th2"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("encoding utf-8\n"
               "scrap a -projection plan\n"
               "point 0 0 station -name A1\n"
               "endscrap\n"
               "\n"
               "scrap b -projection plan\n"
               "point 20 20 station -name B1\n"
               "endscrap\n");
    file.close();

    fileSystem_ = new QtFileSystem;
    sessionStore_ = new FakeSessionStore;
    hostWindow_ = new QMainWindow;
    hostWindow_->resize(800, 600);
    auto *central = new QWidget(hostWindow_);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    mapTab_ = new MapEditorTab(*fileSystem_, *sessionStore_, CommandCatalogStore(), central);
    layout->addWidget(mapTab_);
    hostWindow_->setCentralWidget(central);
    hostWindow_->show();
    pumpEvents();

    QString errorMessage;
    QVERIFY2(mapTab_->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    pumpEvents();

    auto *mapView = mapTab_->findChild<QGraphicsView *>(QStringLiteral("mapCanvasView"));
    QVERIFY(mapView != nullptr && mapView->scene() != nullptr);
    scene_ = mapView->scene();
    QVERIFY(!objectItems(scene_, kScrapAPointLine).isEmpty());
    QVERIFY(!objectItems(scene_, kScrapBPointLine).isEmpty());
}

void MapEditorScrapVisibilityUiTest::cleanup()
{
    delete hostWindow_;
    delete sessionStore_;
    delete fileSystem_;
    delete temporaryDirectory_;
    hostWindow_ = nullptr;
    sessionStore_ = nullptr;
    fileSystem_ = nullptr;
    temporaryDirectory_ = nullptr;
    mapTab_ = nullptr;
    scene_ = nullptr;
}

void MapEditorScrapVisibilityUiTest::fadesAndHidesOtherScraps()
{
    QHash<QGraphicsItem *, Qt::MouseButtons> originalButtons;
    for (QGraphicsItem *item : objectItems(scene_, kScrapBPointLine)) {
        originalButtons.insert(item, item->acceptedMouseButtons());
    }
    QVERIFY(std::any_of(originalButtons.cbegin(), originalButtons.cend(), [](Qt::MouseButtons buttons) {
        return buttons != Qt::NoButton;
    }));

    mapTab_->setMapScrapVisibilityForTest(kScrapAPointLine,
                                          MapEditorScrapVisibilityTarget::OtherScraps,
                                          MapEditorObjectVisibility::Faded);
    pumpEvents();
    for (QGraphicsItem *item : objectItems(scene_, kScrapBPointLine)) {
        QVERIFY(item->isVisible());
        QCOMPARE(item->opacity(), kMapEditorFadedObjectOpacity);
        QCOMPARE(item->acceptedMouseButtons(), Qt::MouseButtons(Qt::NoButton));
    }
    for (QGraphicsItem *item : objectItems(scene_, kScrapAPointLine)) {
        QVERIFY(item->isVisible());
        QCOMPARE(item->opacity(), 1.0);
    }

    mapTab_->setMapScrapVisibilityForTest(kScrapBLine,
                                          MapEditorScrapVisibilityTarget::ThisScrap,
                                          MapEditorObjectVisibility::Hidden);
    pumpEvents();
    for (QGraphicsItem *item : objectItems(scene_, kScrapBPointLine)) {
        QVERIFY(!item->isVisible());
    }

    mapTab_->showAllMapObjectsForTest();
    pumpEvents();
    for (QGraphicsItem *item : objectItems(scene_, kScrapBPointLine)) {
        QVERIFY(item->isVisible());
        QCOMPARE(item->opacity(), 1.0);
        QCOMPARE(item->acceptedMouseButtons(), originalButtons.value(item));
    }
}

void MapEditorScrapVisibilityUiTest::eyeIconCyclesShownFadedHidden()
{
    auto *tree = mapTab_->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    QVERIFY(tree != nullptr);
    auto *model = qobject_cast<QStandardItemModel *>(tree->model());
    QVERIFY(model != nullptr);

    const auto clickEye = [&]() {
        const QModelIndex eye = visibilityIndexForLine(model, kScrapALine);
        QVERIFY(eye.isValid());
        emit tree->clicked(eye);
        pumpEvents();
    };

    clickEye();
    for (QGraphicsItem *item : objectItems(scene_, kScrapAPointLine)) {
        QVERIFY(item->isVisible());
        QCOMPARE(item->opacity(), kMapEditorFadedObjectOpacity);
    }
    QCOMPARE(visibilityIndexForLine(model, kScrapALine).data(Qt::ToolTipRole).toString(), QStringLiteral("Hide object"));

    clickEye();
    for (QGraphicsItem *item : objectItems(scene_, kScrapAPointLine)) {
        QVERIFY(!item->isVisible());
    }
    QCOMPARE(visibilityIndexForLine(model, kScrapALine).data(Qt::ToolTipRole).toString(), QStringLiteral("Show object"));

    clickEye();
    for (QGraphicsItem *item : objectItems(scene_, kScrapAPointLine)) {
        QVERIFY(item->isVisible());
        QCOMPARE(item->opacity(), 1.0);
    }
    QCOMPARE(visibilityIndexForLine(model, kScrapALine).data(Qt::ToolTipRole).toString(), QStringLiteral("Fade object"));

    // The other scrap is never touched by this scrap's eye icon.
    for (QGraphicsItem *item : objectItems(scene_, kScrapBPointLine)) {
        QVERIFY(item->isVisible());
        QCOMPARE(item->opacity(), 1.0);
    }
}

void MapEditorScrapVisibilityUiTest::emptyCanvasMenuShowsAllScrapsOnlyWhenNeeded()
{
    const QPoint globalPosition = hostWindow_->mapToGlobal(QPoint(100, 100));
    mapTab_->showEmptyMapCanvasContextMenuForTest(globalPosition);
    QCOMPARE(openPopupMenu(), nullptr);

    mapTab_->setMapScrapVisibilityForTest(kScrapBLine,
                                          MapEditorScrapVisibilityTarget::ThisScrap,
                                          MapEditorObjectVisibility::Hidden);
    pumpEvents();
    mapTab_->showEmptyMapCanvasContextMenuForTest(globalPosition);
    QMenu *menu = openPopupMenu();
    QVERIFY(menu != nullptr);
    QAction *showAll = actionWithText(menu, QStringLiteral("Show All Scraps"));
    QVERIFY(showAll != nullptr && showAll->isEnabled());
    showAll->trigger();
    menu->close();
    pumpEvents();

    for (QGraphicsItem *item : objectItems(scene_, kScrapBPointLine)) {
        QVERIFY(item->isVisible());
    }
}

void MapEditorScrapVisibilityUiTest::objectsViewMenuOffersScrapSubmenus()
{
    auto *tree = mapTab_->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    QVERIFY(tree != nullptr);
    auto *model = qobject_cast<QStandardItemModel *>(tree->model());
    QVERIFY(model != nullptr);
    const QModelIndex pointIndex = visibilityIndexForLine(model, kScrapBPointLine).siblingAtColumn(0);
    QVERIFY(pointIndex.isValid());
    tree->scrollTo(pointIndex);
    const QPoint position = tree->visualRect(pointIndex).center();

    QContextMenuEvent event(QContextMenuEvent::Mouse, position, tree->viewport()->mapToGlobal(position));
    QCoreApplication::sendEvent(tree->viewport(), &event);
    QMenu *menu = openPopupMenu();
    QVERIFY(menu != nullptr);
    QAction *thisScrap = actionWithText(menu, QStringLiteral("This Scrap"));
    QVERIFY(thisScrap != nullptr && thisScrap->menu() != nullptr);
    QVERIFY(actionWithText(menu, QStringLiteral("Other Scraps")) != nullptr);
    QAction *shown = actionWithText(thisScrap->menu(), QStringLiteral("Show"));
    QVERIFY(shown != nullptr && shown->isChecked());

    QAction *fade = actionWithText(thisScrap->menu(), QStringLiteral("Fade"));
    QVERIFY(fade != nullptr);
    fade->trigger();
    menu->close();
    pumpEvents();

    for (QGraphicsItem *item : objectItems(scene_, kScrapBPointLine)) {
        QCOMPARE(item->opacity(), kMapEditorFadedObjectOpacity);
    }
    for (QGraphicsItem *item : objectItems(scene_, kScrapAPointLine)) {
        QCOMPARE(item->opacity(), 1.0);
    }
}

void MapEditorScrapVisibilityUiTest::movesObjectToScrapWithUndo()
{
    const QString before = mapTab_->text();
    auto *tree = mapTab_->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
    QVERIFY(tree != nullptr);
    auto *model = qobject_cast<QStandardItemModel *>(tree->model());
    QVERIFY(model != nullptr);
    const QModelIndex pointIndex = visibilityIndexForLine(model, kScrapAPointLine).siblingAtColumn(0);
    QVERIFY(pointIndex.isValid());
    const QPoint position = tree->visualRect(pointIndex).center();

    QContextMenuEvent event(QContextMenuEvent::Mouse, position, tree->viewport()->mapToGlobal(position));
    QCoreApplication::sendEvent(tree->viewport(), &event);
    QMenu *menu = openPopupMenu();
    QVERIFY(menu != nullptr);
    QAction *moveTo = actionWithText(menu, QStringLiteral("Move to Scrap"));
    QVERIFY(moveTo != nullptr && moveTo->menu() != nullptr);
    QAction *target = actionWithText(moveTo->menu(), QStringLiteral("b"));
    QVERIFY(target != nullptr && target->isEnabled());
    target->trigger();
    menu->close();
    pumpEvents();

    QCOMPARE(mapTab_->text(),
             QStringLiteral("encoding utf-8\n"
                            "scrap a -projection plan\n"
                            "endscrap\n"
                            "\n"
                            "scrap b -projection plan\n"
                            "point 20 20 station -name B1\n"
                            "point 0 0 station -name A1\n"
                            "endscrap\n"));

    mapTab_->triggerUndo();
    pumpEvents();
    QCOMPARE(mapTab_->text(), before);
}

int runMapEditorScrapVisibilityUiTest(int argc, char **argv)
{
    MapEditorScrapVisibilityUiTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorScrapVisibilityUiTest.moc"
