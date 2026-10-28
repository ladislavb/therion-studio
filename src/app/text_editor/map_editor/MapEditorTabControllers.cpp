#include "MapEditorTab.h"

#include <QFontMetrics>
#include <QGraphicsView>
#include <QPoint>
#include <QShortcut>
#include <QSplitter>
#include <QStringList>
#include <QTimer>

#include "../../../core/ISessionStore.h"
#include "../TextEditorTab.h"
#include "MapEditorMagnifierOverlay.h"
#include "MapEditorSceneLifecycleController.h"

namespace TherionStudio
{
namespace
{
constexpr int kPointerReadoutUpdateIntervalMs = 16;
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
    if (pointerHasViewportPosition_) {
        schedulePointerReadoutUpdate(viewportPosition.toPoint());
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

// Drives both the magnifier and the status bar pointer coordinates.
void MapEditorTab::updatePointerReadoutFromViewportPosition(const QPoint &viewportPosition)
{
    if (mapView_ == nullptr || mapView_->viewport() == nullptr) {
        return;
    }
    if (!mapView_->viewport()->rect().contains(viewportPosition)) {
        clearPointerReadout();
        return;
    }

    const QPointF scenePosition = mapView_->mapToScene(viewportPosition);
    const QString coordinateText = pointerCoordinateTextForScenePosition(scenePosition);
    QString xviStationName;
    if (sessionStore_ != nullptr && sessionStore_->therionMapXviStationNamesInStatusBarEnabled()) {
        if (const std::optional<MapEditorXviStationSnapCandidate> xviStation =
                xviStationSnapAtScenePosition(scenePosition)) {
            xviStationName = xviStation->name;
        }
    }
    const bool coordinatesEnabled =
        sessionStore_ != nullptr && sessionStore_->therionMapPointerCoordinatesInStatusBarEnabled();
    setPointerStatusText(pointerStatusLabelText(coordinatesEnabled ? coordinateText : QString(), xviStationName));
    if (mapMagnifierOverlay_ == nullptr) {
        return;
    }
    if (!magnifierEnabled_) {
        hideMagnifierOverlay();
        return;
    }
    mapMagnifierOverlay_->setCursorPosition(viewportPosition, scenePosition, coordinateText);
    mapMagnifierOverlay_->setOverlayActive(true);
}

void MapEditorTab::schedulePointerReadoutUpdate(const QPoint &viewportPosition)
{
    if (mapView_ == nullptr || mapView_->viewport() == nullptr) {
        return;
    }

    pointerPendingViewportPosition_ = viewportPosition;
    pointerLastViewportPosition_ = viewportPosition;
    pointerHasViewportPosition_ = true;
    if (pointerUpdatePending_) {
        return;
    }

    const int elapsedMs = pointerThrottleActive_
        ? static_cast<int>(pointerLastUpdateElapsed_.elapsed())
        : kPointerReadoutUpdateIntervalMs;
    const int delayMs = qMax(0, kPointerReadoutUpdateIntervalMs - elapsedMs);

    pointerUpdatePending_ = true;
    QTimer::singleShot(delayMs, this, [this]() {
        pointerUpdatePending_ = false;
        // The pointer may have left the map while the update was pending.
        if (!pointerHasViewportPosition_) {
            return;
        }
        pointerThrottleActive_ = true;
        pointerLastUpdateElapsed_.restart();
        updatePointerReadoutFromViewportPosition(pointerPendingViewportPosition_);
    });
}

void MapEditorTab::refreshPointerReadout()
{
    if (!pointerHasViewportPosition_) {
        return;
    }

    schedulePointerReadoutUpdate(pointerLastViewportPosition_);
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
    } else if (pointerHasViewportPosition_) {
        schedulePointerReadoutUpdate(pointerLastViewportPosition_);
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
}

void MapEditorTab::clearPointerReadout()
{
    hideMagnifierOverlay();
    pointerHasViewportPosition_ = false;
    pointerThrottleActive_ = false;
    setPointerStatusText(QString());
}

void MapEditorTab::setPointerStatusText(const QString &text)
{
    if (pointerStatusText_ == text) {
        return;
    }
    pointerStatusText_ = text;
    emit pointerStatusChanged();
}

QString MapEditorTab::pointerStatusText() const
{
    return pointerStatusText_;
}

QString MapEditorTab::pointerCoordinateText(const QPointF &sourcePosition)
{
    return tr("x %1  y %2").arg(QString::number(sourcePosition.x(), 'f', 1),
                                QString::number(sourcePosition.y(), 'f', 1));
}

QString MapEditorTab::pointerStatusLabelText(const QString &coordinateText, const QString &xviStationName)
{
    QStringList parts;
    if (!coordinateText.isEmpty()) {
        parts.append(coordinateText);
    }
    if (!xviStationName.isEmpty()) {
        parts.append(tr("Station: %1").arg(xviStationName));
    }
    return parts.join(QStringLiteral("   "));
}

bool MapEditorTab::isPointerStatusEnabled() const
{
    return sessionStore_ != nullptr
        && (sessionStore_->therionMapPointerCoordinatesInStatusBarEnabled()
            || sessionStore_->therionMapXviStationNamesInStatusBarEnabled());
}

// Wide enough for any coordinates, so the status bar does not shift as the pointer moves.
int MapEditorTab::pointerStatusMinimumWidth(const QFontMetrics &metrics) const
{
    if (sessionStore_ == nullptr || !sessionStore_->therionMapPointerCoordinatesInStatusBarEnabled()) {
        return 0;
    }
    return metrics.horizontalAdvance(pointerCoordinateText(QPointF(-99999.9, -99999.9))) + 12;
}

void MapEditorTab::refreshPointerStatusOptions()
{
    setPointerStatusText(QString());
    // Labels re-read the options even when the text was already empty.
    emit pointerStatusChanged();
    refreshPointerReadout();
}

QString MapEditorTab::pointerCoordinateTextForScenePosition(const QPointF &scenePosition) const
{
    return pointerCoordinateText(sourcePointFromScenePosition(scenePosition));
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
