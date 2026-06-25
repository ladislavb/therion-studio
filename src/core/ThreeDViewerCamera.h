#pragma once

#include "ThreeDViewerSceneModel.h"

namespace TherionStudio
{

enum class ThreeDViewerViewPreset
{
    Isometric,
    Top,
    Side,
};

struct ThreeDViewerCameraState
{
    ThreeDViewerVec3 target;
    double yaw = -0.85;
    double pitch = 0.45;
    double distance = 120.0;
    double focalLengthMm = 35.0;
};

class ThreeDViewerCamera
{
public:
    const ThreeDViewerCameraState &state() const;
    void setState(const ThreeDViewerCameraState &state);

    void fitToBounds(const ThreeDViewerBounds &bounds);
    void fitToBounds(const ThreeDViewerBounds &bounds, int viewportWidth, int viewportHeight);
    void resetToBounds(const ThreeDViewerBounds &bounds);
    void setViewPreset(ThreeDViewerViewPreset preset);

    void yawByRadians(double deltaRadians);
    void orbitByPixels(double deltaX,
                       double deltaY,
                       double yawScale = 0.008,
                       double pitchScale = 0.008);
    void panByPixels(double deltaX,
                     double deltaY,
                     int viewportHeight);
    void zoomByFactor(double factor);

    ThreeDViewerVec3 position() const;
    ThreeDViewerVec3 forwardVector() const;
    ThreeDViewerVec3 rightVector() const;
    ThreeDViewerVec3 upVector() const;
    double headingDegrees() const;

    double fieldOfViewRadians() const;
    double screenPanScale(int viewportHeight) const;

    static double clampDistance(double distance);
    static double clampPitch(double pitch);
    static double clampFocalLengthMm(double focalLengthMm);
    static double fieldOfViewRadiansForFocalLengthMm(double focalLengthMm);
    static constexpr double defaultDistance() { return 120.0; }
    static constexpr double defaultFocalLengthMm() { return 35.0; }

private:
    static constexpr double minDistance()
    {
        return 4.0;
    }

    static constexpr double maxDistance()
    {
        return 100000.0;
    }

    ThreeDViewerCameraState state_;
};

} // namespace TherionStudio
