#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
constexpr int kInspectorObjectNameColumn = 0;
constexpr int kInspectorObjectVisibilityColumn = 2;
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;

QString fixtureSource()
{
    return QStringLiteral(
        "encoding utf-8\n"                       // 1
        "scrap a -projection plan\n"             // 2
        "point 0 0 station -name a1\n"           // 3
        "point 60 0 station -name a2\n"          // 4
        "point 60 30 stalactite\n"               // 5
        "line wall\n"                            // 6
        "  0 60\n  120 60\n"                     // 7-8
        "endline\n"                              // 9
        "line wall\n"                            // 10
        "  0 80\n  120 80\n"                     // 11-12
        "endline\n"                              // 13
        "endscrap\n"                             // 14
        "scrap b -projection plan\n"             // 15
        "point 0 -120 station -name b1\n"        // 16
        "line border\n"                          // 17
        "  0 -120\n  60 -120\n"                  // 18-19
        "endline\n"                              // 20
        "endscrap\n");                           // 21
}

QList<QGraphicsItem *> sceneGeometryItems(QGraphicsScene *scene)
{
    QList<QGraphicsItem *> result;
    if (scene == nullptr) {
        return result;
    }
    for (QGraphicsItem *item : scene->items()) {
        if (item == nullptr) {
            continue;
        }
        bool ok = false;
        const int line = item->data(kMapSceneLineNumberRole).toInt(&ok);
        if (ok && line > 0 && !item->data(kMapSceneSelectionGatedRole).toBool()) {
            result.append(item);
        }
    }
    return result;
}

int countVisibleItems(const QList<QGraphicsItem *> &items)
{
    int count = 0;
    for (QGraphicsItem *item : items) {
        if (item != nullptr && item->isVisible()) {
            ++count;
        }
    }
    return count;
}

int countRowsWithVisibilityTooltip(const QStandardItemModel *model, const QString &tooltip, const QModelIndex &parent = QModelIndex())
{
    if (model == nullptr) {
        return 0;
    }
    int count = 0;
    const int rows = model->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex visIdx = model->index(row, kInspectorObjectVisibilityColumn, parent);
        if (visIdx.data(Qt::ToolTipRole).toString() == tooltip) {
            ++count;
        }
        const QModelIndex nameIdx = model->index(row, kInspectorObjectNameColumn, parent);
        count += countRowsWithVisibilityTooltip(model, tooltip, nameIdx);
    }
    return count;
}
}

class MapEditorBulkVisibilityTest final : public QObject
{
    Q_OBJECT

private:
    struct Harness
    {
        QTemporaryDir directory{QDir::current().filePath(QStringLiteral(".verification/bulk-vis-XXXXXX"))};
        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        std::unique_ptr<MapEditorTab> tab;
        QGraphicsView *view = nullptr;
    };

    bool open(Harness &harness, const QString &source = fixtureSource())
    {
        QDir().mkpath(QStringLiteral(".verification"));
        if (!harness.directory.isValid()) {
            return false;
        }
        const QString path = harness.directory.filePath(QStringLiteral("bulk_visibility.th2"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(source.toUtf8());
        file.close();
        harness.tab = std::make_unique<MapEditorTab>(harness.fileSystem, harness.sessionStore, CommandCatalogStore());
        harness.tab->resize(1280, 900);
        harness.tab->show();
        QString error;
        if (!harness.tab->loadFile(path, &error)) {
            qWarning() << error;
            return false;
        }
        harness.view = harness.tab->findChild<QGraphicsView *>();
        if (harness.view == nullptr) {
            return false;
        }
        QTest::qWait(50);
        harness.tab->triggerFit();
        QCoreApplication::processEvents();
        return true;
    }

private slots:
    void testNextScrapRemainsResponsiveOnLargeMap()
    {
        QString source = QStringLiteral("encoding utf-8\n");
        QList<int> scrapLines;
        int line = 2;
        for (int scrap = 0; scrap < 3; ++scrap) {
            scrapLines.append(line);
            source += QStringLiteral("scrap s%1 -projection plan\n").arg(scrap);
            ++line;
            for (int object = 0; object < 600; ++object) {
                source += QStringLiteral("line wall\n %1 %2\n %3 %2\nendline\n")
                              .arg(object % 30 * 20).arg(scrap * 1000 + object / 30 * 20)
                              .arg(object % 30 * 20 + 10);
                line += 4;
            }
            source += QStringLiteral("endscrap\n");
            ++line;
        }
        Harness harness;
        QVERIFY(open(harness, source));
        auto *hide = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsHideAllButton"));
        auto *next = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsNextButton"));
        QVERIFY(hide != nullptr && next != nullptr);
        hide->click();
        harness.tab->goToLine(scrapLines.first());
        QCoreApplication::processEvents();
        QSignalSpy selectionChanges(harness.view->scene(), &QGraphicsScene::selectionChanged);
        for (int step = 0; step < 4; ++step) {
            selectionChanges.clear();
            QElapsedTimer timer;
            timer.start();
            next->click();
            QCoreApplication::processEvents();
            const qint64 elapsed = timer.elapsed();
            qInfo("next-scrap step=%d elapsed_ms=%lld", step, static_cast<long long>(elapsed));
            QCOMPARE(harness.tab->currentLineNumber(), scrapLines.at((step + 1) % 3));
            QCOMPARE(harness.view->scene()->selectedItems().size(), 600);
            QVERIFY(selectionChanges.count() < 20);
            QVERIFY2(elapsed < 1000, qPrintable(QStringLiteral("Next blocked the UI for %1 ms").arg(elapsed)));
        }
    }

    void testBulkHideAndShowAll()
    {
        Harness harness;
        QVERIFY(open(harness));

        auto *hideAllBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsHideAllButton"));
        auto *showAllBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsShowAllButton"));
        QVERIFY(hideAllBtn != nullptr);
        QVERIFY(showAllBtn != nullptr);

        auto *objectsTree = harness.tab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
        QVERIFY(objectsTree != nullptr);
        auto *model = qobject_cast<const QStandardItemModel *>(objectsTree->model());
        QVERIFY(model != nullptr);

        QGraphicsScene *scene = harness.view->scene();
        QVERIFY(scene != nullptr);

        const QList<QGraphicsItem *> items = sceneGeometryItems(scene);
        QVERIFY(!items.isEmpty());

        // Initial state: all geometry items are visible
        const int totalItems = items.size();
        QCOMPARE(countVisibleItems(items), totalItems);
        QVERIFY(countRowsWithVisibilityTooltip(model, QStringLiteral("Hide object")) > 0);
        QCOMPARE(countRowsWithVisibilityTooltip(model, QStringLiteral("Show object")), 0);
        QVERIFY(harness.tab->hiddenObjectLines().isEmpty());

        // Click Hide All
        hideAllBtn->click();
        QCoreApplication::processEvents();

        // All geometry items should now be hidden
        QCOMPARE(countVisibleItems(items), 0);
        QVERIFY(harness.tab->hiddenObjectLines().contains(2));
        QVERIFY(harness.tab->hiddenObjectLines().contains(6));
        QVERIFY(harness.tab->hiddenObjectLines().contains(15));
        QCOMPARE(countRowsWithVisibilityTooltip(model, QStringLiteral("Hide object")), 0);
        QVERIFY(countRowsWithVisibilityTooltip(model, QStringLiteral("Show object")) > 0);

        // Click Show All
        showAllBtn->click();
        QCoreApplication::processEvents();

        // All geometry items should now be visible again
        QCOMPARE(countVisibleItems(items), totalItems);
        QVERIFY(harness.tab->hiddenObjectLines().isEmpty());
        QVERIFY(countRowsWithVisibilityTooltip(model, QStringLiteral("Hide object")) > 0);
        QCOMPARE(countRowsWithVisibilityTooltip(model, QStringLiteral("Show object")), 0);
    }

    void testCycleScrapsWhenAllVisible()
    {
        Harness harness;
        QVERIFY(open(harness));

        auto *nextBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsNextButton"));
        auto *prevBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsPreviousButton"));
        QVERIFY(nextBtn != nullptr);
        QVERIFY(prevBtn != nullptr);

        QGraphicsScene *scene = harness.view->scene();
        QVERIFY(scene != nullptr);
        const QList<QGraphicsItem *> items = sceneGeometryItems(scene);
        const int totalItems = items.size();

        harness.tab->goToLine(2);
        QCoreApplication::processEvents();

        nextBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 15);
        QCOMPARE(countVisibleItems(items), totalItems);

        nextBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 2);
        QCOMPARE(countVisibleItems(items), totalItems);

        prevBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 15);
        QCOMPARE(countVisibleItems(items), totalItems);
    }

    void testCycleScrapsWithSoloVisibility()
    {
        Harness harness;
        QVERIFY(open(harness));

        auto *hideAllBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsHideAllButton"));
        auto *nextBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsNextButton"));
        auto *prevBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsPreviousButton"));
        QVERIFY(hideAllBtn != nullptr);
        QVERIFY(nextBtn != nullptr);
        QVERIFY(prevBtn != nullptr);

        QGraphicsScene *scene = harness.view->scene();
        QVERIFY(scene != nullptr);

        hideAllBtn->click();
        QCoreApplication::processEvents();

        harness.tab->goToLine(2);
        QCoreApplication::processEvents();

        nextBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 15);

        QVERIFY(harness.tab->hiddenObjectLines().contains(2));
        QVERIFY(!harness.tab->hiddenObjectLines().contains(15));
        QVERIFY(!harness.tab->hiddenObjectLines().contains(17));
        for (QGraphicsItem *item : sceneGeometryItems(scene)) {
            const int line = item->data(kMapSceneLineNumberRole).toInt();
            if (line >= 15 && line <= 21) {
                QVERIFY(item->isVisible());
            } else {
                QVERIFY(!item->isVisible());
            }
        }

        nextBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 2);

        QVERIFY(!harness.tab->hiddenObjectLines().contains(2));
        QVERIFY(!harness.tab->hiddenObjectLines().contains(6));
        QVERIFY(harness.tab->hiddenObjectLines().contains(15));
        for (QGraphicsItem *item : sceneGeometryItems(scene)) {
            const int line = item->data(kMapSceneLineNumberRole).toInt();
            if (line >= 2 && line <= 14) {
                QVERIFY(item->isVisible());
            } else {
                QVERIFY(!item->isVisible());
            }
        }

        prevBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 15);

        for (QGraphicsItem *item : sceneGeometryItems(scene)) {
            const int line = item->data(kMapSceneLineNumberRole).toInt();
            if (line >= 15 && line <= 21) {
                QVERIFY(item->isVisible());
            } else {
                QVERIFY(!item->isVisible());
            }
        }
    }

    void testCycleObjectsWithinScrap()
    {
        Harness harness;
        QVERIFY(open(harness));

        auto *nextBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsNextButton"));
        auto *prevBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsPreviousButton"));
        QVERIFY(nextBtn != nullptr);
        QVERIFY(prevBtn != nullptr);

        harness.tab->goToLine(6);
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 6);

        nextBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 10);

        nextBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 6);

        prevBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 10);
    }

    void testScrapCenteringPreservesZoomAndCentersOnPassage()
    {
        Harness harness;
        QVERIFY(open(harness));

        auto *nextBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsNextButton"));
        auto *prevBtn = harness.tab->findChild<QPushButton *>(QStringLiteral("mapObjectsPreviousButton"));
        QVERIFY(nextBtn != nullptr);
        QVERIFY(prevBtn != nullptr);

        // Zoom in via triggerZoomIn
        for (int i = 0; i < 6; ++i) {
            harness.tab->triggerZoomIn();
        }
        QCoreApplication::processEvents();
        const qreal initialScale = harness.view->transform().m11();
        QVERIFY(initialScale > 1.0);

        // Start on scrap a (line 2)
        harness.tab->goToLine(2);
        QCoreApplication::processEvents();

        // Scrap a passage (stalactite and 2 walls) scene center is at ~(599.11, 323.68)
        QPointF centerA = harness.view->mapToScene(harness.view->viewport()->rect().center());
        QVERIFY(qAbs(centerA.x() - 599.11) < 2.0);
        QVERIFY(qAbs(centerA.y() - 323.68) < 2.0);

        // Cycle to scrap b (line 15)
        nextBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 15);

        // Verify zoom scale is preserved!
        QCOMPARE(harness.view->transform().m11(), initialScale);

        // Scrap b passage (border) scene center is at ~(545.81, 628.57)
        QPointF centerB = harness.view->mapToScene(harness.view->viewport()->rect().center());
        QVERIFY(qAbs(centerB.x() - 545.81) < 2.0);
        QVERIFY(qAbs(centerB.y() - 628.57) < 2.0);

        // Cycle back to scrap a
        prevBtn->click();
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 2);
        QCOMPARE(harness.view->transform().m11(), initialScale);

        centerA = harness.view->mapToScene(harness.view->viewport()->rect().center());
        QVERIFY(qAbs(centerA.x() - 599.11) < 2.0);
        QVERIFY(qAbs(centerA.y() - 323.68) < 2.0);

        // Test clicking scrap row directly in inspector tree
        auto *objectsTree = harness.tab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
        QVERIFY(objectsTree != nullptr);
        auto *model = qobject_cast<const QStandardItemModel *>(objectsTree->model());
        QVERIFY(model != nullptr);

        const QModelIndex scrapBIndex = model->index(1, kInspectorObjectNameColumn);
        QCOMPARE(scrapBIndex.data(kInspectorSourceLineRole).toInt(), 15);
        emit objectsTree->clicked(scrapBIndex);
        QCoreApplication::processEvents();
        QCOMPARE(harness.tab->currentLineNumber(), 15);
        QCOMPARE(harness.view->transform().m11(), initialScale);
        centerB = harness.view->mapToScene(harness.view->viewport()->rect().center());
        QVERIFY(qAbs(centerB.x() - 545.81) < 2.0);
        QVERIFY(qAbs(centerB.y() - 628.57) < 2.0);
    }
};

int runMapEditorBulkVisibilityTest(int argc, char **argv)
{
    MapEditorBulkVisibilityTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorBulkVisibilityTest.moc"
