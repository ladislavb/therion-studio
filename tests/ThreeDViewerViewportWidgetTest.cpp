#include "../src/app/three_d_viewer/ThreeDViewerViewportWidget.h"
#include "../src/app/three_d_viewer/ThreeDViewerViewportItem.h"
#include "../src/app/three_d_viewer/ThreeDViewerTab.h"

#include <QtTest/QtTest>
#include <QApplication>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QToolButton>
#include <QVBoxLayout>

using namespace TherionStudio;

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
};

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
