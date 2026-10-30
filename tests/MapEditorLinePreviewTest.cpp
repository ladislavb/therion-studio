#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"
#include "../src/app/text_editor/map_editor/MapEditorLineDecorationItem.h"
#include "../src/app/text_editor/map_editor/MapEditorPerformanceCounters.h"
#include "../src/app/text_editor/map_editor/MapEditorDirectionAidItem.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneThemePolicy.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneLifecycleController.h"
#include "../src/core/TherionDocumentParser.h"

#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QScrollBar>
#include <QTest>

#include <cmath>

using namespace TherionStudio;

class MapEditorLinePreviewTest final : public QObject
{
    Q_OBJECT

private slots:
    void projectionCacheReusesPanAndMagnifier()
    {
        QPainterPath path(QPointF(20, 80));
        path.cubicTo(130, -20, 210, 180, 370, 80);
        MapEditorLineDecorationStyle glyph;
        glyph.kind = MapEditorLineDecorationKind::Parallel;
        glyph.offsets = {-5, 5};
        glyph.strokeWidth = 1;
        glyph.strokeColor = Qt::black;
        QGraphicsScene scene;
        MapEditorLineDecorationItem item(path, {glyph}, Qt::black, false, 123);
        scene.addItem(&item);
        const auto render = [&](const QTransform &transform) {
            QImage image(640, 480, QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            painter.setWorldTransform(transform);
            scene.render(&painter, QRectF(0, 0, 640, 480), QRectF(0, 0, 640, 480));
            painter.end();
            return image;
        };
        QTransform main;
        main.rotate(17);
        QTransform magnifier = main;
        magnifier.scale(2, 2);
        QTransform panned = main;
        panned.setMatrix(main.m11(), main.m12(), main.m13(),
                         main.m21(), main.m22(), main.m23(), 31.25, 23.5, main.m33());
        auto &computations = mapEditorPerformanceCounters().lineDecorationProjectionComputations;
        const int before = computations.load();
        const QImage initial = render(main);
        render(magnifier);
        const QImage moved = render(panned);
        QCOMPARE(render(main), initial);
        QCOMPARE(render(panned), moved);
        qInfo() << "navigation-pixels"
                << QCryptographicHash::hash(QByteArrayView(
                       reinterpret_cast<const char *>(moved.constBits()), moved.sizeInBytes()),
                       QCryptographicHash::Sha256).toHex();
        const int builds = computations.load() - before;
        scene.removeItem(&item);
        QCOMPARE(builds, 2);
    }

    void nominalModesViewportRendering_data()
    {
        QTest::addColumn<int>("kind");
        QTest::addColumn<bool>("curved");
        QTest::addColumn<qreal>("pixelRatio");
        QTest::addColumn<bool>("reversed");
        QTest::addColumn<int>("interaction");
        QTest::addColumn<bool>("multipleSubpaths");
        for (int kind = 0; kind <= int(MapEditorLineDecorationKind::SlopeTicks); ++kind) {
            for (int variant = 0; variant < 3; ++variant) {
                const QByteArray name = QByteArray::number(kind) + "-" + QByteArray::number(variant);
                QTest::newRow(name.constData()) << kind << (variant == 1)
                    << qreal(variant == 1 ? 2 : 1) << (variant != 0)
                    << variant << (variant == 2);
            }
        }
    }

    void nominalModesViewportRendering()
    {
        QFETCH(int, kind);
        QFETCH(bool, curved);
        QFETCH(qreal, pixelRatio);
        QFETCH(bool, reversed);
        QFETCH(int, interaction);
        QFETCH(bool, multipleSubpaths);
        const QString source = QStringLiteral("scrap symbols -projection plan\nline wall%1\n0 0\n%2endline\nendscrap\n")
            .arg(reversed ? QStringLiteral(" -reverse on") : QString(),
                 curved ? QStringLiteral("33333 6000 66666 -6000 100000 0\n")
                        : QStringLiteral("100000 0\n"));
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(source));
        auto catalog = loadMapEditorObjectStyleCatalog(QString());
        MapEditorLineDecorationStyle glyph;
        glyph.kind = static_cast<MapEditorLineDecorationKind>(kind);
        glyph.side = MapEditorLineDecorationSide::Left;
        glyph.size = 6;
        glyph.length = 10;
        glyph.spacing = 12;
        glyph.offset = 3;
        glyph.offsets = {-5, 5};
        glyph.fromOffset = -5;
        glyph.toOffset = 5;
        glyph.sizeJitter = 1;
        glyph.offsetJitter = 0.8;
        glyph.distanceJitter = 2;
        glyph.angleJitter = 25;
        glyph.strokeWidth = 1;
        if ((glyph.kind == MapEditorLineDecorationKind::Parallel
             || glyph.kind == MapEditorLineDecorationKind::OffsetStroke) && (curved || interaction == 0)) {
            glyph.strokeStyle = Qt::CustomDashLine;
            glyph.dashPattern = {3, 2};
        }
        glyph.strokeColor = Qt::black;
        glyph.fillColor = Qt::black;
        glyph.seed = 123;
        MapEditorLineStyleRule rule;
        rule.selector.rawType = QStringLiteral("wall");
        rule.decorations = QVector<MapEditorLineDecorationStyle>{glyph};
        catalog.lineStyles.append(rule);
        QGraphicsScene scene;
        renderMapGeometryItemGroupForFeature(
            &scene, features.first(), QRectF(0, 0, 1000, 1000), nullptr, nullptr,
            {}, {}, {}, {}, catalog);
        MapEditorLineDecorationItem *decoration = nullptr;
        for (auto *item : scene.items()) {
            if (auto *candidate = dynamic_cast<MapEditorLineDecorationItem *>(item)) {
                decoration = candidate;
            } else {
                item->setVisible(false);
            }
        }
        QVERIFY(decoration);
        if (multipleSubpaths) {
            QPainterPath path(QPointF(0, 0));
            path.lineTo(100000, 0);
            path.moveTo(0, 60);
            path.lineTo(100000, 60);
            decoration->setDecorationPath(path);
        }
        decoration->setData(kMapSceneInteractionSelectionRole, interaction == 1);
        decoration->setData(kMapSceneInteractionHoverRole, interaction == 2);
        const QPainterPath originalPath = decoration->path();
        const int count = scene.items().size();
        QGraphicsView view(&scene);
        view.setFrameStyle(QFrame::NoFrame);
        view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.resize(640, 480);
        view.setBackgroundBrush(Qt::white);
        view.show();
        QApplication::processEvents();
        for (qreal scale : {0.5, 1.0, 8.0, 55.0, 1.0}) {
            QTransform transform;
            transform.rotate(17);
            transform.scale(scale, scale);
            view.setTransform(transform);
            updateMapEditorScreenSpaceMetrics(&scene, view.viewportTransform());
            for (qreal location : {0.5, 0.9}) {
                view.centerOn(originalPath.pointAtPercent(location));
                QImage image(view.viewport()->size() * pixelRatio, QImage::Format_RGB32);
                image.setDevicePixelRatio(pixelRatio);
                image.fill(Qt::white);
                QPainter painter(&image);
                QElapsedTimer timer;
                timer.start();
                view.render(&painter, QRectF(QPointF(), view.viewport()->size()), view.viewport()->rect());
                painter.end();
                QVERIFY2(timer.elapsed() < 1000, "paint must not enumerate offscreen zoom-sized marker lists");
                QImage repeated(image.size(), image.format());
                repeated.setDevicePixelRatio(pixelRatio);
                repeated.fill(Qt::white);
                QPainter repeatPainter(&repeated);
                view.render(&repeatPainter, QRectF(QPointF(), view.viewport()->size()), view.viewport()->rect());
                repeatPainter.end();
                QCOMPARE(image, repeated);
                if (!curved && !multipleSubpaths && location == 0.5) {
                    auto *scroll = view.horizontalScrollBar();
                    const int before = scroll->value();
                    scroll->setValue(before + 32);
                    QCOMPARE(scroll->value() - before, 32);
                    QImage panned(image.size(), image.format());
                    panned.setDevicePixelRatio(pixelRatio);
                    panned.fill(Qt::white);
                    QPainter panPainter(&panned);
                    view.render(&panPainter, QRectF(QPointF(), view.viewport()->size()), view.viewport()->rect());
                    panPainter.end();
                    const int shift = qRound(32 * pixelRatio);
                    const int margin = qRound(64 * pixelRatio);
                    int changes = 0;
                    int maximumDelta = 0;
                    int maskChanges = 0;
                    for (int y = margin; y < image.height() - margin; ++y) {
                        for (int x = margin; x < image.width() - margin - shift; ++x) {
                            const int oldGray = qGray(image.pixel(x + shift, y));
                            const int newGray = qGray(panned.pixel(x, y));
                            changes += oldGray != newGray;
                            maximumDelta = qMax(maximumDelta, std::abs(oldGray - newGray));
                            maskChanges += (oldGray < 220) != (newGray < 220);
                        }
                    }
                    qInfo() << "pan mode=" << kind << "scale=" << scale << "changed-pixels=" << changes
                            << "max-gray-delta=" << maximumDelta << "threshold-edge-pixels=" << maskChanges;
                    if (glyph.kind == MapEditorLineDecorationKind::Parallel
                        || glyph.kind == MapEditorLineDecorationKind::OffsetStroke) {
                        QVERIFY2(maximumDelta <= 2, "Qt native stroke clipping may round antialias coverage, but must not shift dash geometry");
                    } else {
                        QCOMPARE(image.copy(margin + shift, margin,
                                            image.width() - 2 * margin - shift, image.height() - 2 * margin),
                                 panned.copy(margin, margin,
                                             image.width() - 2 * margin - shift, image.height() - 2 * margin));
                    }
                    scroll->setValue(before);
                }
                const auto polygons = view.viewportTransform().map(originalPath).toSubpathPolygons();
                qreal extent = 0;
                int pixels = 0;
                for (int y = 0; y < image.height(); ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        const QColor color = image.pixelColor(x, y);
                        if (qMin(color.red(), qMin(color.green(), color.blue())) >= 220) {
                            continue;
                        }
                        ++pixels;
                        const QPointF point((x + 0.5) / pixelRatio, (y + 0.5) / pixelRatio);
                        qreal nearest = 1e20;
                        for (const auto &polygon : polygons) {
                            for (int i = 1; i < polygon.size(); ++i) {
                                const QPointF delta = polygon[i] - polygon[i - 1];
                                const QPointF relative = point - polygon[i - 1];
                                const qreal squared = QPointF::dotProduct(delta, delta);
                                const qreal fraction = qBound<qreal>(0, QPointF::dotProduct(relative, delta)
                                    / qMax<qreal>(1e-9, squared), 1);
                                const QPointF difference = relative - delta * fraction;
                                nearest = qMin(nearest, std::hypot(difference.x(), difference.y()));
                            }
                        }
                        extent = qMax(extent, nearest);
                    }
                }
                qInfo() << "mode=" << kind << "scale=" << scale << "extent-px=" << extent
                        << "pixels=" << pixels << "scene-items=" << count;
                QVERIFY(pixels > 100);
                QVERIFY(extent >= 2);
                QVERIFY2(extent <= 16, "override metrics and jitter must stay screen-sized, including interaction strokes");
                QCOMPARE(scene.items().size(), count);
                QCOMPARE(decoration->path(), originalPath);
                if ((glyph.kind == MapEditorLineDecorationKind::Parallel
                     || glyph.kind == MapEditorLineDecorationKind::OffsetStroke) && location == 0.9) {
                    const QPointF screenPoint = view.viewportTransform().map(originalPath.pointAtPercent(location));
                    const QPointF tangent = view.viewportTransform().map(originalPath.pointAtPercent(location + 0.00001))
                        - view.viewportTransform().map(originalPath.pointAtPercent(location - 0.00001));
                    const QPointF normal = QPointF(tangent.y(), -tangent.x())
                        * (reversed ? -1 : 1) / std::hypot(tangent.x(), tangent.y());
                    const qreal offset = glyph.kind == MapEditorLineDecorationKind::Parallel ? 5 : glyph.offset;
                    const auto inverse = view.viewportTransform().inverted();
                    QVERIFY(decoration->hitDistancePixels(inverse.map(screenPoint + normal * offset),
                                                         view.viewportTransform()) < 1);
                    QVERIFY(!std::isfinite(decoration->hitDistancePixels(
                        inverse.map(screenPoint + normal * 30), view.viewportTransform()))
                        || decoration->hitDistancePixels(inverse.map(screenPoint + normal * 30),
                                                         view.viewportTransform()) > 5);
                }
            }
        }
    }

    void explicitSlopeLengthsStayGeometry_data()
    {
        QTest::addColumn<bool>("segmented");
        QTest::newRow("whole-line") << false;
        QTest::newRow("subtype-segments") << true;
    }

    void explicitSlopeLengthsStayGeometry()
    {
        QFETCH(bool, segmented);
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(
            QStringLiteral("scrap slope -projection plan\nline slope\n0 0\nl-size 2\norientation 180\n%1"
                           "\n1000 0\nl-size 2\norientation 180\nendline\nendscrap\n")
                .arg(segmented ? QStringLiteral("500 0\nl-size 2\norientation 180\nsubtype custom\n") : QString())));
        auto catalog = loadMapEditorObjectStyleCatalog(QString());
        MapEditorLineStyleRule rule;
        rule.selector.rawType = QStringLiteral("slope");
        rule.selector.subtype = QStringLiteral("custom");
        rule.decorations = resolveMapEditorLineStyle(catalog, QStringLiteral("slope")).decorations;
        catalog.lineStyles.append(rule);
        QGraphicsScene scene;
        renderMapGeometryItemGroupForFeature(
            &scene, features.first(), QRectF(0, 0, 1000, 1000), nullptr, nullptr,
            {}, {}, {}, {}, catalog);
        MapEditorLineDecorationItem *decoration = nullptr;
        int decorationCount = 0;
        for (auto *item : scene.items()) {
            if (auto *candidate = dynamic_cast<MapEditorLineDecorationItem *>(item)) {
                decoration = candidate;
                ++decorationCount;
            } else {
                item->setVisible(false);
            }
        }
        QVERIFY(decoration);
        QCOMPARE(decorationCount, segmented ? 2 : 1);
        QGraphicsView view(&scene);
        view.resize(640, 480);
        view.setBackgroundBrush(Qt::white);
        view.show();
        QApplication::processEvents();
        const qreal expectedSceneLength = 2 * decoration->path().length() / (segmented ? 500 : 1000);
        const int metricsBeforeRepeatedPaints = mapEditorPerformanceCounters().lineDecorationSourceMetricComputations.load();
        for (qreal scale : {0.5, 1.0, 8.0, 55.0}) {
            view.setTransform(QTransform::fromScale(scale, scale));
            updateMapEditorScreenSpaceMetrics(&scene, view.viewportTransform());
            view.centerOn(decoration->path().pointAtPercent(0.5));
            QImage image(view.viewport()->size(), QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            view.render(&painter, QRectF(image.rect()), view.viewport()->rect());
            painter.end();
            int top = image.height();
            int bottom = -1;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const QColor color = image.pixelColor(x, y);
                    if (qMin(color.red(), qMin(color.green(), color.blue())) < 220) {
                        top = qMin(top, y);
                        bottom = qMax(bottom, y);
                    }
                }
            }
            const qreal height = bottom - top + 1;
            qInfo() << "source l-size scale=" << scale << "height-px=" << height;
            QVERIFY(height > 0);
            QVERIFY(std::abs(height - expectedSceneLength * scale) <= 3);
        }
        QCOMPARE(mapEditorPerformanceCounters().lineDecorationSourceMetricComputations.load()
                     - metricsBeforeRepeatedPaints,
                 0);
        const int metricsBeforeVertexUpdate = mapEditorPerformanceCounters().lineDecorationSourceMetricComputations.load();
        decoration->setDecorationPath(decoration->path());
        QImage updatedImage(view.viewport()->size(), QImage::Format_RGB32);
        updatedImage.fill(Qt::white);
        QPainter updatedPainter(&updatedImage);
        view.render(&updatedPainter, QRectF(updatedImage.rect()), view.viewport()->rect());
        updatedPainter.end();
        QCOMPARE(mapEditorPerformanceCounters().lineDecorationSourceMetricComputations.load()
                     - metricsBeforeVertexUpdate,
                 1);
        QCOMPARE(features.first().lineVertices.first().leftSize.value(), qreal(2));
        QCOMPARE(features.first().lineVertices.last().leftSize.value(), qreal(2));
    }

    void overhangScreenSize()
    {
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(
            QStringLiteral("scrap teeth -projection plan\n"
                           "line overhang\n"
                           "0 0\n"
                           "6 0\n"
                           "endline\n"
                           "endscrap\n")));
        QCOMPARE(features.size(), 1);
        QGraphicsScene scene;
        const auto catalog = loadMapEditorObjectStyleCatalog(QString());
        renderMapGeometryItemGroupForFeature(
            &scene, features.first(), QRectF(0, 0, 1000, 1000), nullptr, nullptr,
            {}, {}, {}, {}, catalog);
        MapEditorLineDecorationItem *decoration = nullptr;
        for (auto *item : scene.items()) {
            if (auto *candidate = dynamic_cast<MapEditorLineDecorationItem *>(item)) {
                decoration = candidate;
            } else {
                item->setVisible(false);
            }
        }
        QVERIFY(decoration);
        QGraphicsView view(&scene);
        view.setFrameStyle(QFrame::NoFrame);
        view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.resize(640, 480);
        view.setBackgroundBrush(Qt::white);
        view.show();
        QApplication::processEvents();
        const QPointF center = decoration->path().pointAtPercent(0.5);
        bool oversized = false;
        for (qreal scale : {0.5, 1.0, 8.0, 55.0, 100.0}) {
            view.setTransform(QTransform::fromScale(scale, scale));
            updateMapEditorScreenSpaceMetrics(&scene, view.viewportTransform());
            view.centerOn(center);
            QImage image(view.viewport()->size(), QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            view.render(&painter, QRectF(image.rect()), view.viewport()->rect());
            painter.end();
            int top = image.height();
            int bottom = -1;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    if (qGray(image.pixel(x, y)) < 100) {
                        top = qMin(top, y);
                        bottom = qMax(bottom, y);
                    }
                }
            }
            const int height = bottom - top + 1;
            qInfo() << "overhang view-scale=" << scale << "painted-height-px=" << height;
            QVERIFY(height >= 2);
            oversized |= height > 6;
        }
        QVERIFY2(!oversized, "overhang teeth must not grow with viewport magnification");
    }

    void overhangViewportRendering_data()
    {
        QTest::addColumn<bool>("curved");
        QTest::addColumn<bool>("overrideStyle");
        QTest::addColumn<bool>("selected");
        QTest::addColumn<bool>("longLine");
        QTest::addColumn<qreal>("pixelRatio");
        QTest::addColumn<bool>("reversed");
        QTest::newRow("straight") << false << false << false << false << qreal(1) << false;
        QTest::newRow("curved") << true << false << false << false << qreal(1) << false;
        QTest::newRow("override") << false << true << false << false << qreal(1) << false;
        QTest::newRow("curved-selected-override") << true << true << true << false << qreal(1) << false;
        QTest::newRow("long-line") << false << false << false << true << qreal(1) << false;
        QTest::newRow("hidpi") << false << false << false << false << qreal(2) << false;
        QTest::newRow("reversed") << false << false << false << false << qreal(1) << true;
    }

    void overhangViewportRendering()
    {
        QFETCH(bool, curved);
        QFETCH(bool, overrideStyle);
        QFETCH(bool, selected);
        QFETCH(bool, longLine);
        QFETCH(qreal, pixelRatio);
        QFETCH(bool, reversed);
        const QString source = QStringLiteral("scrap teeth -projection plan\nline overhang%1\n0 0\n")
                .arg(reversed ? QStringLiteral(" -reverse on") : QString())
            + (curved ? QStringLiteral("333 60 666 -60 1000 0\n")
                      : longLine ? QStringLiteral("100000 0\n") : QStringLiteral("1000 0\n"))
            + QStringLiteral("endline\nendscrap\n");
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(source));
        QCOMPARE(features.size(), 1);
        QCOMPARE(features.first().reversed, reversed);
        auto catalog = loadMapEditorObjectStyleCatalog(QString());
        if (overrideStyle) {
            for (auto &rule : catalog.lineStyles) {
                if (rule.selector.rawType == QStringLiteral("overhang") && rule.decorations) {
                    rule.decorations->first().size = 6.5;
                    rule.decorations->first().spacing = 9.0;
                }
            }
        }
        const auto style = resolveMapEditorLineStyle(catalog, QStringLiteral("overhang"));
        const qreal depth = overrideStyle ? 6.5 : 3.25;
        const qreal spacing = overrideStyle ? 9.0 : 4.5;
        QCOMPARE(style.decorations.first().size, depth);
        QGraphicsScene scene;
        renderMapGeometryItemGroupForFeature(
            &scene, features.first(), QRectF(0, 0, 1000, 1000), nullptr, nullptr,
            {}, {}, {}, {}, catalog);
        MapEditorLineDecorationItem *decoration = nullptr;
        for (auto *item : scene.items()) {
            if (auto *candidate = dynamic_cast<MapEditorLineDecorationItem *>(item)) {
                decoration = candidate;
            } else {
                item->setVisible(false);
            }
        }
        QVERIFY(decoration);
        const int itemCount = scene.items().size();
        const QPainterPath originalPath = decoration->path();
        decoration->setSelected(selected);
        QGraphicsView view(&scene);
        view.setFrameStyle(QFrame::NoFrame);
        view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view.resize(640, 480);
        view.setBackgroundBrush(Qt::white);
        view.show();
        QApplication::processEvents();
        for (qreal scale : {0.5, 1.0, 8.0, 55.0, 1.0}) {
            view.setTransform(QTransform::fromScale(scale, scale));
            updateMapEditorScreenSpaceMetrics(&scene, view.viewportTransform());
            for (qreal location : {0.5, 0.9}) {
                view.centerOn(decoration->path().pointAtPercent(location));
                QImage image(view.viewport()->size() * pixelRatio, QImage::Format_RGB32);
                image.setDevicePixelRatio(pixelRatio);
                image.fill(Qt::white);
                QPainter painter(&image);
                QElapsedTimer timer;
                timer.start();
                view.render(&painter, QRectF(QPointF(), view.viewport()->size()), view.viewport()->rect());
                painter.end();
                const qint64 elapsed = timer.nsecsElapsed();
                const auto screenPath = view.viewportTransform().map(decoration->path());
                const auto polygons = screenPath.toSubpathPolygons();
                qreal maximumDistance = 0.0;
                int paintedPixels = 0;
                for (int y = 0; y < image.height(); ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        const QColor pixel = image.pixelColor(x, y);
                        if (qMin(pixel.red(), qMin(pixel.green(), pixel.blue())) >= 180) {
                            continue;
                        }
                        ++paintedPixels;
                        qreal nearest = 1e20;
                        const QPointF point((x + 0.5) / pixelRatio, (y + 0.5) / pixelRatio);
                        for (const auto &polygon : polygons) {
                            for (int i = 1; i < polygon.size(); ++i) {
                                const QPointF a = polygon[i - 1];
                                const QPointF delta = polygon[i] - a;
                                const qreal squaredLength = delta.x() * delta.x() + delta.y() * delta.y();
                                const QPointF relative = point - a;
                                const qreal fraction = qBound<qreal>(0.0,
                                    (relative.x() * delta.x() + relative.y() * delta.y())
                                        / qMax<qreal>(1e-9, squaredLength), 1.0);
                                const QPointF difference = point - (a + delta * fraction);
                                nearest = qMin(nearest, std::hypot(difference.x(), difference.y()));
                            }
                        }
                        maximumDistance = qMax(maximumDistance, nearest);
                    }
                }
                qInfo() << "overhang scale=" << scale << "location=" << location
                        << "depth-px=" << maximumDistance << "render-ns=" << elapsed;
                QVERIFY(paintedPixels > 100);
                QVERIFY(maximumDistance >= depth - 1.5);
                QVERIFY(maximumDistance <= depth + (selected ? 2.5 : 1.5));
                if (!curved && !selected) {
                    const int row = qRound(pixelRatio * (
                        view.mapFromScene(decoration->path().pointAtPercent(location)).y()
                        + (reversed ? 1 : -1) * depth * 0.65));
                    QVector<int> starts;
                    bool previousDark = false;
                    for (int x = 32; x < image.width() - 32; ++x) {
                        const bool dark = qGray(image.pixel(x, row)) < 100;
                        if (dark && !previousDark) {
                            starts.append(x);
                        }
                        previousDark = dark;
                    }
                    QVERIFY(starts.size() > 20);
                    const qreal measuredSpacing = qreal(starts.last() - starts.first())
                        / (starts.size() - 1) / pixelRatio;
                    qInfo() << "overhang spacing-px=" << measuredSpacing;
                    QVERIFY(std::abs(measuredSpacing - spacing) < 0.3);
                }
                QCOMPARE(scene.items().size(), itemCount);
                QVERIFY(decoration->boundingRect().contains(originalPath.controlPointRect().topLeft()));
                QVERIFY(decoration->boundingRect().contains(originalPath.controlPointRect().bottomRight()));
                QCOMPARE(decoration->path(), originalPath);
            }
        }
    }

    void nominalDecorationsScreenSize_data()
    {
        QTest::addColumn<QString>("lineType");
        QTest::newRow("pit") << QStringLiteral("pit");
        QTest::newRow("custom-wall-teeth") << QStringLiteral("wall");
        for (const char *type : {"pitch", "floor-step", "ceiling-step", "steps",
                                 "handrail", "wall:blocks", "wall:debris", "wall:pebbles",
                                 "wall:ice", "wall:sand", "flowstone", "moonmilk",
                                 "wall:clay", "fixed-ladder", "rope-ladder", "slope"}) {
            QTest::newRow(type) << QString::fromLatin1(type);
        }
    }

    void nominalDecorationsScreenSize()
    {
        QFETCH(QString, lineType);
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(
            QStringLiteral("scrap sizing -projection plan\nline %1\n0 0\n1000 0\nendline\nendscrap\n")
                .arg(lineType)));
        auto catalog = loadMapEditorObjectStyleCatalog(QString());
        if (lineType == QStringLiteral("wall")) {
            MapEditorLineStyleRule rule;
            rule.selector.rawType = lineType;
            rule.decorations = resolveMapEditorLineStyle(catalog, QStringLiteral("overhang")).decorations;
            catalog.lineStyles.append(rule);
        }
        QGraphicsScene scene;
        renderMapGeometryItemGroupForFeature(
            &scene, features.first(), QRectF(0, 0, 1000, 1000), nullptr, nullptr,
            {}, {}, {}, {}, catalog);
        MapEditorLineDecorationItem *decoration = nullptr;
        for (auto *item : scene.items()) {
            if (auto *candidate = dynamic_cast<MapEditorLineDecorationItem *>(item)) {
                decoration = candidate;
            } else {
                item->setVisible(false);
            }
        }
        QVERIFY(decoration);
        const auto style = resolveMapEditorLineStyle(catalog, features.first().label, features.first().subtype);
        qreal nominalExtent = 0.0;
        for (const auto &part : style.decorations) {
            qreal offsets = std::abs(part.offset);
            for (qreal offset : part.offsets) {
                offsets = qMax(offsets, std::abs(offset));
            }
            offsets = qMax(offsets, std::abs(part.fromOffset.value_or(0.0)));
            offsets = qMax(offsets, std::abs(part.toOffset.value_or(0.0)));
            nominalExtent = qMax(nominalExtent, offsets + part.size + part.length
                                + part.sizeJitter + part.offsetJitter + part.strokeWidth);
        }
        QGraphicsView view(&scene);
        view.resize(640, 480);
        view.setBackgroundBrush(Qt::white);
        view.show();
        QApplication::processEvents();
        bool oversized = false;
        bool missing = false;
        int baselineHeight = 0;
        const auto originalPath = decoration->path();
        const int originalCount = scene.items().size();
        for (qreal scale : {0.5, 1.0, 8.0, 55.0, 100.0}) {
            view.setTransform(QTransform::fromScale(scale, scale));
            updateMapEditorScreenSpaceMetrics(&scene, view.viewportTransform());
            view.centerOn(decoration->path().pointAtPercent(0.9));
            QImage image(view.viewport()->size(), QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            view.render(&painter, QRectF(image.rect()), view.viewport()->rect());
            painter.end();
            int top = image.height();
            int bottom = -1;
            int darkTop = image.height();
            int darkBottom = -1;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    if (qMin(image.pixelColor(x, y).red(),
                             qMin(image.pixelColor(x, y).green(), image.pixelColor(x, y).blue())) < 220) {
                        top = qMin(top, y);
                        bottom = qMax(bottom, y);
                    }
                    if (qGray(image.pixel(x, y)) < 100) {
                        darkTop = qMin(darkTop, y);
                        darkBottom = qMax(darkBottom, y);
                    }
                }
            }
            const int height = bottom - top + 1;
            qInfo() << lineType << "scale=" << scale << "height-px=" << height
                    << "dark-height-px=" << qMax(0, darkBottom - darkTop + 1);
            missing |= height <= 0;
            oversized |= height > std::ceil(2 * nominalExtent + 4);
            if (baselineHeight == 0) {
                baselineHeight = height;
            } else {
                oversized |= std::abs(height - baselineHeight) > 3;
            }
            QCOMPARE(decoration->path(), originalPath);
            QCOMPARE(scene.items().size(), originalCount);
        }
        QVERIFY2(!missing, "visible far-end decorations must not be truncated");
        QVERIFY2(!oversized, "nominal symbol dimensions must remain in logical screen pixels");
    }

    void directionAidScreenSize_data()
    {
        QTest::addColumn<bool>("curved");
        QTest::addColumn<bool>("reversed");
        QTest::addColumn<qreal>("angle");
        QTest::addColumn<qreal>("pixelRatio");
        QTest::newRow("straight") << false << false << qreal(0) << qreal(1);
        QTest::newRow("reversed") << false << true << qreal(0) << qreal(1);
        QTest::newRow("curve-rotated-hidpi") << true << false << qreal(37) << qreal(2);
        QTest::newRow("curve-reversed-rotated") << true << true << qreal(-23) << qreal(1);
    }

    void directionAidScreenSize()
    {
        QFETCH(bool, curved);
        QFETCH(bool, reversed);
        QFETCH(qreal, angle);
        QFETCH(qreal, pixelRatio);
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(
            QStringLiteral("scrap direction -projection plan\nline wall%1\n0 0\n%2endline\nendscrap\n")
                .arg(reversed ? QStringLiteral(" -reverse on") : QString(),
                     curved ? QStringLiteral("333 200 666 0 1000 0\n") : QStringLiteral("1000 0\n"))));
        QGraphicsScene scene;
        renderMapGeometryItemGroupForFeature(
            &scene, features.first(), QRectF(0, 0, 1000, 1000), nullptr, nullptr,
            {}, {}, {}, {}, loadMapEditorObjectStyleCatalog(QString()));
        MapEditorDirectionAidItem *tick = nullptr;
        for (auto *item : scene.items()) {
            if (auto *line = dynamic_cast<MapEditorDirectionAidItem *>(item)) {
                tick = line;
                QVERIFY(!tick->isVisible());
                QVERIFY(tick->data(kMapSceneSelectionGatedRole).toBool());
                QCOMPARE(tick->data(kMapSceneLineNumberRole).toInt(), features.first().lineNumber);
                tick->setVisible(true);
            } else {
                item->setVisible(false);
            }
        }
        QVERIFY(tick);
        QGraphicsView view(&scene);
        view.resize(640, 480);
        view.setBackgroundBrush(Qt::white);
        view.show();
        QApplication::processEvents();
        const QPointF anchor = tick->pos();
        const QPointF unrotatedDirection = tick->line().p2() / 18;
        const int itemCount = scene.items().size();
        for (qreal scale : {0.5, 1.0, 8.0, 55.0, 100.0}) {
            QTransform transform;
            transform.rotate(angle);
            transform.scale(scale, scale);
            view.setTransform(transform);
            updateMapEditorScreenSpaceMetrics(&scene, view.viewportTransform());
            view.centerOn(anchor);
            const QPointF unit = tick->line().p2() / 18;
            const QPointF expected = QTransform().rotate(angle).map(unrotatedDirection);
            QVERIFY(std::hypot(unit.x() - expected.x(), unit.y() - expected.y()) < 1e-6);
            QCOMPARE(tick->pos(), anchor);
            QVERIFY(std::abs(tick->line().length() - 18) < 1e-6);
            QImage image(view.viewport()->size() * pixelRatio, QImage::Format_RGB32);
            image.setDevicePixelRatio(pixelRatio);
            image.fill(Qt::white);
            QPainter painter(&image);
            view.render(&painter, QRectF(QPointF(), view.viewport()->size()), view.viewport()->rect());
            painter.end();
            int top = image.height();
            int bottom = -1;
            qreal maximumLength = 0;
            qreal maximumWidth = 0;
            const QPointF screenAnchor = view.viewportTransform().map(anchor);
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const QColor pixel = image.pixelColor(x, y);
                    if (pixel.red() > 150 && pixel.green() > 120 && pixel.blue() < 120) {
                        top = qMin(top, y);
                        bottom = qMax(bottom, y);
                        const QPointF relative = QPointF((x + 0.5) / pixelRatio, (y + 0.5) / pixelRatio) - screenAnchor;
                        maximumLength = qMax(maximumLength, QPointF::dotProduct(relative, unit));
                        maximumWidth = qMax(maximumWidth, std::abs(QPointF::dotProduct(relative, QPointF(-unit.y(), unit.x()))));
                    }
                }
            }
            const int height = bottom - top + 1;
            qInfo() << "direction scale=" << scale << "height-px=" << height;
            QVERIFY(height > 0);
            QVERIFY(maximumLength >= 17);
            QVERIFY(maximumLength <= 21);
            QVERIFY(maximumWidth <= 3);
            QCOMPARE(scene.items().size(), itemCount);
        }
        tick->setVisible(false);
        QVERIFY(!tick->isVisible());
    }

    void directionAidDragUpdates()
    {
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(
            QStringLiteral("scrap direction -projection plan\nline wall -reverse on\n20 20\n"
                           "30 40 60 60 80 80\nendline\nendscrap\n")));
        QGraphicsScene scene;
        QHash<QString, QGraphicsItem *> vertices;
        renderMapGeometryItemGroupForFeature(
            &scene, features.first(), QRectF(0, 0, 100, 100), nullptr, &vertices,
            {}, {}, {}, {}, loadMapEditorObjectStyleCatalog(QString()));
        MapEditableGeometryVertexItem *anchor = nullptr;
        MapEditableGeometryVertexItem *control = nullptr;
        MapEditorDirectionAidItem *tick = nullptr;
        for (auto *item : scene.items()) {
            if (auto *aid = dynamic_cast<MapEditorDirectionAidItem *>(item)) {
                tick = aid;
            }

        }
        for (auto *item : vertices) {
            auto *vertex = dynamic_cast<MapEditableGeometryVertexItem *>(item);
            if (!vertex) {
                continue;
            }
            if (vertex->vertexIndex() == features.first().lineVertices.first().anchorSourceVertexIndex
                && vertex->geometryKind() == QStringLiteral("line")) {
                anchor = vertex;
            }
            if (vertex->vertexIndex() == features.first().lineVertices.first().outgoingSourceVertexIndex
                && vertex->geometryKind() == QStringLiteral("line control")) {
                control = vertex;
            }
        }
        QVERIFY(anchor && control && tick);
        for (auto *moved : {anchor, control}) {
            moved->setVisible(true);
            QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
            press.setButton(Qt::LeftButton);
            press.setButtons(Qt::LeftButton);
            press.setScenePos(moved->scenePos());
            QVERIFY(scene.sendEvent(moved, &press));
            moved->setPos(moved->pos() + QPointF(7, -3));
            QCOMPARE(tick->pos(), anchor->pos());
            const QPointF tangent = control->pos() - anchor->pos();
            const QPointF expected = QPointF(-tangent.y(), tangent.x())
                * (18 / std::hypot(tangent.x(), tangent.y()));
            QVERIFY(QLineF(tick->line().p2(), expected).length() < 1e-6);
            QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
            release.setButton(Qt::LeftButton);
            release.setScenePos(moved->scenePos());
            QVERIFY(scene.sendEvent(moved, &release));
        }
    }

    void screenBoundsKeepOffPathGlyphVisible()
    {
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(
            QStringLiteral("scrap bounds -projection plan\nline wall\n0 0\n1000 0\nendline\nendscrap\n")));
        auto catalog = loadMapEditorObjectStyleCatalog(QString());
        MapEditorLineDecorationStyle tick;
        tick.kind = MapEditorLineDecorationKind::Ticks;
        tick.side = MapEditorLineDecorationSide::Right;
        tick.length = 80;
        tick.offset = 20;
        tick.spacing = 12;
        tick.adjustSpacing = true;
        tick.strokeColor = Qt::black;
        tick.strokeWidth = 1;
        MapEditorLineStyleRule rule;
        rule.selector.rawType = QStringLiteral("wall");
        rule.decorations = QVector<MapEditorLineDecorationStyle>{tick};
        catalog.lineStyles.append(rule);
        QGraphicsScene scene;
        renderMapGeometryItemGroupForFeature(
            &scene, features.first(), QRectF(0, 0, 1000, 1000), nullptr, nullptr,
            {}, {}, {}, {}, catalog);
        scene.setSceneRect(-1e8, -1e8, 2e8, 2e8);
        MapEditorLineDecorationItem *decoration = nullptr;
        for (auto *item : scene.items()) {
            if (auto *candidate = dynamic_cast<MapEditorLineDecorationItem *>(item)) {
                decoration = candidate;
            } else {
                item->setVisible(false);
            }
        }
        QVERIFY(decoration);
        QGraphicsView view(&scene);
        view.setFrameStyle(QFrame::NoFrame);
        view.resize(640, 480);
        view.setBackgroundBrush(Qt::white);
        view.show();
        QApplication::processEvents();
        for (qreal scale : {0.005, 0.1, 0.5, 55.0}) {
            view.setTransform(QTransform::fromScale(scale, scale));
            updateMapEditorScreenSpaceMetrics(&scene, view.viewportTransform());
            view.centerOn(decoration->path().pointAtPercent(0.5) + QPointF(0, 280 / scale));
            const auto visible = view.mapToScene(view.viewport()->rect()).boundingRect();
            QVERIFY(!visible.intersects(decoration->path().controlPointRect().adjusted(
                -1 / scale, -1 / scale, 1 / scale, 1 / scale)));
            QImage image(view.viewport()->size(), QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            view.render(&painter, QRectF(image.rect()), view.viewport()->rect());
            painter.end();
            int pixels = 0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    pixels += qGray(image.pixel(x, y)) < 220;
                }
            }
            QVERIFY2(pixels > 20, "scene index must retain visible nominal glyphs even when their source path is outside the viewport");
        }
    }

    void zoomAndFitKeepSceneAndSourceGeometry()
    {
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(
            QStringLiteral("scrap fit -projection plan\nline fixed-ladder\n0 0\n1000 100\nendline\nendscrap\n")));
        QGraphicsScene ownedScene;
        QGraphicsScene *scene = &ownedScene;
        renderMapGeometryItemGroupForFeature(
            scene, features.first(), QRectF(0, 0, 1000, 1000), nullptr, nullptr,
            {}, {}, {}, {}, loadMapEditorObjectStyleCatalog(QString()));
        QGraphicsView view(scene);
        view.resize(640, 480);
        view.show();
        QApplication::processEvents();
        bool autoFit = false;
        qreal zoom = 1;
        MapEditorSceneLifecycleController controller(MapEditorSceneLifecycleContext{
            .scene = &scene, .view = &view, .autoFitEnabled = &autoFit, .zoomFactor = &zoom,
            .mapBackgroundFitBounds = [] { return QRectF(); },
            .updateCommandSurfaceState = [] {},
            .zoomPercent = [] { return 100; },
            .emitZoomStatusChanged = [](int) {}});
        const QList<QGraphicsItem *> items = scene->items();
        const QRectF sourceBounds = controller.mapGeometryFitBounds();
        controller.fitMapToView(false);
        const QTransform fitted = view.transform();
        for (int cycle = 0; cycle < 5; ++cycle) {
            controller.applyZoomAtViewportPosition(2, QPointF(320, 240));
            controller.applyZoomAtViewportPosition(0.5, QPointF(320, 240));
            QCOMPARE(scene->items(), items);
            QCOMPARE(controller.mapGeometryFitBounds(), sourceBounds);
            controller.fitMapToView(false);
            QCOMPARE(view.transform(), fitted);
        }
    }

    void coupledMove_data()
    {
        QTest::addColumn<bool>("moveControl");
        QTest::addColumn<QString>("lineType");
        QTest::newRow("anchor") << false << QStringLiteral("wall");
        QTest::newRow("smooth-control") << true << QStringLiteral("wall");
        QTest::newRow("overhang-anchor") << false << QStringLiteral("overhang");
        QTest::newRow("overhang-control") << true << QStringLiteral("overhang");
    }

    void coupledMove()
    {
        QFETCH(bool, moveControl);
        QFETCH(QString, lineType);
        const auto features = collectGeometryFeatures(TherionDocumentParser::parseTokenLines(
            QStringLiteral("scrap preview -projection plan\n"
                           "line %1\n"
                           "20 20\n"
                           "30 20 40 50 50 50\n"
                           "60 50 70 80 80 80\n"
                           "endline\n"
                           "endscrap\n").arg(lineType)));
        QCOMPARE(features.size(), 1);
        const auto &feature = features.first();
        QCOMPARE(feature.lineVertices.size(), 3);
        QGraphicsScene scene;
        QHash<int, QGraphicsItem *> itemsByLine;
        QHash<QString, QGraphicsItem *> vertices;
        const auto catalog = loadMapEditorObjectStyleCatalog(QString());
        const auto rendered = renderMapGeometryItemGroupForFeature(
            &scene, feature, QRectF(0, 0, 100, 100), &itemsByLine, &vertices,
            {}, {}, {}, {}, catalog);
        QVERIFY(rendered.addedPrimaryItem);
        auto *pathItem = dynamic_cast<QGraphicsPathItem *>(itemsByLine.value(feature.lineNumber));
        QVERIFY(pathItem != nullptr);
        MapEditorLineDecorationItem *decoration = nullptr;
        for (auto *item : scene.items()) {
            if (auto *candidate = dynamic_cast<MapEditorLineDecorationItem *>(item)) {
                decoration = candidate;
            }
        }
        if (lineType == QStringLiteral("overhang")) {
            QVERIFY(decoration);
        }
        const auto vertexItem = [&](int index, const QString &kind) {
            for (QGraphicsItem *item : vertices) {
                auto *vertex = dynamic_cast<MapEditableGeometryVertexItem *>(item);
                if (vertex != nullptr && vertex->vertexIndex() == index
                    && vertex->geometryKind() == kind) {
                    return vertex;
                }
            }
            return static_cast<MapEditableGeometryVertexItem *>(nullptr);
        };
        const QString anchorKind = QStringLiteral("line");
        const QString controlKind = QStringLiteral("line control");
        auto *first = vertexItem(feature.lineVertices[0].anchorSourceVertexIndex, anchorKind);
        auto *middle = vertexItem(feature.lineVertices[1].anchorSourceVertexIndex, anchorKind);
        auto *last = vertexItem(feature.lineVertices[2].anchorSourceVertexIndex, anchorKind);
        auto *firstOutgoing = vertexItem(feature.lineVertices[0].outgoingSourceVertexIndex, controlKind);
        auto *middleIncoming = vertexItem(feature.lineVertices[1].incomingSourceVertexIndex, controlKind);
        auto *middleOutgoing = vertexItem(feature.lineVertices[1].outgoingSourceVertexIndex, controlKind);
        auto *lastIncoming = vertexItem(feature.lineVertices[2].incomingSourceVertexIndex, controlKind);
        QVERIFY(first && middle && last && firstOutgoing && middleIncoming && middleOutgoing && lastIncoming);
        const auto expectedPath = [&]() {
            QPainterPath path(first->pos());
            path.cubicTo(firstOutgoing->pos(), middleIncoming->pos(), middle->pos());
            path.cubicTo(middleOutgoing->pos(), lastIncoming->pos(), last->pos());
            return path;
        };
        auto *moved = moveControl ? middleIncoming : middle;
        moved->setVisible(true);
        QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
        press.setButton(Qt::LeftButton);
        press.setButtons(Qt::LeftButton);
        press.setScenePos(moved->scenePos());
        press.setPos(QPointF());
        QVERIFY(scene.sendEvent(moved, &press));
        const QPointF start = moved->pos();
        const QPointF incomingStart = middleIncoming->pos();
        const QPointF outgoingStart = middleOutgoing->pos();
        constexpr int movesPerSample = 200;
        for (int sample = 0; sample < 5; ++sample) {
            QElapsedTimer timer;
            timer.start();
            for (int step = 0; step < movesPerSample; ++step) {
                moved->setPos(start + QPointF((step % 2) ? 3.0 : 1.0, 2.0));
            }
            const qint64 elapsedNs = timer.nsecsElapsed();
            qInfo().nospace() << "map-line-coupled-preview control=" << moveControl
                              << " moves=" << movesPerSample << " sample=" << sample
                              << " elapsed_ns=" << elapsedNs;
            QCOMPARE(pathItem->path(), expectedPath());
            if (decoration) {
                QCOMPARE(decoration->path(), expectedPath());
            }
        }
        const QPointF delta = moved->pos() - start;
        QVERIFY(middleOutgoing->pos() != outgoingStart);
        if (!moveControl) {
            QCOMPARE(middleIncoming->pos(), incomingStart + delta);
            QCOMPARE(middleOutgoing->pos(), outgoingStart + delta);
        }
        QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
        release.setButton(Qt::LeftButton);
        release.setScenePos(moved->scenePos());
        QVERIFY(scene.sendEvent(moved, &release));
        moved->setPos(start);
        QCOMPARE(pathItem->path(), expectedPath());
        if (decoration) {
            QCOMPARE(decoration->path(), expectedPath());
        }
    }
};

int runMapEditorLinePreviewTest(int argc, char **argv)
{
    MapEditorLinePreviewTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorLinePreviewTest.moc"
