#include "../src/app/MainWindowSettingsDialog.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorXviBackgroundItem.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/SessionStore.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QLabel>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStyleOptionGraphicsItem>
#include <QTabWidget>
#include <QTimer>
#include <QUndoStack>
#include <QWheelEvent>
#include <QtTest>

#include <limits>

using namespace TherionStudio;

class MapEditorAppearanceTest final : public QObject
{
    Q_OBJECT

private slots:
    void backupSettingsAreDiscoverableInSettingsDialog()
    {
        MainWindowSettingsDialog dialog({});
        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        tabs->setCurrentIndex(1);
        auto *enable = tabs->currentWidget()->findChild<QCheckBox *>(QStringLiteral("localHistoryEnabled"));
        auto *remote = tabs->currentWidget()->findChild<QLabel *>(QStringLiteral("githubBackupStatus"));
        QVERIFY(enable);
        QVERIFY(!enable->isEnabled());
        QVERIFY(remote);
        QVERIFY(!remote->text().isEmpty());
    }

    void mapZoomReachesTenThousandPercent_data()
    {
        QTest::addColumn<bool>("detached");
        QTest::newRow("embedded") << false;
        QTest::newRow("detached") << true;
    }

    void mapZoomReachesTenThousandPercent()
    {
        QFETCH(bool, detached);
        QtFileSystem files;
        InMemorySessionStore prefs;
        MapEditorTab tab(files, prefs, CommandCatalogStore{});
        tab.resize(900, 600);
        tab.show();
        auto *view = tab.findChild<QGraphicsView *>();
        QVERIFY(view);
        if (detached) {
            tab.toggleMapPaneWindow();
        }
        QCoreApplication::processEvents();
        view->scene()->setSceneRect(-10000, -10000, 20000, 20000);
        view->resetTransform();
        view->scale(50.0, 50.0);
        view->centerOn(500, 500);
        const QPoint anchor(130, 100);
        const QPointF before = view->mapToScene(anchor);
        const auto wheel = [&](int delta) {
            QWheelEvent event(anchor, view->viewport()->mapToGlobal(anchor), {}, QPoint(0, delta),
                              Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(view->viewport(), &event);
        };
        wheel(120);
        QVERIFY(view->transform().m11() > 50.0);
        for (int step = 0; step < 16; ++step) {
            wheel(120);
        }
        QCOMPARE(view->transform().m11(), 200.0);
        QCOMPARE(tab.zoomPercent(), 20000);
        QVERIFY(QLineF(before, view->mapToScene(anchor)).length() < 0.1);
        for (int step = 0; step < 4; ++step) {
            tab.triggerZoomIn();
        }
        QCOMPARE(view->transform().m11(), 200.0);
        view->resetTransform();
        view->scale(190.0, 190.0);
        QNativeGestureEvent pinch(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(),
                                  2, anchor, anchor, view->viewport()->mapToGlobal(anchor), 0.25, {}, 1);
        QApplication::sendEvent(view->viewport(), &pinch);
        QCOMPARE(view->transform().m11(), 200.0);
        QCOMPARE(tab.zoomPercent(), 20000);
        for (int step = 0; step < 80; ++step) {
            tab.triggerZoomOut();
        }
        QVERIFY(qAbs(view->transform().m11() - 0.1) < 1e-9);
        QCOMPARE(tab.zoomPercent(), 10);
        for (int step = 0; step < 80; ++step) {
            tab.triggerZoomIn();
        }
        QCOMPARE(view->transform().m11(), 200.0);
        QCOMPARE(tab.zoomPercent(), 20000);
        if (detached) {
            tab.toggleMapPaneWindow();
        }
    }

    void defaultsAndPersistence()
    {
        const QString path = QStringLiteral(THERION_STUDIO_TEST_WORKSPACE)
            + QStringLiteral("/map-appearance-settings.ini");
        QFile::remove(path);
        MapAppearanceSettings appearance;
        {
            SessionSettingsStore store(path);
            QCOMPARE(store.therionMapAppearance().altitudeUnderlayStrengthPercent, 19);
            QCOMPARE(store.therionMapAppearance().xviCenterlineColor, QStringLiteral("#82122c58"));
            QCOMPARE(store.therionMapAppearance().xviCenterlineWidth, 1.8);
            QCOMPARE(store.therionMapAppearance().xviCenterlineColorMode,
                     MapAppearanceSettings::XviCenterlineColorMode::ByAltitude);
            appearance.altitudeUnderlayStrengthPercent = 0;
            appearance.xviCenterlineColorMode = MapAppearanceSettings::XviCenterlineColorMode::SingleColor;
            appearance.xviCenterlineColor = QColor(190, 20, 40, 87).name(QColor::HexArgb);
            appearance.xviCenterlineWidth = 4.25;
            store.setTherionMapAppearance(appearance);
        }
        {
            SessionSettingsStore store(path);
            QCOMPARE(store.therionMapAppearance().altitudeUnderlayStrengthPercent, 0);
            QCOMPARE(store.therionMapAppearance().xviCenterlineColor, appearance.xviCenterlineColor);
            QCOMPARE(store.therionMapAppearance().xviCenterlineWidth, 4.25);
            QCOMPARE(store.therionMapAppearance().xviCenterlineColorMode,
                     MapAppearanceSettings::XviCenterlineColorMode::SingleColor);
            appearance.altitudeUnderlayStrengthPercent = 100;
            store.setTherionMapAppearance(appearance);
        }
        {
            SessionSettingsStore store(path);
            QCOMPARE(store.therionMapAppearance().altitudeUnderlayStrengthPercent, 100);
        }
        QFile::remove(path);
        InMemorySessionStore memory;
        FakeSessionStore fake;
        for (ISessionStore *store : {static_cast<ISessionStore *>(&memory),
                                     static_cast<ISessionStore *>(&fake)}) {
            QCOMPARE(store->therionMapAppearance().altitudeUnderlayStrengthPercent, 19);
            QCOMPARE(store->therionMapAppearance().xviCenterlineColorMode,
                     MapAppearanceSettings::XviCenterlineColorMode::ByAltitude);
            store->setTherionMapAppearance(appearance);
            QCOMPARE(store->therionMapAppearance().xviCenterlineColor, appearance.xviCenterlineColor);
            QCOMPARE(store->therionMapAppearance().xviCenterlineWidth, 4.25);
            auto invalid = appearance;
            invalid.xviCenterlineWidth = std::numeric_limits<double>::quiet_NaN();
            QTest::ignoreMessage(QtWarningMsg, "Invalid map appearance settings; keeping previous values.");
            store->setTherionMapAppearance(invalid);
            QCOMPARE(store->therionMapAppearance().xviCenterlineWidth, 4.25);
        }
    }

    void invalidStoredValuesAreReported()
    {
        const QString path = QStringLiteral(THERION_STUDIO_TEST_WORKSPACE)
            + QStringLiteral("/map-appearance-invalid.ini");
        QFile::remove(path);
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("settings/mapAltitudeUnderlayStrengthPercent"), 101);
            settings.setValue(QStringLiteral("settings/mapXviCenterlineColor"), QStringLiteral("invalid"));
            settings.setValue(QStringLiteral("settings/mapXviCenterlineWidth"), -3);
            settings.setValue(QStringLiteral("settings/mapXviCenterlineColorMode"), QStringLiteral("invalid"));
        }
        {
            SessionSettingsStore store(path);
            QTest::ignoreMessage(QtWarningMsg, "Invalid stored scrap underlay strength; using 19 percent.");
            QTest::ignoreMessage(QtWarningMsg, "Invalid stored XVI centerline color; using the default color.");
            QTest::ignoreMessage(QtWarningMsg, "Invalid stored XVI centerline width; using 1.8 px.");
            QTest::ignoreMessage(QtWarningMsg, "Invalid stored XVI centerline color mode; using altitude colors.");
            const auto appearance = store.therionMapAppearance();
            QCOMPARE(appearance.altitudeUnderlayStrengthPercent, 19);
            QCOMPARE(appearance.xviCenterlineColor, QStringLiteral("#82122c58"));
            QCOMPARE(appearance.xviCenterlineWidth, 1.8);
            QCOMPARE(appearance.xviCenterlineColorMode,
                     MapAppearanceSettings::XviCenterlineColorMode::ByAltitude);
            auto invalid = appearance;
            invalid.altitudeUnderlayStrengthPercent = -1;
            QTest::ignoreMessage(QtWarningMsg, "Invalid map appearance settings; keeping previous values.");
            store.setTherionMapAppearance(invalid);
        }
        QFile::remove(path);
    }

    void settingsDialogRoundTrip()
    {
        MainWindowSettingsDialog::Settings initial;
        MainWindowSettingsDialog dialog(initial);
        auto *strength = dialog.findChild<QSpinBox *>(QStringLiteral("mapAltitudeUnderlayStrengthSpin"));
        auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("mapAltitudeUnderlayCheckBox"));
        auto *width = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("xviCenterlineWidthSpin"));
        auto *color = dialog.findChild<QPushButton *>(QStringLiteral("xviCenterlineColorButton"));
        auto *colorMode = dialog.findChild<QComboBox *>(QStringLiteral("xviCenterlineColorModeCombo"));
        QVERIFY(strength && enabled && width && color && colorMode);
        QCOMPARE(colorMode->currentData().toString(), QStringLiteral("by-altitude"));
        colorMode->setCurrentIndex(colorMode->findData(QStringLiteral("single-color")));
        QCOMPARE(dialog.settings().mapAppearance.xviCenterlineColorMode,
                 MapAppearanceSettings::XviCenterlineColorMode::SingleColor);
        colorMode->setCurrentIndex(colorMode->findData(QStringLiteral("by-altitude")));
        QCOMPARE(strength->value(), 19);
        QVERIFY(!strength->isEnabled());
        enabled->setChecked(true);
        QVERIFY(strength->isEnabled());
        strength->setValue(0);
        QCOMPARE(dialog.settings().mapAppearance.altitudeUnderlayStrengthPercent, 0);
        MainWindowSettingsDialog zeroDialog(dialog.settings());
        QCOMPARE(zeroDialog.settings().mapAppearance.altitudeUnderlayStrengthPercent, 0);
        strength->setValue(100);
        enabled->setChecked(false);
        QCOMPARE(dialog.settings().mapAppearance.altitudeUnderlayStrengthPercent, 100);
        width->setValue(3.25);
        const QColor selected(200, 60, 20, 80);
        QTimer::singleShot(0, &dialog, [selected] {
            for (auto *widget : QApplication::topLevelWidgets()) {
                if (auto *picker = qobject_cast<QColorDialog *>(widget)) {
                    picker->setCurrentColor(selected);
                    picker->accept();
                }
            }
        });
        color->click();
        QCOMPARE(dialog.settings().mapAppearance.xviCenterlineColor, selected.name(QColor::HexArgb));
        QCOMPARE(dialog.settings().mapAppearance.xviCenterlineWidth, 3.25);
        MainWindowSettingsDialog reopened(dialog.settings());
        QCOMPARE(reopened.settings().mapAppearance.altitudeUnderlayStrengthPercent, 100);
        QCOMPARE(reopened.settings().mapAppearance.xviCenterlineColor, selected.name(QColor::HexArgb));
        QCOMPARE(reopened.settings().mapAppearance.xviCenterlineWidth, 3.25);
    }

    void liveXviStyleSurvivesReload()
    {
        const QString root = QStringLiteral(THERION_STUDIO_TEST_WORKSPACE);
        const QString xviPath = root + QStringLiteral("/map-appearance.xvi");
        const QString mapPath = root + QStringLiteral("/map-appearance-xvi.th2");
        QFile xviFile(xviPath);
        QVERIFY(xviFile.open(QIODevice::WriteOnly));
        xviFile.write("set XVIgrid {0 0}\nset XVIstations {\n{0 0 A}\n{100 0 B}\n}\n"
                      "set XVIshots {\n{0 0 100 0}\n}\n");
        xviFile.close();
        QFile mapFile(mapPath);
        QVERIFY(mapFile.open(QIODevice::WriteOnly));
        mapFile.write("##XTHERION## xth_me_area_adjust -100 -100 200 200\n"
                      "##MAPIAH## image_insert_v1 {format=xvi;filename=map-appearance.xvi;"
                      "xx=0;yy=0;xScale=1;yScale=1;rotationDeg=0;pivotSet=false;xviRoot=A}\n"
                      "scrap s -projection plan\npoint 0 0 station -name A\nendscrap\n");
        mapFile.close();
        QtFileSystem files;
        InMemorySessionStore prefs;
        MapAppearanceSettings appearance;
        appearance.xviCenterlineColor = QColor(255, 20, 20, 255).name(QColor::HexArgb);
        appearance.xviCenterlineWidth = 5.0;
        prefs.setTherionMapAppearance(appearance);
        MapEditorTab tab(files, prefs, CommandCatalogStore{});
        QString error;
        QVERIFY2(tab.loadFile(mapPath, &error), qPrintable(error));
        auto findXviIn = [](MapEditorTab &map) -> MapEditorXviBackgroundItem * {
            for (auto *view : map.findChildren<QGraphicsView *>()) {
                if (view->scene()) {
                    for (auto *item : view->scene()->items()) {
                        if (auto *xvi = dynamic_cast<MapEditorXviBackgroundItem *>(item)) {
                            return xvi;
                        }
                    }
                }
            }
            return nullptr;
        };
        QTRY_VERIFY(findXviIn(tab));
        auto *item = findXviIn(tab);
        QCOMPARE(item->geometryData().traverseShotLines.size(), 1);
        QCOMPARE(item->geometryData().traverseShotStations.size(), 1);
        QCOMPARE(item->geometryData().traverseShotStations.first().from, QStringLiteral("A"));
        QCOMPARE(item->geometryData().traverseShotStations.first().to, QStringLiteral("B"));
        QCOMPARE(item->centerlineColor(), QColor(appearance.xviCenterlineColor));

        ProjectIndexSnapshot index;
        ProjectStructureEntry scrapEntry;
        scrapEntry.kind = ProjectStructureEntryKind::Scrap;
        scrapEntry.sourceFile = mapPath;
        scrapEntry.namespacePath = QStringLiteral("cave");
        index.entries.append(scrapEntry);
        index.stationReferenceCandidateKeysByLookupKey.insert(
            QStringLiteral("a@cave"), QSet<QString>{QStringLiteral("a@cave")});
        index.stationReferenceCandidateKeysByLookupKey.insert(
            QStringLiteral("b@cave"), QSet<QString>{QStringLiteral("b@cave")});
        ScrapAltitudeStations stations;
        ThreeDViewerSceneModel loxScene;
        loxScene.surveys = {{1, 0, QStringLiteral("CaVe"), {}}};
        loxScene.stations = {
            {1, 1, QStringLiteral("A"), {}, {0, 0, 100}, false, true},
            {2, 1, QStringLiteral("B"), {}, {0, 0, 200}}};
        stations = scrapAltitudeStations(loxScene);
        tab.setAltitudeUnderlayData(false, index, stations);
        QCOMPARE(item->centerlineAltitudeColors().size(), 1);
        QVERIFY(item->centerlineAltitudeColors().first().hasAltitude);
        QCOMPARE(item->centerlineAltitudeColors().first().from, QColor::fromHslF(0.78, 0.62, 0.72, 1.0));
        QCOMPARE(item->centerlineAltitudeColors().first().to, QColor::fromHslF(0.06, 0.62, 0.72, 1.0));
        const auto renderCenterline = [item]() {
            const QLineF line = item->geometryData().traverseShotLines.first();
            QImage image(300, 100, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.translate(20, 50);
            const qreal scale = 260.0 / (line.p2().x() - line.p1().x());
            painter.scale(scale, scale);
            painter.translate(-line.p1());
            QStyleOptionGraphicsItem option;
            option.exposedRect = item->boundingRect();
            item->paint(&painter, &option, nullptr);
            return image;
        };
        const QImage altitudeImage = renderCenterline();
        auto uniform = appearance;
        uniform.xviCenterlineColorMode = MapAppearanceSettings::XviCenterlineColorMode::SingleColor;
        tab.setMapAppearance(uniform);
        const QImage uniformImage = renderCenterline();
        QVERIFY(altitudeImage.pixelColor(70, 50) != uniformImage.pixelColor(70, 50));
        QVERIFY(altitudeImage.pixelColor(230, 50) != uniformImage.pixelColor(230, 50));
        QVERIFY(altitudeImage.pixelColor(70, 50) != altitudeImage.pixelColor(230, 50));
        tab.setMapAppearance(appearance);
        auto *legend = tab.findChild<QWidget *>(QStringLiteral("xviAltitudeLegend"));
        auto *legendHigh = tab.findChild<QLabel *>(QStringLiteral("xviAltitudeLegendHigh"));
        QVERIFY(legend && legendHigh);
        QVERIFY(!legend->isHidden());
        QVERIFY(legendHigh->text().contains(QStringLiteral("100")));
        auto *legendFallback = tab.findChild<QLabel *>(QStringLiteral("xviAltitudeLegendFallback"));
        QVERIFY(legendFallback);
        QVERIFY(legendFallback->text().contains(QStringLiteral("1 altitude-colored, 0 fallback")));

        index.stationReferenceCandidateKeysByLookupKey.insert(
            QStringLiteral("a@cave"), QSet<QString>{QStringLiteral("a@cave"), QStringLiteral("a@other")});
        tab.setAltitudeUnderlayData(false, index, stations);
        QVERIFY(!item->centerlineAltitudeColors().first().hasAltitude);
        QVERIFY(legendFallback->text().contains(QStringLiteral("0 altitude-colored, 1 fallback")));
        auto unavailable = stations;
        unavailable.sourceState = ScrapAltitudeStations::SourceState::AmbiguousOutputs;
        unavailable.outputCount = 2;
        unavailable.heights.clear();
        unavailable.entranceHeightMetres.reset();
        tab.setAltitudeUnderlayData(false, index, unavailable);
        QVERIFY(legendFallback->text().contains(QStringLiteral("Multiple LOX outputs (2)")));
        auto singleColorAppearance = appearance;
        singleColorAppearance.xviCenterlineColorMode = MapAppearanceSettings::XviCenterlineColorMode::SingleColor;
        tab.setMapAppearance(singleColorAppearance);
        QVERIFY(legend->isHidden());
        singleColorAppearance.xviCenterlineColorMode = MapAppearanceSettings::XviCenterlineColorMode::ByAltitude;
        tab.setMapAppearance(singleColorAppearance);
        QVERIFY(!legend->isHidden());
        QCOMPARE(item->centerlineWidth(), 5.0);
        const QString source = tab.text();
        const auto *lines = item->geometryData().traverseShotLines.constData();
        const auto *tiles = item->geometryData().traverseShotTiles.constData();
        appearance.xviCenterlineColor = QColor(10, 170, 30, 255).name(QColor::HexArgb);
        appearance.xviCenterlineWidth = 7.0;
        prefs.setTherionMapAppearance(appearance);
        tab.setMapAppearance(appearance);
        QCOMPARE(findXviIn(tab), item);
        QCOMPARE(item->geometryData().traverseShotLines.constData(), lines);
        QCOMPARE(item->geometryData().traverseShotTiles.constData(), tiles);
        QCOMPARE(item->centerlineColor(), QColor(appearance.xviCenterlineColor));
        QCOMPARE(item->centerlineWidth(), 7.0);
        QCOMPARE(tab.text(), source);
        QVERIFY(!tab.isDirty());
        MapEditorTab reopened(files, prefs, CommandCatalogStore{});
        QVERIFY2(reopened.loadFile(mapPath, &error), qPrintable(error));
        QTRY_VERIFY(findXviIn(reopened));
        QCOMPARE(findXviIn(reopened)->centerlineColor(), QColor(appearance.xviCenterlineColor));
        QCOMPARE(findXviIn(reopened)->centerlineWidth(), 7.0);
        QCOMPARE(reopened.text(), source);
        QFile::remove(mapPath);
        QFile::remove(xviPath);
    }

    void xviAltitudeStrokeUsesEndpointGradientAndConfiguredFallback()
    {
        MapEditorXviLayerGeometryData geometry;
        geometry.contentBounds = QRectF(0.0, 0.0, 100.0, 40.0);
        geometry.traverseShotLines.append(QLineF(QPointF(10.0, 20.0), QPointF(90.0, 20.0)));
        geometry.traverseShotStations.append({QStringLiteral("A"), QStringLiteral("B")});
        auto *item = new MapEditorXviBackgroundItem;
        item->setGeometryData(geometry);
        const QColor fallback(18, 44, 88, 255);
        item->setCenterlineAppearance(fallback, 3.0);
        QGraphicsScene scene;
        scene.addItem(item);
        scene.setSceneRect(0.0, 0.0, 100.0, 40.0);

        auto render = [&scene]() {
            QImage image(200, 80, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            scene.render(&painter, QRectF(0.0, 0.0, 200.0, 80.0),
                         QRectF(0.0, 0.0, 100.0, 40.0), Qt::IgnoreAspectRatio);
            return image;
        };

        item->setCenterlineAltitudeColors({{QColor(20, 40, 220), QColor(240, 40, 20), true}}, true);
        const QImage gradient = render();
        const QColor low = gradient.pixelColor(24, 40);
        const QColor high = gradient.pixelColor(176, 40);
        QVERIFY(low.blue() > low.red());
        QVERIFY(high.red() > high.blue());

        item->setCenterlineAltitudeColors({{QColor(), QColor(), false}}, true);
        const QColor fallbackPixel = render().pixelColor(100, 40);
        QCOMPARE(fallbackPixel, fallback);
    }

    void liveUnderlayStrength_data()
    {
        QTest::addColumn<bool>("approximate");
        QTest::newRow("precise") << false;
        QTest::newRow("approximate") << true;
    }

    void liveUnderlayStrength()
    {
        QFETCH(bool, approximate);
        const QString path = QStringLiteral(THERION_STUDIO_TEST_WORKSPACE)
            + QStringLiteral("/map-appearance-underlay.th2");
        const QString text = QStringLiteral("scrap s -projection plan\n")
            + (approximate
                ? QStringLiteral("line wall\n0 0\n100 0\n100 100\nendline\n"
                                 "line wall\n0 0.5\n0 100\n99.5 100\nendline\n")
                : QStringLiteral("line wall -close on\n0 0\n100 0\n100 100\n0 100\nendline\n"))
            + QStringLiteral("endscrap\n");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text.toUtf8());
        file.close();
        QtFileSystem files;
        InMemorySessionStore prefs;
        MapEditorTab tab(files, prefs, CommandCatalogStore{});
        QString error;
        QVERIFY2(tab.loadFile(path, &error), qPrintable(error));
        tab.setAltitudeUnderlayData(true, {}, {});
        QGraphicsPathItem *underlay = nullptr;
        for (auto *view : tab.findChildren<QGraphicsView *>()) {
            if (!view->scene()) {
                continue;
            }
            for (auto *item : view->scene()->items()) {
                if (item->data(kMapAltitudeUnderlayRole).toBool()) {
                    underlay = dynamic_cast<QGraphicsPathItem *>(item);
                }
            }
        }
        QVERIFY(underlay);
        QCOMPARE(underlay->data(kMapAltitudeUnderlayApproximateRole).toBool(), approximate);
        auto *scene = underlay->scene();
        const auto items = scene->items();
        const auto originalPath = underlay->path();
        const auto undoOwner = tab.nextUndoOwner();
        const auto redoOwner = tab.nextRedoOwner();
        auto *undo = tab.findChild<QUndoStack *>();
        QVERIFY(undo);
        const int undoIndex = undo->index();
        const int undoCount = undo->count();
        const QColor defaultColor = underlay->brush().color();
        for (auto *item : items) {
            if (item != underlay) {
                item->setVisible(false);
            }
        }
        MapAppearanceSettings appearance;
        for (const int percent : {0, 19, 100}) {
            appearance.altitudeUnderlayStrengthPercent = percent;
            tab.setMapAppearance(appearance);
            QCOMPARE(scene->items(), items);
            QCOMPARE(underlay->path(), originalPath);
            QColor expected = defaultColor;
            expected.setAlphaF(percent / 100.0);
            QCOMPARE(underlay->brush().color(), expected);
            QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            scene->render(&painter, QRectF(0, 0, 100, 100), underlay->boundingRect(),
                          Qt::IgnoreAspectRatio);
            painter.end();
            QCOMPARE(image.pixelColor(50, 50).alpha(), qRound(percent * 2.55));
            QCOMPARE(tab.text(), text);
            QVERIFY(!tab.isDirty());
            QCOMPARE(undo->index(), undoIndex);
            QCOMPARE(undo->count(), undoCount);
            QCOMPARE(tab.nextUndoOwner(), undoOwner);
            QCOMPARE(tab.nextRedoOwner(), redoOwner);
        }
        QFile::remove(path);
    }
};

int runMapEditorAppearanceTest(int argc, char **argv)
{
    MapEditorAppearanceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorAppearanceTest.moc"
