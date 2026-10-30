#include "../src/app/text_editor/map_editor/MapEditorAltitudeUnderlay.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/core/SessionStore.h"
#include "../src/core/TherionSourceDocument.h"
#include "../src/core/TherionBackgroundMetadata.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/QtFileSystem.h"

#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtTest/QtTest>
#include <QTextStream>
#include <algorithm>
#include <cmath>

using namespace TherionStudio;

namespace
{
MapEditorSourceProjectionSnapshot snapshot()
{
    const QString text = QStringLiteral(
        "scrap s -projection plan\n"
        "point 10 10 station -name a@cave\n"
        "point 90 90 station -name b@cave\n"
        "line wall -close on\n0 0\n100 0\n100 100\n0 100\nendline\n"
        "line wall -close on\n40 40\n60 40\n60 60\n40 60\nendline\nendscrap\n");
    const auto source = TherionSourceDocument::fromText(text);
    const auto logical = TherionSourceLogicalDocument::fromSourceDocument(source);
    MapEditorSourceProjectionSnapshot result;
    result.logicalCommands = logical.commands();
    result.geometryProjection = Th2GeometryProjection::fromDocuments(source, logical);
    result.sourceBounds = QRectF(0, 0, 100, 100);
    return result;
}

QString largeAreaHeavyMapText(int scrapCount, int wallsPerScrap, int areasPerScrap, int stationsPerScrap)
{
    QString text;
    QTextStream out(&text);
    for (int s = 0; s < scrapCount; ++s) {
        const int ox = (s % 10) * 400;
        const int oy = (s / 10) * 400;
        out << "scrap s" << s << " -projection plan\n";
        out << "line wall -id outline" << s << " -close on\n";
        for (int k = 0; k < 24; ++k) {
            const double angle = (6.283185307179586 * k) / 24.0;
            out << ox + 180 + 170 * std::cos(angle) << ' ' << oy + 180 + 170 * std::sin(angle) << '\n';
        }
        out << "endline\n";
        for (int w = 0; w < wallsPerScrap; ++w) {
            out << "line wall -id w" << s << '_' << w << '\n';
            for (int k = 0; k < 8; ++k) {
                out << ox + 30 + (w * 7) % 300 << ' ' << oy + 30 + k * 35 + (w % 5) << '\n';
            }
            out << "endline\n";
        }
        for (int a = 0; a < areasPerScrap; ++a) {
            out << "area water -id a" << s << '_' << a << "\n w" << s << '_' << a << "\n w" << s << '_' << (a + 1)
                << "\nendarea\n";
        }
        for (int p = 0; p < stationsPerScrap; ++p) {
            out << "point " << ox + 40 + p * 9 << ' ' << oy + 200 << " station -name st" << s << '_' << p << "@cave\n";
        }
        out << "endscrap\n";
    }
    return text;
}

qint64 residentKilobytes()
{
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly)) {
        return -1;
    }
    for (const QByteArray &line : status.readAll().split('\n')) {
        if (line.startsWith("VmRSS:")) {
            return line.mid(6).trimmed().split(' ').value(0).toLongLong();
        }
    }
    return -1;
}

QList<QGraphicsItem *> underlayItems(QGraphicsScene *scene)
{
    QList<QGraphicsItem *> result;
    for (auto *item : scene->items()) {
        if (item->data(kMapAltitudeUnderlayRole).toBool()) {
            result.append(item);
        }
    }
    return result;
}
}

