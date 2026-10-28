// Status bar pointer coordinates in the map editor.
//
// The text is the magnifier's coordinate readout, so both always agree. It must
// follow the pointer whether or not the magnifier is shown, and clear when the
// pointer leaves the map. Over a visible XVI station it also names the station.
// Both parts are settings, off by default.

#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorXviBackgroundItem.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMouseEvent>
#include <QObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

#include <memory>

using namespace TherionStudio;

namespace
{
void pumpEvents()
{
    for (int index = 0; index < 8; ++index) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

bool writeFixture(const QDir &directory, QString *filePath)
{
    *filePath = directory.filePath(QStringLiteral("pointer.th2"));
    QFile file(*filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    // Drawing units differ from view units so a missing conversion would show.
    file.write("encoding utf-8\n"
               "##XTHERION## xth_me_area_adjust -20 -40 60 40\n"
               "##XTHERION## xth_me_area_zoom_to 100\n"
               "\nscrap pointer -projection plan\n"
               "point 0 0 station -name A\n"
               "point 40 20 station -name B\n"
               "endscrap\n");
    return true;
}

// The XVI root station is aligned to drawing station A at 0 0.
bool writeXviFixture(const QDir &directory, QString *filePath)
{
    QFile xviFile(directory.filePath(QStringLiteral("background.xvi")));
    if (!xviFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    xviFile.write("set XVIgrid {0 0}\n"
                  "set XVIstations {\n"
                  "  {0 0 1.0@pointer}\n"
                  "  {100 0 1.1@pointer}\n"
                  "}\n"
                  "set XVIshots {\n"
                  "  {0 0 100 0}\n"
                  "}\n");
    xviFile.close();

    *filePath = directory.filePath(QStringLiteral("pointer-xvi.th2"));
    QFile file(*filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write("encoding utf-8\n"
               "##XTHERION## xth_me_area_adjust -100 -100 300 200\n"
               "##XTHERION## xth_me_area_zoom_to 100\n"
               "##MAPIAH## image_insert_v1 {format=xvi;filename=background.xvi;xx=0;yy=0;"
               "xScale=1;yScale=1;rotationCenterDx=0;rotationCenterDy=0;rotationDeg=0;"
               "pivotSet=false;xviRoot=1.0@pointer}\n"
               "\nscrap pointer -projection plan\n"
               "point 0 0 station -name 1.0@pointer\n"
               "endscrap\n");
    return true;
}

std::unique_ptr<QMainWindow> showMapTab(QtFileSystem &fileSystem,
                                        FakeSessionStore &sessionStore,
                                        MapEditorTab **mapTab)
{
    auto hostWindow = std::make_unique<QMainWindow>();
    hostWindow->resize(800, 600);
    auto *central = new QWidget(hostWindow.get());
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(*mapTab);
    hostWindow->setCentralWidget(central);
    hostWindow->show();
    pumpEvents();
    return hostWindow;
}

void sendMouseMove(QWidget *viewport, const QPoint &position)
{
    QMouseEvent event(QEvent::MouseMove,
                      QPointF(position),
                      QPointF(viewport->mapToGlobal(position)),
                      Qt::NoButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}
}

class MapEditorPointerStatusTest final : public QObject
{
    Q_OBJECT

private slots:
    void coordinateTextUsesOneDecimalAndDotSeparator();
    void statusFollowsPointerAndClearsOnLeave_data();
    void statusFollowsPointerAndClearsOnLeave();
    void statusStaysEmptyWithSettingsOff();
    void labelTextJoinsEnabledParts();
    void statusNamesHoveredVisibleXviStation_data();
    void statusNamesHoveredVisibleXviStation();
};

void MapEditorPointerStatusTest::coordinateTextUsesOneDecimalAndDotSeparator()
{
    const QLocale previousLocale;
    QLocale::setDefault(QLocale(QLocale::Czech));
    const QString text = MapEditorTab::pointerCoordinateText(QPointF(1234.54, -67.84));
    QLocale::setDefault(previousLocale);

    QCOMPARE(text, QStringLiteral("x 1234.5  y -67.8"));
}

void MapEditorPointerStatusTest::statusFollowsPointerAndClearsOnLeave_data()
{
    QTest::addColumn<bool>("magnifierEnabled");

    QTest::newRow("magnifier shown") << true;
    QTest::newRow("magnifier hidden") << false;
}

void MapEditorPointerStatusTest::statusFollowsPointerAndClearsOnLeave()
{
    QFETCH(bool, magnifierEnabled);

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QString filePath;
    QVERIFY(writeFixture(QDir(temporaryDirectory.path()), &filePath));

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    sessionStore.setTherionMapPointerCoordinatesInStatusBarEnabled(true);
    MapEditorTab *mapTab = nullptr;
    const std::unique_ptr<QMainWindow> hostWindow = showMapTab(fileSystem, sessionStore, &mapTab);

    QString errorMessage;
    QVERIFY2(mapTab->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    mapTab->setMagnifierEnabled(magnifierEnabled);
    pumpEvents();

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    QVERIFY(mapView != nullptr);
    QWidget *viewport = mapView->viewport();
    QVERIFY(mapTab->pointerStatusText().isEmpty());

    QSignalSpy statusSpy(mapTab, &MapEditorTab::pointerStatusChanged);
    const QPoint position = viewport->rect().center() + QPoint(17, -11);
    sendMouseMove(viewport, position);

    const QString expected = MapEditorTab::pointerCoordinateText(
        mapTab->sourcePointFromScenePositionForTest(mapView->mapToScene(position)));
    QTRY_COMPARE(mapTab->pointerStatusText(), expected);
    QVERIFY(statusSpy.count() >= 1);

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(viewport, &leaveEvent);
    QVERIFY(mapTab->pointerStatusText().isEmpty());
}

void MapEditorPointerStatusTest::statusStaysEmptyWithSettingsOff()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QString filePath;
    QVERIFY(writeFixture(QDir(temporaryDirectory.path()), &filePath));

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    MapEditorTab *mapTab = nullptr;
    const std::unique_ptr<QMainWindow> hostWindow = showMapTab(fileSystem, sessionStore, &mapTab);

    QString errorMessage;
    QVERIFY2(mapTab->loadFile(filePath, &errorMessage), qPrintable(errorMessage));
    pumpEvents();
    QVERIFY(!mapTab->isPointerStatusEnabled());

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    QVERIFY(mapView != nullptr);
    sendMouseMove(mapView->viewport(), mapView->viewport()->rect().center());
    QTest::qWait(100);
    QVERIFY(mapTab->pointerStatusText().isEmpty());

    // Switching the setting on takes effect at the pointer's current position.
    sessionStore.setTherionMapPointerCoordinatesInStatusBarEnabled(true);
    mapTab->refreshPointerStatusOptions();
    QVERIFY(mapTab->isPointerStatusEnabled());
    QTRY_VERIFY(!mapTab->pointerStatusText().isEmpty());
}

void MapEditorPointerStatusTest::labelTextJoinsEnabledParts()
{
    const QString coordinates = QStringLiteral("x 1.0  y 2.0");
    const QString station = QStringLiteral("1.0@pointer");
    QCOMPARE(MapEditorTab::pointerStatusLabelText(QString(), QString()), QString());
    QCOMPARE(MapEditorTab::pointerStatusLabelText(coordinates, QString()), coordinates);
    QCOMPARE(MapEditorTab::pointerStatusLabelText(QString(), station), QStringLiteral("Station: 1.0@pointer"));
    QCOMPARE(MapEditorTab::pointerStatusLabelText(coordinates, station),
             QStringLiteral("x 1.0  y 2.0   Station: 1.0@pointer"));
}

void MapEditorPointerStatusTest::statusNamesHoveredVisibleXviStation_data()
{
    QTest::addColumn<bool>("coordinatesEnabled");
    QTest::addColumn<bool>("stationNamesEnabled");

    QTest::newRow("coordinates and station names") << true << true;
    QTest::newRow("station names only") << false << true;
    QTest::newRow("coordinates only") << true << false;
}

void MapEditorPointerStatusTest::statusNamesHoveredVisibleXviStation()
{
    QFETCH(bool, coordinatesEnabled);
    QFETCH(bool, stationNamesEnabled);

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QString filePath;
    QVERIFY(writeXviFixture(QDir(temporaryDirectory.path()), &filePath));

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    sessionStore.setTherionMapPointerCoordinatesInStatusBarEnabled(coordinatesEnabled);
    sessionStore.setTherionMapXviStationNamesInStatusBarEnabled(stationNamesEnabled);
    MapEditorTab *mapTab = nullptr;
    const std::unique_ptr<QMainWindow> hostWindow = showMapTab(fileSystem, sessionStore, &mapTab);

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

    auto *mapView = mapTab->findChild<QGraphicsView *>();
    QVERIFY(mapView != nullptr);
    QWidget *viewport = mapView->viewport();
    MapEditorXviBackgroundItem *xviItem = nullptr;
    for (QGraphicsItem *item : mapView->scene()->items()) {
        if ((xviItem = dynamic_cast<MapEditorXviBackgroundItem *>(item)) != nullptr) {
            break;
        }
    }
    QVERIFY(xviItem != nullptr);
    QCOMPARE(xviItem->geometryData().stations.size(), 2);
    const MapEditorXviStationData &station = xviItem->geometryData().stations.at(1);
    mapTab->triggerFitWithBackground();
    pumpEvents();
    const QPointF stationScenePosition = xviItem->mapToScene(station.position);
    const QPoint stationPosition = mapView->mapFromScene(stationScenePosition);
    QVERIFY(viewport->rect().contains(stationPosition));

    const auto expectedAt = [&](const QPoint &position, const QString &stationName) {
        const QString coordinates = MapEditorTab::pointerCoordinateText(
            mapTab->sourcePointFromScenePositionForTest(mapView->mapToScene(position)));
        return MapEditorTab::pointerStatusLabelText(coordinatesEnabled ? coordinates : QString(),
                                                    stationNamesEnabled ? stationName : QString());
    };

    // Within the snap radius, but not on the station's exact position.
    const QPoint nearStation = stationPosition + QPoint(3, 2);
    sendMouseMove(viewport, nearStation);
    QTRY_COMPARE(mapTab->pointerStatusText(), expectedAt(nearStation, QStringLiteral("1.1@pointer")));

    const QPoint awayFromStation = stationPosition + QPoint(0, 40);
    sendMouseMove(viewport, awayFromStation);
    QTRY_COMPARE(mapTab->pointerStatusText(), expectedAt(awayFromStation, QString()));

    mapTab->setSelectedBackgroundLayerIndex(0);
    mapTab->toggleSelectedBackgroundLayerVisibility();
    pumpEvents();
    QVERIFY(!xviItem->isVisible());
    sendMouseMove(viewport, stationPosition);
    QTRY_COMPARE(mapTab->pointerStatusText(), expectedAt(stationPosition, QString()));
}

int runMapEditorPointerStatusTest(int argc, char **argv)
{
    MapEditorPointerStatusTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorPointerStatusTest.moc"
