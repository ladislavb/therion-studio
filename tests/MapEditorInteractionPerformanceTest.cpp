#include "../src/app/text_editor/map_editor/MapEditorInspectorData.h"
#include "../src/app/text_editor/map_editor/MapEditorPerformanceCounters.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTemporaryDir>
#include <QTreeView>
#include <QUndoStack>
#include <QtTest/QtTest>

using namespace TherionStudio;

// Work-count regression tests for the interaction hot paths measured in
// PERF-AUDIT-TH2.md (proposals 1, 2, 7, 8). They drive the real MapEditorTab
// and count the expensive work through MapEditorPerformanceCounters.
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

LineSet hoveredLines(QGraphicsScene *scene)
{
    LineSet lines;
    for (QGraphicsItem *item : scene->items()) {
        if (item->data(kMapSceneInteractionHoverRole).toBool()) {
            lines.insert(item->data(kMapSceneLineNumberRole).toInt());
        }
    }
    return lines;
}

LineSet qtSelectedLines(QGraphicsScene *scene)
{
    LineSet lines;
    for (QGraphicsItem *item : scene->selectedItems()) {
        const int line = item->data(kMapSceneLineNumberRole).toInt();
        if (line > 0) {
            lines.insert(line);
        }
    }
    return lines;
}

QPoint pixelOnLine(QGraphicsView *view, int line, qreal percent)
{
    QGraphicsPathItem *path = linePath(view->scene(), line);
    if (path == nullptr) {
        return {};
    }
    return view->mapFromScene(path->mapToScene(path->path().pointAtPercent(percent)));
}

