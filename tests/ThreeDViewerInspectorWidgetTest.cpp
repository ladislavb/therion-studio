#include "../src/app/three_d_viewer/ThreeDViewerInspectorWidget.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QQuickItem>
#include <QScopeGuard>
#include <QtTest/QtTest>

using namespace TherionStudio;

class ThreeDViewerInspectorWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTheQmlInspectorSurface();
    void keepsCheckboxTextReadableWithDarkPalette();
};

void ThreeDViewerInspectorWidgetTest::loadsTheQmlInspectorSurface()
{
    ThreeDViewerInspectorWidget widget;
    widget.resize(640, 480);
    widget.show();

    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);
    QVERIFY(widget.rootObject() != nullptr);
}

void ThreeDViewerInspectorWidgetTest::keepsCheckboxTextReadableWithDarkPalette()
{
    const QPalette originalPalette = qApp->palette();
    auto restorePalette = qScopeGuard([&]() { qApp->setPalette(originalPalette); });
    QPalette darkPalette = originalPalette;
    const QColor textColor(QStringLiteral("#e8edf4"));
    darkPalette.setColor(QPalette::Window, QColor(QStringLiteral("#20242c")));
    darkPalette.setColor(QPalette::Base, QColor(QStringLiteral("#171b22")));
    darkPalette.setColor(QPalette::Text, textColor);
    darkPalette.setColor(QPalette::WindowText, textColor);
    darkPalette.setColor(QPalette::ButtonText, textColor);
    qApp->setPalette(darkPalette);

    ThreeDViewerInspectorWidget widget;
    widget.resize(640, 480);
    widget.show();
    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);

    QObject *checkboxText = widget.rootObject()->findChild<QObject *>(QStringLiteral("inspectorCheckBoxText"));
    QVERIFY(checkboxText != nullptr);
    QCOMPARE(checkboxText->property("color").value<QColor>(), textColor);
}

int runThreeDViewerInspectorWidgetTest(int argc, char **argv)
{
    ThreeDViewerInspectorWidgetTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerInspectorWidgetTest.moc"
