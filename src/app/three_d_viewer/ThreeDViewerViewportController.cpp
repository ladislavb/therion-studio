#include "ThreeDViewerViewportController.h"

#include <cmath>

namespace TherionStudio
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kRotateStepDegrees = 5.0;

double degreesToRadians(double degrees)
{
    return degrees * kPi / 180.0;
}

double radiansToDegrees(double radians)
{
    return radians * 180.0 / kPi;
}

double headingDegreesToYawRadians(double headingDegrees)
{
    const double headingRadians = degreesToRadians(headingDegrees);
    return std::atan2(-std::cos(headingRadians), -std::sin(headingRadians));
}
} // namespace

ThreeDViewerViewportController::ThreeDViewerViewportController(QObject *parent)
    : QObject(parent)
{
}

const ThreeDViewerCamera &ThreeDViewerViewportController::camera() const
{
    return camera_;
}

void ThreeDViewerViewportController::fitToScene(const ThreeDViewerSceneModel &sceneModel, int viewportWidth, int viewportHeight)
{
    camera_.fitToBounds(sceneModel.bounds(), viewportWidth, viewportHeight);
    emitCameraChanged();
}

void ThreeDViewerViewportController::resetView(const ThreeDViewerSceneModel &sceneModel, int viewportWidth, int viewportHeight)
{
    camera_.resetToBounds(sceneModel.bounds());
    camera_.fitToBounds(sceneModel.bounds(), viewportWidth, viewportHeight);
    emitCameraChanged();
}

void ThreeDViewerViewportController::setViewPreset(ThreeDViewerViewPreset preset,
                                                   const ThreeDViewerSceneModel &sceneModel,
                                                   int viewportWidth,
                                                   int viewportHeight)
{
    camera_.setViewPreset(preset);
    camera_.fitToBounds(sceneModel.bounds(), viewportWidth, viewportHeight);
    emitCameraChanged();
}

void ThreeDViewerViewportController::rotateLeft()
{
    camera_.yawByRadians(degreesToRadians(kRotateStepDegrees));
    emitCameraChanged();
}

void ThreeDViewerViewportController::rotateRight()
{
    camera_.yawByRadians(-degreesToRadians(kRotateStepDegrees));
    emitCameraChanged();
}

void ThreeDViewerViewportController::rotateByRadians(double radians)
{
    if (radians == 0.0) {
        return;
    }

    camera_.yawByRadians(radians);
    emitCameraChanged();
}

void ThreeDViewerViewportController::adjustTiltDegrees(double deltaDegrees)
{
    if (deltaDegrees == 0.0) {
        return;
    }

    setTiltDegrees(radiansToDegrees(camera_.state().pitch) + deltaDegrees);
}

void ThreeDViewerViewportController::setFacingDegrees(double degrees)
{
    ThreeDViewerCameraState state = camera_.state();
    const double newYaw = std::remainder(headingDegreesToYawRadians(degrees), 2.0 * kPi);
    if (std::abs(state.yaw - newYaw) < 1e-10) {
        return;
    }
    state.yaw = newYaw;
    camera_.setState(state);
    emitCameraChanged();
}

void ThreeDViewerViewportController::setTiltDegrees(double degrees)
{
    ThreeDViewerCameraState state = camera_.state();
    const double newPitch = ThreeDViewerCamera::clampPitch(degreesToRadians(degrees));
    if (std::abs(state.pitch - newPitch) < 1e-10) {
        return;
    }
    state.pitch = newPitch;
    camera_.setState(state);
    emitCameraChanged();
}

void ThreeDViewerViewportController::setDistanceMeters(double distanceMeters)
{
    ThreeDViewerCameraState state = camera_.state();
    const double newDistance = ThreeDViewerCamera::clampDistance(distanceMeters);
    if (std::abs(state.distance - newDistance) < 1e-10) {
        return;
    }
    state.distance = newDistance;
    camera_.setState(state);
    emitCameraChanged();
}

void ThreeDViewerViewportController::setFocalLengthMm(double focalLengthMm)
{
    ThreeDViewerCameraState state = camera_.state();
    const double newFocalLength = ThreeDViewerCamera::clampFocalLengthMm(focalLengthMm);
    if (std::abs(state.focalLengthMm - newFocalLength) < 1e-10) {
        return;
    }
    state.focalLengthMm = newFocalLength;
    camera_.setState(state);
    emitCameraChanged();
}

bool ThreeDViewerViewportController::mousePress(Qt::MouseButton button, const QPoint &position)
{
    lastMousePosition_ = position;
    cameraAtPress_ = camera_.state();

    if (button == Qt::LeftButton) {
        interactionMode_ = InteractionMode::Orbit;
        return true;
    }
    if (button == Qt::RightButton || button == Qt::MiddleButton) {
        interactionMode_ = InteractionMode::Pan;
        return true;
    }
    return false;
}

bool ThreeDViewerViewportController::mouseMove(const QPoint &position, int viewportHeight)
{
    if (interactionMode_ == InteractionMode::None) {
        return false;
    }

    const QPoint delta = position - lastMousePosition_;
    camera_.setState(cameraAtPress_);
    if (interactionMode_ == InteractionMode::Orbit) {
        camera_.orbitByPixels(delta.x(), delta.y());
    } else if (interactionMode_ == InteractionMode::Pan) {
        camera_.panByPixels(delta.x(), delta.y(), viewportHeight);
    }
    emitCameraChanged();
    return true;
}

bool ThreeDViewerViewportController::mouseRelease(Qt::MouseButton button)
{
    if (button == Qt::LeftButton || button == Qt::RightButton || button == Qt::MiddleButton) {
        interactionMode_ = InteractionMode::None;
        return true;
    }
    return false;
}

bool ThreeDViewerViewportController::wheel(const QPoint &angleDelta)
{
    if (angleDelta.y() == 0) {
        return false;
    }

    const double factor = angleDelta.y() > 0 ? 0.88 : 1.0 / 0.88;
    camera_.zoomByFactor(factor);
    emitCameraChanged();
    return true;
}

void ThreeDViewerViewportController::emitCameraChanged()
{
    emit cameraChanged();
}

} // namespace TherionStudio
