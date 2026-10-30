#include "../src/app/three_d_viewer/ThreeDViewerViewportWidget.h"
#include "../src/app/three_d_viewer/ThreeDViewerViewportItem.h"
#include "../src/app/three_d_viewer/ThreeDViewerTab.h"
#include "../src/core/TherionScrap3D.h"

#include <QtTest/QtTest>
#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>

using namespace TherionStudio;

namespace
{
class TestViewportItem : public ThreeDViewerViewportItem
{
public:
    using ThreeDViewerViewportItem::updatePaintNode;
};
}

class ThreeDViewerViewportWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTheQmlViewportSurface();
    void escapeRequestsMeasurementModeExit();
    void escapeLeavesTabMeasurementModeAfterToolbarActivation();
    void arrowKeysNavigateWhenTabHasFocus();
    void arrowKeysNavigateWhenWindowFocusIsOutsideCanvas();
    void viewPresetReturnsFocusForArrowNavigation();
    void toolbarViewPresetReturnsFocusForArrowNavigation();
    void grabImageReturnsRequestedPixelSize();
    void batchesWallSegments_data();
    void batchesWallSegments();
    void rendersBatchedWallColors_data();
    void rendersBatchedWallColors();
    void rendersObjectsWithoutPassageFill_data();
    void rendersObjectsWithoutPassageFill();
    void rendersThinLinesAndPointSymbols();
};

void ThreeDViewerViewportWidgetTest::rendersThinLinesAndPointSymbols()
{
    ThreeDViewerSceneModel scene;
    ThreeDViewerScrapPolyline wall;
    wall.type = QStringLiteral("wall");
    wall.points = {{-10, -5, 0}, {10, -5, 0}};
    wall.strokeWidth = 3;
    wall.hasStrokeColor = true;
    wall.strokeColorRgba = QColor(Qt::red).rgba();
    scene.scrapLinework.append(wall);
    auto slope = wall;
    slope.type = QStringLiteral("slope");
    slope.points = {{-10, 5, 0}, {10, 5, 0}};
    slope.strokeWidth = 1.35;
    slope.strokeColorRgba = QColor(Qt::green).rgba();
    scene.scrapLinework.append(slope);
    ThreeDViewerScrapPoint point;
    point.position = {0, 0, 0};
    point.hasStrokeColor = true;
    point.strokeColorRgba = QColor(Qt::blue).rgba();
    point.size = 16;
    point.symbolStrokes = {{{-8, 0}, {8, 0}}, {{0, -8}, {0, 8}}};
    scene.scrapPoints.append(point);
    ThreeDViewerViewportWidget widget;
    widget.resize(640, 480);
    widget.show();
    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);
    if (widget.quickWindow()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        QSKIP("Custom scene-graph geometry requires a GPU backend; run this case with QT_QPA_PLATFORM=xcb.");
    }
    widget.setSceneOverlayVisibility(false, false, false);
    widget.setLayerVisibility({false, false, false, true, false});
    widget.setMeshColorMode(ThreeDViewerMeshColorMode::None);
    widget.setSceneModel(scene);
    widget.setOrthographicProjection(true);
    widget.setViewPreset(ThreeDViewerViewPreset::Top);
    widget.fitToScene();
    QImage image;
    widget.grabImage(QSize(640, 480), [&image](const QImage &result) { image = result; });
    QTRY_VERIFY(!image.isNull());
    int redPixels = 0;
    int greenPixels = 0;
    int bluePixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto color = image.pixelColor(x, y);
            redPixels += color.red() > 100 && color.green() < 30 && color.blue() < 30;
            greenPixels += color.green() > 100 && color.red() < 30 && color.blue() < 30;
            bluePixels += color.blue() > 100 && color.red() < 30 && color.green() < 30;
        }
    }
    QVERIFY(redPixels > 100);
    QVERIFY(greenPixels > 50);
    QVERIFY2(redPixels > greenPixels * 1.5,
             qPrintable(QStringLiteral("wall %1 pixels, slope %2 pixels").arg(redPixels).arg(greenPixels)));
    QVERIFY(bluePixels > 10);
}

void ThreeDViewerViewportWidgetTest::rendersObjectsWithoutPassageFill_data()
{
    QTest::addColumn<int>("colorMode");
    QTest::addColumn<bool>("topView");
    QTest::newRow("altitude") << int(ThreeDViewerMeshColorMode::Altitude) << true;
    QTest::newRow("single-color") << int(ThreeDViewerMeshColorMode::None) << true;
    QTest::newRow("altitude-inclined") << int(ThreeDViewerMeshColorMode::Altitude) << false;
    QTest::newRow("single-color-inclined") << int(ThreeDViewerMeshColorMode::None) << false;
}

