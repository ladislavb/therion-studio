#include "ThreeDViewerTab.h"

#include "ThreeDViewerInspectorState.h"
#include "ThreeDViewerInspectorWidget.h"
#include "ThreeDViewerLayerListModel.h"
#include "ThreeDViewerViewportWidget.h"

#include <QFileInfo>
#include <QFrame>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>

namespace TherionStudio
{

ThreeDViewerTab::ThreeDViewerTab(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

bool ThreeDViewerTab::loadFile(const QString &filePath, QString *errorMessage)
{
    const ThreeDViewerLoxLoader::Result result = loader_.loadFile(filePath);
    if (!result.ok()) {
        if (errorMessage != nullptr) {
            *errorMessage = result.error;
        }
        return false;
    }

    sceneModel_ = result.scene;
    if (layerModel_ != nullptr) {
        layerModel_->setSceneModel(sceneModel_);
    }

    const QFileInfo fileInfo(filePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    filePath_ = canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
    displayName_ = fileInfo.fileName();
    if (displayName_.isEmpty()) {
        displayName_ = filePath_;
    }

    updateSceneSummary();
    rebuildScene();
    emit titleChanged();
    return true;
}

bool ThreeDViewerTab::reloadFile(QString *errorMessage)
{
    if (filePath_.isEmpty()) {
        if (errorMessage != nullptr) {
            errorMessage->clear();
        }
        return false;
    }

    const ThreeDViewerLoxLoader::Result result = loader_.loadFile(filePath_);
    if (!result.ok()) {
        if (errorMessage != nullptr) {
            *errorMessage = result.error;
        }
        return false;
    }

    sceneModel_ = result.scene;
    if (layerModel_ != nullptr) {
        layerModel_->setSceneModel(sceneModel_);
    }
    updateSceneSummary();
    rebuildScene(false);
    emit titleChanged();
    return true;
}

void ThreeDViewerTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
    Q_UNUSED(projectRootPath);
}

QString ThreeDViewerTab::filePath() const
{
    return filePath_;
}

QString ThreeDViewerTab::displayName() const
{
    return displayName_;
}

bool ThreeDViewerTab::isDirty() const
{
    return false;
}

bool ThreeDViewerTab::save(QString *errorMessage)
{
    if (errorMessage != nullptr) {
        *errorMessage = tr("The 3D viewer is read-only.");
    }
    return false;
}

void ThreeDViewerTab::fitToScene()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->fitToScene();
}

void ThreeDViewerTab::resetView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->resetView();
}

void ThreeDViewerTab::setTopView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->setViewPreset(ThreeDViewerViewPreset::Top);
}

void ThreeDViewerTab::setSideView()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->setViewPreset(ThreeDViewerViewPreset::Side);
}

void ThreeDViewerTab::rollViewLeft()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->rollLeft();
}

void ThreeDViewerTab::rollViewRight()
{
    if (viewport_ == nullptr) {
        return;
    }
    viewport_->rollRight();
}

void ThreeDViewerTab::setMeasurementMode(bool measurementMode)
{
    if (inspectorState_ != nullptr) {
        inspectorState_->setMeasurementMode(measurementMode);
    }
    if (viewport_ != nullptr) {
        viewport_->setMeasurementMode(measurementMode);
        if (measurementMode) {
            viewport_->focusViewport(Qt::ShortcutFocusReason);
        }
    }
}

bool ThreeDViewerTab::measurementMode() const
{
    return inspectorState_ != nullptr ? inspectorState_->measurementMode() : false;
}

void ThreeDViewerTab::setAutoRotationEnabled(bool autoRotationEnabled)
{
    if (inspectorState_ != nullptr) {
        inspectorState_->setAutoRotationEnabled(autoRotationEnabled);
    }
    if (viewport_ != nullptr) {
        viewport_->setAutoRotationEnabled(autoRotationEnabled);
    }
}

bool ThreeDViewerTab::autoRotationEnabled() const
{
    return inspectorState_ != nullptr ? inspectorState_->autoRotationEnabled() : false;
}

