#pragma once

#include "../../core/ThreeDViewerSceneModel.h"
#include "ThreeDViewerLayerListModel.h"
#include "ThreeDViewerMeshColorMode.h"
#include "ThreeDViewerViewportController.h"

#include <QQuickWidget>

#include <array>

namespace TherionStudio
{

class ThreeDViewerViewportItem;

class ThreeDViewerViewportWidget final : public QQuickWidget
{
    Q_OBJECT

public:
    explicit ThreeDViewerViewportWidget(QWidget *parent = nullptr);

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel, bool fitToScene = true);
    void setLayerVisibility(const std::array<bool, 5> &layerVisibility);
    void setFeatureVisibility(const ThreeDViewerLayerListModel::FeatureVisibility &featureVisibility);
    void setMeshColorMode(ThreeDViewerMeshColorMode meshColorMode);
    void setMeasurementMode(bool measurementMode);
    void setAutoRotationEnabled(bool autoRotationEnabled);
    void setAutoRotationSpeed(double autoRotationSpeed);
    void setOrthographicProjection(bool orthographicProjection);
    void setSceneOverlayVisibility(bool showBoundingBox, bool showHud, bool showInfo);
    void setCameraFacingDegrees(double degrees);
    void setCameraTiltDegrees(double degrees);
    void setCameraDistanceMeters(double distanceMeters);
    void setCameraFocalLengthMm(double focalLengthMm);
    void fitToScene();
    void resetView();
    void setViewPreset(ThreeDViewerViewPreset preset);
    void rollLeft();
    void rollRight();
    void focusViewport(Qt::FocusReason reason = Qt::ShortcutFocusReason);

signals:
    void cameraSettingsChanged(double facingDegrees, double tiltDegrees, double distanceMeters, double focalLengthMm);
    void measurementModeExitRequested();

private:
    void syncRootItem();
    ThreeDViewerViewportItem *rootViewportItem() const;
    void handleCameraSettingsChanged(double facingDegrees, double tiltDegrees, double distanceMeters, double focalLengthMm);

    ThreeDViewerSceneModel sceneModel_;
    std::array<bool, 5> layerVisibility_ = {true, true, true, true, true};
    ThreeDViewerLayerListModel::FeatureVisibility featureVisibility_;
    ThreeDViewerMeshColorMode meshColorMode_ = ThreeDViewerMeshColorMode::Altitude;
    bool measurementMode_ = false;
    bool orthographicProjection_ = false;
    bool showBoundingBox_ = true;
    bool showHud_ = true;
    bool showInfo_ = true;
    bool autoRotationEnabled_ = false;
    double autoRotationSpeed_ = 30.0;
    double cameraFacingDegrees_ = 311.0;
    double cameraTiltDegrees_ = 26.0;
    double cameraDistanceMeters_ = 120.0;
    double cameraFocalLengthMm_ = 35.0;
    bool pendingFitToScene_ = false;
    bool pendingResetView_ = false;
    bool pendingRollLeft_ = false;
    bool pendingRollRight_ = false;
    bool hasPendingViewPreset_ = false;
    bool rootSceneModelSynced_ = false;
    bool pendingSceneFitToScene_ = true;
    ThreeDViewerViewPreset pendingViewPreset_ = ThreeDViewerViewPreset::Isometric;
};

} // namespace TherionStudio
