#include "../src/app/text_editor/map_editor/MapEditorPerformanceCounters.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGraphicsView>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
using LineSet = QSet<int>;

QString describe(const LineSet &lines)
{
    QList<int> sorted(lines.cbegin(), lines.cend());
    std::sort(sorted.begin(), sorted.end());
    QStringList parts;
    for (int line : sorted) {
        parts << QString::number(line);
    }
    return QStringLiteral("{%1}").arg(parts.join(QLatin1Char(',')));
}

// Fixture line numbers (1-based):
//  scrap a (line 2): stations 3, 4, 5; non-station point 6; wall 7
//  scrap b (line 12): stations 13, 14
QString fixtureSource()
{
    return QStringLiteral(
        "encoding utf-8\n"                       // 1
        "scrap a -projection plan\n"             // 2
        "point 0 0 station -name a1\n"           // 3
        "point 60 0 station -name a2\n"          // 4
        "point 120 0 station -name a3\n"         // 5
        "point 60 30 stalactite\n"               // 6
        "line wall\n"                            // 7
        "  0 60\n  120 60\n"                     // 8-9
        "endline\n"                              // 10
        "endscrap\n"                             // 11
        "scrap b -projection plan\n"             // 12
        "point 0 -120 station -name b1\n"        // 13
        "point 120 -120 station -name b2\n"      // 14
        "endscrap\n");                           // 15
}

MapEditablePointItem *pointItem(QGraphicsScene *scene, int line)
{
    for (QGraphicsItem *item : scene->items()) {
        auto *point = dynamic_cast<MapEditablePointItem *>(item);
        if (point != nullptr && point->lineNumber() == line) {
            return point;
        }
    }
    return nullptr;
}

QGraphicsPathItem *linePath(QGraphicsScene *scene, int line)
{
    for (QGraphicsItem *item : scene->items()) {
        auto *path = dynamic_cast<QGraphicsPathItem *>(item);
        if (path != nullptr && path->data(kMapItemRole).toInt() == kMapItemGeometryValue
            && path->data(kMapSceneLineNumberRole).toInt() == line
            && path->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeGeneric) {
            return path;
        }
    }
    return nullptr;
}

LineSet linesWithRole(QGraphicsScene *scene, int role)
{
    LineSet lines;
    for (QGraphicsItem *item : scene->items()) {
        if (item->data(role).toBool()) {
            lines.insert(item->data(kMapSceneLineNumberRole).toInt());
        }
    }
    return lines;
}

LineSet selectedPointLines(QGraphicsScene *scene)
{
    LineSet lines;
    for (QGraphicsItem *item : scene->selectedItems()) {
        lines.insert(item->data(kMapSceneLineNumberRole).toInt());
    }
    return lines;
}

void click(QGraphicsView *view, const QPoint &pixel)
{
    QTest::mouseMove(view->viewport(), pixel);
    QCoreApplication::processEvents();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, pixel);
    QCoreApplication::processEvents();
}

QImage renderItem(QGraphicsView *view, MapEditablePointItem *item)
{
    item->update();
    QCoreApplication::processEvents();
    const QPoint center = view->mapFromScene(item->scenePos());
    return view->viewport()->grab(QRect(center - QPoint(24, 24), QSize(48, 48))).toImage();
}

int differingPixels(const QImage &left, const QImage &right)
{
    int count = 0;
    for (int y = 0; y < left.height(); ++y) {
        for (int x = 0; x < left.width(); ++x) {
            if (left.pixel(x, y) != right.pixel(x, y)) {
                ++count;
            }
        }
    }
    return count;
}
}

class MapEditorScrapStationHighlightTest final : public QObject
{
    Q_OBJECT

private:
    struct Harness
    {
        QTemporaryDir directory{QDir::current().filePath(QStringLiteral(".verification/scrap-station-XXXXXX"))};
        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        std::unique_ptr<MapEditorTab> tab;
        QGraphicsView *view = nullptr;
    };

