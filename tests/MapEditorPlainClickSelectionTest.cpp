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
#include <QItemSelectionModel>
#include <QMenu>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <QUndoStack>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;

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

LineSet highlightedLines(QGraphicsScene *scene)
{
    LineSet lines;
    for (QGraphicsItem *item : scene->items()) {
        if (item->data(kMapSceneInteractionSelectionRole).toBool()) {
            lines.insert(item->data(kMapSceneLineNumberRole).toInt());
        }
    }
    return lines;
}

LineSet visibleSelectionHandleLines(QGraphicsScene *scene)
{
    LineSet lines;
    for (QGraphicsItem *item : scene->items()) {
        if (item->data(kMapSceneSelectionGatedRole).toBool() && item->isVisible()) {
            lines.insert(item->data(kMapSceneLineNumberRole).toInt());
        }
    }
    return lines;
}

int objectsTreeSelectedLine(QTreeView *tree)
{
    if (tree == nullptr || tree->selectionModel() == nullptr) {
        return -1;
    }
    const QModelIndexList rows = tree->selectionModel()->selectedRows(0);
    if (rows.size() != 1) {
        return rows.isEmpty() ? 0 : -rows.size();
    }
    return rows.first().data(kInspectorSourceLineRole).toInt();
}

QPoint pixelOnLine(QGraphicsView *view, int line, qreal percent)
{
    QGraphicsPathItem *path = linePath(view->scene(), line);
    if (path == nullptr) {
        return {};
    }
    return view->mapFromScene(path->mapToScene(path->path().pointAtPercent(percent)));
}

// Real pointer sequence used on the desktop: the pointer hovers over the line,
// pauses, then presses and releases without any keyboard modifier.
void hoverThenClick(QGraphicsView *view, const QPoint &pixel, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QTest::mouseMove(view->viewport(), pixel + QPoint(6, 6));
    QCoreApplication::processEvents();
    QTest::mouseMove(view->viewport(), pixel);
    QCoreApplication::processEvents();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, modifiers, pixel);
    QCoreApplication::processEvents();
}

void sendPointerEvent(QWidget *viewport, QEvent::Type type, const QPoint &position,
                      Qt::MouseButton button, Qt::MouseButtons buttons)
{
    const QPoint globalPosition = viewport->mapToGlobal(position);
    QMouseEvent event(type, QPointF(position), QPointF(globalPosition), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

void dragPointer(QGraphicsView *view, const QPoint &start, const QPoint &end)
{
    QWidget *viewport = view->viewport();
    sendPointerEvent(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    QCoreApplication::processEvents();
    constexpr int steps = 6;
    for (int step = 1; step <= steps; ++step) {
        const qreal ratio = static_cast<qreal>(step) / steps;
        const QPoint position(qRound(start.x() + ((end.x() - start.x()) * ratio)),
                              qRound(start.y() + ((end.y() - start.y()) * ratio)));
        sendPointerEvent(viewport, QEvent::MouseMove, position, Qt::NoButton, Qt::LeftButton);
        QCoreApplication::processEvents();
    }
    sendPointerEvent(viewport, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents();
}

// Ctrl + left click without drag is the documented secondary click: it selects
// the hit object (replacing the selection) and opens the context menu.
void ctrlClickClosingContextMenu(QGraphicsView *view, const QPoint &pixel, bool *menuShown)
{
    QTimer closer;
    closer.setInterval(10);
    QObject::connect(&closer, &QTimer::timeout, [menuShown]() {
        if (auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget())) {
            *menuShown = true;
            menu->close();
        }
    });
    closer.start();
    hoverThenClick(view, pixel, Qt::ControlModifier);
    QTest::qWait(50);
}

void rightClickClosingContextMenu(QGraphicsView *view, const QPoint &pixel, bool *menuShown)
{
    QTimer closer;
    closer.setInterval(10);
    QObject::connect(&closer, &QTimer::timeout, [menuShown]() {
        if (auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget())) {
            *menuShown = true;
            menu->close();
        }
    });
    closer.start();
    QTest::mouseClick(view->viewport(), Qt::RightButton, Qt::NoModifier, pixel);
    QContextMenuEvent contextMenu(QContextMenuEvent::Mouse,
                                  pixel,
                                  view->viewport()->mapToGlobal(pixel),
                                  Qt::NoModifier);
    QCoreApplication::sendEvent(view->viewport(), &contextMenu);
    QTest::qWait(50);
}

QString writeFixture(const QTemporaryDir &directory, const QString &text)
{
    const QString path = directory.filePath(QStringLiteral("selection.th2"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(text.toUtf8());
    return path;
}

struct Fixture
{
    QString source;
    QList<int> clickLines;
    int fillerLines = 0;
};

Fixture smallFixture()
{
    // Three walls, one decorated pit line overlapping wall 2's neighbourhood,
    // and an area whose border line participates in selection propagation.
    Fixture fixture;
    fixture.source = QStringLiteral(
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
    fixture.clickLines = {3, 7, 11};
    return fixture;
}

Fixture overlappingAreaFixture()
{
    Fixture fixture;
    fixture.source = QStringLiteral(
        "encoding utf-8\n"
        "scrap s -projection plan\n"
        "line border -id boundary -close on\n"
        "  0 0\n  100 0\n  100 -100\n  0 -100\n  0 0\n"
        "endline\n"
        "area water\n  boundary\nendarea\n"
        "line wall -id crossing\n"
        "  20 -50\n  80 -50\n"
        "endline\n"
        "point station -name inside 50 -75\n"
        "endscrap\n");
    fixture.clickLines = {11};
    return fixture;
}

Fixture largeFixture()
{
    Fixture fixture;
    QString text = QStringLiteral("encoding utf-8\nscrap s -projection plan\n");
    int lineNumber = 3;
    for (int index = 0; index < 400; ++index) {
        const int y = -index * 12;
        text += QStringLiteral("line wall\n  0 %1\n  80 %1\nendline\n").arg(y);
        if (index == 10 || index == 200 || index == 399) {
            fixture.clickLines << lineNumber;
        }
        lineNumber += 4;
    }
    text += QStringLiteral("endscrap\nscrap t -projection plan\nline wall\n  400 0\n  480 0\nendline\nendscrap\n");
    fixture.clickLines << lineNumber + 2;
    fixture.source = text;
    fixture.fillerLines = 400;
    return fixture;
}
}

#define VERIFY_ONLY(harness, ...)                         \
    do {                                                  \
        verifyOnly(harness, LineSet(__VA_ARGS__));        \
        if (QTest::currentTestFailed()) {                 \
            return;                                       \
        }                                                 \
    } while (false)

class MapEditorPlainClickSelectionTest final : public QObject
{
    Q_OBJECT

private:
    struct Harness
    {
        QTemporaryDir directory{QDir::current().filePath(QStringLiteral(".verification/plain-click-XXXXXX"))};
        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        std::unique_ptr<MapEditorTab> tab;
        QGraphicsView *view = nullptr;
        QUndoStack *stack = nullptr;
        QTreeView *objects = nullptr;
    };

    bool open(Harness &harness, const QString &source)
    {
        QDir().mkpath(QStringLiteral(".verification"));
        if (!harness.directory.isValid()) {
            return false;
        }
        const QString path = writeFixture(harness.directory, source);
        harness.tab = std::make_unique<MapEditorTab>(harness.fileSystem, harness.sessionStore, CommandCatalogStore());
        harness.tab->resize(1280, 900);
        harness.tab->show();
        QString error;
        if (!harness.tab->loadFile(path, &error)) {
            qWarning() << error;
            return false;
        }
        harness.view = harness.tab->findChild<QGraphicsView *>();
        harness.stack = harness.tab->findChild<QUndoStack *>();
        harness.objects = harness.tab->findChild<QTreeView *>(QStringLiteral("mapObjectsTree"));
        if (harness.view == nullptr || harness.stack == nullptr) {
            return false;
        }
        QTest::qWait(50);
        harness.tab->triggerFit();
        QCoreApplication::processEvents();
        return true;
    }

    // Asserts Qt selection, custom highlight role, visible selection-gated
    // handles and the Objects tree all describe exactly `expected`.
    static void verifyOnly(Harness &harness, const LineSet &expected)
    {
        QGraphicsScene *scene = harness.view->scene();
        QTRY_COMPARE_WITH_TIMEOUT(describe(qtSelectedLines(scene)), describe(expected), 1000);
        QCOMPARE(describe(highlightedLines(scene)), describe(expected));
        QCOMPARE(describe(visibleSelectionHandleLines(scene)), describe(expected));
        if (harness.objects != nullptr && expected.size() == 1) {
            QCOMPARE(objectsTreeSelectedLine(harness.objects), *expected.cbegin());
        }
    }

private slots:
    void overlappingAreaYieldToGeometryAndRemainSelectableInEmptyInterior()
    {
        Harness harness;
        QVERIFY(open(harness, overlappingAreaFixture().source));
        QGraphicsScene *scene = harness.view->scene();
        QGraphicsPathItem *areaFill = nullptr;
        MapEditablePointItem *point = nullptr;
        for (QGraphicsItem *item : scene->items()) {
            if (item->data(kMapSceneLineNumberRole).toInt() == 10
                && item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeAreaFill) {
                areaFill = dynamic_cast<QGraphicsPathItem *>(item);
            }
            if (item->data(kMapSceneLineNumberRole).toInt() == 17) {
                point = dynamic_cast<MapEditablePointItem *>(item);
            }
        }
        QVERIFY(areaFill != nullptr);
        QCOMPARE(areaFill->acceptedMouseButtons(), Qt::NoButton);
        QVERIFY(point != nullptr);
        QVERIFY(linePath(scene, 13) != nullptr);

        const QPoint crossingPixel = pixelOnLine(harness.view, 13, 0.5);
        QTest::mouseMove(harness.view->viewport(), crossingPixel);
        QCoreApplication::processEvents();
        QTRY_COMPARE_WITH_TIMEOUT(describe(hoveredLines(scene)), describe({13}), 1000);
        hoverThenClick(harness.view, crossingPixel);
        VERIFY_ONLY(harness, {13});
        bool menuShown = false;
        ctrlClickClosingContextMenu(harness.view, crossingPixel, &menuShown);
        QVERIFY(menuShown);
        VERIFY_ONLY(harness, {13});
        bool rightClickMenuShown = false;
        rightClickClosingContextMenu(harness.view, crossingPixel, &rightClickMenuShown);
        QVERIFY(rightClickMenuShown);
        VERIFY_ONLY(harness, {13});
        hoverThenClick(harness.view, harness.view->mapFromScene(point->scenePos()));
        QTRY_COMPARE_WITH_TIMEOUT(describe(qtSelectedLines(scene)), describe({17}), 1000);
        QCOMPARE(describe(highlightedLines(scene)), describe({17}));

        QPointF emptyAreaPoint = areaFill->path().boundingRect().center();
        emptyAreaPoint.ry() -= areaFill->path().boundingRect().height() * 0.3;
        QVERIFY(areaFill->path().contains(emptyAreaPoint));
        hoverThenClick(harness.view, harness.view->mapFromScene(areaFill->mapToScene(emptyAreaPoint)));
        QTRY_COMPARE_WITH_TIMEOUT(describe(qtSelectedLines(scene)), describe({10}), 1000);
        QCOMPARE(describe(highlightedLines(scene)), describe({3, 10}));
        QCOMPARE(describe(visibleSelectionHandleLines(scene)), describe({3}));

        hoverThenClick(harness.view, pixelOnLine(harness.view, 13, 0.5));
        VERIFY_ONLY(harness, {13});
        MapEditableGeometryVertexItem *lineVertex = nullptr;
        for (QGraphicsItem *item : scene->items()) {
            auto *candidate = dynamic_cast<MapEditableGeometryVertexItem *>(item);
            if (candidate != nullptr && candidate->lineNumber() == 13
                && candidate->geometryKind() == QStringLiteral("line") && candidate->isVisible()) {
                lineVertex = candidate;
                break;
            }
        }
        QVERIFY(lineVertex != nullptr);
        const QString beforeLineDrag = harness.tab->text();
        const QPoint lineDragStart = harness.view->mapFromScene(lineVertex->scenePos());
        dragPointer(harness.view, lineDragStart, lineDragStart + QPoint(20, 12));
        QTRY_VERIFY_WITH_TIMEOUT(harness.tab->text() != beforeLineDrag, 1000);
        QCOMPARE(harness.stack->count(), 1);
        harness.tab->triggerUndo();
        QTRY_COMPARE_WITH_TIMEOUT(harness.tab->text(), beforeLineDrag, 1000);

        point = nullptr;
        for (QGraphicsItem *item : scene->items()) {
            if (item->data(kMapSceneLineNumberRole).toInt() == 17) {
                point = dynamic_cast<MapEditablePointItem *>(item);
                break;
            }
        }
        QVERIFY(point != nullptr);
        hoverThenClick(harness.view, harness.view->mapFromScene(point->scenePos()));
        QTRY_COMPARE_WITH_TIMEOUT(describe(qtSelectedLines(scene)), describe({17}), 1000);
        QCOMPARE(describe(highlightedLines(scene)), describe({17}));
        const QString beforePointDrag = harness.tab->text();
        const QPoint pointDragStart = harness.view->mapFromScene(point->scenePos());
        dragPointer(harness.view, pointDragStart, pointDragStart + QPoint(20, 12));
        QTRY_VERIFY_WITH_TIMEOUT(harness.tab->text() != beforePointDrag, 1000);
        QCOMPARE(harness.stack->count(), 1);
        harness.tab->triggerUndo();
        QTRY_COMPARE_WITH_TIMEOUT(harness.tab->text(), beforePointDrag, 1000);

        hoverThenClick(harness.view, crossingPixel);
        VERIFY_ONLY(harness, {13});
        QTest::mouseDClick(harness.view->viewport(), Qt::LeftButton, Qt::NoModifier, crossingPixel);
        QCoreApplication::processEvents();
        VERIFY_ONLY(harness, {13});
    }

    void plainClicksReplaceSelection_data()
    {
        QTest::addColumn<bool>("large");
        QTest::addColumn<double>("zoom");
        QTest::newRow("small-fit") << false << 0.0;
        QTest::newRow("small-zoomed") << false << 4.0;
        QTest::newRow("large-scene-fit") << true << 0.0;
    }

    void plainClicksReplaceSelection()
    {
        QFETCH(bool, large);
        QFETCH(double, zoom);
        const Fixture fixture = large ? largeFixture() : smallFixture();
        Harness harness;
        QVERIFY(open(harness, fixture.source));
        QGraphicsScene *scene = harness.view->scene();
        for (int line : fixture.clickLines) {
            QVERIFY2(linePath(scene, line) != nullptr, qPrintable(QStringLiteral("missing line %1").arg(line)));
        }
        const QString original = harness.tab->text();

        for (int line : fixture.clickLines) {
            if (zoom > 0.0) {
                harness.view->resetTransform();
                harness.view->scale(zoom, zoom);
                QGraphicsPathItem *path = linePath(scene, line);
                harness.view->centerOn(path->mapToScene(path->path().pointAtPercent(0.5)));
                QCoreApplication::processEvents();
            }
            const QPoint pixel = pixelOnLine(harness.view, line, 0.5);
            QVERIFY(harness.view->viewport()->rect().contains(pixel));
            hoverThenClick(harness.view, pixel);
            VERIFY_ONLY(harness, {line});
            QCOMPARE(harness.tab->text(), original);
            QCOMPARE(harness.stack->count(), 0);
        }

        // Re-clicking the current line keeps exactly one stable selection.
        const int last = fixture.clickLines.last();
        hoverThenClick(harness.view, pixelOnLine(harness.view, last, 0.5));
        VERIFY_ONLY(harness, {last});
        QCOMPARE(harness.tab->text(), original);
        QCOMPARE(harness.stack->count(), 0);
        QVERIFY(!harness.tab->isDirty());
    }

    void plainClickAfterAreaSelectionClearsAreaBorder()
    {
        const Fixture fixture = smallFixture();
        Harness harness;
        QVERIFY(open(harness, fixture.source));
        hoverThenClick(harness.view, pixelOnLine(harness.view, 15, 0.1));
        VERIFY_ONLY(harness, {15});
        hoverThenClick(harness.view, pixelOnLine(harness.view, 3, 0.5));
        VERIFY_ONLY(harness, {3});
        hoverThenClick(harness.view, pixelOnLine(harness.view, 7, 0.5));
        VERIFY_ONLY(harness, {7});
    }

    void ctrlClickContextSelectionReplacesWithoutStaleHighlight()
    {
        const Fixture fixture = smallFixture();
        Harness harness;
        QVERIFY(open(harness, fixture.source));
        const QString original = harness.tab->text();
        hoverThenClick(harness.view, pixelOnLine(harness.view, 3, 0.5));
        VERIFY_ONLY(harness, {3});
        bool menuShown = false;
        ctrlClickClosingContextMenu(harness.view, pixelOnLine(harness.view, 7, 0.5), &menuShown);
        QVERIFY(menuShown);
        VERIFY_ONLY(harness, {7});
        hoverThenClick(harness.view, pixelOnLine(harness.view, 11, 0.5));
        VERIFY_ONLY(harness, {11});
        QCOMPARE(harness.tab->text(), original);
        QCOMPARE(harness.stack->count(), 0);
    }

    void deliberateMultiSelectionIsPreservedThenReplacedByPlainClick()
    {
        const Fixture fixture = smallFixture();
        Harness harness;
        QVERIFY(open(harness, fixture.source));
        const QString original = harness.tab->text();
        // Placing the source cursor on the scrap header selects every scrap object.
        harness.tab->goToLine(2);
        QTRY_VERIFY(qtSelectedLines(harness.view->scene()).size() > 1);
        const LineSet scrapSelection = qtSelectedLines(harness.view->scene());
        QVERIFY(scrapSelection.contains(LineSet({3, 7, 11, 15})));
        QVERIFY(highlightedLines(harness.view->scene()).contains(LineSet({3, 7, 11, 15})));
        // Hovering without clicking keeps the deliberate multi-selection.
        QTest::mouseMove(harness.view->viewport(), pixelOnLine(harness.view, 7, 0.5));
        QCoreApplication::processEvents();
        QCOMPARE(describe(qtSelectedLines(harness.view->scene())), describe(scrapSelection));
        hoverThenClick(harness.view, pixelOnLine(harness.view, 7, 0.5));
        VERIFY_ONLY(harness, {7});
        QCOMPARE(harness.tab->text(), original);
        QCOMPARE(harness.stack->count(), 0);
    }

    void emptyClickClearsSelection()
    {
        const Fixture fixture = smallFixture();
        Harness harness;
        QVERIFY(open(harness, fixture.source));
        hoverThenClick(harness.view, pixelOnLine(harness.view, 3, 0.5));
        VERIFY_ONLY(harness, {3});
        const QPoint first = pixelOnLine(harness.view, 3, 0.5);
        const QPoint second = pixelOnLine(harness.view, 7, 0.5);
        const QPoint empty((first.x() + second.x()) / 2, (first.y() + second.y()) / 2);
        QVERIFY(harness.view->viewport()->rect().contains(empty));
        hoverThenClick(harness.view, empty);
        QTRY_VERIFY(qtSelectedLines(harness.view->scene()).isEmpty());
        QCOMPARE(describe(highlightedLines(harness.view->scene())), describe({}));
        QCOMPARE(describe(visibleSelectionHandleLines(harness.view->scene())), describe({}));
        hoverThenClick(harness.view, pixelOnLine(harness.view, 11, 0.5));
        VERIFY_ONLY(harness, {11});
    }

    void plainClicksReplaceSelectionAcrossSourceRefresh()
    {
        // A source-driven refresh (e.g. after an edit elsewhere) rebuilds the
        // scene while a line is selected; subsequent plain clicks must replace.
        const Fixture fixture = smallFixture();
        Harness harness;
        QVERIFY(open(harness, fixture.source));
        hoverThenClick(harness.view, pixelOnLine(harness.view, 3, 0.5));
        VERIFY_ONLY(harness, {3});
        harness.tab->testRefreshMapScene();
        QCoreApplication::processEvents();
        QTRY_VERIFY(linePath(harness.view->scene(), 7) != nullptr);
        hoverThenClick(harness.view, pixelOnLine(harness.view, 7, 0.5));
        VERIFY_ONLY(harness, {7});
        hoverThenClick(harness.view, pixelOnLine(harness.view, 11, 0.5));
        VERIFY_ONLY(harness, {11});
    }
};

int runMapEditorPlainClickSelectionTest(int argc, char **argv)
{
    MapEditorPlainClickSelectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorPlainClickSelectionTest.moc"
