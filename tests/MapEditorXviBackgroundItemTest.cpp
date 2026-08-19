#include "../src/app/text_editor/map_editor/MapEditorXviBackgroundItem.h"

#include <QImage>
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

QTEST_MAIN(MapEditorXviBackgroundItemTest)

#include "MapEditorXviBackgroundItemTest.moc"