void ThreeDViewerViewportWidgetTest::rendersObjectsWithoutPassageFill()
{
    QFETCH(int, colorMode);
    QFETCH(bool, topView);
    const auto projection = Th2GeometryProjection::fromText(QStringLiteral(
        "scrap a -projection plan\npoint -10 0 station -name A\npoint 10 0 station -name B\n"
        "line wall -close on\n-10 -10\n10 -10\n10 10\n-10 10\nendline\n"
        "line border\n-8 -5\n8 -5\nendline\npoint 5 5 stalactite\nendscrap\n"));
    const auto model = TherionScrap3D::projectScrap(projection, QStringLiteral("a"), {},
        {{QStringLiteral("A"), {-10, 0, -2}}, {QStringLiteral("B"), {10, 0, 2}}},
        0, 0, {{}, false, true});
    ThreeDViewerSceneModel scene;
    scene.scrapLinework = model.linework;
    scene.scrapPoints = model.pointFeatures;
    QVERIFY(model.meshGroup.triangles.isEmpty());
    QVERIFY(!scene.isEmpty());
    TestViewportItem item;
    item.setSize(QSizeF(640, 480));
    item.setSceneOverlayVisibility(false, false, false);
    item.setLayerVisibility({false, false, false, true, false});
    item.setSceneModel(scene);
    std::unique_ptr<QSGNode> root(item.updatePaintNode(nullptr, nullptr));
    QCOMPARE(root->childCount(), 2);
    for (auto *node = root->firstChild()->nextSibling(); node != nullptr; node = node->nextSibling()) {
        const auto *geometryNode = static_cast<QSGGeometryNode *>(node);
        QCOMPARE(geometryNode->geometry()->drawingMode(), unsigned(QSGGeometry::DrawLines));
    }
    item.setLayerVisibility({false, false, false, false, false});
    root.reset(item.updatePaintNode(nullptr, nullptr));
    QCOMPARE(root->childCount(), 1);

    ThreeDViewerViewportWidget widget;
    widget.resize(640, 480);
    widget.show();
    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);
    if (widget.quickWindow()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        QSKIP("Custom scene-graph geometry requires a GPU backend; run this case with QT_QPA_PLATFORM=xcb.");
    }
    widget.setSceneOverlayVisibility(false, false, false);
    widget.setLayerVisibility({false, false, false, true, false});
    widget.setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(colorMode));
    widget.setSceneModel(scene);
    widget.setViewPreset(topView ? ThreeDViewerViewPreset::Top : ThreeDViewerViewPreset::Isometric);
    widget.fitToScene();
    QImage image;
    widget.grabImage(QSize(640, 480), [&image](const QImage &result) { image = result; });
    QTRY_VERIFY(!image.isNull());
    int coloredPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            coloredPixels += color.value() > 20;
        }
    }
    QVERIFY2(coloredPixels > 100, qPrintable(QString::number(coloredPixels)));
    QVERIFY2(coloredPixels < 10000, qPrintable(QString::number(coloredPixels)));
    QVERIFY(image.pixelColor(320, 240).value() < 10);
}

void ThreeDViewerViewportWidgetTest::batchesWallSegments_data()
{
    QTest::addColumn<int>("colorMode");
    QTest::addColumn<int>("lineCount");
    QTest::newRow("altitude") << int(ThreeDViewerMeshColorMode::Altitude) << 1;
    QTest::newRow("th2-style") << int(ThreeDViewerMeshColorMode::None) << 1;
    QTest::newRow("altitude-many-lines") << int(ThreeDViewerMeshColorMode::Altitude) << 1000;
    QTest::newRow("th2-style-many-lines") << int(ThreeDViewerMeshColorMode::None) << 1000;
}

void ThreeDViewerViewportWidgetTest::batchesWallSegments()
{
    QFETCH(int, colorMode);
    QFETCH(int, lineCount);
    constexpr int segmentCount = 20000;
    ThreeDViewerSceneModel scene;
    for (int line = 0; line < lineCount; ++line) {
        ThreeDViewerScrapPolyline wall;
        wall.type = QStringLiteral("wall");
        wall.hasStrokeColor = true;
        wall.strokeColorRgba = QColor(13, 125, 250).rgba();
        for (int index = 0; index <= segmentCount / lineCount; ++index) {
            const int position = line * (segmentCount / lineCount) + index;
            wall.points.append({double(position) / 100.0, 0.0, double(position) / 1000.0});
        }
        scene.scrapLinework.append(wall);
    }

    TestViewportItem item;
    item.setSize(QSizeF(800, 600));
    item.setSceneOverlayVisibility(false, false, false);
    item.setLayerVisibility({false, false, false, true, false});
    item.setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(colorMode));
    item.setSceneModel(scene);
    QElapsedTimer timer;
    timer.start();
    std::unique_ptr<QSGNode> root(item.updatePaintNode(nullptr, nullptr));
    qInfo() << "20000 wall segments:" << timer.nsecsElapsed() / 1000000.0
            << "ms," << root->childCount() << "nodes";
    QCOMPARE(root->childCount(), 2);
    auto *wallNode = static_cast<QSGGeometryNode *>(root->lastChild());
    QCOMPARE(wallNode->geometry()->vertexCount(), segmentCount * 2);
    const auto *colors = wallNode->geometry()->vertexDataAsColoredPoint2D();
    if (colorMode == int(ThreeDViewerMeshColorMode::Altitude)) {
        QVERIFY(colors[0].r != colors[segmentCount * 2 - 1].r
                || colors[0].b != colors[segmentCount * 2 - 1].b);
    } else {
        QCOMPARE(colors[0].r, 13);
        QCOMPARE(colors[0].g, 125);
        QCOMPARE(colors[0].b, 250);
        QCOMPARE(colors[segmentCount * 2 - 1].b, 250);
    }
    item.rollLeft();
    root.reset(item.updatePaintNode(nullptr, nullptr));
    QCOMPARE(root->childCount(), 2);
}

void ThreeDViewerViewportWidgetTest::rendersBatchedWallColors_data()
{
    QTest::addColumn<int>("colorMode");
    QTest::newRow("altitude") << int(ThreeDViewerMeshColorMode::Altitude);
    QTest::newRow("th2-style") << int(ThreeDViewerMeshColorMode::None);
}

void ThreeDViewerViewportWidgetTest::rendersBatchedWallColors()
{
    QFETCH(int, colorMode);
    ThreeDViewerSceneModel scene;
    ThreeDViewerScrapPolyline wall;
    wall.type = QStringLiteral("wall");
    wall.points = {{0.0, 0.0, 0.0}, {10.0, 0.0, 10.0}, {10.0, 10.0, 10.0}, {0.0, 10.0, 0.0}};
    wall.closed = true;
    wall.hasStrokeColor = true;
    wall.strokeColorRgba = QColor(Qt::green).rgba();
    wall.strokeWidth = 3.0;
    scene.scrapLinework.append(wall);

    ThreeDViewerViewportWidget widget;
    widget.resize(640, 480);
    widget.show();
    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);
    if (widget.quickWindow()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        QSKIP("Custom scene-graph geometry requires a GPU backend; run this case with QT_QPA_PLATFORM=xcb.");
    }
    widget.setSceneOverlayVisibility(false, false, false);
    widget.setLayerVisibility({false, false, false, true, false});
    widget.setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(colorMode));
    widget.setSceneModel(scene);
    QImage image;
    widget.grabImage(QSize(640, 480), [&image](const QImage &rendered) { image = rendered; });
    QTRY_VERIFY(!image.isNull());
    int coloredPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (colorMode == int(ThreeDViewerMeshColorMode::None)) {
                coloredPixels += color.green() > color.red() + 30 && color.green() > color.blue() + 30;
            } else {
                coloredPixels += color.saturation() > 100 && color.value() > 80;
            }
        }
    }
    QVERIFY2(coloredPixels > 100, qPrintable(QStringLiteral("Only %1 colored wall pixels").arg(coloredPixels)));
}

void ThreeDViewerViewportWidgetTest::loadsTheQmlViewportSurface()
{
    ThreeDViewerViewportWidget widget;
    widget.resize(640, 480);
    widget.show();

    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);
    QVERIFY(widget.rootObject() != nullptr);
}

void ThreeDViewerViewportWidgetTest::escapeRequestsMeasurementModeExit()
{
    ThreeDViewerViewportItem item;
    QSignalSpy spy(&item, &ThreeDViewerViewportItem::measurementModeExitRequested);

    item.setMeasurementMode(true);

    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(&item, &event);

    QCOMPARE(spy.count(), 1);
    QVERIFY(event.isAccepted());
}

