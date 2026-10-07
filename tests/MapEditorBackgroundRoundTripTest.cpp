// Round trip of a Mapiah raster background through the whole chain:
//
//     scale -> pivot restoration -> xx/yy mutation -> serialization
//           -> parsing/reload
//
// The pure transform is covered by MapEditorBackgroundTransformTest.
// What matters here is that scaling a layer rewrites the anchor consistently,
// that reloading the document reproduces the same geometry, and that saving a
// second time leaves the metadata byte for byte identical.

#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionBackgroundMetadata.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QGraphicsPixmapItem>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

using namespace TherionStudio;

namespace
{
constexpr int kRasterWidth = 40;
constexpr int kRasterHeight = 20;

// A view scale of exactly 1 hides mistakes that only show up when preview and
// model units differ, so the drawing area is deliberately not the image size.
constexpr char kAreaAdjust[] = "##XTHERION## xth_me_area_adjust -20 -40 60 40\n";

void pumpEvents()
{
    for (int index = 0; index < 8; ++index) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

QString mapiahMetadataLine(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.contains(QStringLiteral("##MAPIAH## image_insert_v1"))) {
            return line;
        }
    }
    return QString();
}
}

class MapEditorBackgroundRoundTripTest final : public QObject
{
    Q_OBJECT

private slots:
    void scaledLayerSurvivesReload_data();
    void scaledLayerSurvivesReload();
    void xviScalingMatchesMapiahComposition();

private:
    bool writeFixture(const QDir &directory, bool pivotSet, qreal rotationDeg, QString *filePath);
};

