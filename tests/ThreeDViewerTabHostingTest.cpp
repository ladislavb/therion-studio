#include "../src/app/three_d_viewer/ThreeDViewerInspectorWidget.h"
#include "../src/app/three_d_viewer/ThreeDViewerTab.h"
#include "../src/app/three_d_viewer/ThreeDViewerViewportWidget.h"

#include "../src/core/ThreeDViewerSceneModel.h"

#include <QApplication>
#include <QQuickWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
ThreeDViewerSceneModel syntheticScene()
{
    ThreeDViewerSceneModel model;
    ThreeDViewerSurvey survey;
    survey.id = 1;
    survey.name = QStringLiteral("test");
    model.surveys.append(survey);
    for (quint32 index = 0; index < 40; ++index) {
        ThreeDViewerStation station;
        station.id = index + 1;
        station.surveyId = 1;
        station.name = QStringLiteral("s%1").arg(index);
        station.position = ThreeDViewerVec3{double(index) * 3.0, double(index % 7) * 2.0, -double(index) * 0.5};
        model.stations.append(station);
    }
    for (quint32 index = 1; index < 40; ++index) {
        ThreeDViewerShot shot;
        shot.fromStationId = index;
        shot.toStationId = index + 1;
        shot.surveyId = 1;
        model.shots.append(shot);
    }
    return model;
}
} // namespace

class ThreeDViewerTabHostingTest : public QObject
{
    Q_OBJECT

private slots:
    // Regression: opening a 3D tab inside a visible QTabWidget and loading a scene
    // aborted with QQuickWidget's "!d->offscreenWindow->handle()" assertion.
    void loadedSceneSurvivesVisibleTabHost();
    void secondQuickWidgetInVisibleHostSurvives();
    void viewportAloneInVisibleHostSurvives();
    void tabWithoutSceneInVisibleHostSurvives();
};

void ThreeDViewerTabHostingTest::loadedSceneSurvivesVisibleTabHost()
{
    QTabWidget host;
    host.resize(1200, 800);
    auto *tab = new ThreeDViewerTab();
    host.addTab(tab, QStringLiteral("3D"));
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    tab->setSceneModel(syntheticScene(), QStringLiteral("synthetic.lox"));
    QTest::qWait(50);
    QTest::qWait(50);

    QVERIFY(tab->filePath().endsWith(QStringLiteral("synthetic.lox")));
}

void ThreeDViewerTabHostingTest::viewportAloneInVisibleHostSurvives()
{
    QTabWidget host;
    host.resize(1200, 800);
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *viewport = new ThreeDViewerViewportWidget(page);
    layout->addWidget(viewport, 1);
    host.addTab(page, QStringLiteral("3D"));
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QTRY_COMPARE(viewport->status(), QQuickWidget::Ready);
}

void ThreeDViewerTabHostingTest::tabWithoutSceneInVisibleHostSurvives()
{
    QTabWidget host;
    host.resize(1200, 800);
    auto *tab = new ThreeDViewerTab();
    host.addTab(tab, QStringLiteral("3D"));
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QTest::qWait(100);
}

void ThreeDViewerTabHostingTest::secondQuickWidgetInVisibleHostSurvives()
{
    QTabWidget host;
    host.resize(1200, 800);
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *first = new ThreeDViewerInspectorWidget(page);
    auto *second = new ThreeDViewerInspectorWidget(page);
    layout->addWidget(first, 1);
    layout->addWidget(second, 1);
    host.addTab(page, QStringLiteral("3D"));
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QTRY_COMPARE(first->status(), QQuickWidget::Ready);
    QTRY_COMPARE(second->status(), QQuickWidget::Ready);
}

int runThreeDViewerTabHostingTest(int argc, char **argv)
{
    ThreeDViewerTabHostingTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerTabHostingTest.moc"