void sendPointer(QWidget *viewport, QEvent::Type type, const QPoint &position,
                 Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(position), QPointF(viewport->mapToGlobal(position)), button, buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

void postMove(QWidget *viewport, const QPoint &position)
{
    QCoreApplication::postEvent(viewport,
                                new QMouseEvent(QEvent::MouseMove,
                                                QPointF(position),
                                                QPointF(viewport->mapToGlobal(position)),
                                                Qt::NoButton,
                                                Qt::NoButton,
                                                Qt::NoModifier));
}

void click(QGraphicsView *view, const QPoint &pixel)
{
    sendPointer(view->viewport(), QEvent::MouseMove, pixel, Qt::NoButton, Qt::NoButton);
    QCoreApplication::processEvents();
    sendPointer(view->viewport(), QEvent::MouseButtonPress, pixel, Qt::LeftButton, Qt::LeftButton);
    sendPointer(view->viewport(), QEvent::MouseButtonRelease, pixel, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents();
}

int counter(std::atomic<int> &value)
{
    return value.load(std::memory_order_relaxed);
}

const QString kFixture = QStringLiteral(
    "encoding utf-8\n"                       // 1
    "scrap s -projection plan\n"             // 2
    "line wall -id first\n"                  // 3
    "  0 0\n  100 0\nendline\n"              // 4-6
    "line wall -id second\n"                 // 7
    "  0 -60\n  100 -60\nendline\n"          // 8-10
    "line pit -id third\n"                   // 11
    "  0 -120\n  100 -120\nendline\n"        // 12-14
    "line border -id areaborder -close on\n" // 15
    "  150 0\n  250 0\n  250 -120\n  150 -120\n  150 0\nendline\n" // 16-21
    "area water\n  areaborder\nendarea\n"    // 22-24
    "endscrap\n");
}

class MapEditorInteractionPerformanceTest final : public QObject
{
    Q_OBJECT

private:
    struct Harness
    {
        QTemporaryDir directory{QDir::current().filePath(QStringLiteral(".verification/interaction-perf-XXXXXX"))};
        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        std::unique_ptr<MapEditorTab> tab;
        QGraphicsView *view = nullptr;
        QUndoStack *stack = nullptr;
    };

    bool open(Harness &harness)
    {
        QDir().mkpath(QStringLiteral(".verification"));
        if (!harness.directory.isValid()) {
            return false;
        }
        const QString path = harness.directory.filePath(QStringLiteral("perf.th2"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(kFixture.toUtf8());
        file.close();
        harness.tab = std::make_unique<MapEditorTab>(harness.fileSystem, harness.sessionStore, CommandCatalogStore());
        harness.tab->resize(1280, 900);
        harness.tab->show();
        QString error;
        if (!harness.tab->loadFile(path, &error)) {
            qWarning() << error;
            return false;
        }
        harness.view = harness.tab->findChild<QGraphicsView *>(QStringLiteral("mapCanvasView"));
        harness.stack = harness.tab->findChild<QUndoStack *>();
        if (harness.view == nullptr || harness.stack == nullptr) {
            return false;
        }
        QTest::qWait(50);
        harness.tab->triggerFit();
        QTest::qWait(50);
        return linePath(harness.view->scene(), 3) != nullptr;
    }

    static QCheckBox *closedCheck(Harness &harness)
    {
        for (QCheckBox *check : harness.tab->findChildren<QCheckBox *>()) {
            if (check->text().contains(QStringLiteral("-close"))) {
                return check;
            }
        }
        return nullptr;
    }

private slots:
    void draftUndoRedoDoesNotEnumerateScene()
    {
        QGraphicsScene scene;
        scene.setSceneRect(0, 0, 12000, 2000);
        for (int index = 0; index < 10000; ++index) {
            scene.addRect(QRectF(index, 0, 1, 1));
        }
        QVector<QGraphicsRectItem *> drafts;
        auto *draft = new MapDraftGeometryItem(1, DraftGeometryKind::Point);
        QUndoStack stack;
        auto &scans = mapEditorPerformanceCounters().fullSceneItemScans;
        const int before = counter(scans);
        stack.push(createMapDraftAddCommand(&scene, &drafts, draft, QPointF(500, 500)));
        for (int index = 0; index < 20; ++index) {
            QCOMPARE(draft->scene(), &scene);
            QCOMPARE(drafts.size(), 1);
            stack.undo();
            QVERIFY(draft->scene() == nullptr);
            QVERIFY(drafts.isEmpty());
            stack.redo();
        }
        QCOMPARE(draft->scene(), &scene);
        QCOMPARE(draft->pos(), QPointF(500, 500));
        QCOMPARE(drafts.size(), 1);
        QCOMPARE(counter(scans) - before, 0);
    }

    // Proposal 1: the tab used to filter viewport events both as an application
    // filter and as a viewport/view object filter, so every pointer event ran
    // the full input controller (hover hit test, press hit test) twice.
    void eachViewportPointerEventIsDispatchedOnce()
    {
        Harness harness;
        QVERIFY(open(harness));
        QWidget *viewport = harness.view->viewport();
        const QPoint onLine = pixelOnLine(harness.view, 3, 0.5);
        const QPoint empty(viewport->width() - 20, viewport->height() - 20);

        auto &dispatches = mapEditorPerformanceCounters().viewportPointerDispatches;
        int before = counter(dispatches);
        sendPointer(viewport, QEvent::MouseMove, empty, Qt::NoButton, Qt::NoButton);
        QCOMPARE(counter(dispatches) - before, 1);

        before = counter(dispatches);
        sendPointer(viewport, QEvent::MouseButtonPress, onLine, Qt::LeftButton, Qt::LeftButton);
        QCOMPARE(counter(dispatches) - before, 1);
        before = counter(dispatches);
        sendPointer(viewport, QEvent::MouseButtonRelease, onLine, Qt::LeftButton, Qt::NoButton);
        QCOMPARE(counter(dispatches) - before, 1);
        QCoreApplication::processEvents();
        // Behaviour is unchanged: the click still selects exactly the hit line.
        QTRY_COMPARE_WITH_TIMEOUT(describe(qtSelectedLines(harness.view->scene())), describe({3}), 1000);
    }

    // Proposal 2: resolving the selected line for the object details panel must
    // reuse the per-revision geometry instead of re-collecting the whole
    // document, and must still follow text edits and undo.
    void selectionDetailsDoNotRebuildWholeDocumentGeometry()
    {
        Harness harness;
        QVERIFY(open(harness));
        auto &builds = mapEditorPerformanceCounters().wholeDocumentGeometryBuilds;

        int before = counter(builds);
        click(harness.view, pixelOnLine(harness.view, 3, 0.5));
        QTRY_COMPARE_WITH_TIMEOUT(describe(qtSelectedLines(harness.view->scene())), describe({3}), 1000);
        QCOMPARE(counter(builds) - before, 0);

        before = counter(builds);
        click(harness.view, pixelOnLine(harness.view, 7, 0.5));
        QTRY_COMPARE_WITH_TIMEOUT(describe(qtSelectedLines(harness.view->scene())), describe({7}), 1000);
        QCOMPARE(counter(builds) - before, 0);

        QCheckBox *closed = closedCheck(harness);
        QVERIFY(closed != nullptr);
        QTRY_VERIFY(closed->isVisible());
        QVERIFY(!closed->isChecked());

        // A source edit invalidates the cached lookup: the details panel must
        // report the edited line state, and undo must restore it.
        closed->click();
        QTRY_VERIFY_WITH_TIMEOUT(harness.tab->text().contains(QStringLiteral("line wall -id second -close on")), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(closedCheck(harness)->isChecked(), 2000);
        harness.tab->triggerUndo();
        QTRY_VERIFY_WITH_TIMEOUT(!harness.tab->text().contains(QStringLiteral("-id second -close")), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!closedCheck(harness)->isChecked(), 2000);
        QCOMPARE(harness.tab->text(), kFixture);
    }

    // Proposal 7: inspector tree rebuilds used to parse and rasterise the
    // Lucide SVG icons twice per row on every rebuild.
    void inspectorIconsAreCachedAcrossTreeRebuilds()
    {
        Harness harness;
        QVERIFY(open(harness));
        auto *tree = harness.tab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
        QVERIFY(tree != nullptr && tree->model() != nullptr);
        QTRY_VERIFY(tree->model()->rowCount() > 0);
        auto &renders = mapEditorPerformanceCounters().inspectorIconRenders;

        // Edit + undo rebuild the Objects tree twice through the production
        // source-driven refresh path.
        QPlainTextEdit *sourceEditor = nullptr;
        for (QPlainTextEdit *edit : harness.tab->findChildren<QPlainTextEdit *>()) {
            if (edit->toPlainText() == kFixture) {
                sourceEditor = edit;
            }
        }
        QVERIFY(sourceEditor != nullptr);
        const int before = counter(renders);
        sourceEditor->moveCursor(QTextCursor::End);
        sourceEditor->insertPlainText(QStringLiteral("\n# rebuild inspector\n"));
        QTest::qWait(400);
        sourceEditor->moveCursor(QTextCursor::End);
        sourceEditor->insertPlainText(QStringLiteral("# rebuild inspector again\n"));
        QTest::qWait(400);
        QTRY_VERIFY(tree->model()->rowCount() > 0);
        QCOMPARE(counter(renders) - before, 0);

        // Theme change must not reuse icons rendered for the old palette.
        const QPalette originalPalette = QApplication::palette();
        const int themeBefore = counter(renders);
        const QIcon cached = inspectorActionIcon(QStringLiteral("eye"));
        QCOMPARE(counter(renders) - themeBefore, 0);
        QPalette changed = originalPalette;
        changed.setColor(QPalette::Text, QColor(QStringLiteral("#ff00ff")));
        QApplication::setPalette(changed);
        const QIcon recoloured = inspectorActionIcon(QStringLiteral("eye"));
        QVERIFY(counter(renders) - themeBefore > 0);
        QVERIFY(cached.cacheKey() != recoloured.cacheKey());
        QApplication::setPalette(originalPalette);
    }

    // Proposal 8: no hover hit testing while a button is held (drag), at most
    // one hover hit test per frame for a burst of moves, hover correct after
    // the drag and cleared on leave; the magnifier must not re-derive the
    // draft source bounds (whole-text parse) on every pointer move.
    void hoverIsSkippedDuringDragAndCoalescedPerFrame()
    {
        Harness harness;
        QVERIFY(open(harness));
        QWidget *viewport = harness.view->viewport();
        auto &hoverTests = mapEditorPerformanceCounters().hoverHitTests;
        const QPoint empty(viewport->width() - 30, viewport->height() - 30);
        const QPoint line7 = pixelOnLine(harness.view, 7, 0.5);
        const QPoint line11 = pixelOnLine(harness.view, 11, 0.5);

        // Drag from empty canvas over line 7 with the left button held.
        sendPointer(viewport, QEvent::MouseMove, empty, Qt::NoButton, Qt::NoButton);
        QTest::qWait(30);
        int before = counter(hoverTests);
        sendPointer(viewport, QEvent::MouseButtonPress, empty, Qt::LeftButton, Qt::LeftButton);
        constexpr int steps = 8;
        for (int step = 1; step <= steps; ++step) {
            const QPoint position = empty + ((line7 - empty) * step) / steps;
            sendPointer(viewport, QEvent::MouseMove, position, Qt::NoButton, Qt::LeftButton);
            QTest::qWait(20);
        }
        QCOMPARE(counter(hoverTests) - before, 0);
        sendPointer(viewport, QEvent::MouseButtonRelease, line7, Qt::LeftButton, Qt::NoButton);
        // After the drag ends the hover reflects the line under the pointer.
        QTRY_COMPARE_WITH_TIMEOUT(describe(hoveredLines(harness.view->scene())), describe({7}), 1000);

        // A burst of queued moves is coalesced to one hit test for the frame.
        QTest::qWait(40);
        before = counter(hoverTests);
        for (int step = 1; step <= 20; ++step) {
            postMove(viewport, line7 + ((line11 - line7) * step) / 20);
        }
        QCoreApplication::processEvents();
        QVERIFY2(counter(hoverTests) - before <= 1,
                 qPrintable(QStringLiteral("hover hit tests for 20 queued moves: %1").arg(counter(hoverTests) - before)));
        QTRY_COMPARE_WITH_TIMEOUT(describe(hoveredLines(harness.view->scene())), describe({11}), 1000);
        QVERIFY(counter(hoverTests) - before <= 2);

        // Leaving the viewport clears hover, and no stale coalesced update
        // re-applies it afterwards.
        postMove(viewport, line7);
        QEvent leave(QEvent::Leave);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QCoreApplication::sendEvent(viewport, &leave);
        QTest::qWait(60);
        QCOMPARE(describe(hoveredLines(harness.view->scene())), describe({}));

        // Magnifier: one draft-bounds derivation per document revision at most.
        harness.tab->setMagnifierEnabled(true);
        QTest::qWait(30);
        auto &boundsComputations = mapEditorPerformanceCounters().draftSourceBoundsComputations;
        const int boundsBefore = counter(boundsComputations);
        auto *magnifier = harness.tab->findChild<QWidget *>(QStringLiteral("mapMagnifierOverlay"));
        for (int step = 0; step < 8; ++step) {
            sendPointer(viewport, QEvent::MouseMove, line7 + QPoint(step * 3, 0), Qt::NoButton, Qt::NoButton);
            QTest::qWait(25);
        }
        if (magnifier != nullptr) {
            QVERIFY(magnifier->isVisible());
        }
        QVERIFY2(counter(boundsComputations) - boundsBefore <= 1,
                 qPrintable(QStringLiteral("draft bounds computations for 8 magnifier frames: %1")
                                .arg(counter(boundsComputations) - boundsBefore)));
    }
};

int runMapEditorInteractionPerformanceTest(int argc, char **argv)
{
    MapEditorInteractionPerformanceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorInteractionPerformanceTest.moc"
