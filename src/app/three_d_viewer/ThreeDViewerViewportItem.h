#pragma once

#include "../../core/ThreeDViewerSceneModel.h"
#include "ThreeDViewerLayerListModel.h"
#include "ThreeDViewerBackgroundMode.h"
#include "ThreeDViewerMeshColorMode.h"
#include "ThreeDViewerViewportController.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QPointF>
#include <QQuickItem>
#include <QSet>
#include <QTimer>

#include <array>

class QMouseEvent;
class QHoverEvent;
class QKeyEvent;
class QWheelEvent;
class QSGNode;

#include "../../core/ThreeDViewerSceneStatistics.h"

#include <QHash>
#include <QSizeF>
#include <memory>

namespace TherionStudio
{
struct ThreeDViewerPreparedScene
{
    ThreeDViewerSceneModel sceneModel;
    ThreeDViewerBounds bounds;
    QHash<quint32, int> stationIndexById;
    QVector<QString> stationQualifiedNames;
    QVector<QString> stationDisplayLabels;
    QVector<QSizeF> stationLabelSizes;
    ThreeDViewerSceneStatistics statistics;
    bool stationFeatureFiltersActive = false;
};
} // namespace TherionStudio

namespace TherionStudio
{

class ThreeDViewerViewportItem : public QQuickItem
{
    Q_OBJECT

public:
    explicit ThreeDViewerViewportItem(QQuickItem *parent = nullptr);

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel, bool fitToScene = true);
    void setLayerVisibility(const std::array<bool, 5> &layerVisibility);
    void setFeatureVisibility(const ThreeDViewerLayerListModel::FeatureVisibility &featureVisibility);
    void setMeshColorMode(ThreeDViewerMeshColorMode meshColorMode);
    void setCenterlineColorMode(ThreeDViewerCenterlineColorMode colorMode);
    void setBackgroundMode(ThreeDViewerBackgroundMode backgroundMode);
    void setMeasurementMode(bool measurementMode);
    void setAutoRotationEnabled(bool autoRotationEnabled);
    void setAutoRotationSpeed(double autoRotationSpeedDegreesPerSecond);
    void setOrthographicProjection(bool orthographicProjection);
    void setSceneOverlayVisibility(bool showBoundingBox, bool showHud, bool showInfo);
    void setCameraFacingDegrees(double degrees);
    void setCameraTiltDegrees(double degrees);
    void setCameraDistanceMeters(double distanceMeters);
    void setCameraFocalLengthMm(double focalLengthMm);
    void setExportRenderScale(qreal scale);
    void fitToScene();
    void resetView();
    void setViewPreset(ThreeDViewerViewPreset preset);
    void rollLeft();
    void rollRight();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

signals:
    void cameraSettingsChanged(double facingDegrees, double tiltDegrees, double distanceMeters, double focalLengthMm);
    void measurementModeExitRequested();

private:
    struct Snapshot
    {
        std::shared_ptr<const ThreeDViewerPreparedScene> preparedScene;
        const ThreeDViewerSceneModel &sceneModel() const
        {
            static const ThreeDViewerSceneModel kEmpty;
            return preparedScene ? preparedScene->sceneModel : kEmpty;
        }
        std::array<bool, 5> layerVisibility = {true, true, true, true, true};
        ThreeDViewerLayerListModel::FeatureVisibility featureVisibility;
        ThreeDViewerMeshColorMode meshColorMode = ThreeDViewerMeshColorMode::Altitude;
        ThreeDViewerCenterlineColorMode centerlineColorMode = ThreeDViewerCenterlineColorMode::Red;
        ThreeDViewerBackgroundMode backgroundMode = ThreeDViewerBackgroundMode::Black;
        bool measurementMode = false;
        bool orthographicProjection = false;
        bool showBoundingBox = true;
        bool showHud = true;
        bool showInfo = true;
        bool hasHoveredStation = false;
        quint32 hoveredStationId = 0;
        QPointF hoveredStationScreenPosition;
        bool hasMeasurementStartStation = false;
        quint32 measurementStartStationId = 0;
        bool hasMeasurementEndStation = false;
        quint32 measurementEndStationId = 0;
        bool autoRotationDeclutterLocked = false;
        QSet<quint32> autoRotationStationIds;
        QSet<quint32> autoRotationLabelIds;
        qreal exportRenderScale = 1.0;
        ThreeDViewerCamera camera;
    };

    Snapshot snapshot() const;
    void scheduleUpdate();
    void handleAutoRotationTick();
    void rebuildAutoRotationDeclutterLocks();

    mutable QMutex mutex_;
    std::shared_ptr<const ThreeDViewerPreparedScene> preparedScene_;
    std::array<bool, 5> layerVisibility_ = {true, true, true, true, true};
    ThreeDViewerLayerListModel::FeatureVisibility featureVisibility_;
    ThreeDViewerMeshColorMode meshColorMode_ = ThreeDViewerMeshColorMode::Altitude;
    ThreeDViewerCenterlineColorMode centerlineColorMode_ = ThreeDViewerCenterlineColorMode::Red;
    ThreeDViewerBackgroundMode backgroundMode_ = ThreeDViewerBackgroundMode::Black;
    bool measurementMode_ = false;
    bool orthographicProjection_ = false;
    bool showBoundingBox_ = true;
    bool showHud_ = true;
    bool showInfo_ = true;
    bool autoRotationEnabled_ = false;
    double autoRotationSpeedDegreesPerSecond_ = 30.0;
    QTimer autoRotationTimer_;
    QElapsedTimer autoRotationElapsedTimer_;
    bool autoRotationDeclutterLocked_ = false;
    QSet<quint32> autoRotationStationIds_;
    QSet<quint32> autoRotationLabelIds_;
    ThreeDViewerCamera cameraSnapshot_;
    ThreeDViewerViewportController controller_;
    quint32 hoveredStationId_ = 0;
    bool hasHoveredStation_ = false;
    QPointF hoveredStationScreenPosition_;
    quint32 measurementStartStationId_ = 0;
    bool hasMeasurementStartStation_ = false;
    quint32 measurementEndStationId_ = 0;
    bool hasMeasurementEndStation_ = false;
    qreal exportRenderScale_ = 1.0;
};

} // namespace TherionStudio
