#pragma once

#include "../../core/ThreeDViewerSceneModel.h"
#include "ThreeDViewerBackgroundMode.h"
#include "ThreeDViewerMeshColorMode.h"

#include <QObject>
#include <QString>

namespace TherionStudio
{

class ThreeDViewerInspectorState final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
    Q_PROPERTY(int meshColorMode READ meshColorMode WRITE setMeshColorMode NOTIFY meshColorModeChanged)
    Q_PROPERTY(int backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY backgroundModeChanged)
    Q_PROPERTY(bool measurementMode READ measurementMode WRITE setMeasurementMode NOTIFY measurementModeChanged)
    Q_PROPERTY(bool autoRotationEnabled READ autoRotationEnabled WRITE setAutoRotationEnabled NOTIFY autoRotationEnabledChanged)
    Q_PROPERTY(bool orthographicProjection READ orthographicProjection WRITE setOrthographicProjection NOTIFY orthographicProjectionChanged)
    Q_PROPERTY(bool showBoundingBox READ showBoundingBox WRITE setShowBoundingBox NOTIFY showBoundingBoxChanged)
    Q_PROPERTY(bool showHud READ showHud WRITE setShowHud NOTIFY showHudChanged)
    Q_PROPERTY(bool showInfo READ showInfo WRITE setShowInfo NOTIFY showInfoChanged)
    Q_PROPERTY(double autoRotationSpeed READ autoRotationSpeed WRITE setAutoRotationSpeed NOTIFY autoRotationSpeedChanged)
    Q_PROPERTY(double cameraFacingDegrees READ cameraFacingDegrees WRITE setCameraFacingDegrees NOTIFY cameraFacingDegreesChanged)
    Q_PROPERTY(double cameraTiltDegrees READ cameraTiltDegrees WRITE setCameraTiltDegrees NOTIFY cameraTiltDegreesChanged)
    Q_PROPERTY(double cameraDistanceMeters READ cameraDistanceMeters WRITE setCameraDistanceMeters NOTIFY cameraDistanceMetersChanged)
    Q_PROPERTY(double cameraFocalLengthMm READ cameraFocalLengthMm WRITE setCameraFocalLengthMm NOTIFY cameraFocalLengthMmChanged)
    Q_PROPERTY(QString caveLengthText READ caveLengthText NOTIFY sceneMetricsChanged)
    Q_PROPERTY(QString caveDepthText READ caveDepthText NOTIFY sceneMetricsChanged)
    Q_PROPERTY(int surveyCount READ surveyCount NOTIFY sceneMetricsChanged)
    Q_PROPERTY(int stationCount READ stationCount NOTIFY sceneMetricsChanged)
    Q_PROPERTY(int shotCount READ shotCount NOTIFY sceneMetricsChanged)
    Q_PROPERTY(int meshCount READ meshCount NOTIFY sceneMetricsChanged)
    Q_PROPERTY(int surfaceCount READ surfaceCount NOTIFY sceneMetricsChanged)

public:
    explicit ThreeDViewerInspectorState(QObject *parent = nullptr);

    QString filePath() const;
    void setFilePath(const QString &filePath);

    int meshColorMode() const;
    void setMeshColorMode(int meshColorMode);

    int backgroundMode() const;
    void setBackgroundMode(int backgroundMode);

    bool measurementMode() const;
    void setMeasurementMode(bool measurementMode);

    bool autoRotationEnabled() const;
    void setAutoRotationEnabled(bool autoRotationEnabled);

    bool orthographicProjection() const;
    void setOrthographicProjection(bool orthographicProjection);

    bool showBoundingBox() const;
    void setShowBoundingBox(bool showBoundingBox);

    bool showHud() const;
    void setShowHud(bool showHud);

    bool showInfo() const;
    void setShowInfo(bool showInfo);

    double autoRotationSpeed() const;
    void setAutoRotationSpeed(double autoRotationSpeed);

    double cameraFacingDegrees() const;
    void setCameraFacingDegrees(double cameraFacingDegrees);

    double cameraTiltDegrees() const;
    void setCameraTiltDegrees(double cameraTiltDegrees);

    double cameraDistanceMeters() const;
    void setCameraDistanceMeters(double cameraDistanceMeters);

    double cameraFocalLengthMm() const;
    void setCameraFocalLengthMm(double cameraFocalLengthMm);

    QString caveLengthText() const;
    QString caveDepthText() const;
    int surveyCount() const;
    int stationCount() const;
    int shotCount() const;
    int meshCount() const;
    int surfaceCount() const;

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel);

signals:
    void filePathChanged();
    void meshColorModeChanged();
    void backgroundModeChanged();
    void measurementModeChanged();
    void autoRotationEnabledChanged();
    void orthographicProjectionChanged();
    void showBoundingBoxChanged();
    void showHudChanged();
    void showInfoChanged();
    void autoRotationSpeedChanged();
    void cameraFacingDegreesChanged();
    void cameraTiltDegreesChanged();
    void cameraDistanceMetersChanged();
    void cameraFocalLengthMmChanged();
    void sceneMetricsChanged();

private:
    QString filePath_;
    ThreeDViewerMeshColorMode meshColorMode_ = ThreeDViewerMeshColorMode::Altitude;
    ThreeDViewerBackgroundMode backgroundMode_ = ThreeDViewerBackgroundMode::Black;
    bool measurementMode_ = false;
    bool autoRotationEnabled_ = false;
    bool orthographicProjection_ = false;
    bool showBoundingBox_ = true;
    bool showHud_ = true;
    bool showInfo_ = true;
    double autoRotationSpeed_ = 30.0;
    double cameraFacingDegrees_ = 311.0;
    double cameraTiltDegrees_ = 26.0;
    double cameraDistanceMeters_ = 120.0;
    double cameraFocalLengthMm_ = 35.0;
    QString caveLengthText_;
    QString caveDepthText_;
    int surveyCount_ = 0;
    int stationCount_ = 0;
    int shotCount_ = 0;
    int meshCount_ = 0;
    int surfaceCount_ = 0;
};

} // namespace TherionStudio