void ThreeDViewerTab::setOrthographicProjection(bool orthographicProjection)
{
    if (orthographicProjection_ == orthographicProjection) {
        return;
    }

    orthographicProjection_ = orthographicProjection;
    if (inspectorState_ != nullptr) {
        inspectorState_->setOrthographicProjection(orthographicProjection_);
    }
    if (viewport_ != nullptr) {
        viewport_->setOrthographicProjection(orthographicProjection_);
    }
    emit orthographicProjectionChanged(orthographicProjection_);
}

bool ThreeDViewerTab::orthographicProjection() const
{
    return orthographicProjection_;
}

void ThreeDViewerTab::showFindBar(bool)
{
}

void ThreeDViewerTab::hideFindBar()
{
}

void ThreeDViewerTab::goToLine(int)
{
}

int ThreeDViewerTab::currentLineNumber() const
{
    return 0;
}

void ThreeDViewerTab::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);
    rootLayout->addWidget(splitter_, 1);

    auto *viewportFrame = new QFrame(splitter_);
    viewportFrame->setFrameStyle(QFrame::StyledPanel);
    auto *viewportLayout = new QVBoxLayout(viewportFrame);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    viewportLayout->setSpacing(0);
    viewport_ = new ThreeDViewerViewportWidget(viewportFrame);
    viewportLayout->addWidget(viewport_);
    splitter_->addWidget(viewportFrame);

    auto *measurementEscapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    measurementEscapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(measurementEscapeShortcut, &QShortcut::activated, this, [this] {
        if (measurementMode()) {
            setMeasurementMode(false);
        }
    });

    auto *inspector = new QWidget(splitter_);
    auto *inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(8, 8, 8, 8);
    inspectorLayout->setSpacing(8);

    inspectorState_ = new ThreeDViewerInspectorState(this);
    layerModel_ = new ThreeDViewerLayerListModel(this);
    connect(layerModel_, &ThreeDViewerLayerListModel::layerVisibilityChanged, this, [this](int, bool) {
        if (viewport_ != nullptr && layerModel_ != nullptr) {
            viewport_->setLayerVisibility(layerModel_->layerVisibility());
            viewport_->setFeatureVisibility(layerModel_->featureVisibility());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::meshColorModeChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::measurementModeChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setMeasurementMode(inspectorState_->measurementMode());
        }
        emit measurementModeChanged(inspectorState_ != nullptr ? inspectorState_->measurementMode() : false);
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::autoRotationEnabledChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setAutoRotationEnabled(inspectorState_->autoRotationEnabled());
        }
        emit autoRotationEnabledChanged(inspectorState_ != nullptr ? inspectorState_->autoRotationEnabled() : false);
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::autoRotationSpeedChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setAutoRotationSpeed(inspectorState_->autoRotationSpeed());
        }
    });
    const auto syncSceneOverlayVisibility = [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setSceneOverlayVisibility(inspectorState_->showBoundingBox(),
                                                 inspectorState_->showHud(),
                                                 inspectorState_->showInfo());
        }
    };
    connect(inspectorState_, &ThreeDViewerInspectorState::showBoundingBoxChanged, this, syncSceneOverlayVisibility);
    connect(inspectorState_, &ThreeDViewerInspectorState::showHudChanged, this, syncSceneOverlayVisibility);
    connect(inspectorState_, &ThreeDViewerInspectorState::showInfoChanged, this, syncSceneOverlayVisibility);
    connect(inspectorState_, &ThreeDViewerInspectorState::cameraFacingDegreesChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCameraFacingDegrees(inspectorState_->cameraFacingDegrees());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::cameraTiltDegreesChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCameraTiltDegrees(inspectorState_->cameraTiltDegrees());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::cameraDistanceMetersChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCameraDistanceMeters(inspectorState_->cameraDistanceMeters());
        }
    });
    connect(inspectorState_, &ThreeDViewerInspectorState::cameraFocalLengthMmChanged, this, [this] {
        if (viewport_ != nullptr && inspectorState_ != nullptr) {
            viewport_->setCameraFocalLengthMm(inspectorState_->cameraFocalLengthMm());
        }
    });
    connect(viewport_, &ThreeDViewerViewportWidget::cameraSettingsChanged, this, [this](double facingDegrees, double tiltDegrees, double distanceMeters, double focalLengthMm) {
        if (inspectorState_ == nullptr) {
            return;
        }
        inspectorState_->setCameraFacingDegrees(facingDegrees);
        inspectorState_->setCameraTiltDegrees(tiltDegrees);
        inspectorState_->setCameraDistanceMeters(distanceMeters);
        inspectorState_->setCameraFocalLengthMm(focalLengthMm);
    });
    connect(viewport_, &ThreeDViewerViewportWidget::measurementModeExitRequested, this, [this] {
        setMeasurementMode(false);
    });

    inspectorWidget_ = new ThreeDViewerInspectorWidget(inspector);
    inspectorWidget_->setInspectorState(inspectorState_);
    inspectorWidget_->setLayerModel(layerModel_);
    inspectorLayout->addWidget(inspectorWidget_, 1);

    if (viewport_ != nullptr && inspectorState_ != nullptr) {
        viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        viewport_->setMeasurementMode(inspectorState_->measurementMode());
        viewport_->setOrthographicProjection(orthographicProjection_);
        inspectorState_->setOrthographicProjection(orthographicProjection_);
        viewport_->setCameraFacingDegrees(inspectorState_->cameraFacingDegrees());
        viewport_->setCameraTiltDegrees(inspectorState_->cameraTiltDegrees());
        viewport_->setCameraDistanceMeters(inspectorState_->cameraDistanceMeters());
        viewport_->setCameraFocalLengthMm(inspectorState_->cameraFocalLengthMm());
        viewport_->setAutoRotationSpeed(inspectorState_->autoRotationSpeed());
        viewport_->setAutoRotationEnabled(inspectorState_->autoRotationEnabled());
        viewport_->setSceneOverlayVisibility(inspectorState_->showBoundingBox(),
                                             inspectorState_->showHud(),
                                             inspectorState_->showInfo());
    }

    splitter_->addWidget(inspector);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 0);
    splitter_->setSizes({900, 320});
}

