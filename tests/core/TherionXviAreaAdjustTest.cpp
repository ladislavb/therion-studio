#include "../../src/core/TherionBackgroundMetadata.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class TherionXviAreaAdjustTest final : public QObject
{
    Q_OBJECT

private slots:
    void replacesDefaultCanvasWithPlacedXviBounds();
    void preservesExistingGeometryWhenReplacingDefaultCanvas();
    void leavesNonDefaultCanvasUntouched();
};

void TherionXviAreaAdjustTest::replacesDefaultCanvasWithPlacedXviBounds()
{
    const TherionAreaAdjust defaultArea{QRectF(0.0, 0.0, 256.0, 256.0), true};
    const QRectF xviBounds(-128.0, -256.0, 1256.0, 768.0);

    const std::optional<QRectF> result =
        initialTherionAreaAdjustForXviImport(defaultArea, QRectF(0.0, 0.0, 256.0, 256.0), xviBounds);

    QVERIFY(result.has_value());
    QCOMPARE(*result, xviBounds);
}

void TherionXviAreaAdjustTest::preservesExistingGeometryWhenReplacingDefaultCanvas()
{
    const TherionAreaAdjust defaultArea{QRectF(0.0, 0.0, 256.0, 256.0), true};
    const QRectF existingGeometryBounds(-20.0, -30.0, 80.0, 60.0);
    const QRectF xviBounds(1000.0, 2000.0, 400.0, 600.0);

    const std::optional<QRectF> result =
        initialTherionAreaAdjustForXviImport(defaultArea, existingGeometryBounds, xviBounds);

    QVERIFY(result.has_value());
    QCOMPARE(*result, existingGeometryBounds.united(xviBounds));
}

void TherionXviAreaAdjustTest::leavesNonDefaultCanvasUntouched()
{
    const TherionAreaAdjust manualArea{QRectF(-1000.0, -500.0, 4000.0, 3000.0), true};

    const std::optional<QRectF> result = initialTherionAreaAdjustForXviImport(
        manualArea, QRectF(), QRectF(-128.0, -256.0, 1256.0, 768.0));

    QVERIFY(!result.has_value());
}

int runTherionXviAreaAdjustTest(int argc, char **argv)
{
    TherionXviAreaAdjustTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionXviAreaAdjustTest.moc"
