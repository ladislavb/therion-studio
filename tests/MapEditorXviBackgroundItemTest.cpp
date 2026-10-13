#include "../src/app/text_editor/map_editor/MapEditorXviBackgroundItem.h"

#include <QImage>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtTest/QtTest>

using namespace TherionStudio;

class MapEditorXviBackgroundItemTest final : public QObject
{
    Q_OBJECT

private slots:
    void paintsLrudPassageEnvelope();
    void paintsStationMarkers();
    void stylesOnlyCenterline_data();
    void stylesOnlyCenterline();
    void wideCenterlineIsNotCulledAtZoomedOutEdge();
};

void MapEditorXviBackgroundItemTest::paintsLrudPassageEnvelope()
{
    MapEditorXviLayerGeometryData geometry;
    geometry.contentBounds = QRectF(0.0, 0.0, 100.0, 100.0);
    MapEditorXviPassagePolygonData passage;
    passage.polygon = QPolygonF({QPointF(10.0, 20.0),
                                 QPointF(90.0, 20.0),
                                 QPointF(90.0, 80.0),
                                 QPointF(10.0, 80.0)});
    passage.bounds = passage.polygon.boundingRect();
    geometry.passagePolygons.append(passage);

    MapEditorXviBackgroundItem item;
    item.setGeometryData(geometry);

    QImage image(QSize(120, 120), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    QStyleOptionGraphicsItem option;
    option.exposedRect = QRectF(0.0, 0.0, 120.0, 120.0);
    item.paint(&painter, &option);
    painter.end();

    const QColor insidePassage = image.pixelColor(50, 50);
    QVERIFY(insidePassage != QColor(Qt::white));
    QVERIFY(insidePassage.blue() > insidePassage.red());
}

void MapEditorXviBackgroundItemTest::paintsStationMarkers()
{
    MapEditorXviLayerGeometryData geometry;
    geometry.contentBounds = QRectF(0.0, 0.0, 100.0, 100.0);
    geometry.stations.append({QStringLiteral("12"), QPointF(50.0, 50.0)});

    MapEditorXviBackgroundItem item;
    item.setGeometryData(geometry);

    QImage image(QSize(120, 120), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    QStyleOptionGraphicsItem option;
    option.exposedRect = QRectF(0.0, 0.0, 120.0, 120.0);
    item.paint(&painter, &option);
    painter.end();

    const QColor stationCenter = image.pixelColor(50, 50);
    QVERIFY(stationCenter != QColor(Qt::white));
    QVERIFY(stationCenter.blue() > stationCenter.red());
}

void MapEditorXviBackgroundItemTest::stylesOnlyCenterline_data()
{
    QTest::addColumn<qreal>("zoom");
    QTest::addColumn<qreal>("devicePixelRatio");
    QTest::newRow("native") << qreal(1.0) << qreal(1.0);
    QTest::newRow("zoom-in") << qreal(2.0) << qreal(1.0);
    QTest::newRow("zoom-out") << qreal(0.5) << qreal(1.0);
    QTest::newRow("hidpi") << qreal(1.0) << qreal(2.0);
}

void MapEditorXviBackgroundItemTest::stylesOnlyCenterline()
{
    QFETCH(qreal, zoom);
    QFETCH(qreal, devicePixelRatio);
    MapEditorXviLayerGeometryData geometry;
    geometry.contentBounds = QRectF(0, 0, 120, 120);
    geometry.traverseShotLines.append(QLineF(20, 30, 100, 30));
    MapEditorXviPassagePolygonData passage;
    passage.polygon = QPolygonF({QPointF(20, 60), QPointF(100, 60),
                                 QPointF(100, 100), QPointF(20, 100)});
    passage.bounds = passage.polygon.boundingRect();
    geometry.passagePolygons.append(passage);
    geometry.splayShotLines.append(QLineF(20, 110, 100, 110));
    geometry.gridLines.append(QLineF(10, 10, 100, 10));
    geometry.stations.append({QStringLiteral("A"), QPointF(110, 110)});
    MapEditorXviBackgroundItem item;
    item.setGeometryData(geometry);
    const auto *lines = item.geometryData().traverseShotLines.constData();
    const auto *tiles = item.geometryData().traverseShotTiles.constData();
    auto raster = [&] {
        QImage image(QSize(300, 300), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(devicePixelRatio);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.scale(zoom, zoom);
        QStyleOptionGraphicsItem option;
        option.exposedRect = geometry.contentBounds;
        item.paint(&painter, &option);
        painter.end();
        return image;
    };
    const QImage before = raster();
    item.setCenterlineAppearance(QColor(255, 0, 0, 255), 6.0);
    const QImage after = raster();
    const qreal scale = zoom * devicePixelRatio;
    QVERIFY(after.pixelColor(qRound(60 * scale), qRound(30 * scale)).red() > 240);
    int changed = 0;
    for (int y = 0; y < before.height(); ++y) {
        for (int x = 0; x < before.width(); ++x) {
            if (before.pixel(x, y) != after.pixel(x, y)) {
                ++changed;
                QVERIFY(y >= 30 * scale - 8 && y <= 30 * scale + 8);
            }

        }
    }
    QVERIFY(changed > 0);
    QCOMPARE(item.geometryData().traverseShotLines.constData(), lines);
    QCOMPARE(item.geometryData().traverseShotTiles.constData(), tiles);
    QVERIFY(item.boundingRect().contains(QRectF(-4, -4, 128, 128)));
    if (zoom >= 1.0) {
        int paintedRows = 0;
        for (int y = 0; y < after.height(); ++y) {
            if (after.pixelColor(qRound(60 * scale), y).red() > 240) {
                ++paintedRows;
            }
        }
        QVERIFY(paintedRows >= 6 && paintedRows <= 7);
    }
}

void MapEditorXviBackgroundItemTest::wideCenterlineIsNotCulledAtZoomedOutEdge()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.scale(0.1, 0.1);
    auto *item = new MapEditorXviBackgroundItem;
    MapEditorXviLayerGeometryData geometry;
    geometry.contentBounds = QRectF(20, 30, 80, 1);
    geometry.traverseShotLines.append(QLineF(20, 30, 100, 30));
    item->setGeometryData(geometry);
    scene.addItem(item);
    item->setCenterlineAppearance(QColor(255, 0, 0, 255), 12);
    QVERIFY(item->boundingRect().bottom() > 40);
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    scene.render(&painter, QRectF(0, 0, 100, 100), QRectF(0, 40, 1000, 1000),
                 Qt::IgnoreAspectRatio);
    painter.end();
    QVERIFY(image.pixelColor(6, 0).red() > 240);
    QVERIFY(image.pixelColor(6, 0).alpha() > 0);
    view.scale(1.0, 0.5);
    item->refreshPaintBounds();
    QVERIFY(item->boundingRect().bottom() > 70);
    view.resetTransform();
    item->refreshPaintBounds();
    QVERIFY(item->boundingRect().bottom() < 40);
}

QTEST_MAIN(MapEditorXviBackgroundItemTest)

#include "MapEditorXviBackgroundItemTest.moc"