void ThreeDViewerTab::rebuildScene(bool fitToScene)
{
    loadSceneIntoView(fitToScene);
}

void ThreeDViewerTab::updateSceneSummary()
{
    if (inspectorState_ != nullptr) {
        inspectorState_->setFilePath(filePath_);
        inspectorState_->setSceneModel(sceneModel_);
    }
}

void ThreeDViewerTab::loadSceneIntoView(bool fitToScene)
{
    if (viewport_ == nullptr) {
        return;
    }

    viewport_->setSceneModel(sceneModel_, fitToScene);
    if (layerModel_ != nullptr) {
        layerModel_->setSceneModel(sceneModel_);
        viewport_->setLayerVisibility(layerModel_->layerVisibility());
        viewport_->setFeatureVisibility(layerModel_->featureVisibility());
    }
    if (inspectorState_ != nullptr) {
        viewport_->setMeshColorMode(static_cast<ThreeDViewerMeshColorMode>(inspectorState_->meshColorMode()));
        viewport_->setMeasurementMode(inspectorState_->measurementMode());
        viewport_->setOrthographicProjection(orthographicProjection_);
        inspectorState_->setOrthographicProjection(orthographicProjection_);
        viewport_->setCameraFacingDegrees(inspectorState_->cameraFacingDegrees());
        viewport_->setCameraTiltDegrees(inspectorState_->cameraTiltDegrees());
        viewport_->setCameraDistanceMeters(inspectorState_->cameraDistanceMeters());
        viewport_->setCameraFocalLengthMm(inspectorState_->cameraFocalLengthMm());
        viewport_->setAutoRotationSpeed(inspectorState_->autoRotationSpeed());
        viewport_->setAutoRotationEnabled(inspectorState_->autoRotationEnabled());
        viewport_->setSceneOverlayVisibility(inspectorState_->showBoundingBox(),
                                             inspectorState_->showHud(),
                                             inspectorState_->showInfo());
    }
    if (fitToScene) {
        viewport_->fitToScene();
    }
}

} // namespace TherionStudio
