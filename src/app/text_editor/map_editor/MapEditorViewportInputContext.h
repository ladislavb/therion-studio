#pragma once

#include "MapEditorInteractiveDrawContext.h"
#include "MapEditorInteractiveDrawLogic.h"

#include <QDateTime>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

class QElapsedTimer;
class QGraphicsScene;
class QGraphicsView;

namespace TherionStudio
{
class TextEditorTab;

struct MapEditorViewportInputContext
{
    TextEditorTab *textEditor = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsView *view = nullptr;
    QString *toolbarStatusNote = nullptr;
    bool *touchFriendlyControlsEnabled = nullptr;
    bool *selectModeActive = nullptr;
    bool *autoFitEnabled = nullptr;
    bool *fitBackgroundRequested = nullptr;
    bool *mapPanActive = nullptr;
    bool *mapPanMoved = nullptr;
    bool *mapSpacePanKeyDown = nullptr;
    bool *mapControlPanActive = nullptr;
    QPoint *mapPanStartPosition = nullptr;
    QPoint *mapPanLastPosition = nullptr;
    bool *primaryPointerInteractionActive = nullptr;
    bool *touchPanCandidate = nullptr;
    bool *touchPanActive = nullptr;
    QPointF *touchPanStartPosition = nullptr;
    QPointF *touchPanLastPosition = nullptr;
    QDateTime *lastTabletInteractionUtc = nullptr;
    bool *nativeZoomGestureActive = nullptr;
    QDateTime *lastNativeZoomGestureUtc = nullptr;
    bool *pendingClickSelection = nullptr;
    QPointF *pendingClickScenePosition = nullptr;
    QElapsedTimer *pendingClickElapsed = nullptr;
    int *pendingClickLineNumber = nullptr;
    int *pendingClickSourceVertexIndex = nullptr;
    QString *pendingClickGeometryKind = nullptr;
    QVector<QPointF> *interactiveDrawSourceVertices = nullptr;
    QVector<QPointF> *interactiveDrawSceneVertices = nullptr;
    QVector<MapEditorInteractiveLineDraftVertex> *interactiveDrawLineVertices = nullptr;
    bool *interactiveDrawStrokeActive = nullptr;
    bool *interactiveDrawAnchorPressActive = nullptr;
    QPointF *interactiveDrawAnchorPressScenePoint = nullptr;
    bool *interactiveDrawAnchorDragActive = nullptr;
    QPointF *interactiveDrawAnchorDragScenePoint = nullptr;
    bool *interactiveDrawControlDragActive = nullptr;
    MapEditorInteractiveLineControlHandleRef *interactiveDrawControlDragHandle = nullptr;
    bool *interactiveDrawHoverActive = nullptr;
    QPointF *interactiveDrawHoverScenePoint = nullptr;
    bool *interactiveDrawHoverSnapActive = nullptr;
    QPointF *interactiveDrawHoverSnapScenePoint = nullptr;
    QVector<QPointF> *interactiveDrawHoverSnapGuideScenePoints = nullptr;

    std::function<MapEditorInteractiveDrawMode()> drawMode;
    std::function<QString(const char *)> translate;
    std::function<void(bool)> clearInteractiveDrawSession;
    std::function<QPointF(const QPointF &)> sourcePointFromScenePosition;
    std::function<void()> updateInteractiveDrawPreview;
    std::function<void()> refreshToolbarSummary;
    std::function<void()> updateCommandSurfaceState;
    std::function<void()> syncMapSelectionFromScene;
    std::function<std::optional<MapEditorInteractiveLineControlHandleRef>(const QPointF &, qreal)> interactiveLineControlAt;
    std::function<bool(const MapEditorInteractiveLineControlHandleRef &, const QPointF &)> setInteractiveLineControlScenePoint;
    std::function<bool(const QPointF &)> handleInteractiveDrawClick;
    std::function<void(const QPointF &, const std::optional<QPointF> &)> captureInteractiveLineAnchor;
    std::function<bool(const QString &, const QVector<QPointF> &, const QString &)> commitInteractiveDrawVertices;
    std::function<bool(bool)> commitInteractiveDrawSession;
    std::function<bool()> cancelInteractiveDrawingToSelectMode;
    std::function<void()> updateHelpPanel;
    std::function<void()> syncZoomFactorFromView;
    std::function<void(qreal, const QPointF &)> applyZoomAtViewportPosition;
    std::function<void(bool)> fitMapToView;
    std::function<void(bool)> fitMapToViewAfterViewportResize;
    std::function<void(int, int, const QString &, const QPointF &)> prepareSelectionContextMenuState;
    std::function<void(const QPoint &)> showSelectionContextMenu;
    std::function<bool()> insertLineVertexFromSelection;
    std::function<bool()> removeLineVertexFromSelection;
    std::function<bool()> deleteSelectedObjectFromSelection;
    std::function<bool()> toggleLineVertexSmoothFromSelection;
};
}
