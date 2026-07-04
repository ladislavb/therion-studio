#include "ThreeDViewerInspectorState.h"

#include "../../core/ThreeDViewerCamera.h"
#include "../../core/ThreeDViewerSceneStatistics.h"

#include <QLocale>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{

QString formatMeters(double value)
{
    return QLocale::system().toString(value, 'f', std::abs(value) < 10.0 ? 1 : 0) + QStringLiteral(" m");
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
    return radians * 180.0 / 3.14159265358979323846;
}

double degreesToRadians(double degrees)
{
    return degrees * 3.14159265358979323846 / 180.0;
}

double headingDegreesFromYaw(double yaw)
{
    return normalizeDegrees(radiansToDegrees(std::atan2(-std::cos(yaw), -std::sin(yaw))));
}

ThreeDViewerInspectorState::ThreeDViewerInspectorState(QObject *parent)
    : QObject(parent)
{
    caveLengthText_ = formatMeters(0.0);
    caveDepthText_ = formatMeters(0.0);
    cameraFacingDegrees_ = headingDegreesFromYaw(-0.85);
    cameraTiltDegrees_ = radiansToDegrees(0.45);
    cameraDistanceMeters_ = ThreeDViewerCamera::defaultDistance();
    cameraFocalLengthMm_ = ThreeDViewerCamera::defaultFocalLengthMm();
}

QString ThreeDViewerInspectorState::filePath() const
{
    return filePath_;
}

void ThreeDViewerInspectorState::setFilePath(const QString &filePath)
{
    if (filePath_ == filePath) {
        return;
    }

    filePath_ = filePath;
    emit filePathChanged();
}

int ThreeDViewerInspectorState::meshColorMode() const
{
    return int(meshColorMode_);
}

void ThreeDViewerInspectorState::setMeshColorMode(int meshColorMode)
{
    const auto newMode = meshColorMode == int(ThreeDViewerMeshColorMode::None)
        ? ThreeDViewerMeshColorMode::None
        : ThreeDViewerMeshColorMode::Altitude;
    if (meshColorMode_ == newMode) {
        return;
    }

    meshColorMode_ = newMode;
    emit meshColorModeChanged();
}

int ThreeDViewerInspectorState::backgroundMode() const
{
    return int(backgroundMode_);
}

void ThreeDViewerInspectorState::setBackgroundMode(int backgroundMode)
{
    const auto newMode = backgroundMode == int(ThreeDViewerBackgroundMode::White)
        ? ThreeDViewerBackgroundMode::White
        : ThreeDViewerBackgroundMode::Black;
    if (backgroundMode_ == newMode) {
        return;
    }

    backgroundMode_ = newMode;
    emit backgroundModeChanged();
}

bool ThreeDViewerInspectorState::measurementMode() const
{
    return measurementMode_;
}

void ThreeDViewerInspectorState::setMeasurementMode(bool measurementMode)
{
    if (measurementMode_ == measurementMode) {
        return;
    }

    measurementMode_ = measurementMode;
    emit measurementModeChanged();
}

bool ThreeDViewerInspectorState::autoRotationEnabled() const
{
    return autoRotationEnabled_;
}

void ThreeDViewerInspectorState::setAutoRotationEnabled(bool autoRotationEnabled)
{
    if (autoRotationEnabled_ == autoRotationEnabled) {
        return;
    }

    autoRotationEnabled_ = autoRotationEnabled;
    emit autoRotationEnabledChanged();
}

bool ThreeDViewerInspectorState::orthographicProjection() const
{
    return orthographicProjection_;
}

void ThreeDViewerInspectorState::setOrthographicProjection(bool orthographicProjection)
{
    if (orthographicProjection_ == orthographicProjection) {
        return;
    }

    orthographicProjection_ = orthographicProjection;
    emit orthographicProjectionChanged();
}

bool ThreeDViewerInspectorState::showBoundingBox() const
{
    return showBoundingBox_;
}

void ThreeDViewerInspectorState::setShowBoundingBox(bool showBoundingBox)
{
    if (showBoundingBox_ == showBoundingBox) {
        return;
    }

    showBoundingBox_ = showBoundingBox;
    emit showBoundingBoxChanged();
}

bool ThreeDViewerInspectorState::showHud() const
{
    return showHud_;
}

void ThreeDViewerInspectorState::setShowHud(bool showHud)
{
    if (showHud_ == showHud) {
        return;
    }

    showHud_ = showHud;
    emit showHudChanged();
}

bool ThreeDViewerInspectorState::showInfo() const
{
    return showInfo_;
}

void ThreeDViewerInspectorState::setShowInfo(bool showInfo)
{
    if (showInfo_ == showInfo) {
        return;
    }

    showInfo_ = showInfo;
    emit showInfoChanged();
}

double ThreeDViewerInspectorState::autoRotationSpeed() const
{
    return autoRotationSpeed_;
}