void ThreeDViewerViewportWidgetTest::escapeLeavesTabMeasurementModeAfterToolbarActivation()
{
    ThreeDViewerTab tab;
    tab.resize(800, 600);
    tab.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tab));

    auto *viewport = tab.findChild<ThreeDViewerViewportWidget *>();
    QVERIFY(viewport != nullptr);

    tab.setMeasurementMode(true);
    QVERIFY(tab.measurementMode());
    QTRY_COMPARE(QApplication::focusWidget(), viewport);

    QTest::keyClick(viewport, Qt::Key_Escape);

    QTRY_VERIFY(!tab.measurementMode());
}

void ThreeDViewerViewportWidgetTest::arrowKeysNavigateWhenTabHasFocus()
{
    ThreeDViewerTab tab;
    tab.resize(800, 600);
    tab.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tab));

    auto *viewport = tab.findChild<ThreeDViewerViewportWidget *>();
    QVERIFY(viewport != nullptr);
    QTRY_COMPARE(viewport->status(), QQuickWidget::Ready);

    QSignalSpy spy(viewport, &ThreeDViewerViewportWidget::cameraSettingsChanged);

    QTest::keyClick(&tab, Qt::Key_Left);

    QTRY_VERIFY(spy.count() > 0);
}

void ThreeDViewerViewportWidgetTest::arrowKeysNavigateWhenWindowFocusIsOutsideCanvas()
{
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *button = new QToolButton(&window);
    auto *tab = new ThreeDViewerTab(&window);
    button->setText(QStringLiteral("Focus"));
    layout->addWidget(button);
    layout->addWidget(tab);
    window.resize(800, 640);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *viewport = tab->findChild<ThreeDViewerViewportWidget *>();
    QVERIFY(viewport != nullptr);
    QTRY_COMPARE(viewport->status(), QQuickWidget::Ready);

    button->setFocus(Qt::MouseFocusReason);
    QCOMPARE(QApplication::focusWidget(), button);

    QSignalSpy spy(viewport, &ThreeDViewerViewportWidget::cameraSettingsChanged);

    QTest::keyClick(button, Qt::Key_Left);

    QTRY_VERIFY(spy.count() > 0);
}

void ThreeDViewerViewportWidgetTest::viewPresetReturnsFocusForArrowNavigation()
{
    ThreeDViewerTab tab;
    tab.resize(800, 600);
    tab.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tab));

    auto *viewport = tab.findChild<ThreeDViewerViewportWidget *>();
    QVERIFY(viewport != nullptr);
    QTRY_COMPARE(viewport->status(), QQuickWidget::Ready);

    tab.setTopView();
    QTRY_COMPARE(QApplication::focusWidget(), viewport);

    QSignalSpy spy(viewport, &ThreeDViewerViewportWidget::cameraSettingsChanged);

    QTest::keyClick(viewport, Qt::Key_Left);

    QTRY_VERIFY(spy.count() > 0);
}

void ThreeDViewerViewportWidgetTest::toolbarViewPresetReturnsFocusForArrowNavigation()
{
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *button = new QToolButton(&window);
    auto *tab = new ThreeDViewerTab(&window);
    button->setText(QStringLiteral("Top"));
    layout->addWidget(button);
    layout->addWidget(tab);
    window.resize(800, 640);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *viewport = tab->findChild<ThreeDViewerViewportWidget *>();
    QVERIFY(viewport != nullptr);
    QTRY_COMPARE(viewport->status(), QQuickWidget::Ready);

    connect(button, &QToolButton::clicked, tab, &ThreeDViewerTab::setTopView);
    button->setFocus(Qt::MouseFocusReason);
    QCOMPARE(QApplication::focusWidget(), button);

    QTest::mouseClick(button, Qt::LeftButton);
    QTRY_COMPARE(QApplication::focusWidget(), viewport);

    QSignalSpy spy(viewport, &ThreeDViewerViewportWidget::cameraSettingsChanged);

    QTest::keyClick(viewport, Qt::Key_Left);

    QTRY_VERIFY(spy.count() > 0);
}

void ThreeDViewerViewportWidgetTest::grabImageReturnsRequestedPixelSize()
{
    ThreeDViewerViewportWidget widget;
    widget.resize(320, 180);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);

    QImage grabbedImage;
    widget.grabImage(QSize(640, 360), [&grabbedImage](const QImage &image) {
        grabbedImage = image;
    });

    QTRY_VERIFY(!grabbedImage.isNull());
    QCOMPARE(grabbedImage.size(), QSize(640, 360));
    QCOMPARE(grabbedImage.devicePixelRatio(), 1.0);
}

int runThreeDViewerViewportWidgetTest(int argc, char **argv)
{
    ThreeDViewerViewportWidgetTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerViewportWidgetTest.moc"