class MapEditorAltitudeUnderlayTest final : public QObject
{
    Q_OBJECT
private slots:
    void segmentedPassageWithClosedInnerWall()
    {
        const QString text = QStringLiteral(
            "scrap s -projection plan\n"
            "line wall\n0 0\n100 0\n100 100\nendline\n"
            "line wall -reverse on\n0 0\n0 100\n100 100\nendline\n"
            "line wall -close on -outline in\n40 40\n60 40\n60 60\n40 60\nendline\n"
            "line wall -outline none\n20 20\n30 30\nendline\nendscrap\n");
        const auto doc = TherionSourceDocument::fromText(text);
        const auto logical = TherionSourceLogicalDocument::fromSourceDocument(doc);
        auto source = snapshot();
        source.logicalCommands = logical.commands();
        source.geometryProjection = Th2GeometryProjection::fromDocuments(doc, logical);
        QGraphicsScene scene;
        renderMapEditorAltitudeUnderlay(&scene, source, {{1, {QColor(140, 180, 240, 48), {}}}});
        QImage image(200, 200, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        scene.render(&painter, QRectF(0, 0, 200, 200), mapEditorCanvasPreviewBounds(), Qt::IgnoreAspectRatio);
        painter.end();
        QCOMPARE(image.pixelColor(30, 30).alpha(), 48);
        QCOMPARE(image.pixelColor(100, 100).alpha(), 0);
    }

    void connectedOutlineComponents_data()
    {
        QTest::addColumn<QString>("walls");
        QTest::addColumn<int>("filled");
        QTest::newRow("straight-four-segments") << QStringLiteral(
            "line wall\n0 0\n100 0\nendline\nline wall\n100 100\n100 0\nendline\n"
            "line wall\n100 100\n0 100\nendline\nline wall\n0 100\n0 0\nendline\n") << 1;
        QTest::newRow("disjoint-connected-components") << QStringLiteral(
            "line wall\n0 0\n40 0\n40 40\nendline\nline wall\n40 40\n0 40\n0 0\nendline\n"
            "line wall\n60 60\n100 60\n100 100\nendline\nline wall\n100 100\n60 100\n60 60\nendline\n") << 1;
        QTest::newRow("inner-outline-alone") << QStringLiteral(
            "line wall -close on -outline in\n0 0\n100 0\n100 100\n0 100\nendline\n") << 0;
        QTest::newRow("unclosed-passage") << QStringLiteral(
            "line wall\n0 0\n0 100\nendline\nline wall\n100 0\n100 100\nendline\n") << 0;
        QTest::newRow("non-wall-explicit-outline") << QStringLiteral(
            "line pit -outline out\n0 0\n100 0\n100 100\n0 100\n0 0\nendline\n") << 1;
        QTest::newRow("curved-reversed-component") << QStringLiteral(
            "line wall\n0 0\n30 0 70 0 100 0\n100 100\nendline\n"
            "line wall\n0 0\n0 100\n30 100 70 100 100 100\nendline\n") << 1;
    }

    void connectedOutlineComponents()
    {
        QFETCH(QString, walls);
        QFETCH(int, filled);
        const auto doc = TherionSourceDocument::fromText(
            QStringLiteral("scrap s -projection plan\n") + walls + QStringLiteral("endscrap\n"));
        const auto logical = TherionSourceLogicalDocument::fromSourceDocument(doc);
        auto source = snapshot();
        source.logicalCommands = logical.commands();
        source.geometryProjection = Th2GeometryProjection::fromDocuments(doc, logical);
        QGraphicsScene scene;
        QCOMPARE(renderMapEditorAltitudeUnderlay(&scene, source, {{1, {QColor(140, 180, 240, 48), {}}}}), filled);
        QCOMPARE(underlayItems(&scene).size(), filled);
    }

    void approximateGapClosure_data()
    {
        QTest::addColumn<QString>("walls");
        QTest::addColumn<int>("filled");
        QTest::newRow("open-corridor") << QStringLiteral(
            "line wall\n0 0\n100 0\n100 100\nendline\n"
            "line wall\n0 0.5\n0 100\n99.5 100\nendline\n"
            "line wall -close on -outline in\n40 40\n60 40\n60 60\n40 60\nendline\n") << 1;
        QTest::newRow("remote-corridor-walls") << QStringLiteral(
            "line wall\n0 0\n0 100\nendline\nline wall\n100 0\n100 100\nendline\n") << 0;
        QTest::newRow("long-gap-budget") << QStringLiteral(
            "line wall\n0 0\n0 10\n10 10\nendline\n"
            "line wall\n10 0\n20 0\n20 10\nendline\n") << 0;
        QTest::newRow("ambiguous-junction") << QStringLiteral(
            "line wall\n0 0\n100 0\n100 100\nendline\n"
            "line wall\n100 100\n0 100\n0 5\nendline\n"
            "line wall\n100 100\n110 100\nendline\n") << 0;
    }

    void approximateGapClosure()
    {
        QFETCH(QString, walls);
        QFETCH(int, filled);
        const auto doc = TherionSourceDocument::fromText(
            QStringLiteral("scrap s -projection plan\n") + walls + QStringLiteral("endscrap\n"));
        const auto logical = TherionSourceLogicalDocument::fromSourceDocument(doc);
        auto source = snapshot();
        source.logicalCommands = logical.commands();
        source.geometryProjection = Th2GeometryProjection::fromDocuments(doc, logical);
        const auto features = collectGeometryFeatures(source.geometryProjection, source.logicalCommands);
        QVERIFY(scrapClipPathsForFeatures(features, source.sourceBounds, mapEditorCanvasPreviewBounds(), true).isEmpty());
        QGraphicsScene scene;
        int approximate = -1;
        QCOMPARE(renderMapEditorAltitudeUnderlay(&scene, source, {{1, {QColor(140, 180, 240, 48), {}}}},
                                                &approximate), filled);
        QCOMPARE(approximate, filled);
        if (filled) {
            const auto *item = qgraphicsitem_cast<QGraphicsPathItem *>(underlayItems(&scene).first());
            QVERIFY(item != nullptr);
            QVERIFY(item->data(kMapAltitudeUnderlayApproximateRole).toBool());
            QVERIFY(item->toolTip().contains(QStringLiteral("Approximate")));
            QImage image(200, 200, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            scene.render(&painter, QRectF(0, 0, 200, 200), mapEditorCanvasPreviewBounds(), Qt::IgnoreAspectRatio);
            painter.end();
            QCOMPARE(image.pixelColor(30, 30).alpha(), 48);
            QCOMPARE(image.pixelColor(100, 100).alpha(), 0);
        }
    }

    void privateMapProductionRaster()
    {
        const QString path = qEnvironmentVariable("THERION_UNDERLAY_REPRO");
        if (path.isEmpty()) {
            QSKIP("Opt-in read-only local map repro; no private fixture in the repository.");
        }
        QtFileSystem files;
        InMemorySessionStore prefs;
        const CommandCatalogStore catalog;
        MapEditorTab tab(files, prefs, catalog);
        QString error;
        QVERIFY2(tab.loadFile(path, &error), qPrintable(error));
        QGraphicsView *view = nullptr;
        for (auto *candidate : tab.findChildren<QGraphicsView *>()) {
            if (candidate->scene() != nullptr
                && (view == nullptr || candidate->scene()->items().size() > view->scene()->items().size())) {
                view = candidate;
            }
        }
        QVERIFY(view != nullptr);
        auto *scene = view->scene();
        const auto doc = TherionSourceDocument::fromText(tab.text());
        const auto logical = TherionSourceLogicalDocument::fromSourceDocument(doc);
        const auto geometry = Th2GeometryProjection::fromDocuments(doc, logical);
        QSet<int> stationLines;
        for (const auto &point : geometry.points()) {
            if (point.command.type == QStringLiteral("station")) {
                stationLines.insert(point.command.sourceRange.startLineNumber);
            }
        }
        QRectF geometryBounds;
        for (auto *item : scene->items()) {
            if (item->isVisible() && item->data(kMapItemRole).toInt() == kMapItemGeometryValue) {
                geometryBounds = geometryBounds.united(item->sceneBoundingRect());
            }
        }
        QVERIFY(geometryBounds.isValid());
        geometryBounds.adjust(-12, -12, 12, 12);
        const auto raster = [scene, geometryBounds]() {
            QImage image(2400, 1800, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);
            QPainter painter(&image);
            scene->render(&painter, QRectF(0, 0, 2400, 1800), geometryBounds, Qt::IgnoreAspectRatio);
            painter.end();
            return image;
        };
        const QImage off = raster();
        tab.setAltitudeUnderlayData(true, {}, {});
        const QImage on = raster();
        for (auto *item : underlayItems(scene)) {
            if (item->data(kMapAltitudeUnderlayApproximateRole).toBool()) {
                item->setVisible(false);
            }
        }
        const QImage preciseOnly = raster();
        for (auto *item : underlayItems(scene)) {
            item->setVisible(true);
        }
        QList<QPainterPath> passages;
        QList<QPainterPath> approximatePassages;
        QList<QPainterPath> areas;
        int approximateCount = 0;
        for (auto *item : scene->items()) {
            if (item->data(kMapAltitudeUnderlayRole).toBool()) {
                QVERIFY(item->isVisible());
                QCOMPARE(item->parentItem(), nullptr);
                QCOMPARE(item->zValue(), 1.9);
                QVERIFY(!(item->flags() & QGraphicsItem::ItemIsSelectable));
                QCOMPARE(item->acceptedMouseButtons(), Qt::NoButton);
                const auto *pathItem = qgraphicsitem_cast<QGraphicsPathItem *>(item);
                QVERIFY(pathItem != nullptr);
                QCOMPARE(pathItem->brush().color().alpha(), 48);
                passages.append(item->mapToScene(item->shape()));
                approximateCount += item->data(kMapAltitudeUnderlayApproximateRole).toBool() ? 1 : 0;
                if (item->data(kMapAltitudeUnderlayApproximateRole).toBool()) {
                    approximatePassages.append(item->mapToScene(item->shape()));
                }
            } else if (item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeAreaFill) {
                areas.append(item->mapToScene(item->shape()));
            }
        }
        int eligible = 0;
        int changed = 0;
        int addedPixels = 0;
        QSet<int> stationsInPassages;
        for (auto *item : scene->items()) {
            const int line = item->data(kMapSceneLineNumberRole).toInt();
            if (stationLines.contains(line)
                && std::any_of(passages.cbegin(), passages.cend(), [&](const QPainterPath &passage) {
                    return passage.contains(item->mapToScene(QPointF()));
                })) {
                stationsInPassages.insert(line);
            }
        }
        for (int y = 0; y < on.height(); ++y) {
            for (int x = 0; x < on.width(); ++x) {
                if (on.pixel(x, y) == off.pixel(x, y)) {
                    continue;
                }
                const QPointF point(geometryBounds.left() + (x + 0.5) * geometryBounds.width() / on.width(),
                                    geometryBounds.top() + (y + 0.5) * geometryBounds.height() / on.height());
                if (std::any_of(areas.cbegin(), areas.cend(), [&](const QPainterPath &area) { return area.contains(point); })) {
                    continue;
                }
                if (std::any_of(passages.cbegin(), passages.cend(), [&](const QPainterPath &passage) { return passage.contains(point); })) {
                    ++eligible;
                    const QColor a = on.pixelColor(x, y);
                    const QColor b = off.pixelColor(x, y);
                    if (qMax(qAbs(a.red() - b.red()), qMax(qAbs(a.green() - b.green()), qAbs(a.blue() - b.blue()))) >= 5) {
                        ++changed;
                        if (on.pixel(x, y) != preciseOnly.pixel(x, y)
                            && std::any_of(approximatePassages.cbegin(), approximatePassages.cend(),
                                [&](const QPainterPath &passage) { return passage.contains(point); })) {
                            ++addedPixels;
                        }
                    }
                }
            }
        }
        qInfo("private-map original_scraps=%lld underlay_items=%lld approximate=%d stations_in_passages=%lld outside_area_changed_pixels=%d detectable_pixels=%d added_approximate_pixels=%d rss_kb=%lld",
              static_cast<long long>(std::count_if(geometry.blockObjects().cbegin(), geometry.blockObjects().cend(),
                  [](const auto &block) { return block.kind == Th2GeometryObjectKind::Scrap; })),
              static_cast<long long>(passages.size()), approximateCount, static_cast<long long>(stationsInPassages.size()),
              eligible, changed, addedPixels, static_cast<long long>(residentKilobytes()));
        const QString output = qEnvironmentVariable("THERION_UNDERLAY_RASTER_PREFIX");
        if (!output.isEmpty()) {
            QVERIFY(off.save(output + QStringLiteral("-off.png")));
            QVERIFY(on.save(output + QStringLiteral("-on.png")));
            QVERIFY(preciseOnly.save(output + QStringLiteral("-precise.png")));
        }
        QVERIFY(!passages.isEmpty());
        QVERIFY(!stationsInPassages.isEmpty());
        const QByteArray expected = qgetenv("THERION_UNDERLAY_EXPECTED_SCRAPS");
        if (!expected.isEmpty()) {
            QCOMPARE(passages.size(), expected.toInt());
        }
        QVERIFY(changed > 100);
        if (approximateCount > 0) {
            QVERIFY(addedPixels > 0);
        }
        tab.setAltitudeUnderlayData(false, {}, {});
        QCOMPARE(raster(), off);
    }

    void privateMapContourCoverage()
    {
        const QString path = qEnvironmentVariable("THERION_UNDERLAY_REPRO");
        if (path.isEmpty()) {
            QSKIP("Opt-in bounded read-only contour analysis; no GUI map load.");
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.size() < 8 * 1024 * 1024);
        const QString text = QString::fromUtf8(file.readAll());
        const auto doc = TherionSourceDocument::fromText(text);
        const auto logical = TherionSourceLogicalDocument::fromSourceDocument(doc);
        const auto geometry = Th2GeometryProjection::fromDocuments(doc, logical);
        const auto features = collectGeometryFeatures(geometry, logical.commands());
        const auto areaAdjust = parseTherionAreaAdjust(text);
        QVERIFY(areaAdjust.valid);
        QElapsedTimer timer;
        timer.start();
        const auto precise = scrapClipPathsForFeatures(features, areaAdjust.modelRect,
                                                      mapEditorCanvasPreviewBounds(), true);
        const auto inferred = scrapClipPathsForFeatures(features, areaAdjust.modelRect,
                                                       mapEditorCanvasPreviewBounds(), true, true);
        int approximate = 0;
        for (auto it = inferred.cbegin(); it != inferred.cend(); ++it) {
            if (!it.value().isEmpty() && precise.value(it.key()).isEmpty()) {
                ++approximate;
            }
        }
        qInfo("private-contours precise=%lld approximate=%d elapsed_ms=%lld rss_kb=%lld",
              static_cast<long long>(precise.size()), approximate,
              static_cast<long long>(timer.elapsed()), static_cast<long long>(residentKilobytes()));
        QCOMPARE(precise.size(), 22);
        QVERIFY(approximate > 0);
        QVERIFY(timer.elapsed() < 3000);
    }

    void overlappingScrapsRemainTranslucent()
    {
        QGraphicsScene scene;
        auto source = snapshot();
        const QString text = QStringLiteral(
            "scrap first -projection plan\nline wall -close on\n0 0\n100 0\n100 100\n0 100\nendline\nendscrap\n"
            "scrap second -projection plan\nline wall -close on\n0 0\n100 0\n100 100\n0 100\nendline\nendscrap\n");
        const auto doc = TherionSourceDocument::fromText(text);
        const auto logical = TherionSourceLogicalDocument::fromSourceDocument(doc);
        source.logicalCommands = logical.commands();
        source.geometryProjection = Th2GeometryProjection::fromDocuments(doc, logical);
        renderMapEditorAltitudeUnderlay(&scene, source,
            {{1, {QColor(140, 180, 240, 48), {}}}, {9, {QColor(240, 180, 140, 48), {}}}});
        QCOMPARE(scene.items().size(), 2);
        QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        scene.render(&painter, QRectF(0, 0, 100, 100), mapEditorCanvasPreviewBounds(), Qt::IgnoreAspectRatio);
        painter.end();
        QCOMPARE(image.pixelColor(50, 50).alpha(), 87);
    }
    void toggleDoesNotMutateSourceOrDirtyState()
    {
        QtFileSystem files;
        InMemorySessionStore prefs;
        const CommandCatalogStore catalog;
        MapEditorTab tab(files, prefs, catalog);
        const QString path = QStringLiteral(THERION_STUDIO_TEST_SOURCE_ROOT)
            + QStringLiteral("/tests/fixtures/projects/map_editor_regression/default_template_like/scraps/scrap1.th2");
        QString error;
        QVERIFY2(tab.loadFile(path, &error), qPrintable(error));
        const QString original = tab.text();
        const bool dirty = tab.isDirty();
        tab.setAltitudeUnderlayData(true, {}, {});
        QCOMPARE(tab.text(), original);
        QCOMPARE(tab.isDirty(), dirty);
        QVERIFY(tab.statusHintText().contains(QStringLiteral("not altitude")));
        tab.setAltitudeUnderlayData(false, {}, {});
        QCOMPARE(tab.text(), original);
        QCOMPARE(tab.isDirty(), dirty);
        QVERIFY(!tab.statusHintText().contains(QStringLiteral("Underlay:")));
    }
    void paintsTranslucentPassagesWithoutHolesOrSelection()
    {
        QGraphicsScene scene;
        const auto source = snapshot();
        const QColor pastel(140, 180, 240, 48);
        renderMapEditorAltitudeUnderlay(&scene, source, {{1, {pastel, QStringLiteral("categorical")}}});
        QCOMPARE(scene.items().size(), 1);
        QVERIFY(!(scene.items().first()->flags() & QGraphicsItem::ItemIsSelectable));
        QCOMPARE(scene.items().first()->acceptedMouseButtons(), Qt::NoButton);
        const QRectF preview = mapEditorCanvasPreviewBounds();
        QImage image(200, 200, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        scene.render(&painter, QRectF(0, 0, 200, 200), preview, Qt::IgnoreAspectRatio);
        painter.end();
        QCOMPARE(image.pixelColor(100, 100).alpha(), 0);
        QCOMPARE(image.pixelColor(30, 30).alpha(), 48);
        renderMapEditorAltitudeUnderlay(&scene, source, {{1, {pastel, QString()}}});
        QCOMPARE(scene.items().size(), 1);
        renderMapEditorAltitudeUnderlay(&scene, source, {});
        QCOMPARE(scene.items().size(), 0);
    }

    void mapsRealSourceReferencesAndLabelsFallback()
    {
        const auto source = snapshot();
        ProjectIndexSnapshot index;
        index.stationReferenceCandidateKeysByLookupKey = {
            {QStringLiteral("a@cave"), {QStringLiteral("a@cave")}},
            {QStringLiteral("b@cave"), {QStringLiteral("b@cave")}}};
        ScrapAltitudeStations stations;
        stations.heights = {{QStringLiteral("a@cave"), 300}, {QStringLiteral("b@cave"), 280}};
        stations.entranceReference = QStringLiteral("a@cave");
        stations.entranceHeightMetres = 300;
        stations.minimumHeightMetres = 280;
        stations.maximumHeightMetres = 300;
        const auto mean = mapEditorAltitudeScraps(source, QStringLiteral("/project/s.th2"), index, stations);
        QVERIFY(mean.value(1).description.contains(QStringLiteral("-10.0")));
        QVERIFY(mean.value(1).description.contains(QStringLiteral("not an absolute datum")));
        for (const int percent : {0, 19, 100}) {
            const auto styled = mapEditorAltitudeScraps(source, QStringLiteral("/project/s.th2"),
                                                       index, stations, percent);
            QColor expected = mean.value(1).color;
            expected.setAlphaF(percent / 100.0);
            QCOMPARE(styled.value(1).color, expected);
            QCOMPARE(styled.value(1).description, mean.value(1).description);
        }
        stations.heights.remove(QStringLiteral("b@cave"));
        const auto partial = mapEditorAltitudeScraps(source, QStringLiteral("/project/s.th2"), index, stations);
        QVERIFY(partial.value(1).description.contains(QStringLiteral("not altitude")));
        QCOMPARE(partial.value(1).color,
                 mapEditorAltitudeScraps(source, QStringLiteral("/project/s.th2"), {}, {}).value(1).color);
    }

    void largeAreaHeavyMapStaysResponsiveWithUnderlayOffAndOn()
    {
        // Regression for the real-map "not responding" incident: thousands of objects with border-line
        // area references must not trigger quadratic Objects-panel work, and the underlay must stay bounded.
        const QString path = QStringLiteral(THERION_STUDIO_TEST_WORKSPACE) + QStringLiteral("/altitude-large-area-heavy.th2");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(largeAreaHeavyMapText(80, 22, 2, 12).toUtf8());
        file.close();
        QtFileSystem files;
        InMemorySessionStore prefs;
        const CommandCatalogStore catalog;
        MapEditorTab tab(files, prefs, catalog);
        tab.resize(1200, 800);
        QElapsedTimer timer;
        timer.start();
        QString error;
        QVERIFY2(tab.loadFile(path, &error), qPrintable(error));
        tab.show();
        QCoreApplication::processEvents();
        const qint64 offLoadMs = timer.elapsed();
        QGraphicsView *view = nullptr;
        for (auto *candidate : tab.findChildren<QGraphicsView *>()) {
            if (candidate->scene() != nullptr
                && (view == nullptr || candidate->scene()->items().size() > view->scene()->items().size())) {
                view = candidate;
            }
        }
        QVERIFY(view != nullptr);
        QGraphicsScene *scene = view->scene();
        QVERIFY(scene->items().size() > 5000);
        QVERIFY(underlayItems(scene).isEmpty());
        const qint64 offRssKb = residentKilobytes();

        timer.restart();
        tab.setAltitudeUnderlayData(true, {}, {});
        QCoreApplication::processEvents();
        const qint64 enableMs = timer.elapsed();
        const QList<QGraphicsItem *> enabledItems = underlayItems(scene);
        const qint64 onRssKb = residentKilobytes();
        qInfo("large-area-heavy items=%lld off_load_ms=%lld enable_ms=%lld underlay_items=%lld rss_off_kb=%lld rss_on_kb=%lld",
              static_cast<long long>(scene->items().size()), static_cast<long long>(offLoadMs),
              static_cast<long long>(enableMs), static_cast<long long>(enabledItems.size()),
              static_cast<long long>(offRssKb), static_cast<long long>(onRssKb));
        QCOMPARE(enabledItems.size(), 80);
        QVERIFY2(offLoadMs < 6000, qPrintable(QStringLiteral("load with underlay off took %1 ms").arg(offLoadMs)));
        QVERIFY2(enableMs < 3000, qPrintable(QStringLiteral("underlay enable took %1 ms").arg(enableMs)));
        if (offRssKb > 0 && onRssKb > 0) {
            QVERIFY2(onRssKb - offRssKb < 128 * 1024,
                     qPrintable(QStringLiteral("underlay grew RSS by %1 KiB").arg(onRssKb - offRssKb)));
        }

        auto *viewport = view->viewport();
        const QPointF center = viewport->rect().center();
        timer.restart();
        for (int step = 0; step < 4; ++step) {
            QWheelEvent wheel(center, viewport->mapToGlobal(center), QPoint(), QPoint(0, 120), Qt::NoButton,
                              Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(viewport, &wheel);
            view->horizontalScrollBar()->setValue(view->horizontalScrollBar()->value() + 60);
            QCoreApplication::processEvents();
        }
        const qint64 navigationMs = timer.elapsed();
        QCOMPARE(underlayItems(scene), enabledItems);
        QVERIFY2(navigationMs < 3000, qPrintable(QStringLiteral("zoom/pan took %1 ms").arg(navigationMs)));

        tab.setAltitudeUnderlayData(false, {}, {});
        QVERIFY(underlayItems(scene).isEmpty());
        QFile::remove(path);
    }


    void defaultsOffAndPersistsPreference()
    {
        const QString path = QStringLiteral(THERION_STUDIO_TEST_WORKSPACE) + QStringLiteral("/altitude-settings.ini");
        QFile::remove(path);
        {
            SessionSettingsStore prefs(path);
            QVERIFY(!prefs.therionMapAltitudeUnderlayEnabled());
            prefs.setTherionMapAltitudeUnderlayEnabled(true);
        }
        {
            SessionSettingsStore prefs(path);
            QVERIFY(prefs.therionMapAltitudeUnderlayEnabled());
            prefs.setTherionMapAltitudeUnderlayEnabled(false);
        }
        {
            SessionSettingsStore prefs(path);
            QVERIFY(!prefs.therionMapAltitudeUnderlayEnabled());
        }
        QFile::remove(path);
        InMemorySessionStore memory;
        QVERIFY(!memory.therionMapAltitudeUnderlayEnabled());
        memory.setTherionMapAltitudeUnderlayEnabled(true);
        QVERIFY(memory.therionMapAltitudeUnderlayEnabled());
    }
};

int runMapEditorAltitudeUnderlayTest(int argc, char **argv)
{
    MapEditorAltitudeUnderlayTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorAltitudeUnderlayTest.moc"
