#include "../src/app/text_editor/map_editor/MapEditorPerformanceCounters.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneHandleIndex.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorXviBackgroundItem.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QRandomGenerator>
#include <QScreen>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>

using namespace TherionStudio;

// Regression tests for PERF-AUDIT-TH2.md proposals 3 (spatial handle index)
// and 4 (partial viewport repaint + cached XVI background). They drive the
// real MapEditorTab / QGraphicsView paths and compare the optimised results
// against brute-force or full-repaint references.
namespace
{

static qreal boundedReal(QRandomGenerator &random, qreal low, qreal high)
{
    return low + random.generateDouble() * (high - low);
}

int counter(std::atomic<int> &value)
{
    return value.load(std::memory_order_relaxed);
}

bool isHandleItem(const QGraphicsItem *item)
{
    return dynamic_cast<const MapEditablePointItem *>(item) != nullptr
        || dynamic_cast<const MapEditableGeometryVertexItem *>(item) != nullptr
        || dynamic_cast<const MapLinePointSizeHandleItem *>(item) != nullptr
        || dynamic_cast<const MapPointOrientationHandleItem *>(item) != nullptr;
}

QPointF handleKey(const QGraphicsItem *item)
{
    if (dynamic_cast<const MapEditablePointItem *>(item) != nullptr
        || dynamic_cast<const MapEditableGeometryVertexItem *>(item) != nullptr) {
        return item->scenePos();
    }
    return item->mapToScene(item->boundingRect().center());
}

int handleLine(const QGraphicsItem *item)
{
    if (auto *point = dynamic_cast<const MapEditablePointItem *>(item)) {
        return point->data(kMapSceneLineNumberRole).toInt();
    }
    if (auto *vertex = dynamic_cast<const MapEditableGeometryVertexItem *>(item)) {
        return vertex->lineNumber();
    }
    return item->data(kMapSceneLineNumberRole).toInt();
}

bool keyInRect(const QPointF &key, const QRectF &rect)
{
    return key.x() >= rect.left() && key.x() <= rect.right() && key.y() >= rect.top() && key.y() <= rect.bottom();
}

QVector<QGraphicsItem *> bruteHandlesInRect(QGraphicsScene *scene, const QRectF &rect)
{
    QVector<QGraphicsItem *> result;
    for (QGraphicsItem *item : scene->items()) {
        if (isHandleItem(item) && keyInRect(handleKey(item), rect)) {
            result.append(item);
        }
    }
    return result;
}

// Former whole-scene implementation of nearestWallGeometrySnapPoint().
std::optional<QPointF> bruteWallSnap(const QGraphicsScene *scene, const QPointF &candidate, qreal radius,
                                     int excludedLineNumber)
{
    qreal bestDistanceSquared = radius * radius;
    std::optional<QPointF> bestPoint;
    auto consider = [&](const QPointF &point) {
        const QPointF delta = point - candidate;
        const qreal distanceSquared = QPointF::dotProduct(delta, delta);
        if (distanceSquared <= bestDistanceSquared && (!bestPoint || distanceSquared < bestDistanceSquared)) {
            bestDistanceSquared = distanceSquared;
            bestPoint = point;
        }
    };
    for (QGraphicsItem *item : scene->items()) {
        if (!isMapSceneWallItem(item) || item->data(kMapSceneLineNumberRole).toInt() == excludedLineNumber) {
            continue;
        }
        auto *vertex = dynamic_cast<MapEditableGeometryVertexItem *>(item);
        if (vertex != nullptr
            && item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineAnchor) {
            consider(vertex->scenePos());
        }
    }
    if (bestPoint) {
        return bestPoint;
    }
    // Segment fallback is unchanged production code (BSP probe).
    return nearestWallGeometrySnapPoint(scene, candidate, radius, excludedLineNumber);
}

// Former whole-scene implementation of the direct vertex-like hit test.
QGraphicsItem *bruteDirectVertexLike(QGraphicsScene *scene, QGraphicsView *view, const QPoint &position,
                                     bool requireSelected, bool requireVisible)
{
    QGraphicsItem *nearest = nullptr;
    qreal nearestDistance = std::numeric_limits<qreal>::max();
    for (QGraphicsItem *candidate : scene->items()) {
        const int subtype = candidate->data(kMapSceneSelectionSubtypeRole).toInt();
        const bool connectorHandle = subtype == kMapSceneSelectionSubtypeLineControlConnector
            && dynamic_cast<QGraphicsLineItem *>(candidate) == nullptr
            && dynamic_cast<QGraphicsPathItem *>(candidate) == nullptr;
        const bool vertexLike = dynamic_cast<MapEditableGeometryVertexItem *>(candidate) != nullptr
            || subtype == kMapSceneSelectionSubtypeLineAnchor || subtype == kMapSceneSelectionSubtypeLineControl
            || connectorHandle || subtype == kMapSceneSelectionSubtypeAreaVertex
            || subtype == kMapSceneSelectionSubtypePointOrientationHandle;
        if (!vertexLike) {
            continue;
        }
        const bool gated = candidate->data(kMapSceneSelectionGatedRole).toBool();
        if ((!candidate->isVisible() && !gated) || !(candidate->flags() & QGraphicsItem::ItemIsSelectable)
            || !mapSceneItemParticipatesInPointerSelection(candidate)) {
            continue;
        }
        if ((requireVisible && !candidate->isVisible()) || (requireSelected && !candidate->isSelected())
            || candidate->data(kMapSceneLineNumberRole).toInt() <= 0) {
            continue;
        }
        const QPointF center = dynamic_cast<MapEditableGeometryVertexItem *>(candidate) != nullptr
            ? candidate->scenePos()
            : candidate->mapToScene(candidate->boundingRect().center());
        const QPoint delta = position - view->mapFromScene(center);
        const qreal distance = std::hypot(delta.x(), delta.y());
        if (distance <= 12.0 && distance < nearestDistance) {
            nearestDistance = distance;
            nearest = candidate;
        }
    }
    return nearest;
}

QString describeItems(const QVector<QGraphicsItem *> &items)
{
    QStringList parts;
    for (QGraphicsItem *item : items) {
        const QPointF key = handleKey(item);
        parts << QStringLiteral("%1@(%2,%3)z%4").arg(handleLine(item)).arg(key.x()).arg(key.y()).arg(item->zValue());
    }
    return parts.join(QLatin1Char(' '));
}

QString readFixture(const QString &relativePath)
{
    QFile file(QStringLiteral(THERION_STUDIO_TEST_SOURCE_ROOT "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

// Seeded random TH2 with coincident wall endpoints, curves, slope l-size
// handles and oriented points.
QString randomDocument(quint32 seed)
{
    QRandomGenerator random(seed);
    QString text = QStringLiteral("encoding utf-8\nscrap s -projection plan\n");
    const QStringList types{QStringLiteral("wall"), QStringLiteral("wall"), QStringLiteral("slope"),
                            QStringLiteral("pit"), QStringLiteral("border")};
    QVector<QPoint> shared;
    for (int line = 0; line < 40; ++line) {
        const QString type = types.at(random.bounded(types.size()));
        text += QStringLiteral("line %1\n").arg(type);
        const int vertices = 2 + random.bounded(6);
        for (int vertex = 0; vertex < vertices; ++vertex) {
            QPoint p(random.bounded(0, 400), random.bounded(-300, 0));
            if ((vertex == 0 || vertex == vertices - 1) && !shared.isEmpty() && random.bounded(2) == 0) {
                p = shared.at(random.bounded(shared.size()));
            }
            shared.append(p);
            if (vertex > 0 && random.bounded(3) == 0) {
                text += QStringLiteral("  %1 %2 %3 %4 %5 %6\n")
                            .arg(p.x() + 7).arg(p.y() - 5).arg(p.x() - 6).arg(p.y() + 4).arg(p.x()).arg(p.y());
            } else {
                text += QStringLiteral("  %1 %2\n").arg(p.x()).arg(p.y());
            }
            if (type == QStringLiteral("slope") && random.bounded(2) == 0) {
                text += QStringLiteral("  orientation %1\n  l-size %2\n")
                            .arg(random.bounded(360)).arg(5 + random.bounded(40));
            }
        }
        text += QStringLiteral("endline\n");
    }
    for (int point = 0; point < 25; ++point) {
        const QPoint p = random.bounded(3) == 0 && !shared.isEmpty()
            ? shared.at(random.bounded(shared.size()))
            : QPoint(random.bounded(0, 400), random.bounded(-300, 0));
        text += random.bounded(2) == 0
            ? QStringLiteral("point %1 %2 entrance -orientation %3\n").arg(p.x()).arg(p.y()).arg(random.bounded(360))
            : QStringLiteral("point %1 %2 station -name %3\n").arg(p.x()).arg(p.y()).arg(point);
    }
    text += QStringLiteral("endscrap\n");
    return text;
}

void sendPointer(QWidget *viewport, QEvent::Type type, const QPoint &position, Qt::MouseButton button,
                 Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(position), QPointF(viewport->mapToGlobal(position)), button, buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

struct Timings
{
    QVector<double> ms;

    QString summary() const
    {
        QVector<double> sorted = ms;
        std::sort(sorted.begin(), sorted.end());
        if (sorted.isEmpty()) {
            return QStringLiteral("n=0");
        }
        auto at = [&](double q) { return sorted.at(qMin<int>(sorted.size() - 1, int(q * (sorted.size() - 1) + 0.5))); };
        return QStringLiteral("n=%1 median=%2ms p95=%3ms max=%4ms")
            .arg(sorted.size()).arg(at(0.5), 0, 'f', 3).arg(at(0.95), 0, 'f', 3).arg(sorted.last(), 0, 'f', 3);
    }
};

// Simulates the window backing store: only the regions delivered in viewport
// paint events are refreshed, everything else keeps its previous pixels. A
// partial-update bug (dirty trail) makes it diverge from a clean full grab.
// Item whose appearance can change without update(): a deliberate stale
// region that proves the repaint comparison is red-capable.
class SilentColorItem final : public QGraphicsRectItem
{
public:
    using QGraphicsRectItem::QGraphicsRectItem;
    QColor color = Qt::red;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        painter->fillRect(rect(), color);
    }
};

class BackingStoreSimulator final : public QObject
{
public:
    explicit BackingStoreSimulator(QGraphicsView *view)
        : view_(view)
    {
        view_->viewport()->installEventFilter(this);
        image_ = grabFull();
    }

    ~BackingStoreSimulator() override
    {
        view_->viewport()->removeEventFilter(this);
    }

    QImage grabFull()
    {
        paused_ = true;
        QImage image = view_->viewport()->grab().toImage().convertToFormat(QImage::Format_ARGB32);
        paused_ = false;
        return image;
    }

    void scrollBy(const QPoint &delta)
    {
        QImage shifted(image_.size(), image_.format());
        shifted.fill(Qt::magenta);
        QPainter painter(&shifted);
        painter.drawImage(delta, image_);
        painter.end();
        image_ = shifted;
    }

    // Applies the pending dirty region; returns the painted area fraction.
    double settle()
    {
        for (int round = 0; round < 6; ++round) {
            QTest::qWait(20);
        }
        const QImage full = grabFull();
        if (full.size() != image_.size()) {
            image_ = full;
            lastRegion_ = QRegion(full.rect());
            region_ = QRegion();
            return 1.0;
        }
        QPainter painter(&image_);
        painter.setClipRegion(region_);
        painter.drawImage(0, 0, full);
        painter.end();
        double area = 0.0;
        for (const QRect &rect : region_) {
            area += double(rect.width()) * rect.height();
        }
        lastRegion_ = region_;
        region_ = QRegion();
        return area / (double(full.width()) * full.height());
    }

    // Pixels the platform backing store actually holds after the partial
    // repaints (the offscreen platform grabs windows from its backing store).
    QImage backingStoreImage() const
    {
        QWidget *viewport = view_->viewport();
        QWidget *top = viewport->window();
        const QPoint origin = viewport->mapTo(top, QPoint(0, 0));
        return top->screen()
            ->grabWindow(top->winId(), origin.x(), origin.y(), viewport->width(), viewport->height())
            .toImage()
            .convertToFormat(QImage::Format_ARGB32);
    }

    // Viewport pixels covered by other widgets stacked above it (floating
    // panels/overlays). They are in the window backing store but not in a
    // grab of the viewport, so they are excluded from that comparison.
    QRegion overlayRegion() const
    {
        QWidget *viewport = view_->viewport();
        QWidget *top = viewport->window();
        const QRect viewportRect(viewport->mapTo(top, QPoint(0, 0)), viewport->size());
        QRegion region;
        for (QWidget *widget : top->findChildren<QWidget *>()) {
            if (!widget->isVisible() || widget == viewport || viewport->isAncestorOf(widget)
                || widget->isAncestorOf(viewport) || widget->isWindow()) {
                continue;
            }
            const QRect rect(widget->mapTo(top, QPoint(0, 0)), widget->size());
            if (rect.intersects(viewportRect)) {
                region |= rect.intersected(viewportRect).translated(-viewportRect.topLeft());
            }
        }
        return region;
    }

    // Compares a clean full repaint with (a) the real backing store, which
    // catches both missing dirty regions and clip-dependent painting, and
    // (b) the simulated store assembled from the paint-event regions.
    int mismatchedPixels(QString *where = nullptr)
    {
        const QImage full = grabFull();
        const QImage actual = backingStoreImage();
        const QRegion overlays = overlayRegion();
        int mismatches = 0;
        QRect bounds;
        const char *source = "";
        for (const QImage *candidate : std::initializer_list<const QImage *>{&actual, &image_}) {
            if (candidate->size() != full.size()) {
                mismatches += full.width() * full.height();
                bounds = full.rect();
                source = candidate == &actual ? "backing store size" : "simulated store size";
                continue;
            }
            for (int y = 0; y < full.height(); ++y) {
                const auto *a = reinterpret_cast<const QRgb *>(full.constScanLine(y));
                const auto *b = reinterpret_cast<const QRgb *>(candidate->constScanLine(y));
                for (int x = 0; x < full.width(); ++x) {
                    if (a[x] != b[x] && !(candidate == &actual && overlays.contains(QPoint(x, y)))) {
                        ++mismatches;
                        bounds |= QRect(x, y, 1, 1);
                        source = candidate == &actual ? "backing store" : "simulated store";
                    }
                }
            }
        }
        if (where != nullptr) {
            *where = QStringLiteral("%1 px (%6) in [%2,%3 %4x%5]")
                         .arg(mismatches).arg(bounds.x()).arg(bounds.y()).arg(bounds.width()).arg(bounds.height())
                         .arg(QLatin1String(source));
        }
        return mismatches;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!paused_ && watched == view_->viewport() && event->type() == QEvent::Paint) {
            region_ += static_cast<QPaintEvent *>(event)->region();
        }
        return false;
    }

private:
    QGraphicsView *view_ = nullptr;
    QImage image_;
    QRegion region_;
    QRegion lastRegion_;
    bool paused_ = false;
};
}

class MapEditorSpatialIndexRepaintTest final : public QObject
{
    Q_OBJECT

private:
    struct Harness
    {
        QTemporaryDir directory{QDir::current().filePath(QStringLiteral(".verification/spatial-index-XXXXXX"))};
        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        std::unique_ptr<MapEditorTab> tab;
        QGraphicsView *view = nullptr;
        QGraphicsScene *scene = nullptr;
    };

    bool open(Harness &harness, const QString &text)
    {
        QDir().mkpath(QStringLiteral(".verification"));
        if (!harness.directory.isValid() || text.isEmpty()) {
            return false;
        }
        const QString path = harness.directory.filePath(QStringLiteral("scene.th2"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(text.toUtf8());
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
        if (harness.view == nullptr) {
            return false;
        }
        harness.scene = harness.view->scene();
        QTest::qWait(50);
        harness.tab->triggerFit();
        QTest::qWait(50);
        return harness.scene != nullptr;
    }

    static QString compareIndex(QGraphicsScene *scene, quint32 seed, int *hiddenSeen)
    {
        QRandomGenerator random(seed);
        const QRectF bounds = scene->itemsBoundingRect();
        for (int query = 0; query < 300; ++query) {
            const qreal size = std::pow(10.0, boundedReal(random, -1.0, 2.6));
            const QPointF center(bounds.left() + random.bounded(bounds.width()),
                                 bounds.top() + random.bounded(bounds.height()));
            const QRectF rect(center - QPointF(size, size * 0.7), center + QPointF(size, size * 0.7));
            const QVector<QGraphicsItem *> expected = bruteHandlesInRect(scene, rect);
            const QVector<QGraphicsItem *> actual = MapEditorSceneHandleIndex::itemsInRect(scene, rect);
            if (expected != actual) {
                return QStringLiteral("rect query %1 mismatch\n expected %2\n actual   %3")
                    .arg(query).arg(describeItems(expected), describeItems(actual));
            }
            for (QGraphicsItem *item : expected) {
                *hiddenSeen += item->isVisible() ? 0 : 1;
            }
            const qreal radius = boundedReal(random, 0.5, 25.0);
            const int excluded = random.bounded(3) == 0 ? 0 : random.bounded(1, 200);
            const auto expectedSnap = bruteWallSnap(scene, center, radius, excluded);
            const auto actualSnap = nearestWallGeometrySnapPoint(scene, center, radius, excluded);
            if (expectedSnap.has_value() != actualSnap.has_value()
                || (expectedSnap && *expectedSnap != *actualSnap)) {
                return QStringLiteral("wall snap mismatch at (%1,%2) r=%3").arg(center.x()).arg(center.y()).arg(radius);
            }
        }
        QSet<int> lines;
        for (QGraphicsItem *item : scene->items()) {
            if (isHandleItem(item)) {
                lines.insert(handleLine(item));
            }
        }
        for (int line : std::as_const(lines)) {
            QVector<QGraphicsItem *> expected;
            for (QGraphicsItem *item : scene->items()) {
                if (isHandleItem(item) && handleLine(item) == line) {
                    expected.append(item);
                }
            }
            if (MapEditorSceneHandleIndex::itemsForLine(scene, line) != expected) {
                return QStringLiteral("line %1 query mismatch").arg(line);
            }
        }
        return {};
    }

    static QString compareDirectHits(Harness &harness, quint32 seed)
    {
        QRandomGenerator random(seed);
        QWidget *viewport = harness.view->viewport();
        QVector<QPoint> probes;
        for (QGraphicsItem *item : harness.scene->items()) {
            if (isHandleItem(item) && probes.size() < 200) {
                probes.append(harness.view->mapFromScene(handleKey(item))
                              + QPoint(random.bounded(-13, 14), random.bounded(-13, 14)));
            }
        }
        for (int i = 0; i < 100; ++i) {
            probes.append(QPoint(random.bounded(viewport->width()), random.bounded(viewport->height())));
        }
        for (const QPoint &probe : std::as_const(probes)) {
            for (int flags = 0; flags < 4; ++flags) {
                QGraphicsItem *expected = bruteDirectVertexLike(harness.scene, harness.view, probe, flags & 1, flags & 2);
                QGraphicsItem *actual = nearestDirectVertexLikeMapItem(harness.scene, harness.view, probe, flags & 1,
                                                                       flags & 2);
                if (expected != actual) {
                    return QStringLiteral("direct hit mismatch at %1,%2 flags %3").arg(probe.x()).arg(probe.y()).arg(flags);
                }
            }
        }
        return {};
    }

    static QString verifyAll(Harness &harness, quint32 seed, int *hiddenSeen)
    {
        QString failure = compareIndex(harness.scene, seed, hiddenSeen);
        if (failure.isEmpty()) {
            failure = compareDirectHits(harness, seed);
        }
        return failure;
    }

    static MapEditableGeometryVertexItem *wallAnchor(QGraphicsScene *scene, int index)
    {
        for (QGraphicsItem *item : scene->items()) {
            auto *vertex = dynamic_cast<MapEditableGeometryVertexItem *>(item);
            if (vertex != nullptr && isMapSceneWallItem(vertex)
                && vertex->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineAnchor
                && vertex->vertexIndex() == index) {
                return vertex;
            }
        }
        return nullptr;
    }

private slots:
    // Proposal 3: the handle index must return exactly what the former
    // whole-scene walks saw, in the same stacking order, including hidden
    // selection-gated anchors, across moves, removals and source edits.
    void handleIndexMatchesBruteForceIncludingHiddenAnchors_data()
    {
        QTest::addColumn<QString>("document");
        QTest::newRow("real-clopy01") << readFixture(
            QStringLiteral("tests/fixtures/projects/map_editor_regression/real_cave_raster/data/clopy01.th2"));
        for (quint32 seed = 1; seed <= 3; ++seed) {
            QTest::newRow(qPrintable(QStringLiteral("random-%1").arg(seed))) << randomDocument(seed * 7919u);
        }
    }

    void handleIndexMatchesBruteForceIncludingHiddenAnchors()
    {
        QFETCH(QString, document);
        Harness harness;
        QVERIFY(open(harness, document));
        int hiddenSeen = 0;
        QString failure = verifyAll(harness, 11, &hiddenSeen);
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        QVERIFY2(hiddenSeen > 0, "Hidden selection-gated anchors must be indexed.");

        // Fact check (not an assumption): Qt's BSP region query skips hidden
        // items, which is why bounded region queries alone lost hidden anchors.
        MapEditableGeometryVertexItem *hidden = wallAnchor(harness.scene, 0);
        if (hidden != nullptr && !hidden->isVisible()) {
            const QRectF probe(hidden->scenePos() - QPointF(1, 1), QSizeF(2, 2));
            const bool bspSeesHidden = harness.scene->items(probe).contains(hidden);
            qInfo("BSP items(rect) includes hidden ItemIgnoresTransformations anchor: %s",
                  bspSeesHidden ? "yes" : "no");
        }

        // Visibility toggles (selection), zoom and pan change nothing in the
        // index but must still agree with the brute-force walk.
        harness.tab->goToLine(document.left(document.indexOf(QStringLiteral("\nline "))).count(QLatin1Char('\n')) + 2);
        QCoreApplication::processEvents();
        for (int i = 0; i < 6; ++i) {
            harness.tab->triggerZoomIn();
        }
        QCoreApplication::processEvents();
        failure = verifyAll(harness, 12, &hiddenSeen);
        QVERIFY2(failure.isEmpty(), qPrintable(failure));

        // Item moves and removals are tracked incrementally.
        QRandomGenerator random(5);
        QVector<QGraphicsItem *> handles;
        for (QGraphicsItem *item : harness.scene->items()) {
            if (isHandleItem(item)) {
                handles.append(item);
            }
        }
        QVERIFY(handles.size() > 10);
        for (int i = 0; i < 10; ++i) {
            QGraphicsItem *item = handles.at(random.bounded(handles.size()));
            if (dynamic_cast<MapEditablePointItem *>(item) || dynamic_cast<MapEditableGeometryVertexItem *>(item)) {
                item->setPos(item->pos() + QPointF(boundedReal(random, -20.0, 20.0), boundedReal(random, -20.0, 20.0)));
            }
        }
        for (int i = 0; i < 3; ++i) {
            QGraphicsItem *item = handles.takeAt(random.bounded(handles.size()));
            harness.scene->removeItem(item);
            delete item;
        }
        failure = verifyAll(harness, 13, &hiddenSeen);
        QVERIFY2(failure.isEmpty(), qPrintable(failure));

        // A source edit re-renders the scene (items destroyed and recreated).
        harness.tab->triggerFit();
        QString edited = harness.tab->text();
        edited.replace(QStringLiteral("endscrap"), QStringLiteral("line wall\n  1 1\n  30 -30\nendline\nendscrap"));
        auto *sourceEditor = harness.tab->findChild<QPlainTextEdit *>();
        QVERIFY(sourceEditor != nullptr);
        sourceEditor->setPlainText(edited);
        QTest::qWait(400);
        failure = verifyAll(harness, 14, &hiddenSeen);
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        const MapEditorSceneHandleIndexStats stats = MapEditorSceneHandleIndex::stats(harness.scene);
        qInfo("handle index: %d tracked items, cell %.3f scene units", stats.trackedItems, stats.cellSize);
    }

    // Proposal 3: hover, vertex drag with snapping and new wall drawing must
    // not walk the whole scene per pointer move.
    void pointerMovesDoNotScanWholeScene()
    {
        Harness harness;
        QVERIFY(open(harness, readFixture(QStringLiteral(
                                  "tests/fixtures/projects/map_editor_regression/real_cave_raster/data/clopy01.th2"))));
        QWidget *viewport = harness.view->viewport();
        auto &scans = mapEditorPerformanceCounters().fullSceneItemScans;
        auto &examined = mapEditorPerformanceCounters().sceneHandleIndexExaminedEntries;
        auto &queries = mapEditorPerformanceCounters().sceneHandleIndexQueries;

        MapEditableGeometryVertexItem *anchor = wallAnchor(harness.scene, 3);
        QVERIFY(anchor != nullptr);
        for (int i = 0; i < 3; ++i) {
            harness.tab->triggerZoomIn();
        }
        harness.view->centerOn(anchor->scenePos());
        QTest::qWait(30);

        // 100 hover moves across dense wall geometry.
        const QPoint center = harness.view->mapFromScene(anchor->scenePos());
        sendPointer(viewport, QEvent::MouseMove, center, Qt::NoButton, Qt::NoButton);
        QTest::qWait(30);
        int before = counter(scans);
        Timings hover;
        for (int step = 0; step < 100; ++step) {
            const QPoint position = center + QPoint((step % 20) * 6 - 60, (step / 20) * 9 - 18);
            QElapsedTimer timer;
            timer.start();
            sendPointer(viewport, QEvent::MouseMove, position, Qt::NoButton, Qt::NoButton);
            QCoreApplication::processEvents();
            hover.ms.append(timer.nsecsElapsed() / 1e6);
            QTest::qWait(1);
        }
        QCOMPARE(counter(scans) - before, 0);
        qInfo("hover move+hit: %s", qPrintable(hover.summary()));

        // Vertex drag with neighbour/wall snapping (50 moves).
        harness.tab->goToLine(anchor->lineNumber());
        QTest::qWait(30);
        const QPoint start = harness.view->mapFromScene(anchor->scenePos());
        sendPointer(viewport, QEvent::MouseMove, start, Qt::NoButton, Qt::NoButton);
        QTest::qWait(30);
        sendPointer(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        before = counter(scans);
        int examinedBefore = counter(examined);
        int queriesBefore = counter(queries);
        Timings drag;
        for (int step = 1; step <= 50; ++step) {
            QElapsedTimer timer;
            timer.start();
            sendPointer(viewport, QEvent::MouseMove, start + QPoint(step * 3, step), Qt::NoButton, Qt::LeftButton);
            QCoreApplication::processEvents();
            drag.ms.append(timer.nsecsElapsed() / 1e6);
        }
        const int dragScans = counter(scans) - before;
        const int dragQueries = counter(queries) - queriesBefore;
        qInfo("vertex drag move: %s; index queries %d, examined entries %d",
              qPrintable(drag.summary()), dragQueries, counter(examined) - examinedBefore);
        sendPointer(viewport, QEvent::MouseButtonRelease, start + QPoint(150, 50), Qt::LeftButton, Qt::NoButton);
        QTest::qWait(50);
        harness.tab->triggerUndo();
        QTest::qWait(50);
        QCOMPARE(dragScans, 0);

        // New wall drawing: snap target + snap guides per move.
        harness.tab->triggerAddLine();
        auto *typeCombo = harness.tab->findChild<QComboBox *>(QStringLiteral("mapObjectQuickTypeCombo"));
        QVERIFY(typeCombo != nullptr);
        typeCombo->setCurrentText(QStringLiteral("wall"));
        QVERIFY(QMetaObject::invokeMethod(typeCombo, "activated", Q_ARG(int, typeCombo->currentIndex())));
        QTest::qWait(30);
        before = counter(scans);
        const int rebuildsBefore = counter(mapEditorPerformanceCounters().sceneRevisionIndexRebuilds);
        QStringList scanningMoves;
        Timings draw;
        const QPoint drawCenter = harness.view->mapFromScene(wallAnchor(harness.scene, 3)->scenePos());
        for (int step = 0; step < 50; ++step) {
            QElapsedTimer timer;
            timer.start();
            const int scansBeforeMove = counter(scans);
            sendPointer(viewport, QEvent::MouseMove, drawCenter + QPoint(step - 25, (step % 7) - 3), Qt::NoButton,
                        Qt::NoButton);
            QCoreApplication::processEvents();
            draw.ms.append(timer.nsecsElapsed() / 1e6);
            if (counter(scans) != scansBeforeMove) {
                scanningMoves << QString::number(step);
            }
        }
        qInfo("wall draw move: %s", qPrintable(draw.summary()));
        // The undo above produced a new scene generation: its per-line item
        // index may be rebuilt once, never per move.
        QVERIFY(counter(mapEditorPerformanceCounters().sceneRevisionIndexRebuilds) - rebuildsBefore <= 1);
        QVERIFY2(counter(scans) - before == 0,
                 qPrintable(QStringLiteral("whole-scene scans on draw moves %1").arg(scanningMoves.join(QLatin1Char(',')))));
    }

    // Proposal 4: hover/selection/drag/zoom/pan/detach must repaint only
    // dirty regions, and the partially updated viewport must equal a clean
    // full repaint (no dirty trails), including at very high zoom.
    void partialRepaintMatchesFullRepaint()
    {
        Harness harness;
        QVERIFY(open(harness, readFixture(QStringLiteral(
                                  "tests/fixtures/projects/map_editor_regression/real_cave_raster/data/clopy01.th2"))));
        QWidget *viewport = harness.view->viewport();
        MapEditableGeometryVertexItem *anchor = wallAnchor(harness.scene, 3);
        QVERIFY(anchor != nullptr);
        const int wallLine = anchor->lineNumber();
        auto findWallPath = [&]() -> QGraphicsPathItem * {
            for (QGraphicsItem *item : harness.scene->items()) {
                auto *path = dynamic_cast<QGraphicsPathItem *>(item);
                if (path != nullptr && path->data(kMapSceneLineNumberRole).toInt() == wallLine
                    && path->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeGeneric) {
                    return path;
                }
            }
            return nullptr;
        };
        QGraphicsPathItem *wallPath = findWallPath();
        QVERIFY(wallPath != nullptr);
        auto onWall = [&]() {
            return harness.view->mapFromScene(wallPath->mapToScene(wallPath->path().pointAtPercent(0.37)));
        };
        const QPoint empty(viewport->width() - 15, viewport->height() - 15);
        sendPointer(viewport, QEvent::MouseMove, empty, Qt::NoButton, Qt::NoButton);
        QTest::qWait(60);

        BackingStoreSimulator store(harness.view);
        store.settle();
        QString where;
        auto check = [&](const char *step, double maxFraction) {
            const double fraction = store.settle();
            const int mismatches = store.mismatchedPixels(&where);
            qInfo("%s: repainted %.1f%% of viewport", step, fraction * 100.0);
            QVERIFY2(mismatches == 0, qPrintable(QStringLiteral("%1 left dirty trail: %2").arg(QLatin1String(step), where)));
            QVERIFY2(fraction <= maxFraction,
                     qPrintable(QStringLiteral("%1 repainted %2% of the viewport")
                                    .arg(QLatin1String(step)).arg(fraction * 100.0)));
        };

        {
            auto *stale = new SilentColorItem(harness.view->mapToScene(QRect(20, 20, 30, 30)).boundingRect());
            stale->setZValue(1000.0);
            harness.scene->addItem(stale);
            store.settle();
            QVERIFY2(store.mismatchedPixels(&where) == 0, qPrintable(where));
            stale->color = Qt::blue;
            store.settle();
            QVERIFY2(store.mismatchedPixels(&where) > 0, "repaint comparison cannot see a stale region");
            stale->update();
            store.settle();
            QVERIFY2(store.mismatchedPixels(&where) == 0, qPrintable(where));
            delete stale;
            store.settle();
        }

        sendPointer(viewport, QEvent::MouseMove, onWall(), Qt::NoButton, Qt::NoButton);
        check("hover-enter", 0.5);
        sendPointer(viewport, QEvent::MouseMove, empty, Qt::NoButton, Qt::NoButton);
        check("hover-leave", 0.5);
        sendPointer(viewport, QEvent::MouseMove, onWall(), Qt::NoButton, Qt::NoButton);
        QTest::qWait(30);
        sendPointer(viewport, QEvent::MouseButtonPress, onWall(), Qt::LeftButton, Qt::LeftButton);
        sendPointer(viewport, QEvent::MouseButtonRelease, onWall(), Qt::LeftButton, Qt::NoButton);
        check("select", 1.0);

        // Vertex drag of the now-visible anchor.
        const QPoint start = harness.view->mapFromScene(anchor->scenePos());
        sendPointer(viewport, QEvent::MouseMove, start, Qt::NoButton, Qt::NoButton);
        QTest::qWait(30);
        store.settle();
        sendPointer(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 5; ++step) {
            sendPointer(viewport, QEvent::MouseMove, start + QPoint(step * 9, step * 4), Qt::NoButton, Qt::LeftButton);
            check("vertex-drag-move", 1.0);
        }
        sendPointer(viewport, QEvent::MouseButtonRelease, start + QPoint(45, 20), Qt::LeftButton, Qt::NoButton);
        check("vertex-drag-release", 1.0);
        harness.tab->triggerUndo();
        check("undo", 1.0);
        // Undo re-renders the scene; earlier item pointers are gone.
        wallPath = findWallPath();
        QVERIFY(wallPath != nullptr);

        // High zoom: screen-space markers and decorations must not trail.
        for (int i = 0; i < 12; ++i) {
            harness.tab->triggerZoomIn();
        }
        harness.view->centerOn(wallPath->mapToScene(wallPath->path().pointAtPercent(0.37)));
        check("zoom-in", 1.0);
        sendPointer(viewport, QEvent::MouseMove, onWall(), Qt::NoButton, Qt::NoButton);
        check("high-zoom-hover-enter", 1.0);
        sendPointer(viewport, QEvent::MouseMove, empty, Qt::NoButton, Qt::NoButton);
        check("high-zoom-hover-leave", 1.0);

        // Pan: the backing store scrolls, the exposed strip is repainted.
        for (int step = 0; step < 4; ++step) {
            const QPointF probe = harness.view->mapToScene(viewport->rect().center());
            const QPoint before = harness.view->mapFromScene(probe);
            harness.view->horizontalScrollBar()->setValue(harness.view->horizontalScrollBar()->value() + 37);
            harness.view->verticalScrollBar()->setValue(harness.view->verticalScrollBar()->value() - 23);
            store.scrollBy(harness.view->mapFromScene(probe) - before);
            check("pan", 1.0);
        }
        sendPointer(viewport, QEvent::MouseMove, onWall(), Qt::NoButton, Qt::NoButton);
        check("hover-after-pan", 1.0);

        // Detached map window keeps the same partial-update behaviour.
        harness.tab->toggleMapPaneWindow();
        QTest::qWait(100);
        store.settle();
        sendPointer(viewport, QEvent::MouseMove, empty, Qt::NoButton, Qt::NoButton);
        check("detached-hover-leave", 1.0);
        sendPointer(viewport, QEvent::MouseMove, onWall(), Qt::NoButton, Qt::NoButton);
        check("detached-hover-enter", 1.0);
        harness.tab->toggleMapPaneWindow();
        QTest::qWait(50);
        store.settle();

        // Draft previews while drawing (the reason the canvas once used
        // FullViewportUpdate): rubber band, draft vertices and snap markers.
        harness.tab->triggerFit();
        store.settle();
        const QPoint drawOrigin(viewport->width() / 3, viewport->height() / 3);
        auto drawSteps = [&](const char *name, bool clickVertices) {
            for (int step = 0; step < 4; ++step) {
                const QPoint position = drawOrigin + QPoint(step * 41, (step % 2) * 37);
                if (clickVertices) {
                    sendPointer(viewport, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
                    sendPointer(viewport, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton);
                    check(name, 1.0);
                }
                for (int move = 1; move <= 3; ++move) {
                    sendPointer(viewport, QEvent::MouseMove, position + QPoint(move * 11, move * 5), Qt::NoButton,
                                Qt::NoButton);
                    check(name, 1.0);
                }
            }
            QTest::keyClick(viewport, Qt::Key_Escape);
            QTest::qWait(30);
            check(name, 1.0);
        };
        harness.tab->triggerAddLine();
        QTest::qWait(30);
        store.settle();
        drawSteps("draw-line-draft", true);
        harness.tab->triggerAddArea();
        QTest::qWait(30);
        store.settle();
        drawSteps("draw-area-draft", true);
        harness.tab->triggerAddPoint();
        QTest::qWait(30);
        store.settle();
        drawSteps("draw-point-preview", false);
        harness.tab->triggerSelectMode();
    }

    // Proposal 4: the XVI sketch is rasterised once per camera/appearance
    // state and reused for hover/selection repaints; every appearance or
    // camera change invalidates it, and cached output equals direct paint.
    void xviBackgroundIsCachedPerViewAndInvalidated()
    {
        QGraphicsScene scene(QRectF(0, 0, 400, 300));
        QGraphicsView view(&scene);
        view.setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
        view.setRenderHint(QPainter::Antialiasing, true);
        view.resize(420, 320);
        auto *xvi = new MapEditorXviBackgroundItem;
        MapEditorXviLayerGeometryData geometry;
        geometry.contentBounds = QRectF(0, 0, 400, 300);
        for (int i = 0; i < 40; ++i) {
            geometry.traverseShotLines.append(QLineF(10 + i * 9, 20, 15 + i * 9, 280));
            geometry.splayShotLines.append(QLineF(10 + i * 9, 150, 30 + i * 9, 170));
        }
        MapEditorXviSketchPathData sketch;
        for (int i = 0; i < 60; ++i) {
            sketch.lines.append(QLineF(5 + i * 6, 40, 9 + i * 6, 260));
        }
        sketch.color = QColor(120, 40, 20);
        geometry.sketchPaths.append(sketch);
        MapEditorXviPassagePolygonData passage;
        passage.polygon = QPolygonF({QPointF(50, 50), QPointF(350, 60), QPointF(340, 240), QPointF(60, 230)});
        passage.bounds = passage.polygon.boundingRect();
        geometry.passagePolygons.append(passage);
        geometry.stations.append({QStringLiteral("A"), QPointF(200, 150)});
        xvi->setGeometryData(geometry);
        xvi->setOpacity(0.7);
        scene.addItem(xvi);
        auto *hoverable = scene.addRect(QRectF(180, 130, 30, 30), QPen(Qt::black), QBrush(Qt::yellow));
        hoverable->setZValue(5);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto &renders = mapEditorPerformanceCounters().xviBackgroundRenders;
        auto settle = [&] {
            for (int i = 0; i < 5; ++i) {
                QTest::qWait(10);
            }
        };
        auto grab = [&] { return view.viewport()->grab().toImage().convertToFormat(QImage::Format_ARGB32); };
        auto reference = [&] {
            MapEditorXviBackgroundItem::setViewportCacheEnabledForTesting(false);
            const QImage image = grab();
            MapEditorXviBackgroundItem::setViewportCacheEnabledForTesting(true);
            return image;
        };
        // The raster holds the semi-transparent sketch in 8-bit premultiplied
        // ARGB; compositing it once instead of per primitive may round a
        // channel by a few levels. Geometry/colour errors are far larger.
        constexpr int kRoundingTolerance = 3;
        auto maxChannelDelta = [](const QImage &a, const QImage &b) {
            int delta = 0;
            for (int y = 0; y < a.height(); ++y) {
                for (int x = 0; x < a.width(); ++x) {
                    const QRgb pa = a.pixel(x, y);
                    const QRgb pb = b.pixel(x, y);
                    delta = qMax(delta, qAbs(qRed(pa) - qRed(pb)));
                    delta = qMax(delta, qAbs(qGreen(pa) - qGreen(pb)));
                    delta = qMax(delta, qAbs(qBlue(pa) - qBlue(pb)));
                }
            }
            return delta;
        };

        // Warm up: the cache is (re)built once the camera has settled.
        view.viewport()->repaint();
        view.viewport()->repaint();
        settle();
        const int warm = counter(renders);
        for (int i = 0; i < 10; ++i) {
            hoverable->setBrush(i % 2 ? Qt::yellow : Qt::cyan);
            settle();
        }
        view.viewport()->repaint();
        QCOMPARE(counter(renders) - warm, 0);
        QVERIFY(maxChannelDelta(grab(), reference()) <= kRoundingTolerance);

        auto expectInvalidation = [&](const char *what, const std::function<void()> &change) {
            const QImage before = grab();
            change();
            settle();
            view.viewport()->repaint();
            view.viewport()->repaint();
            const QImage after = grab();
            const QImage direct = reference();
            QVERIFY2(after != before, what);
            QVERIFY2(maxChannelDelta(after, direct) <= kRoundingTolerance, what);
            const int count = counter(renders);
            hoverable->setBrush(Qt::magenta);
            settle();
            view.viewport()->repaint();
            QVERIFY2(counter(renders) == count, what);
        };
        expectInvalidation("centerline colour", [&] { xvi->setCenterlineAppearance(QColor(255, 0, 0), 1.8); });
        expectInvalidation("centerline width", [&] { xvi->setCenterlineAppearance(QColor(255, 0, 0), 4.0); });
        QVector<MapEditorXviTraverseColor> colors(geometry.traverseShotLines.size(),
                                                  MapEditorXviTraverseColor{Qt::green, Qt::blue, true});
        expectInvalidation("altitude colours", [&] { xvi->setCenterlineAltitudeColors(colors, true); });
        expectInvalidation("opacity", [&] { xvi->setOpacity(0.3); });
        expectInvalidation("transform", [&] { xvi->setTransform(QTransform::fromTranslate(12, 7).rotate(3)); });
        expectInvalidation("zoom", [&] { view.scale(1.7, 1.7); });
        expectInvalidation("pan", [&] { view.horizontalScrollBar()->setValue(view.horizontalScrollBar()->value() + 40); });
        expectInvalidation("hidden", [&] { xvi->setVisible(false); });
        expectInvalidation("shown", [&] { xvi->setVisible(true); });
        MapEditorXviLayerGeometryData moved = geometry;
        for (QLineF &line : moved.traverseShotLines) {
            line.translate(3, 0);
        }
        expectInvalidation("geometry", [&] { xvi->setGeometryData(moved); });

        // Very high zoom keeps the buffer viewport-sized (bounded memory) and
        // vector-exact.
        view.resetTransform();
        view.scale(200.0, 200.0);
        view.centerOn(QPointF(200, 150));
        settle();
        view.viewport()->repaint();
        view.viewport()->repaint();
        QVERIFY(maxChannelDelta(grab(), reference()) <= kRoundingTolerance);
        QVERIFY(xvi->viewportCacheBytesForTesting() <= qint64(view.viewport()->width()) * view.viewport()->height()
                                                             * 4 * view.viewport()->devicePixelRatioF()
                                                             * view.viewport()->devicePixelRatioF());

        // Off-screen rendering (export, magnifier) never uses the raster cache.
        const int beforeRender = counter(renders);
        QImage exported(200, 150, QImage::Format_ARGB32_Premultiplied);
        exported.fill(Qt::white);
        QPainter painter(&exported);
        scene.render(&painter);
        painter.end();
        QVERIFY(counter(renders) > beforeRender);
    }
};

int runMapEditorSpatialIndexRepaintTest(int argc, char **argv)
{
    MapEditorSpatialIndexRepaintTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorSpatialIndexRepaintTest.moc"
