#include "../src/app/three_d_viewer/ThreeDViewerViewportController.h"

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <cmath>

using namespace TherionStudio;

class ThreeDViewerViewportControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void rotatesAroundBlueAxisAndZooms();
    void rotatesByArbitraryRadians();
    void adjustsTiltByDegrees();
    void appliesExplicitCameraSettings();
    void orbitsAroundScene();
    void emitsCameraChanged();
};

void ThreeDViewerViewportControllerTest::rotatesAroundBlueAxisAndZooms()
{
    ThreeDViewerViewportController controller;

    const ThreeDViewerCameraState beforeRotate = controller.camera().state();
    controller.rotateLeft();
    const ThreeDViewerCameraState afterRotate = controller.camera().state();
    QVERIFY(std::abs(afterRotate.yaw - (beforeRotate.yaw + 5.0 * 3.14159265358979323846 / 180.0)) < 1e-12);
    QCOMPARE(afterRotate.pitch, beforeRotate.pitch);
    QCOMPARE(afterRotate.distance, beforeRotate.distance);
    QCOMPARE(afterRotate.target.x, beforeRotate.target.x);
    QCOMPARE(afterRotate.target.y, beforeRotate.target.y);
    QCOMPARE(afterRotate.target.z, beforeRotate.target.z);

    const ThreeDViewerCameraState beforeZoom = controller.camera().state();
    QVERIFY(controller.wheel(QPoint(0, 120)));
    const ThreeDViewerCameraState afterZoom = controller.camera().state();
    QVERIFY(afterZoom.distance < beforeZoom.distance);
}

void ThreeDViewerViewportControllerTest::rotatesByArbitraryRadians()
{
    ThreeDViewerViewportController controller;
    QSignalSpy spy(&controller, &ThreeDViewerViewportController::cameraChanged);

    const ThreeDViewerCameraState beforeRotate = controller.camera().state();
    controller.rotateByRadians(0.125);
    const ThreeDViewerCameraState afterRotate = controller.camera().state();

    QVERIFY(std::abs(afterRotate.yaw - (beforeRotate.yaw + 0.125)) < 1e-12);
    QCOMPARE(afterRotate.pitch, beforeRotate.pitch);
    QCOMPARE(afterRotate.distance, beforeRotate.distance);
    QCOMPARE(spy.count(), 1);

    controller.rotateByRadians(0.0);
    QCOMPARE(spy.count(), 1);
}

void ThreeDViewerViewportControllerTest::adjustsTiltByDegrees()
{
    ThreeDViewerViewportController controller;
    QSignalSpy spy(&controller, &ThreeDViewerViewportController::cameraChanged);

    const ThreeDViewerCameraState beforeTilt = controller.camera().state();
    controller.adjustTiltDegrees(5.0);
    const ThreeDViewerCameraState afterTilt = controller.camera().state();

    QVERIFY(std::abs(afterTilt.pitch - (beforeTilt.pitch + 5.0 * 3.14159265358979323846 / 180.0)) < 1e-12);
    QCOMPARE(afterTilt.yaw, beforeTilt.yaw);
    QCOMPARE(afterTilt.distance, beforeTilt.distance);
    QCOMPARE(spy.count(), 1);

    controller.adjustTiltDegrees(0.0);
    QCOMPARE(spy.count(), 1);
}

void ThreeDViewerViewportControllerTest::appliesExplicitCameraSettings()
{
    ThreeDViewerViewportController controller;
    QSignalSpy spy(&controller, &ThreeDViewerViewportController::cameraChanged);

    controller.setFacingDegrees(45.0);
    controller.setTiltDegrees(-30.0);
    controller.setDistanceMeters(240.0);
    controller.setFocalLengthMm(70.0);

    const ThreeDViewerCameraState state = controller.camera().state();
    const ThreeDViewerVec3 forward = controller.camera().forwardVector();
    const double heading = std::atan2(forward.x, forward.y) * 180.0 / 3.14159265358979323846;
    QVERIFY(std::abs(heading - 45.0) < 1e-12);
    QVERIFY(std::abs(state.pitch + 30.0 * 3.14159265358979323846 / 180.0) < 1e-12);
    QCOMPARE(state.distance, 240.0);
    QCOMPARE(state.focalLengthMm, 70.0);
    QCOMPARE(spy.count(), 4);
}

void ThreeDViewerViewportControllerTest::orbitsAroundScene()
{
    ThreeDViewerViewportController controller;

    QVERIFY(controller.mousePress(Qt::LeftButton, QPoint(10, 10)));
    QVERIFY(controller.mouseMove(QPoint(30, 45), 400));
    const ThreeDViewerCameraState orbitState = controller.camera().state();
    QVERIFY(std::abs(orbitState.yaw - (-0.85 + 20.0 * 0.008)) < 1e-12);
    QVERIFY(std::abs(orbitState.pitch - (0.45 + 35.0 * 0.008)) < 1e-12);
    QVERIFY(controller.mouseRelease(Qt::LeftButton));
}

void ThreeDViewerViewportControllerTest::emitsCameraChanged()
{
    ThreeDViewerViewportController controller;
    QSignalSpy spy(&controller, &ThreeDViewerViewportController::cameraChanged);

    controller.rotateLeft();
    controller.fitToScene(ThreeDViewerSceneModel{});
    controller.wheel(QPoint(0, 120));

    QCOMPARE(spy.count(), 3);
}

int runThreeDViewerViewportControllerTest(int argc, char **argv)
{
    ThreeDViewerViewportControllerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerViewportControllerTest.moc"
