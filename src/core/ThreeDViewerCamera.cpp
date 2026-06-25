#include "ThreeDViewerCamera.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace TherionStudio
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr ThreeDViewerVec3 worldUp{0.0, 0.0, 1.0};

double vectorLengthSquared(const ThreeDViewerVec3 &value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

double vectorLength(const ThreeDViewerVec3 &value)
{
    return std::sqrt(vectorLengthSquared(value));
}

ThreeDViewerVec3 subtract(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

ThreeDViewerVec3 add(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

ThreeDViewerVec3 multiply(const ThreeDViewerVec3 &value, double factor)
{
    return {value.x * factor, value.y * factor, value.z * factor};
}

double dot(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

ThreeDViewerVec3 cross(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

ThreeDViewerVec3 normalizedOrFallback(const ThreeDViewerVec3 &value, const ThreeDViewerVec3 &fallback)
{
    const double length = vectorLength(value);
    if (length <= 0.0001) {
        return fallback;
    }
    return {value.x / length, value.y / length, value.z / length};
}

double normalizeDegrees(double value)
{
    value = std::fmod(value, 360.0);
    if (value < 0.0) {
        value += 360.0;
    }
    return value;
}

double radiansToDegrees(double radians)
{
    return radians * 180.0 / kPi;
}

double headingDegreesFromYaw(double yaw)
{
    return normalizeDegrees(radiansToDegrees(std::atan2(-std::cos(yaw), -std::sin(yaw))));
}

std::array<ThreeDViewerVec3, 8> boundsCorners(const ThreeDViewerBounds &bounds)
{
    return {{{bounds.minimum.x, bounds.minimum.y, bounds.minimum.z},
             {bounds.minimum.x, bounds.minimum.y, bounds.maximum.z},
             {bounds.minimum.x, bounds.maximum.y, bounds.minimum.z},
             {bounds.minimum.x, bounds.maximum.y, bounds.maximum.z},
             {bounds.maximum.x, bounds.minimum.y, bounds.minimum.z},
             {bounds.maximum.x, bounds.minimum.y, bounds.maximum.z},
             {bounds.maximum.x, bounds.maximum.y, bounds.minimum.z},
             {bounds.maximum.x, bounds.maximum.y, bounds.maximum.z}}};
}
} // namespace

const ThreeDViewerCameraState &ThreeDViewerCamera::state() const
{
    return state_;
}

void ThreeDViewerCamera::setState(const ThreeDViewerCameraState &state)
{
    state_ = state;
    state_.pitch = clampPitch(state_.pitch);
    state_.distance = clampDistance(state_.distance);
    state_.focalLengthMm = clampFocalLengthMm(state_.focalLengthMm);
}

void ThreeDViewerCamera::fitToBounds(const ThreeDViewerBounds &bounds)
{
    fitToBounds(bounds, 0, 0);
}

void ThreeDViewerCamera::fitToBounds(const ThreeDViewerBounds &bounds, int viewportWidth, int viewportHeight)
{
    if (!bounds.valid) {
        state_.target = {0.0, 0.0, 0.0};
        state_.distance = defaultDistance();
        return;
    }

    state_.target = {(bounds.minimum.x + bounds.maximum.x) * 0.5,
                     (bounds.minimum.y + bounds.maximum.y) * 0.5,
                     (bounds.minimum.z + bounds.maximum.z) * 0.5};

    const ThreeDViewerVec3 diagonal = subtract(bounds.maximum, bounds.minimum);
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        const double radius = std::max(1.0, vectorLength(diagonal) * 0.5);
        const double distance = radius / std::tan(fieldOfViewRadians() * 0.5) * 1.05;
        state_.distance = clampDistance(distance);
        return;
    }

    const double aspectRatio = std::max(0.1, double(viewportWidth) / double(viewportHeight));
    const double tanHalfVerticalFov = std::tan(fieldOfViewRadians() * 0.5);
    const double tanHalfHorizontalFov = tanHalfVerticalFov * aspectRatio;
    constexpr double usableFrame = 0.9;

    const ThreeDViewerVec3 forward = forwardVector();
    const ThreeDViewerVec3 right = rightVector();
    const ThreeDViewerVec3 up = upVector();

    double distance = minDistance();
    for (const ThreeDViewerVec3 &corner : boundsCorners(bounds)) {
        const ThreeDViewerVec3 offset = subtract(corner, state_.target);
        const double cameraX = std::abs(dot(offset, right));
        const double cameraY = std::abs(dot(offset, up));
        const double cameraZ = dot(offset, forward);
        distance = std::max(distance, cameraX / (tanHalfHorizontalFov * usableFrame) - cameraZ);
        distance = std::max(distance, cameraY / (tanHalfVerticalFov * usableFrame) - cameraZ);
        distance = std::max(distance, 1.0 - cameraZ);
    }

    state_.distance = clampDistance(distance);
}

void ThreeDViewerCamera::resetToBounds(const ThreeDViewerBounds &bounds)
{
    state_.yaw = -0.85;
    state_.pitch = 0.45;
    fitToBounds(bounds);
}

void ThreeDViewerCamera::setViewPreset(ThreeDViewerViewPreset preset)
{
    switch (preset) {
    case ThreeDViewerViewPreset::Isometric:
        state_.yaw = -0.85;
        state_.pitch = 0.45;
        break;
    case ThreeDViewerViewPreset::Top:
        state_.pitch = kPi * 0.5;
        break;
    case ThreeDViewerViewPreset::Side:
        state_.pitch = 0.0;
        break;
    }
    state_.pitch = clampPitch(state_.pitch);
}

void ThreeDViewerCamera::yawByRadians(double deltaRadians)
{
    state_.yaw = std::remainder(state_.yaw + deltaRadians, 2.0 * kPi);
}

void ThreeDViewerCamera::orbitByPixels(double deltaX, double deltaY, double yawScale, double pitchScale)
{
    state_.yaw += deltaX * yawScale;
    state_.pitch = clampPitch(state_.pitch + deltaY * pitchScale);
}

void ThreeDViewerCamera::panByPixels(double deltaX, double deltaY, int viewportHeight)
{
    if (viewportHeight <= 0) {
        return;
    }

    const ThreeDViewerVec3 right = rightVector();
    const ThreeDViewerVec3 up = upVector();
    const double scale = screenPanScale(viewportHeight);
    state_.target = subtract(state_.target,
                             add(multiply(right, deltaX * scale), multiply(up, -deltaY * scale)));
}

void ThreeDViewerCamera::zoomByFactor(double factor)
{
    if (!(factor > 0.0)) {
        return;
    }
    state_.distance = clampDistance(state_.distance * factor);
}

ThreeDViewerVec3 ThreeDViewerCamera::position() const
{
    const double cosPitch = std::cos(state_.pitch);
    return add(state_.target,
               {state_.distance * cosPitch * std::cos(state_.yaw),
                state_.distance * cosPitch * std::sin(state_.yaw),
                state_.distance * std::sin(state_.pitch)});
}

ThreeDViewerVec3 ThreeDViewerCamera::forwardVector() const
{
    const ThreeDViewerVec3 forward = subtract(state_.target, position());
    return normalizedOrFallback(forward, {0.0, 0.0, -1.0});
}

ThreeDViewerVec3 ThreeDViewerCamera::rightVector() const
{
    return {-std::sin(state_.yaw), std::cos(state_.yaw), 0.0};
}

ThreeDViewerVec3 ThreeDViewerCamera::upVector() const
{
    const ThreeDViewerVec3 forward = forwardVector();
    const ThreeDViewerVec3 right = rightVector();
    return normalizedOrFallback(cross(right, forward), worldUp);
}

double ThreeDViewerCamera::headingDegrees() const
{
    return headingDegreesFromYaw(state_.yaw);
}

double ThreeDViewerCamera::fieldOfViewRadians() const
{
    return fieldOfViewRadiansForFocalLengthMm(state_.focalLengthMm);
}

double ThreeDViewerCamera::screenPanScale(int viewportHeight) const
{
    if (viewportHeight <= 0) {
        return 0.0;
    }

    return 2.0 * state_.distance * std::tan(fieldOfViewRadians() * 0.5) / double(viewportHeight);
}

double ThreeDViewerCamera::clampDistance(double distance)
{
    return std::clamp(distance, minDistance(), maxDistance());
}

double ThreeDViewerCamera::clampPitch(double pitch)
{
    constexpr double verticalLimit = kPi * 0.5;
    return std::clamp(pitch, -verticalLimit, verticalLimit);
}

double ThreeDViewerCamera::clampFocalLengthMm(double focalLengthMm)
{
    return std::clamp(focalLengthMm, 10.0, 80.0);
}

double ThreeDViewerCamera::fieldOfViewRadiansForFocalLengthMm(double focalLengthMm)
{
    constexpr double sensorHeightMm = 24.0;
    const double clampedFocalLength = clampFocalLengthMm(focalLengthMm);
    return 2.0 * std::atan(sensorHeightMm / (2.0 * clampedFocalLength));
}

} // namespace TherionStudio