bool MapEditorBackgroundRoundTripTest::writeFixture(const QDir &directory,
                                                    bool pivotSet,
                                                    qreal rotationDeg,
                                                    QString *filePath)
{
    QImage raster(kRasterWidth, kRasterHeight, QImage::Format_ARGB32);
    raster.fill(Qt::darkCyan);
    if (!raster.save(directory.filePath(QStringLiteral("background.png")))) {
        return false;
    }

    const QString metadata =
        QStringLiteral("##MAPIAH## image_insert_v1 {format=raster;filename=background.png;"
                       "xx=5;yy=30;xScale=1.4;yScale=0.7;rotationCenterDx=%1;rotationCenterDy=%2;"
                       "rotationDeg=%3;pivotSet=%4}\n")
            .arg(pivotSet ? QStringLiteral("7") : QStringLiteral("0"),
                 pivotSet ? QStringLiteral("-4") : QStringLiteral("0"),
                 QString::number(rotationDeg),
                 pivotSet ? QStringLiteral("true") : QStringLiteral("false"));

    *filePath = directory.filePath(QStringLiteral("roundtrip.th2"));
    QFile file(*filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QString contents = QStringLiteral("encoding utf-8\n")
        + QLatin1String(kAreaAdjust)
        + QStringLiteral("##XTHERION## xth_me_area_zoom_to 100\n")
        + metadata
        + QStringLiteral("\nscrap roundtrip -projection plan\n"
                         "point 0 0 station -name A\n"
                         "endscrap\n");
    file.write(contents.toUtf8());
    file.close();
    return true;
}

void MapEditorBackgroundRoundTripTest::scaledLayerSurvivesReload_data()
{
    QTest::addColumn<bool>("pivotSet");
    QTest::addColumn<qreal>("rotationDeg");

    QTest::newRow("implicit pivot, no rotation") << false << 0.0;
    QTest::newRow("implicit pivot, rotated") << false << 37.0;
    QTest::newRow("explicit pivot, rotated") << true << 37.0;
}

void MapEditorBackgroundRoundTripTest::scaledLayerSurvivesReload()
{
    QFETCH(bool, pivotSet);
    QFETCH(qreal, rotationDeg);

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir directory(temporaryDirectory.path());

    QString filePath;
    QVERIFY(writeFixture(directory, pivotSet, rotationDeg, &filePath));

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(800, 600);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    QVERIFY2(mapTab->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    for (int attempt = 0; attempt < 200
             && mapTab->backgroundLayerSourcePixelSize(0) != QSize(kRasterWidth, kRasterHeight);
         ++attempt) {
        pumpEvents();
    }
    QCOMPARE(mapTab->backgroundLayerCount(), 1);
    QCOMPARE(mapTab->backgroundLayerSourcePixelSize(0), QSize(kRasterWidth, kRasterHeight));

    mapTab->setSelectedBackgroundLayerIndex(0);
    pumpEvents();

    // Scaling must keep the pivot the user sees where it is, and push the
    // compensation into the anchor written back to the file.
    const QPointF pivotBefore = mapTab->backgroundLayerPivotScenePositionForTest(0);
    mapTab->setSelectedBackgroundLayerXScale(2.5);
    pumpEvents();
    mapTab->setSelectedBackgroundLayerYScale(1.75);
    pumpEvents();

    const QPointF pivotAfter = mapTab->backgroundLayerPivotScenePositionForTest(0);
    QVERIFY2(std::abs(pivotAfter.x() - pivotBefore.x()) < 0.5
                 && std::abs(pivotAfter.y() - pivotBefore.y()) < 0.5,
             qPrintable(QStringLiteral("pivot moved from (%1, %2) to (%3, %4) while scaling")
                            .arg(pivotBefore.x())
                            .arg(pivotBefore.y())
                            .arg(pivotAfter.x())
                            .arg(pivotAfter.y())));

    const QString savedMetadata = mapiahMetadataLine(mapTab->text());
    QVERIFY(!savedMetadata.isEmpty());
    QVERIFY(mapTab->save(&errorMessage));
    pumpEvents();

    const QRectF geometryBefore = mapTab->backgroundLayerSceneBoundingRectForTest(0);

    // Reload from disk: the geometry has to come back where it was left.
    QVERIFY2(mapTab->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    for (int attempt = 0; attempt < 200
             && mapTab->backgroundLayerSourcePixelSize(0) != QSize(kRasterWidth, kRasterHeight);
         ++attempt) {
        pumpEvents();
    }
    mapTab->setSelectedBackgroundLayerIndex(0);
    pumpEvents();

    QCOMPARE(mapiahMetadataLine(mapTab->text()), savedMetadata);

    const QRectF geometryAfter = mapTab->backgroundLayerSceneBoundingRectForTest(0);
    QVERIFY2(std::abs(geometryAfter.x() - geometryBefore.x()) < 0.5
                 && std::abs(geometryAfter.y() - geometryBefore.y()) < 0.5
                 && std::abs(geometryAfter.width() - geometryBefore.width()) < 0.5
                 && std::abs(geometryAfter.height() - geometryBefore.height()) < 0.5,
             qPrintable(QStringLiteral("geometry changed across reload: %1,%2 %3x%4 -> %5,%6 %7x%8")
                            .arg(geometryBefore.x())
                            .arg(geometryBefore.y())
                            .arg(geometryBefore.width())
                            .arg(geometryBefore.height())
                            .arg(geometryAfter.x())
                            .arg(geometryAfter.y())
                            .arg(geometryAfter.width())
                            .arg(geometryAfter.height())));

    const QPointF pivotReloaded = mapTab->backgroundLayerPivotScenePositionForTest(0);
    QVERIFY2(std::abs(pivotReloaded.x() - pivotAfter.x()) < 0.5
                 && std::abs(pivotReloaded.y() - pivotAfter.y()) < 0.5,
             qPrintable(QStringLiteral("pivot moved across reload: (%1, %2) -> (%3, %4)")
                            .arg(pivotAfter.x())
                            .arg(pivotAfter.y())
                            .arg(pivotReloaded.x())
                            .arg(pivotReloaded.y())));

    // Re-applying the reloaded scale is what actually exercises the serializer a
    // second time: MapEditorTab::save() only writes the text already in the
    // buffer, so comparing after a plain save would be very nearly tautological.
    // Feeding the reloaded values back through the setters forces the metadata
    // to be rebuilt, and an idempotent chain must reproduce it byte for byte --
    // including the pivot compensation, which has to be a no-op here.
    mapTab->setSelectedBackgroundLayerXScale(mapTab->backgroundLayerXScale(0));
    pumpEvents();
    mapTab->setSelectedBackgroundLayerYScale(mapTab->backgroundLayerYScale(0));
    pumpEvents();
    QCOMPARE(mapiahMetadataLine(mapTab->text()), savedMetadata);

    const QPointF pivotAfterReserialization = mapTab->backgroundLayerPivotScenePositionForTest(0);
    QVERIFY2(std::abs(pivotAfterReserialization.x() - pivotReloaded.x()) < 0.5
                 && std::abs(pivotAfterReserialization.y() - pivotReloaded.y()) < 0.5,
             "re-applying the same scale moved the pivot");

    QVERIFY(mapTab->save(&errorMessage));
    pumpEvents();
    QCOMPARE(mapiahMetadataLine(mapTab->text()), savedMetadata);
}

void MapEditorBackgroundRoundTripTest::xviScalingMatchesMapiahComposition()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir directory(temporaryDirectory.path());

    QFile xviFile(directory.filePath(QStringLiteral("background.xvi")));
    QVERIFY(xviFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    xviFile.write("set XVIgrid {0 0}\n"
                  "set XVIstations {\n"
                  "  {0 0 root}\n"
                  "}\n"
                  "set XVIshots {\n"
                  "  {0 0 100 0}\n"
                  "}\n"
                  "set XVIsketchlines {\n"
                  "  {black 0 0 100 100}\n"
                  "}\n");
    xviFile.close();

    const QString filePath = directory.filePath(QStringLiteral("xvi-roundtrip.th2"));
    QFile documentFile(filePath);
    QVERIFY(documentFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    documentFile.write(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust -100 -100 500 500\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##MAPIAH## image_insert_v1 {format=xvi;filename=background.xvi;xx=100;yy=200;"
        "xScale=1;yScale=1;rotationCenterDx=25;rotationCenterDy=15;rotationDeg=0;"
        "pivotSet=true;xviRoot=root}\n"
        "\n"
        "scrap xvi-roundtrip -projection plan\n"
        "point 100 200 station -name root\n"
        "endscrap\n");
    documentFile.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(800, 600);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    QVERIFY2(mapTab->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    for (int attempt = 0;
         attempt < 200
         && (mapTab->backgroundLayerCount() != 1
             || !mapTab->backgroundLayerSceneBoundingRectForTest(0).isValid());
         ++attempt) {
        pumpEvents();
    }
    QCOMPARE(mapTab->backgroundLayerCount(), 1);
    mapTab->setSelectedBackgroundLayerIndex(0);
    pumpEvents();

    const QVector<TherionBackgroundReference> referencesBefore =
        parseTherionBackgroundReferences(mapTab->text(), filePath);
    QCOMPARE(referencesBefore.size(), 1);
    QCOMPARE(referencesBefore.first().layerFormat, TherionBackgroundLayerFormat::Xvi);
    const QPointF anchorBefore = referencesBefore.first().basePosition;
    const QPointF pivotBefore = mapTab->backgroundLayerPivotScenePositionForTest(0);
    const QRectF boundsBefore = mapTab->backgroundLayerSceneBoundingRectForTest(0);
    QVERIFY(boundsBefore.isValid());

    mapTab->setSelectedBackgroundLayerXScale(2.0);
    pumpEvents();
    mapTab->setSelectedBackgroundLayerYScale(1.5);
    pumpEvents();

    const QPointF pivotAfter = mapTab->backgroundLayerPivotScenePositionForTest(0);
    const QRectF boundsAfter = mapTab->backgroundLayerSceneBoundingRectForTest(0);
    QVERIFY(std::abs(pivotAfter.x() - pivotBefore.x()) < 0.5);
    QVERIFY(std::abs(pivotAfter.y() - pivotBefore.y()) < 0.5);
    QVERIFY(std::abs(boundsAfter.width() - (boundsBefore.width() * 2.0)) < 0.5);
    QVERIFY(std::abs(boundsAfter.height() - (boundsBefore.height() * 1.5)) < 0.5);

    const QVector<TherionBackgroundReference> referencesAfter =
        parseTherionBackgroundReferences(mapTab->text(), filePath);
    QCOMPARE(referencesAfter.size(), 1);
    QCOMPARE(referencesAfter.first().layerFormat, TherionBackgroundLayerFormat::Xvi);
    QVERIFY(std::abs(referencesAfter.first().basePosition.x() - 75.0) < 1e-6);
    QVERIFY(std::abs(referencesAfter.first().basePosition.y() - 192.5) < 1e-6);
    QVERIFY(referencesAfter.first().basePosition != anchorBefore);
    QCOMPARE(referencesAfter.first().xScale, 2.0);
    QCOMPARE(referencesAfter.first().yScale, 1.5);

    const QPointF pickedPivot = pivotAfter + QPointF(17.0, -11.0);
    mapTab->setSelectedBackgroundLayerPivotAtScenePositionForTest(pickedPivot);
    pumpEvents();
    const QPointF pivotAfterPick = mapTab->backgroundLayerPivotScenePositionForTest(0);
    QVERIFY(std::abs(pivotAfterPick.x() - pickedPivot.x()) < 0.5);
    QVERIFY(std::abs(pivotAfterPick.y() - pickedPivot.y()) < 0.5);

    mapTab->setSelectedBackgroundLayerRotationDeg(23.0);
    pumpEvents();
    const QPointF pivotAfterRotation = mapTab->backgroundLayerPivotScenePositionForTest(0);
    QVERIFY(std::abs(pivotAfterRotation.x() - pickedPivot.x()) < 0.5);
    QVERIFY(std::abs(pivotAfterRotation.y() - pickedPivot.y()) < 0.5);

    const QVector<TherionBackgroundReference> referencesBeforeRotatedScale =
        parseTherionBackgroundReferences(mapTab->text(), filePath);
    QCOMPARE(referencesBeforeRotatedScale.size(), 1);
    mapTab->setSelectedBackgroundLayerYScale(1.75);
    pumpEvents();
    const QPointF pivotAfterRotatedScale = mapTab->backgroundLayerPivotScenePositionForTest(0);
    QVERIFY(std::abs(pivotAfterRotatedScale.x() - pickedPivot.x()) < 0.5);
    QVERIFY(std::abs(pivotAfterRotatedScale.y() - pickedPivot.y()) < 0.5);
    const QVector<TherionBackgroundReference> referencesAfterRotatedScale =
        parseTherionBackgroundReferences(mapTab->text(), filePath);
    QCOMPARE(referencesAfterRotatedScale.size(), 1);
    QVERIFY(referencesAfterRotatedScale.first().basePosition
            != referencesBeforeRotatedScale.first().basePosition);
    QCOMPARE(referencesAfterRotatedScale.first().yScale, 1.75);
    QCOMPARE(referencesAfterRotatedScale.first().rotationDeg, 23.0);
    const QRectF boundsBeforeSave = mapTab->backgroundLayerSceneBoundingRectForTest(0);

    const QString savedMetadata = mapiahMetadataLine(mapTab->text());
    QVERIFY(!savedMetadata.isEmpty());
    QVERIFY2(mapTab->save(&errorMessage), qPrintable(errorMessage));
    pumpEvents();
    QVERIFY2(mapTab->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    for (int attempt = 0;
         attempt < 200
         && (mapTab->backgroundLayerCount() != 1
             || !mapTab->backgroundLayerSceneBoundingRectForTest(0).isValid());
         ++attempt) {
        pumpEvents();
    }
    QCOMPARE(mapiahMetadataLine(mapTab->text()), savedMetadata);
    const QPointF pivotReloaded = mapTab->backgroundLayerPivotScenePositionForTest(0);
    const QRectF boundsReloaded = mapTab->backgroundLayerSceneBoundingRectForTest(0);
    QVERIFY(std::abs(pivotReloaded.x() - pivotAfterRotatedScale.x()) < 0.5);
    QVERIFY(std::abs(pivotReloaded.y() - pivotAfterRotatedScale.y()) < 0.5);
    QVERIFY(std::abs(boundsReloaded.x() - boundsBeforeSave.x()) < 0.5);
    QVERIFY(std::abs(boundsReloaded.y() - boundsBeforeSave.y()) < 0.5);
    QVERIFY(std::abs(boundsReloaded.width() - boundsBeforeSave.width()) < 0.5);
    QVERIFY(std::abs(boundsReloaded.height() - boundsBeforeSave.height()) < 0.5);
}

int runMapEditorBackgroundRoundTripTest(int argc, char **argv)
{
    MapEditorBackgroundRoundTripTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorBackgroundRoundTripTest.moc"
