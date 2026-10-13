#include "../src/app/text_editor/map_editor/MapEditorSceneLifecycleController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"

#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QTest>

using namespace TherionStudio;

namespace
{
struct SceneFixture
{
    QGraphicsScene ownedScene;
    QGraphicsScene *scene = &ownedScene;
    QHash<int, QGraphicsItem *> itemsByLine;
    QHash<QString, QGraphicsItem *> vertices;
    QVector<QGraphicsRectItem *> drafts;
    QVector<QGraphicsPixmapItem *> backgrounds;
    bool strokeActive = true;
    QGraphicsPathItem *preview = nullptr;
    QVector<QGraphicsItem *> markers;
    bool updatingSelection = false;
    int selectedBackground = 0;
    int controlsRefreshes = 0;
    MapEditorSceneLifecycleController controller{
        MapEditorSceneLifecycleContext{
            .scene = &scene,
            .itemsByLine = &itemsByLine,
            .vertexItemsByKey = &vertices,
            .draftGeometryItems = &drafts,
            .backgroundImageItems = &backgrounds,
            .interactiveDrawStrokeActive = &strokeActive,
            .interactiveDrawPreviewPath = &preview,
            .interactiveDrawPreviewMarkers = &markers,
            .updatingSelection = &updatingSelection,
            .selectedBackgroundLayerIndex = &selectedBackground,
            .refreshBackgroundLayerControls = [this]() { ++controlsRefreshes; },
            .applyBackgroundLayerStackingOrder = []() {},
            .setSelectedBackgroundLayerIndexInternal = [](int) {},
            .mapBackgroundFitBounds = []() { return QRectF(); }}};
};
}

class MapEditorSceneLifecycleTest final : public QObject
{
    Q_OBJECT

private slots:
    void preservesLayersAndDraftsAcrossRepeatedClears()
    {
        SceneFixture fixture;
        auto *registeredBackground = fixture.scene->addPixmap(QPixmap(8, 8));
        auto *discoveredBackground = fixture.scene->addPixmap(QPixmap(4, 4));
        registeredBackground->setPos(17, 23);
        registeredBackground->setVisible(false);
        fixture.backgrounds.append(registeredBackground);
        auto *draft = new MapDraftGeometryItem(1, DraftGeometryKind::Point);
        fixture.scene->addItem(draft);
        fixture.drafts.append(draft);

        for (int cycle = 0; cycle < 3; ++cycle) {
            auto *geometry = fixture.scene->addRect(0, 0, 10, 10);
            new QGraphicsRectItem(0, 0, 1, 1, geometry);
            fixture.itemsByLine.insert(1, geometry);
            fixture.vertices.insert(QStringLiteral("vertex"), geometry);
            fixture.preview = fixture.scene->addPath(QPainterPath());
            fixture.markers.append(fixture.scene->addEllipse(0, 0, 1, 1));
            fixture.strokeActive = true;
            fixture.controller.clearMapScene();

            QVERIFY(fixture.scene->items().isEmpty());
            QVERIFY(fixture.itemsByLine.isEmpty());
            QVERIFY(fixture.vertices.isEmpty());
            QVERIFY(!fixture.strokeActive);
            QVERIFY(fixture.preview == nullptr);
            QVERIFY(fixture.markers.isEmpty());
            QVERIFY(!fixture.updatingSelection);
            QCOMPARE(fixture.backgrounds.size(), 2);
            QVERIFY(fixture.backgrounds.contains(registeredBackground));
            QVERIFY(fixture.backgrounds.contains(discoveredBackground));
            QCOMPARE(fixture.drafts.size(), 1);
            QCOMPARE(fixture.drafts.first(), draft);
            QVERIFY(registeredBackground->scene() == nullptr);
            QCOMPARE(registeredBackground->pos(), QPointF(17, 23));
            QVERIFY(!registeredBackground->isVisible());

            fixture.controller.restoreBackgroundImageItems();
            fixture.controller.restoreDraftGeometryItems();
            fixture.controller.restoreBackgroundImageItems();
            fixture.controller.restoreDraftGeometryItems();
            QCOMPARE(fixture.scene->items().size(), 3);
            QCOMPARE(draft->zValue(), 20.0);
        }
        QCOMPARE(fixture.controlsRefreshes, 6);
    }

    void clearRestoresSelectionGuard_data()
    {
        QTest::addColumn<bool>("alreadyUpdating");
        QTest::newRow("normal-clear") << false;
        QTest::newRow("nested-clear") << true;
    }

    void clearRestoresSelectionGuard()
    {
        QFETCH(bool, alreadyUpdating);
        SceneFixture fixture;
        fixture.updatingSelection = alreadyUpdating;
        auto *item = fixture.scene->addRect(0, 0, 10, 10);
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setSelected(true);
        QVERIFY(item->isSelected());
        bool selectionSignalObserved = false;
        QObject::connect(fixture.scene, &QGraphicsScene::selectionChanged,
                         fixture.scene, [&]() {
            selectionSignalObserved = true;
            QVERIFY(fixture.updatingSelection);
        });
        fixture.controller.clearMapScene();
        QVERIFY(selectionSignalObserved);
        QCOMPARE(fixture.updatingSelection, alreadyUpdating);
    }

    void largeSceneClearTiming()
    {
        SceneFixture fixture;
        constexpr int itemCount = 12000;
        for (int sample = 0; sample < 5; ++sample) {
            for (int index = 0; index < itemCount; ++index) {
                fixture.scene->addRect(index % 120, index / 120, 1, 1);
            }
            QElapsedTimer timer;
            timer.start();
            fixture.controller.clearMapScene();
            const qint64 clearNs = timer.nsecsElapsed();
            qInfo().nospace() << "map-scene-lifecycle-clear items=" << itemCount
                              << " sample=" << sample << " clear_ns=" << clearNs;
            QVERIFY(fixture.scene->items().isEmpty());
        }
    }
};

int runMapEditorSceneLifecycleTest(int argc, char **argv)
{
    MapEditorSceneLifecycleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorSceneLifecycleTest.moc"
