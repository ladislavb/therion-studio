#include "../../src/core/TherionBackgroundMetadata.h"

#include <QDir>
#include <QFileInfo>
#include <QtMath>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.0001)
{
    return qAbs(a - b) <= epsilon;
}

QString normalizedPathForCompare(const QString &path)
{
    return QDir::fromNativeSeparators(QDir::cleanPath(path));
}

QString documentPathForTest(const QString &fileName)
{
    return QDir(QDir::tempPath()).filePath(QStringLiteral("therion-studio-test/project/data/") + fileName);
}
}

class TherionBackgroundMetadataTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesRasterInsert();
    void parsesBracedRasterPath();
    void parsesUnbracedRasterYCoordinate();
    void parsesXviInsert();
    void parsesMapiahRasterInsert();
    void parsesMapiahXviInsert();
    void parsesMapiahSvgInsert();
    void serializesMapiahSvgInsert();
    void parsesAreaAdjust();
    void parsesMixedLineEndings();
    void ignoresMalformedMetadata();
    void upsertsAreaAdjustMetadata();
};

void TherionBackgroundMetadataTest::parsesRasterInsert()
{
    const QString documentPath = documentPathForTest(QStringLiteral("clopy01.th2"));
    const QString text =
        QStringLiteral("encoding utf-8\n"
                       "##XTHERION## xth_me_image_insert {0 1 5.011872336272722} {0 {}} clopy01.png 0 {}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QCOMPARE(references.size(), 1);

    const TherionBackgroundReference &reference = references.first();
    QVERIFY(reference.hasBasePosition);
    QVERIFY(nearlyEqual(reference.basePosition.x(), 0.0));
    QVERIFY(nearlyEqual(reference.basePosition.y(), 0.0));
    QVERIFY(reference.hasImageScale);
    QVERIFY(nearlyEqual(reference.imageScale, 5.011872336272722));
    QVERIFY(reference.hasVisibility);
    QVERIFY(reference.visible);
    QCOMPARE(reference.lineNumber, 2);
    QVERIFY(!reference.xviReference);
    QVERIFY(reference.metadataTopEdgeAnchor);

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(QStringLiteral("clopy01.png"));
    QCOMPARE(normalizedPathForCompare(reference.absolutePath), normalizedPathForCompare(expectedPath));
}

void TherionBackgroundMetadataTest::parsesBracedRasterPath()
{
    const QString documentPath = documentPathForTest(QStringLiteral("map.th2"));
    const QString text =
        QStringLiteral("##XTHERION## xth_me_image_insert {-12.5 0 1.25} {40 {}} {background scans/clopy 01.png} 0 {}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QCOMPARE(references.size(), 1);

    const TherionBackgroundReference &reference = references.first();
    QVERIFY(reference.hasBasePosition);
    QVERIFY(nearlyEqual(reference.basePosition.x(), -12.5));
    QVERIFY(nearlyEqual(reference.basePosition.y(), 40.0));
    QVERIFY(reference.hasVisibility);
    QVERIFY(!reference.visible);
    QVERIFY(reference.hasImageScale);
    QVERIFY(nearlyEqual(reference.imageScale, 1.25));

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(
        QStringLiteral("background scans/clopy 01.png"));
    QCOMPARE(normalizedPathForCompare(reference.absolutePath), normalizedPathForCompare(expectedPath));
}

void TherionBackgroundMetadataTest::parsesUnbracedRasterYCoordinate()
{
    const QString documentPath = documentPathForTest(QStringLiteral("severna.th2"));
    const QString text =
        QStringLiteral("encoding utf-8\n"
                       "##XTHERION## xth_me_area_adjust 1170.74 0.938 25 1560.0\n"
                       "##XTHERION## xth_me_image_insert {245.0 1.0} 822.0 img/sev1.gif 0 {}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QCOMPARE(references.size(), 1);

    const TherionBackgroundReference &reference = references.first();
    QVERIFY(reference.hasBasePosition);
    QVERIFY(nearlyEqual(reference.basePosition.x(), 245.0));
    QVERIFY(nearlyEqual(reference.basePosition.y(), 822.0));
    QVERIFY(reference.hasVisibility);
    QVERIFY(reference.visible);
    QVERIFY(!reference.xviReference);

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(QStringLiteral("img/sev1.gif"));
    QCOMPARE(normalizedPathForCompare(reference.absolutePath), normalizedPathForCompare(expectedPath));
}

void TherionBackgroundMetadataTest::parsesXviInsert()
{
    const QString documentPath = documentPathForTest(QStringLiteral("create.th2"));
    const QString text =
        QStringLiteral("##XTHERION## xth_me_image_insert {456.253 1 1.0} {60.18 0@create.} create.xvi 0 {}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QCOMPARE(references.size(), 1);

    const TherionBackgroundReference &reference = references.first();
    QVERIFY(reference.xviReference);
    QVERIFY(!reference.metadataTopEdgeAnchor);
    QCOMPARE(reference.rootStationName, QStringLiteral("0@create."));
    QVERIFY(reference.hasBasePosition);
    QVERIFY(nearlyEqual(reference.basePosition.x(), 456.253));
    QVERIFY(nearlyEqual(reference.basePosition.y(), 60.18));
}

void TherionBackgroundMetadataTest::parsesMapiahRasterInsert()
{
    const QString documentPath = documentPathForTest(QStringLiteral("mapiah-raster.th2"));
    const QString text =
        QStringLiteral("encoding utf-8\n"
                       "##MAPIAH## image_insert_v1 {format=raster;filename=background%20scans%2Fscan.png;xx=2125.4;yy=1339.1;xScale=1.2;yScale=0.8;rotationCenterDx=10;rotationCenterDy=-5;rotationDeg=-12.5;pivotSet=true;gamma=1.4}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QCOMPARE(references.size(), 1);

    const TherionBackgroundReference &reference = references.first();
    QCOMPARE(reference.metadataFormat, TherionBackgroundMetadataFormat::Mapiah);
    QCOMPARE(reference.layerFormat, TherionBackgroundLayerFormat::Raster);
    QVERIFY(!reference.xviReference);
    QVERIFY(reference.metadataTopEdgeAnchor);
    QVERIFY(reference.hasBasePosition);
    QVERIFY(nearlyEqual(reference.basePosition.x(), 2125.4));
    QVERIFY(nearlyEqual(reference.basePosition.y(), 1339.1));
    QVERIFY(nearlyEqual(reference.xScale, 1.2));
    QVERIFY(nearlyEqual(reference.yScale, 0.8));
    QVERIFY(nearlyEqual(reference.rotationCenterDx, 10.0));
    QVERIFY(nearlyEqual(reference.rotationCenterDy, -5.0));
    QVERIFY(nearlyEqual(reference.rotationDeg, -12.5));
    QVERIFY(reference.pivotSet);
    QVERIFY(reference.hasImageScale);
    QVERIFY(nearlyEqual(reference.imageScale, 1.4));

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(QStringLiteral("background scans/scan.png"));
    QCOMPARE(normalizedPathForCompare(reference.absolutePath), normalizedPathForCompare(expectedPath));
}

void TherionBackgroundMetadataTest::parsesMapiahXviInsert()
{
    const QString documentPath = documentPathForTest(QStringLiteral("mapiah-xvi.th2"));
    const QString text =
        QStringLiteral("##MAPIAH## image_insert_v1 {format=xvi;filename=ptopo%2F4-BulmerResurgence_p.xvi;xx=2222.519685;yy=906.377953;xScale=1;yScale=1;rotationCenterDx=0;rotationCenterDy=0;rotationDeg=-90.2;pivotSet=true;xviRoot=2.32}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QCOMPARE(references.size(), 1);

    const TherionBackgroundReference &reference = references.first();
    QCOMPARE(reference.metadataFormat, TherionBackgroundMetadataFormat::Mapiah);
    QCOMPARE(reference.layerFormat, TherionBackgroundLayerFormat::Xvi);
    QVERIFY(reference.xviReference);
    QVERIFY(!reference.metadataTopEdgeAnchor);
    QVERIFY(reference.hasBasePosition);
    QVERIFY(nearlyEqual(reference.basePosition.x(), 2222.519685));
    QVERIFY(nearlyEqual(reference.basePosition.y(), 906.377953));
    QVERIFY(nearlyEqual(reference.rotationDeg, -90.2));
    QCOMPARE(reference.rootStationName, QStringLiteral("2.32"));

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(
        QStringLiteral("ptopo/4-BulmerResurgence_p.xvi"));
    QCOMPARE(normalizedPathForCompare(reference.absolutePath), normalizedPathForCompare(expectedPath));
}

void TherionBackgroundMetadataTest::parsesMapiahSvgInsert()
{
    const QString documentPath = documentPathForTest(QStringLiteral("mapiah-svg.th2"));
    const QString text =
        QStringLiteral("##MAPIAH## image_insert_v1 {format=svg;filename=sketch.svg;xx=1;yy=2;xScale=1.5;yScale=0.5;rotationCenterDx=10;rotationCenterDy=-20;rotationDeg=30;pivotSet=true;intrinsicWidth=120;intrinsicHeight=80;sourceViewBoxLeft=-5;sourceViewBoxTop=10;sourceViewBoxWidth=240;sourceViewBoxHeight=160}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QCOMPARE(references.size(), 1);

    const TherionBackgroundReference &reference = references.first();
    QCOMPARE(reference.metadataFormat, TherionBackgroundMetadataFormat::Mapiah);
    QCOMPARE(reference.layerFormat, TherionBackgroundLayerFormat::Svg);
    QVERIFY(!reference.xviReference);
    QVERIFY(reference.metadataTopEdgeAnchor);
    QVERIFY(reference.hasBasePosition);
    QVERIFY(nearlyEqual(reference.basePosition.x(), 1.0));
    QVERIFY(nearlyEqual(reference.basePosition.y(), 2.0));
    QVERIFY(nearlyEqual(reference.xScale, 1.5));
    QVERIFY(nearlyEqual(reference.yScale, 0.5));
    QVERIFY(nearlyEqual(reference.rotationCenterDx, 10.0));
    QVERIFY(nearlyEqual(reference.rotationCenterDy, -20.0));
    QVERIFY(nearlyEqual(reference.rotationDeg, 30.0));
    QVERIFY(reference.pivotSet);
    QVERIFY(reference.hasSvgIntrinsicSize);
    QVERIFY(nearlyEqual(reference.svgIntrinsicSize.width(), 120.0));
    QVERIFY(nearlyEqual(reference.svgIntrinsicSize.height(), 80.0));
    QVERIFY(reference.hasSvgSourceViewBox);
    QVERIFY(nearlyEqual(reference.svgSourceViewBox.left(), -5.0));
    QVERIFY(nearlyEqual(reference.svgSourceViewBox.top(), 10.0));
    QVERIFY(nearlyEqual(reference.svgSourceViewBox.width(), 240.0));
    QVERIFY(nearlyEqual(reference.svgSourceViewBox.height(), 160.0));

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(QStringLiteral("sketch.svg"));
    QCOMPARE(normalizedPathForCompare(reference.absolutePath), normalizedPathForCompare(expectedPath));
}

void TherionBackgroundMetadataTest::serializesMapiahSvgInsert()
{
    const QString documentPath = documentPathForTest(QStringLiteral("mapiah-svg-write.th2"));
    const QString svgPath = QFileInfo(documentPath).dir().filePath(QStringLiteral("background scans/sketch.svg"));

    const QString line = therionMapiahImageInsertMetadataLine(svgPath,
                                                               documentPath,
                                                               TherionBackgroundLayerFormat::Svg,
                                                               QPointF(1.0, 2.0),
                                                               1.5,
                                                               0.5,
                                                               10.0,
                                                               -20.0,
                                                               30.0,
                                                               true,
                                                               QString(),
                                                               QSizeF(120.0, 80.0),
                                                               QRectF(-5.0, 10.0, 240.0, 160.0));
    const QString expected =
        QStringLiteral("##MAPIAH## image_insert_v1 {format=svg;filename=background%20scans%2Fsketch.svg;xx=1;yy=2;xScale=1.5;yScale=0.5;rotationCenterDx=10;rotationCenterDy=-20;rotationDeg=30;pivotSet=true;intrinsicWidth=120;intrinsicHeight=80;sourceViewBoxLeft=-5;sourceViewBoxTop=10;sourceViewBoxWidth=240;sourceViewBoxHeight=160}");
    QCOMPARE(line, expected);

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(line, documentPath);
    QCOMPARE(references.size(), 1);
    const TherionBackgroundReference &reference = references.first();
    QCOMPARE(reference.layerFormat, TherionBackgroundLayerFormat::Svg);
    QVERIFY(reference.hasSvgIntrinsicSize);
    QVERIFY(reference.hasSvgSourceViewBox);
}

void TherionBackgroundMetadataTest::parsesAreaAdjust()
{
    const TherionAreaAdjust area = parseTherionAreaAdjust(
        QStringLiteral("##XTHERION## xth_me_area_adjust 851 128 -128 -1152\n"));

    QVERIFY(area.valid);
    QVERIFY(nearlyEqual(area.modelRect.left(), -128.0));
    QVERIFY(nearlyEqual(area.modelRect.top(), -1152.0));
    QVERIFY(nearlyEqual(area.modelRect.right(), 851.0));
    QVERIFY(nearlyEqual(area.modelRect.bottom(), 128.0));
}

void TherionBackgroundMetadataTest::parsesMixedLineEndings()
{
    const QString documentPath = documentPathForTest(QStringLiteral("mixed.th2"));
    const QString text =
        QStringLiteral("encoding utf-8\r"
                       "##XTHERION## xth_me_area_adjust 10 20 30 40\r\n"
                       "##XTHERION## xth_me_image_insert {1 1 2.5} {3 root.station} mixed.xvi 0 {}\n");

    const TherionAreaAdjust area = parseTherionAreaAdjust(text);
    QVERIFY(area.valid);
    QVERIFY(nearlyEqual(area.modelRect.left(), 10.0));
    QVERIFY(nearlyEqual(area.modelRect.top(), 20.0));
    QVERIFY(nearlyEqual(area.modelRect.right(), 30.0));
    QVERIFY(nearlyEqual(area.modelRect.bottom(), 40.0));

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QCOMPARE(references.size(), 1);
    QCOMPARE(references.first().lineNumber, 3);
    QCOMPARE(references.first().rootStationName, QStringLiteral("root.station"));
}

void TherionBackgroundMetadataTest::ignoresMalformedMetadata()
{
    const QString documentPath = documentPathForTest(QStringLiteral("map.th2"));
    const QString text =
        QStringLiteral("##XTHERION## xth_me_image_insert {broken payload\n"
                       "##XTHERION## xth_me_area_adjust 1 2 3\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    QVERIFY(references.isEmpty());

    const TherionAreaAdjust area = parseTherionAreaAdjust(text);
    QVERIFY(!area.valid);
}

void TherionBackgroundMetadataTest::upsertsAreaAdjustMetadata()
{
    const QString text =
        QStringLiteral("encoding utf-8\r\n"
                       "scrap scrap-1\r\n"
                       "endscrap\r\n");

    const QString updated = upsertTherionAreaAdjustMetadata(
        text, QRectF(QPointF(0.0, 0.0), QPointF(256.0, 256.0)));
    const QString expected =
        QStringLiteral("encoding utf-8\r\n"
                       "##XTHERION## xth_me_area_adjust 0 0 256 256\r\n"
                       "##XTHERION## xth_me_area_zoom_to 100\r\n"
                       "scrap scrap-1\r\n"
                       "endscrap\r\n");
    QCOMPARE(updated, expected);

    const QString replaced = upsertTherionAreaAdjustMetadata(
        updated, QRectF(QPointF(-10.5, -20.0), QPointF(30.0, 40.0)));
    QVERIFY(replaced.contains(QStringLiteral("##XTHERION## xth_me_area_adjust -10.5 -20 30 40\r\n")));
    const int firstZoomIndex = replaced.indexOf(QStringLiteral("xth_me_area_zoom_to"));
    QVERIFY(firstZoomIndex >= 0);
    QCOMPARE(firstZoomIndex, replaced.lastIndexOf(QStringLiteral("xth_me_area_zoom_to")));
}

int runTherionBackgroundMetadataTest(int argc, char **argv)
{
    TherionBackgroundMetadataTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionBackgroundMetadataTest.moc"
