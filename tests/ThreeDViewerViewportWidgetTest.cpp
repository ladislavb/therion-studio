#include "../src/app/three_d_viewer/ThreeDViewerViewportWidget.h"
#include "../src/app/three_d_viewer/ThreeDViewerViewportItem.h"
#include "../src/app/three_d_viewer/ThreeDViewerTab.h"

#include <QtTest/QtTest>
#include <QApplication>
#include <QKeyEvent>
#include <QSignalSpy>

using namespace TherionStudio;

class ThreeDViewerViewportWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTheQmlViewportSurface();
    void escapeRequestsMeasurementModeExit();
    void escapeLeavesTabMeasurementModeAfterToolbarActivation();
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

int runThreeDViewerViewportWidgetTest(int argc, char **argv)
{
    ThreeDViewerViewportWidgetTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerViewportWidgetTest.moc"