void ThreeDViewerInspectorState::setAutoRotationSpeed(double autoRotationSpeed)
{
    const double clampedSpeed = std::clamp(autoRotationSpeed, 5.0, 90.0);
    if (qFuzzyCompare(autoRotationSpeed_ + 1.0, clampedSpeed + 1.0)) {
        return;
    }

    autoRotationSpeed_ = clampedSpeed;
    emit autoRotationSpeedChanged();
}

double ThreeDViewerInspectorState::cameraFacingDegrees() const
{
    return cameraFacingDegrees_;
}

void ThreeDViewerInspectorState::setCameraFacingDegrees(double cameraFacingDegrees)
{
    const double normalizedFacing = normalizeDegrees(cameraFacingDegrees);
    if (qFuzzyCompare(cameraFacingDegrees_ + 1.0, normalizedFacing + 1.0)) {
        return;
    }

    cameraFacingDegrees_ = normalizedFacing;
    emit cameraFacingDegreesChanged();
}

double ThreeDViewerInspectorState::cameraTiltDegrees() const
{
    return cameraTiltDegrees_;
}

void ThreeDViewerInspectorState::setCameraTiltDegrees(double cameraTiltDegrees)
{
    const double clampedTilt = radiansToDegrees(ThreeDViewerCamera::clampPitch(degreesToRadians(cameraTiltDegrees)));
    if (qFuzzyCompare(cameraTiltDegrees_ + 1.0, clampedTilt + 1.0)) {
        return;
    }

    cameraTiltDegrees_ = clampedTilt;
    emit cameraTiltDegreesChanged();
}

double ThreeDViewerInspectorState::cameraFocalLengthMm() const
{
    return cameraFocalLengthMm_;
}

double ThreeDViewerInspectorState::cameraDistanceMeters() const
{
    return cameraDistanceMeters_;
}

void ThreeDViewerInspectorState::setCameraDistanceMeters(double cameraDistanceMeters)
{
    const double clampedDistance = ThreeDViewerCamera::clampDistance(cameraDistanceMeters);
    if (qFuzzyCompare(cameraDistanceMeters_ + 1.0, clampedDistance + 1.0)) {
        return;
    }

    cameraDistanceMeters_ = clampedDistance;
    emit cameraDistanceMetersChanged();
}

void ThreeDViewerInspectorState::setCameraFocalLengthMm(double cameraFocalLengthMm)
{
    const double clampedFocalLength = ThreeDViewerCamera::clampFocalLengthMm(cameraFocalLengthMm);
    if (qFuzzyCompare(cameraFocalLengthMm_ + 1.0, clampedFocalLength + 1.0)) {
        return;
    }

    cameraFocalLengthMm_ = clampedFocalLength;
    emit cameraFocalLengthMmChanged();
}

QString ThreeDViewerInspectorState::caveLengthText() const
{
    return caveLengthText_;
}

QString ThreeDViewerInspectorState::caveDepthText() const
{
    return caveDepthText_;
}

int ThreeDViewerInspectorState::surveyCount() const
{
    return surveyCount_;
}

int ThreeDViewerInspectorState::stationCount() const
{
    return stationCount_;
}

int ThreeDViewerInspectorState::shotCount() const
{
    return shotCount_;
}

int ThreeDViewerInspectorState::meshCount() const
{
    return meshCount_;
}

int ThreeDViewerInspectorState::surfaceCount() const
{
    return surfaceCount_;
}

void ThreeDViewerInspectorState::setSceneModel(const ThreeDViewerSceneModel &sceneModel)
{
    const ThreeDViewerSceneStatistics statistics = computeThreeDViewerSceneStatistics(sceneModel);
    const int newSurveyCount = sceneModel.surveys.size();
    const int newStationCount = sceneModel.stations.size();
    const int newShotCount = sceneModel.shots.size();
    const int newMeshCount = sceneModel.meshGroups.size();
    const int newSurfaceCount = sceneModel.surfaces.size();
    const QString newCaveLengthText = formatMeters(statistics.caveLengthMeters);
    const QString newCaveDepthText = formatMeters(statistics.caveDepthMeters);

    if (surveyCount_ == newSurveyCount && stationCount_ == newStationCount && shotCount_ == newShotCount &&
        meshCount_ == newMeshCount && surfaceCount_ == newSurfaceCount &&
        caveLengthText_ == newCaveLengthText && caveDepthText_ == newCaveDepthText) {
        return;
    }

    caveLengthText_ = newCaveLengthText;
    caveDepthText_ = newCaveDepthText;
    surveyCount_ = newSurveyCount;
    stationCount_ = newStationCount;
    shotCount_ = newShotCount;
    meshCount_ = newMeshCount;
    surfaceCount_ = newSurfaceCount;
    emit sceneMetricsChanged();
}

} // namespace TherionStudio