    bool open(Harness &harness)
    {
        QDir().mkpath(QStringLiteral(".verification"));
        if (!harness.directory.isValid()) {
            return false;
        }
        const QString path = harness.directory.filePath(QStringLiteral("stations.th2"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(fixtureSource().toUtf8());
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

    static void clickStation(Harness &harness, int line)
    {
        MapEditablePointItem *point = pointItem(harness.view->scene(), line);
        QVERIFY(point != nullptr);
        click(harness.view, harness.view->mapFromScene(point->scenePos()));
    }

    // State 1 = selected station, state 2 = same-scrap sibling stations,
    // state 3 = everything else (neither role set).
    static void verifyStates(Harness &harness, const LineSet &selected, const LineSet &siblings)
    {
        QGraphicsScene *scene = harness.view->scene();
        QTRY_COMPARE_WITH_TIMEOUT(describe(selectedPointLines(scene)), describe(selected), 1000);
        QCOMPARE(describe(linesWithRole(scene, kMapSceneInteractionSelectionRole)), describe(selected));
        QCOMPARE(describe(linesWithRole(scene, kMapSceneScrapSiblingStationRole)), describe(siblings));
    }

private slots:
    void selectedStationHighlightsItsScrapSiblingStations()
    {
        Harness harness;
        QVERIFY(open(harness));
        QGraphicsScene *scene = harness.view->scene();
        QCOMPARE(pointItem(scene, 3)->data(kMapSceneStationScrapRole).toInt(), 2);
        QCOMPARE(pointItem(scene, 13)->data(kMapSceneStationScrapRole).toInt(), 12);
        QCOMPARE(pointItem(scene, 6)->data(kMapSceneStationScrapRole).toInt(), 0);
        verifyStates(harness, {}, {});

        clickStation(harness, 3);
        if (QTest::currentTestFailed()) {
            return;
        }
        verifyStates(harness, {3}, {4, 5});
        if (QTest::currentTestFailed()) {
            return;
        }

        // Switching to a station of another scrap moves the sibling set
        // without index rebuilds or whole-scene walks beyond those a plain
        // non-station point click already costs (measured at the end).
        auto &scans = mapEditorPerformanceCounters().fullSceneItemScans;
        auto &rebuilds = mapEditorPerformanceCounters().sceneRevisionIndexRebuilds;
        const int scansBefore = scans.load();
        const int rebuildsBefore = rebuilds.load();
        clickStation(harness, 13);
        verifyStates(harness, {13}, {14});
        if (QTest::currentTestFailed()) {
            return;
        }
        QCOMPARE(rebuilds.load() - rebuildsBefore, 0);
        const int stationClickScans = scans.load() - scansBefore;

        clickStation(harness, 5);
        verifyStates(harness, {5}, {3, 4});
        if (QTest::currentTestFailed()) {
            return;
        }

        // Selecting a non-station object clears the sibling highlight.
        QGraphicsPathItem *wall = linePath(scene, 7);
        QVERIFY(wall != nullptr);
        click(harness.view, harness.view->mapFromScene(wall->mapToScene(wall->path().pointAtPercent(0.5))));
        verifyStates(harness, {7}, {});
        if (QTest::currentTestFailed()) {
            return;
        }

        clickStation(harness, 4);
        verifyStates(harness, {4}, {3, 5});
        if (QTest::currentTestFailed()) {
            return;
        }

        // A non-station point of the same scrap clears it too.
        const int pointScansBefore = scans.load();
        clickStation(harness, 6);
        verifyStates(harness, {6}, {});
        QVERIFY2(stationClickScans <= scans.load() - pointScansBefore,
                 qPrintable(QStringLiteral("station click scans %1").arg(stationClickScans)));
    }

    void siblingStationPaintsDistinctlyFromSelectedAndNormal()
    {
        Harness harness;
        QVERIFY(open(harness));
        MapEditablePointItem *point = pointItem(harness.view->scene(), 14);
        QVERIFY(point != nullptr);
        point->setData(kMapSceneInteractionSelectionRole, false);
        point->setData(kMapSceneScrapSiblingStationRole, false);
        const QImage normal = renderItem(harness.view, point);
        point->setData(kMapSceneScrapSiblingStationRole, true);
        const QImage sibling = renderItem(harness.view, point);
        point->setData(kMapSceneScrapSiblingStationRole, false);
        point->setData(kMapSceneInteractionSelectionRole, true);
        const QImage selected = renderItem(harness.view, point);
        QVERIFY2(differingPixels(normal, sibling) > 20, "sibling station must differ from a normal station");
        QVERIFY2(differingPixels(selected, sibling) > 20, "sibling station must differ from the selected station");
    }
};

int runMapEditorScrapStationHighlightTest(int argc, char **argv)
{
    MapEditorScrapStationHighlightTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorScrapStationHighlightTest.moc"
