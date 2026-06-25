#include "../src/app/text_editor/map_editor/MapEditorSvgBackgroundMetadata.h"

#include <QDir>
#include <QObject>
#include <QTest>

using namespace TherionStudio;

namespace
{
QString svgFixturePath(const QString &fileName)
{
    return QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("svg_backgrounds/%1").arg(fileName));
}
}

class MapEditorSvgBackgroundMetadataTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsExplicitSizeAndViewBox();
    void fallsBackToViewBoxSize();
    void readsWikimediaFixtureSize();
};

void MapEditorSvgBackgroundMetadataTest::readsExplicitSizeAndViewBox()
{
    const MapEditorSvgBackgroundMetadata metadata =
        readMapEditorSvgBackgroundMetadata(svgFixturePath(QStringLiteral("explicit-size.svg")));

    QVERIFY(metadata.valid);
    QCOMPARE(metadata.intrinsicSize, QSizeF(120.0, 80.0));
    QCOMPARE(metadata.sourceViewBox, QRectF(-5.0, 10.0, 240.0, 160.0));
}

void MapEditorSvgBackgroundMetadataTest::fallsBackToViewBoxSize()
{
    const MapEditorSvgBackgroundMetadata metadata =
        readMapEditorSvgBackgroundMetadata(svgFixturePath(QStringLiteral("viewbox-only.svg")));

    QVERIFY(metadata.valid);
    QCOMPARE(metadata.intrinsicSize, QSizeF(50.0, 25.0));
    QCOMPARE(metadata.sourceViewBox, QRectF(2.0, 4.0, 50.0, 25.0));
}

void MapEditorSvgBackgroundMetadataTest::readsWikimediaFixtureSize()
{
    const MapEditorSvgBackgroundMetadata metadata =
        readMapEditorSvgBackgroundMetadata(svgFixturePath(QStringLiteral("caves-of-balakanche-2d-map-en.svg")));

    QVERIFY(metadata.valid);
    QCOMPARE(metadata.intrinsicSize, QSizeF(744.09448, 1052.3622));
    QCOMPARE(metadata.sourceViewBox, QRectF(0.0, 0.0, 744.09448, 1052.3622));
}

int runMapEditorSvgBackgroundMetadataTest(int argc, char **argv)
{
    MapEditorSvgBackgroundMetadataTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorSvgBackgroundMetadataTest.moc"
