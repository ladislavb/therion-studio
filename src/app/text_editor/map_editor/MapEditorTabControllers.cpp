#include "MapEditorTab.h"

#include <QGraphicsView>
#include <QPoint>
#include <QShortcut>
#include <QSplitter>
#include <QTimer>

#include "../../../core/ISessionStore.h"
#include "../TextEditorTab.h"
#include "MapEditorMagnifierOverlay.h"
#include "MapEditorSceneLifecycleController.h"

namespace TherionStudio
{
namespace
{
constexpr int kMagnifierUpdateIntervalMs = 16;
}

void MapEditorTab::updateCommandSurfaceState()
{
    if (cancelDrawShortcut_ != nullptr) {
        cancelDrawShortcut_->setEnabled(interactiveDrawState_.mode_ != InteractiveDrawMode::None);
    }
    if (commitDrawShortcut_ != nullptr) {
        commitDrawShortcut_->setEnabled(interactiveDrawState_.mode_ == InteractiveDrawMode::Line
                                        || interactiveDrawState_.mode_ == InteractiveDrawMode::Area
                                        || interactiveDrawState_.mode_ == InteractiveDrawMode::SmartArea);
    }
    const bool canCycleSmartArea = interactiveDrawState_.mode_ == InteractiveDrawMode::SmartArea
        && interactiveDrawState_.smartAreaPreviewActive_
        && interactiveDrawState_.smartAreaCandidates_.size() > 1;
    if (previousSmartAreaCandidateShortcut_ != nullptr) {
        previousSmartAreaCandidateShortcut_->setEnabled(canCycleSmartArea);
    }
    if (nextSmartAreaCandidateShortcut_ != nullptr) {
        nextSmartAreaCandidateShortcut_->setEnabled(canCycleSmartArea);
    }
    refreshToolbarSummary();
    emit commandSurfaceStateChanged();
}

void MapEditorTab::clearMapScene()
{
    backgroundPivotMarker_ = nullptr;
    MapEditorSceneLifecycleController(sceneLifecycleContext()).clearMapScene();
}

void MapEditorTab::clearDraftGeometryItems()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).clearDraftGeometryItems();
}

void MapEditorTab::clearBackgroundImageItems()
{
    invalidateBackgroundRasterJobs();
    MapEditorSceneLifecycleController(sceneLifecycleContext()).clearBackgroundImageItems();
}

void MapEditorTab::restoreDraftGeometryItems()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).restoreDraftGeometryItems();
}

void MapEditorTab::restoreBackgroundImageItems()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).restoreBackgroundImageItems();
}

void MapEditorTab::updateMapSceneScrollBounds()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).updateSceneRectForBackgroundBounds();
}

void MapEditorTab::fitMapToView(bool includeBackgroundImages)
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).fitMapToView(includeBackgroundImages);
}

void MapEditorTab::fitMapToViewAfterViewportResize(bool includeBackgroundImages)
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).fitMapToView(includeBackgroundImages, false);
}

void MapEditorTab::syncZoomFactorFromView()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).syncZoomFactorFromView();
}

void MapEditorTab::applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition)
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).applyZoomAtViewportPosition(factor, viewportPosition);
    if (magnifierEnabled_ && mapMagnifierOverlay_ != nullptr && mapMagnifierOverlay_->isVisible()) {
        scheduleMagnifierOverlayUpdateFromViewportPosition(viewportPosition.toPoint());
    }
}

void MapEditorTab::refreshToolbarSummary()
{
    emit statusHintChanged();
}

void MapEditorTab::updateMagnifierOverlayGeometry()
{
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->updatePlacement();
    }
}

void MapEditorTab::updateMagnifierOverlayFromViewportPosition(const QPoint &viewportPosition, bool active)
{
    if (mapMagnifierOverlay_ == nullptr || mapView_ == nullptr || mapView_->viewport() == nullptr) {
        return;
    }
    if (!magnifierEnabled_) {
        hideMagnifierOverlay();
        return;
    }
    if (!mapView_->viewport()->rect().contains(viewportPosition)) {
        hideMagnifierOverlay();
        return;
    }

    const QPointF scenePosition = mapView_->mapToScene(viewportPosition);
    mapMagnifierOverlay_->setCursorPosition(viewportPosition,
                                            scenePosition,
                                            magnifierCoordinateTextForScenePosition(scenePosition));
    mapMagnifierOverlay_->setOverlayActive(active);
}

void MapEditorTab::scheduleMagnifierOverlayUpdateFromViewportPosition(const QPoint &viewportPosition)
{
    if (mapMagnifierOverlay_ == nullptr || mapView_ == nullptr || mapView_->viewport() == nullptr) {
        return;
    }
    if (!magnifierEnabled_) {
        hideMagnifierOverlay();
        return;
    }

    magnifierPendingViewportPosition_ = viewportPosition;
    magnifierLastViewportPosition_ = viewportPosition;
    magnifierHasViewportPosition_ = true;
    if (magnifierUpdatePending_) {
        return;
    }

    const int elapsedMs = magnifierThrottleActive_
        ? static_cast<int>(magnifierLastUpdateElapsed_.elapsed())
        : kMagnifierUpdateIntervalMs;
    const int delayMs = qMax(0, kMagnifierUpdateIntervalMs - elapsedMs);

    magnifierUpdatePending_ = true;
    QTimer::singleShot(delayMs, this, [this]() {
        magnifierUpdatePending_ = false;
        magnifierThrottleActive_ = true;
        magnifierLastUpdateElapsed_.restart();
        updateMagnifierOverlayFromViewportPosition(magnifierPendingViewportPosition_, true);
    });
}

void MapEditorTab::refreshVisibleMagnifierOverlay()
{
    if (!magnifierEnabled_
        || mapMagnifierOverlay_ == nullptr
        || !mapMagnifierOverlay_->isVisible()
        || !magnifierHasViewportPosition_) {
        return;
    }

    scheduleMagnifierOverlayUpdateFromViewportPosition(magnifierLastViewportPosition_);
}

bool MapEditorTab::isMagnifierEnabled() const
{
    return magnifierEnabled_;
}

bool MapEditorTab::hasRightPanel() const
{
    if (!isMapPaneDetached() && workspaceMode_ == WorkspaceMode::Raw && textEditor_ != nullptr) {
        return textEditor_->hasRightPanel();
    }
    return mapDetailsSplitter_ != nullptr && objectDetailsPanel_ != nullptr;
}

bool MapEditorTab::isRightPanelCollapsed() const
{
    if (!isMapPaneDetached() && workspaceMode_ == WorkspaceMode::Raw && textEditor_ != nullptr) {
        return textEditor_->isRightPanelCollapsed();
    }
    return mapInspectorCollapsed_;
}

QString MapEditorTab::rightPanelLabel() const
{
    if (!isMapPaneDetached() && workspaceMode_ == WorkspaceMode::Raw && textEditor_ != nullptr) {
        return textEditor_->rightPanelLabel();
    }
    return tr("Map Inspector");
}

bool MapEditorTab::hasContextHelpPanel() const
{
    return textEditor_ != nullptr && textEditor_->hasRightPanel();
}

bool MapEditorTab::isContextHelpPanelCollapsed() const
{
    return textEditor_ != nullptr && textEditor_->isRightPanelCollapsed();
}

void MapEditorTab::setMagnifierEnabled(bool enabled)
{
    if (magnifierEnabled_ == enabled) {
        return;
    }

    magnifierEnabled_ = enabled;
    if (sessionStore_ != nullptr) {
        sessionStore_->setTherionMapMagnifierEnabled(enabled);
    }
    if (!magnifierEnabled_) {
        hideMagnifierOverlay();
    } else if (magnifierHasViewportPosition_) {
        scheduleMagnifierOverlayUpdateFromViewportPosition(magnifierLastViewportPosition_);
    }
}

void MapEditorTab::setRightPanelCollapsed(bool collapsed)
{
    if (!isMapPaneDetached() && workspaceMode_ == WorkspaceMode::Raw && textEditor_ != nullptr) {
        textEditor_->setRightPanelCollapsed(collapsed);
        return;
    }
    if (mapDetailsSplitter_ == nullptr || objectDetailsPanel_ == nullptr) {
        return;
    }
    if (mapInspectorCollapsed_ == collapsed) {
        return;
    }

    const QList<int> sizes = mapDetailsSplitter_->sizes();
    if (collapsed) {
        if (sizes.size() >= 2 && sizes.at(1) > 0) {
            mapInspectorPanelExtent_ = sizes.at(1);
        }
        mapInspectorCollapsed_ = true;
        mapDetailsSplitter_->setSizes({qMax(1, sizes.value(0, 980) + sizes.value(1, 0)), 0});
        return;
    }

    mapInspectorCollapsed_ = false;
    int totalWidth = 0;
    for (const int size : sizes) {
        totalWidth += size;
    }
    if (totalWidth <= 0) {
        mapDetailsSplitter_->setSizes({980, mapInspectorPanelExtent_});
        return;
    }

    const int inspectorWidth = qBound(objectDetailsPanel_->minimumWidth(),
                                      mapInspectorPanelExtent_,
                                      qMax(objectDetailsPanel_->minimumWidth(), totalWidth - 360));
    const int mapWidth = qMax(240, totalWidth - inspectorWidth);
    mapDetailsSplitter_->setSizes({mapWidth, inspectorWidth});
}

void MapEditorTab::setContextHelpPanelCollapsed(bool collapsed)
{
    if (textEditor_ != nullptr) {
        textEditor_->setRightPanelCollapsed(collapsed);
    }
}

void MapEditorTab::hideMagnifierOverlay()
{
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->setOverlayActive(false);
    }
    magnifierHasViewportPosition_ = false;
    magnifierThrottleActive_ = false;
}

QString MapEditorTab::magnifierCoordinateTextForScenePosition(const QPointF &scenePosition) const
{
    const QPointF sourcePosition = sourcePointFromScenePosition(scenePosition);
    return tr("x %1  y %2").arg(QString::number(sourcePosition.x(), 'f', 1),
                                QString::number(sourcePosition.y(), 'f', 1));
}

QRectF MapEditorTab::mapGeometryFitBounds() const
{
    return MapEditorSceneLifecycleController(sceneLifecycleContext()).mapGeometryFitBounds();
}

QRectF MapEditorTab::mapPreviewBounds() const
{
    return MapEditorSceneLifecycleController(sceneLifecycleContext()).mapPreviewBounds();
}

void MapEditorTab::adjustMapZoom(qreal factor)
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).adjustMapZoom(factor);
}

} // namespace TherionStudio
