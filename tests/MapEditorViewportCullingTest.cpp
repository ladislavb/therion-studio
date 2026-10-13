#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/Th2GeometryProjection.h"
#include "../src/core/TherionSourceDocument.h"
#include "../src/core/TherionSourceLogicalDocument.h"
#include "FakeSessionStore.h"

#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QScrollBar>
#include <QtTest/QtTest>

#include <algorithm>

using namespace TherionStudio;

namespace
{
class PaintSubmissionScene final : public QGraphicsScene
{
public:
    int submittedItems = 0;

protected:
    void drawItems(QPainter *painter, int count, QGraphicsItem *items[],
                   const QStyleOptionGraphicsItem options[], QWidget *widget) override
    {
        submittedItems += count;
        QGraphicsScene::drawItems(painter, count, items, options, widget);
    }
};

QString separatedScraps(int count)
{
    QString text;
    for (int scrap = 0; scrap < count; ++scrap) {
        const int x = scrap * 300;
        text += QStringLiteral(
            "scrap s%1 -projection plan\n"
            "line wall -id w%1\n%2 0\n%3 20\n%4 0\nendline\n"
            "line wall -id v%1\n%2 60\n%3 40\n%4 60\nendline\nendscrap\n")
                    .arg(scrap).arg(x).arg(x + 50).arg(x + 100);
    }
    return text;
}

QImage renderRegion(PaintSubmissionScene &scene, const QRectF &region)
{
    QImage image(640, 480, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    scene.submittedItems = 0;
    scene.render(&painter, QRectF(0, 0, 640, 480), region, Qt::IgnoreAspectRatio);
    return image;
}

QImage renderView(QGraphicsView &view)
{
    QImage image(640, 480, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    view.render(&painter);
    return image;
}

qint64 medianViewNanoseconds(QGraphicsView &view)
{
    QVector<qint64> samples;
    for (int sample = 0; sample < 11; ++sample) {
        QElapsedTimer timer;
        timer.start();
        for (int frame = 0; frame < 20; ++frame) {
            renderView(view);
        }
        samples.append(timer.nsecsElapsed() / 20);
    }
    std::sort(samples.begin(), samples.end());
    return samples.at(samples.size() / 2);
}

qint64 medianRenderNanoseconds(PaintSubmissionScene &scene, const QRectF &region)
{
    QVector<qint64> samples;
    for (int sample = 0; sample < 11; ++sample) {
        QElapsedTimer timer;
        timer.start();
        for (int frame = 0; frame < 20; ++frame) {
            renderRegion(scene, region);
        }
        samples.append(timer.nsecsElapsed() / 20);
    }
    std::sort(samples.begin(), samples.end());
    return samples.at(samples.size() / 2);
}
}

class MapEditorViewportCullingTest final : public QObject
{
    Q_OBJECT

private slots:
    void productionViewportKeepsItemsSelectionAndSource()
    {
        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        MapEditorTab tab(fileSystem, sessionStore, CommandCatalogStore());
        tab.resize(1000, 700);
        tab.initializeNewDocument(QStringLiteral("viewport-synthetic.th2"), separatedScraps(40));
        tab.show();
        auto *view = tab.findChild<QGraphicsView *>();
        QVERIFY(view != nullptr);
        QTRY_VERIFY(view->scene() != nullptr);
        tab.goToLine(2);
        QCoreApplication::processEvents();
        auto *scene = view->scene();
        const auto sceneItems = scene->items();
        QGraphicsItem *first = nullptr;
        QGraphicsItem *last = nullptr;
        for (auto *item : sceneItems) {
            if (item->data(kMapSceneSelectionSubtypeRole).toInt() != kMapSceneSelectionSubtypeGeneric
                || item->data(kMapItemRole).toInt() != kMapItemGeometryValue) {
                continue;
            }
            if (item->data(kMapSceneLineNumberRole).toInt() == 2) {
                first = item;
            }
            if (item->data(kMapSceneLineNumberRole).toInt() == 2 + 39 * 12) {
                last = item;
            }
        }
        QVERIFY(first != nullptr);
        QVERIFY(last != nullptr);
        const QString beforeText = tab.text();
        const bool beforeDirty = tab.isDirty();
        const bool beforeUndo = tab.canUndo();
        const auto selected = scene->selectedItems();
        QVERIFY(!selected.isEmpty());
        const QRectF fitBounds = scene->sceneRect();
        QHash<QGraphicsItem *, bool> visibility;
        for (auto *item : sceneItems) {
            visibility.insert(item, item->isVisible());
        }
        for (int step = 0; step < 18; ++step) {
            tab.triggerZoomIn();
        }
        view->centerOn(first->sceneBoundingRect().center());
        QCoreApplication::processEvents();
        const int firstH = view->horizontalScrollBar()->value();
        const int firstV = view->verticalScrollBar()->value();
        const QImage firstPixels = renderView(*view);
        const QPointF hitPoint = first->sceneBoundingRect().center();
        const auto hits = scene->items(hitPoint, Qt::IntersectsItemShape,
                                       Qt::DescendingOrder, view->viewportTransform());

        QGraphicsView second(scene);
        second.resize(640, 480);
        second.setTransform(view->transform());
        second.centerOn(last->sceneBoundingRect().center());
        const QImage secondPixels = renderView(second);
        view->centerOn(last->sceneBoundingRect().center());
        QCoreApplication::processEvents();
        QCOMPARE(renderView(second), secondPixels);
        for (auto *item : selected) {
            QVERIFY(item->isSelected());
        }
        view->horizontalScrollBar()->setValue(firstH);
        view->verticalScrollBar()->setValue(firstV);
        QCoreApplication::processEvents();
        QCOMPARE(renderView(*view), firstPixels);
        QCOMPARE(scene->items(hitPoint, Qt::IntersectsItemShape,
                              Qt::DescendingOrder, view->viewportTransform()), hits);
        const QTransform beforeZoom = view->transform();
        tab.triggerZoomIn();
        tab.triggerZoomOut();
        QVERIFY(qAbs(view->transform().m11() - beforeZoom.m11()) < 1e-9);
        QVERIFY(qAbs(view->transform().m22() - beforeZoom.m22()) < 1e-9);
        // Cursor-anchored zoom accumulates subpixel translation; compare pixels
        // only after restoring the identical camera, not merely its scale.
        view->setTransform(beforeZoom);
        view->horizontalScrollBar()->setValue(firstH);
        view->verticalScrollBar()->setValue(firstV);
        QCoreApplication::processEvents();
        QCOMPARE(renderView(*view), firstPixels);
        QCOMPARE(scene->items().size(), sceneItems.size());
        QCOMPARE(scene->sceneRect(), fitBounds);
        for (auto it = visibility.cbegin(); it != visibility.cend(); ++it) {
            QCOMPARE(it.key()->isVisible(), it.value());
        }
        QCOMPARE(tab.text(), beforeText);
        QCOMPARE(tab.isDirty(), beforeDirty);
        QCOMPARE(tab.canUndo(), beforeUndo);
    }

    void nativeCullingVersusExplicitHide()
    {
        constexpr int scrapCount = 400;
        const QString text = separatedScraps(scrapCount);
        const auto source = TherionSourceDocument::fromText(text);
        const auto logical = TherionSourceLogicalDocument::fromSourceDocument(source);
        const auto projection = Th2GeometryProjection::fromDocuments(source, logical);
        const auto features = collectGeometryFeatures(projection, logical.commands());
        QCOMPARE(features.size(), scrapCount * 2);

        PaintSubmissionScene scene;
        QHash<int, QGraphicsItem *> itemsByLine;
        QHash<QString, QGraphicsItem *> verticesByKey;
        renderMapWorkspaceScene(&scene, QString(), {}, features, QRectF(0, 0, 100, 100),
                                false, &itemsByLine, &verticesByKey, {}, {}, {}, {}, {}, {},
                                MapEditorObjectStyleCatalog(), false);
        const auto allItems = scene.items();
        QVERIFY(allItems.size() > scrapCount * 5);
        const QRectF firstRegion = itemsByLine.value(features.first().lineNumber)
                                      ->sceneBoundingRect().adjusted(-80, -100, 80, 100);
        const QRectF farRegion = itemsByLine.value(features.at(features.size() - 2).lineNumber)
                                    ->sceneBoundingRect().adjusted(-80, -100, 80, 100);

        const QImage baseline = renderRegion(scene, firstRegion);
        const int nativeSubmissions = scene.submittedItems;
        QVERIFY(nativeSubmissions > 0);
        QVERIFY(nativeSubmissions < allItems.size() / 20);
        const QImage farBaseline = renderRegion(scene, farRegion);
        QVERIFY(scene.submittedItems > 0);
        QCOMPARE(renderRegion(scene, firstRegion), baseline);
        const qint64 nativeNs = medianRenderNanoseconds(scene, firstRegion);
        QGraphicsView view(&scene);
        view.resize(640, 480);
        view.fitInView(firstRegion, Qt::KeepAspectRatio);
        const QImage viewBaseline = renderView(view);
        const qint64 nativeViewNs = medianViewNanoseconds(view);

        auto *farSelection = itemsByLine.value(features.at(features.size() - 2).lineNumber);
        farSelection->setSelected(true);
        QVERIFY(farSelection->isSelected());
        renderRegion(scene, firstRegion);
        QVERIFY(farSelection->isSelected());
        farSelection->setSelected(false);

        QHash<int, int> scrapsByLine;
        for (const auto &feature : features) {
            scrapsByLine.insert(feature.lineNumber, feature.scrapLineNumber);
        }
        QSet<int> retainedScraps;
        for (auto *item : scene.items(firstRegion.adjusted(-64, -64, 64, 64),
                                     Qt::IntersectsItemBoundingRect)) {
            retainedScraps.insert(scrapsByLine.value(item->data(kMapSceneLineNumberRole).toInt()));
        }
        QVector<QGraphicsItem *> hidden;
        QElapsedTimer hideTimer;
        hideTimer.start();
        for (auto *item : allItems) {
            if (item->isVisible()
                && !retainedScraps.contains(scrapsByLine.value(item->data(kMapSceneLineNumberRole).toInt()))) {
                hidden.append(item);
                item->hide();
            }
        }
        const qint64 hideNs = hideTimer.nsecsElapsed();
        QVERIFY(hidden.size() > scrapCount);
        QCOMPARE(renderRegion(scene, firstRegion), baseline);
        QCOMPARE(scene.submittedItems, nativeSubmissions);
        QCOMPARE(scene.items().size(), allItems.size());
        const qint64 hiddenNs = medianRenderNanoseconds(scene, firstRegion);
        QCOMPARE(renderView(view), viewBaseline);
        const qint64 hiddenViewNs = medianViewNanoseconds(view);
        hideTimer.restart();
        for (auto *item : hidden) {
            item->show();
        }
        const qint64 restoreNs = hideTimer.nsecsElapsed();
        QCOMPARE(renderRegion(scene, firstRegion), baseline);
        QCOMPARE(renderRegion(scene, farRegion), farBaseline);
        QCOMPARE(renderView(view), viewBaseline);
        QCOMPARE(source.toText(), text);
        farSelection->setSelected(true);
        QVERIFY(farSelection->isSelected());
        farSelection->hide();
        QVERIFY(!farSelection->isSelected());
        farSelection->show();
        QVERIFY(!farSelection->isSelected());

        qInfo().nospace() << "VIEWPORT_BASELINE scraps=" << scrapCount
                         << " scene_items=" << allItems.size()
                         << " native_paint_submissions=" << nativeSubmissions
                         << " hidden_paint_submissions=" << nativeSubmissions
                         << " explicitly_hidden=" << hidden.size()
                         << " native_render_median_ns=" << nativeNs
                         << " hidden_render_median_ns=" << hiddenNs
                         << " native_view_median_ns=" << nativeViewNs
                         << " hidden_view_median_ns=" << hiddenViewNs
                         << " hide_transition_ns=" << hideNs
                         << " restore_transition_ns=" << restoreNs;
    }
};

int runMapEditorViewportCullingTest(int argc, char **argv)
{
    MapEditorViewportCullingTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorViewportCullingTest.moc"
