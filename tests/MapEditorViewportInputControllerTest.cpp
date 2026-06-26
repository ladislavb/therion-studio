#include "../src/app/text_editor/map_editor/MapEditorViewportInputController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneLifecycleController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runBackspaceDeleteOnMapViewAndViewportTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);

    int removeVertexCalls = 0;
    int deleteObjectCalls = 0;
    QString toolbarStatus;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.removeLineVertexFromSelection = [&removeVertexCalls]() {
        ++removeVertexCalls;
        return true;
    };
    context.deleteSelectedObjectFromSelection = [&deleteObjectCalls]() {
        ++deleteObjectCalls;
        return true;
    };

    MapEditorViewportInputController controller(context);

    QKeyEvent viewBackspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    const std::optional<bool> viewResult = controller.handleEvent(&view, &viewBackspace);
    if (!expect(viewResult.has_value() && viewResult.value(),
                "Backspace on focused map view should be handled by viewport input controller.")) {
        return 1;
    }
    if (!expect(removeVertexCalls == 0 && deleteObjectCalls == 1,
                "Backspace on a non-vertex selection should delete the selected object without deleting a focused segment vertex.")) {
        return 1;
    }

    auto *vertexItem = new MapEditableGeometryVertexItem(77,
                                                         QStringLiteral("line"),
                                                         2,
                                                         QPointF(0.0, 0.0),
                                                         QRectF(-10.0, -10.0, 20.0, 20.0),
                                                         QRectF(-10.0, -10.0, 20.0, 20.0));
    vertexItem->setData(kMapSceneLineNumberRole, 77);
    vertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineAnchor);
    scene.addItem(vertexItem);
    vertexItem->setSelected(true);

    QKeyEvent viewportBackspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    const std::optional<bool> viewportResult = controller.handleEvent(view.viewport(), &viewportBackspace);
    if (!expect(viewportResult.has_value() && viewportResult.value(),
                "Backspace on map viewport should be handled by viewport input controller.")) {
        return 1;
    }
    if (!expect(removeVertexCalls == 1 && deleteObjectCalls == 1,
                "Backspace on a selected line vertex should delete the vertex before falling back to object delete.")) {
        return 1;
    }

    QKeyEvent modifiedBackspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::MetaModifier);
    const std::optional<bool> modifiedResult = controller.handleEvent(&view, &modifiedBackspace);
    if (!expect(!modifiedResult.has_value(),
                "Backspace with modifiers should stay unhandled so platform shortcuts can propagate.")) {
        return 1;
    }
    if (!expect(removeVertexCalls == 1 && deleteObjectCalls == 1,
                "Backspace with modifiers should not trigger delete handlers.")) {
        return 1;
    }

    return 0;
}

int runResizeAutoFitSuppressesCommandSurfaceUpdateTest()
{
    QGraphicsScene scene;
    scene.addRect(QRectF(0.0, 0.0, 100.0, 100.0));
    QGraphicsView view(&scene);
    view.resize(240, 180);

    bool autoFitEnabled = true;
    qreal zoomFactor = 1.0;
    int commandSurfaceUpdates = 0;
    int zoomStatusUpdates = 0;
    QGraphicsScene *scenePointer = &scene;
    QHash<int, QGraphicsItem *> itemsByLine;
    QVector<QGraphicsRectItem *> draftGeometryItems;
    QVector<QGraphicsPixmapItem *> backgroundImageItems;
    bool interactiveDrawStrokeActive = false;
    QGraphicsPathItem *interactiveDrawPreviewPath = nullptr;
    QVector<QGraphicsItem *> interactiveDrawPreviewMarkers;
    bool updatingSelection = false;
    int nextDraftGeometryId = 1;
    int selectedBackgroundLayerIndex = -1;

    MapEditorSceneLifecycleContext context;
    context.scene = &scenePointer;
    context.view = &view;
    context.itemsByLine = &itemsByLine;
    context.draftGeometryItems = &draftGeometryItems;
    context.backgroundImageItems = &backgroundImageItems;
    context.interactiveDrawStrokeActive = &interactiveDrawStrokeActive;
    context.interactiveDrawPreviewPath = &interactiveDrawPreviewPath;
    context.interactiveDrawPreviewMarkers = &interactiveDrawPreviewMarkers;
    context.updatingSelection = &updatingSelection;
    context.nextDraftGeometryId = &nextDraftGeometryId;
    context.selectedBackgroundLayerIndex = &selectedBackgroundLayerIndex;
    context.autoFitEnabled = &autoFitEnabled;
    context.zoomFactor = &zoomFactor;
    context.mapBackgroundFitBounds = []() {
        return QRectF();
    };
    context.updateCommandSurfaceState = [&commandSurfaceUpdates]() {
        ++commandSurfaceUpdates;
    };
    context.zoomPercent = []() {
        return 100;
    };
    context.emitZoomStatusChanged = [&zoomStatusUpdates](int) {
        ++zoomStatusUpdates;
    };

    MapEditorSceneLifecycleController controller(context);
    controller.fitMapToView(false, false);
    if (!expect(commandSurfaceUpdates == 0,
                "Resize autofit lifecycle path should suppress command-surface updates.")) {
        return 1;
    }
    if (!expect(zoomStatusUpdates > 0,
                "Resize autofit lifecycle path should still synchronize zoom status.")) {
        return 1;
    }

    return 0;
}

int runSecondaryClickOpensContextMenuTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);

    QString toolbarStatus;
    bool touchFriendlyControlsEnabled = false;
    bool selectModeActive = true;
    bool autoFitEnabled = true;
    bool fitBackgroundRequested = false;
    bool mapPanActive = false;
    bool mapPanMoved = false;
    bool mapControlPanActive = false;
    QPoint mapPanStartPosition;
    QPoint mapPanLastPosition;
    bool primaryPointerInteractionActive = false;
    bool touchPanCandidate = false;
    bool touchPanActive = false;
    QPointF touchPanStartPosition;
    QPointF touchPanLastPosition;
    QDateTime lastTabletInteractionUtc;
    bool nativeZoomGestureActive = false;
    QDateTime lastNativeZoomGestureUtc;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;
    QVector<QPointF> interactiveDrawSourceVertices;
    QVector<QPointF> interactiveDrawSceneVertices;
    QVector<MapEditorInteractiveLineDraftVertex> interactiveDrawLineVertices;
    bool interactiveDrawStrokeActive = false;
    bool interactiveDrawAnchorPressActive = false;
    QPointF interactiveDrawAnchorPressScenePoint;
    bool interactiveDrawAnchorDragActive = false;
    QPointF interactiveDrawAnchorDragScenePoint;
    bool interactiveDrawControlDragActive = false;
    MapEditorInteractiveLineControlHandleRef interactiveDrawControlDragHandle;
    bool interactiveDrawHoverActive = false;
    QPointF interactiveDrawHoverScenePoint;
    bool interactiveDrawHoverSnapActive = false;
    QPointF interactiveDrawHoverSnapScenePoint;
    int contextMenuCalls = 0;
    int preparedSelectionCalls = 0;
    int preparedLineNumber = 0;
    int preparedVertexIndex = -1;
    QString preparedGeometryKind;
    int commandSurfaceUpdates = 0;
    int zoomSyncs = 0;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.touchFriendlyControlsEnabled = &touchFriendlyControlsEnabled;
    context.selectModeActive = &selectModeActive;
    context.autoFitEnabled = &autoFitEnabled;
    context.fitBackgroundRequested = &fitBackgroundRequested;
    context.mapPanActive = &mapPanActive;
    context.mapPanMoved = &mapPanMoved;
    context.mapControlPanActive = &mapControlPanActive;
    context.mapPanStartPosition = &mapPanStartPosition;
    context.mapPanLastPosition = &mapPanLastPosition;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.touchPanCandidate = &touchPanCandidate;
    context.touchPanActive = &touchPanActive;
    context.touchPanStartPosition = &touchPanStartPosition;
    context.touchPanLastPosition = &touchPanLastPosition;
    context.lastTabletInteractionUtc = &lastTabletInteractionUtc;
    context.nativeZoomGestureActive = &nativeZoomGestureActive;
    context.lastNativeZoomGestureUtc = &lastNativeZoomGestureUtc;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.interactiveDrawSourceVertices = &interactiveDrawSourceVertices;
    context.interactiveDrawSceneVertices = &interactiveDrawSceneVertices;
    context.interactiveDrawLineVertices = &interactiveDrawLineVertices;
    context.interactiveDrawStrokeActive = &interactiveDrawStrokeActive;
    context.interactiveDrawAnchorPressActive = &interactiveDrawAnchorPressActive;
    context.interactiveDrawAnchorPressScenePoint = &interactiveDrawAnchorPressScenePoint;
    context.interactiveDrawAnchorDragActive = &interactiveDrawAnchorDragActive;
    context.interactiveDrawAnchorDragScenePoint = &interactiveDrawAnchorDragScenePoint;
    context.interactiveDrawControlDragActive = &interactiveDrawControlDragActive;
    context.interactiveDrawControlDragHandle = &interactiveDrawControlDragHandle;
    context.interactiveDrawHoverActive = &interactiveDrawHoverActive;
    context.interactiveDrawHoverScenePoint = &interactiveDrawHoverScenePoint;
    context.interactiveDrawHoverSnapActive = &interactiveDrawHoverSnapActive;
    context.interactiveDrawHoverSnapScenePoint = &interactiveDrawHoverSnapScenePoint;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.showSelectionContextMenu = [&contextMenuCalls](const QPoint &) {
        ++contextMenuCalls;
    };
    context.prepareSelectionContextMenuState = [&preparedSelectionCalls,
                                                &preparedLineNumber,
                                                &preparedVertexIndex,
                                                &preparedGeometryKind](int lineNumber,
                                                                       int vertexIndex,
                                                                       const QString &geometryKind,
                                                                       const QPointF &) {
        ++preparedSelectionCalls;
        preparedLineNumber = lineNumber;
        preparedVertexIndex = vertexIndex;
        preparedGeometryKind = geometryKind;
    };
    context.syncZoomFactorFromView = [&zoomSyncs]() {
        ++zoomSyncs;
    };
    context.updateCommandSurfaceState = [&commandSurfaceUpdates]() {
        ++commandSurfaceUpdates;
    };

    MapEditorViewportInputController controller(context);

    const QPoint clickPosition(40, 40);
    const QPointF clickScenePosition = view.mapToScene(clickPosition);
    QGraphicsRectItem *hitItem = scene.addRect(QRectF(clickScenePosition.x() - 8.0,
                                                     clickScenePosition.y() - 8.0,
                                                     16.0,
                                                     16.0));
    hitItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    hitItem->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    hitItem->setData(kMapSceneLineNumberRole, 77);

    QMouseEvent rightPress(QEvent::MouseButtonPress,
                           QPointF(clickPosition),
                           QPointF(view.viewport()->mapToGlobal(clickPosition)),
                           Qt::RightButton,
                           Qt::RightButton,
                           Qt::NoModifier);
    if (!expect(controller.handleEvent(view.viewport(), &rightPress).value_or(false),
                "Right mouse press should be handled as a map pan candidate.")) {
        return 1;
    }

    if (!expect(contextMenuCalls == 0,
                "Right mouse press should not open the map selection context menu immediately.")) {
        return 1;
    }
    if (!expect(mapPanActive && !mapPanMoved,
                "Right mouse press should start pan state without marking movement yet.")) {
        return 1;
    }

    QMouseEvent rightRelease(QEvent::MouseButtonRelease,
                             QPointF(clickPosition),
                             QPointF(view.viewport()->mapToGlobal(clickPosition)),
                             Qt::RightButton,
                             Qt::NoButton,
                             Qt::NoModifier);
    if (!expect(controller.handleEvent(view.viewport(), &rightRelease).value_or(false),
                "Right mouse release should finish a pan candidate.")) {
        return 1;
    }
    if (!expect(!mapPanActive && !mapPanMoved,
                "Right click without movement should not leave pan state active.")) {
        return 1;
    }

    QContextMenuEvent rightClickContextMenu(QContextMenuEvent::Mouse,
                                            clickPosition,
                                            view.viewport()->mapToGlobal(clickPosition));
    if (!expect(controller.handleEvent(view.viewport(), &rightClickContextMenu).value_or(false),
                "Right click context menu event should be handled after the pan candidate release.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Right click without movement should open the map selection context menu.")) {
        return 1;
    }
    if (!expect(hitItem->isSelected() && pendingClickLineNumber == 77,
                "Right click without movement should select the map item under the cursor before opening the context menu.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1 && preparedLineNumber == 77 && preparedVertexIndex == -1,
                "Right click without movement should prepare selection metadata before opening the context menu.")) {
        return 1;
    }

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    controller.showContextMenuAtViewportPosition(clickPosition, view.viewport()->mapToGlobal(clickPosition));
    if (!expect(contextMenuCalls == 1,
                "Viewport custom context-menu request should open the map selection context menu.")) {
        return 1;
    }
    if (!expect(hitItem->isSelected() && pendingClickLineNumber == 77,
                "Viewport custom context-menu request should select the map item under the cursor.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1 && preparedLineNumber == 77,
                "Viewport custom context-menu request should prepare selection metadata.")) {
        return 1;
    }

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    const QPoint emptyClickPosition(210, 150);
    controller.showContextMenuAtViewportPosition(emptyClickPosition, view.viewport()->mapToGlobal(emptyClickPosition));
    if (!expect(contextMenuCalls == 0 && preparedSelectionCalls == 0,
                "Viewport context-menu request on empty canvas should not open the map selection context menu.")) {
        return 1;
    }

    const QPoint panStartPosition(120, 120);
    const QPoint panMovePosition(150, 145);
    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    mapPanActive = false;
    mapPanMoved = false;
    QMouseEvent panPress(QEvent::MouseButtonPress,
                         QPointF(panStartPosition),
                         QPointF(view.viewport()->mapToGlobal(panStartPosition)),
                         Qt::RightButton,
                         Qt::RightButton,
                         Qt::NoModifier);
    if (!expect(controller.handleEvent(view.viewport(), &panPress).value_or(false),
                "Right-button drag should start map pan state.")) {
        return 1;
    }
    QMouseEvent panMove(QEvent::MouseMove,
                        QPointF(panMovePosition),
                        QPointF(view.viewport()->mapToGlobal(panMovePosition)),
                        Qt::NoButton,
                        Qt::RightButton,
                        Qt::NoModifier);
    if (!expect(controller.handleEvent(view.viewport(), &panMove).value_or(false),
                "Right-button drag should pan the map viewport.")) {
        return 1;
    }
    if (!expect(mapPanActive && mapPanMoved && !autoFitEnabled && commandSurfaceUpdates > 0 && zoomSyncs > 0,
                "Right-button drag should mark pan movement, disable auto-fit, and update viewport state.")) {
        return 1;
    }
    QMouseEvent panRelease(QEvent::MouseButtonRelease,
                           QPointF(panMovePosition),
                           QPointF(view.viewport()->mapToGlobal(panMovePosition)),
                           Qt::RightButton,
                           Qt::NoButton,
                           Qt::NoModifier);
    if (!expect(controller.handleEvent(view.viewport(), &panRelease).value_or(false),
                "Right-button drag release should finish map pan state.")) {
        return 1;
    }
    QContextMenuEvent suppressedPanContextMenu(QContextMenuEvent::Mouse,
                                               panMovePosition,
                                               view.viewport()->mapToGlobal(panMovePosition));
    if (!expect(controller.handleEvent(view.viewport(), &suppressedPanContextMenu).value_or(false),
                "Context menu event after right-button pan should be consumed.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 0 && preparedSelectionCalls == 0 && !mapPanActive && !mapPanMoved,
                "Right-button pan should not open a context menu and should reset pan movement after suppressing it.")) {
        return 1;
    }

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    QMouseEvent controlClickPress(QEvent::MouseButtonPress,
                                  QPointF(clickPosition),
                                  QPointF(view.viewport()->mapToGlobal(clickPosition)),
                                  Qt::LeftButton,
                                  Qt::LeftButton,
                                  Qt::ControlModifier);
    if (!expect(controller.handleEvent(view.viewport(), &controlClickPress).value_or(false),
                "Control-left press should be handled as a map pan candidate.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 0,
                "Control-left press should not open the map selection context menu immediately.")) {
        return 1;
    }
    if (!expect(mapPanActive && !mapPanMoved,
                "Control-left press should start pan state without marking movement yet.")) {
        return 1;
    }
    QMouseEvent controlClickRelease(QEvent::MouseButtonRelease,
                                    QPointF(clickPosition),
                                    QPointF(view.viewport()->mapToGlobal(clickPosition)),
                                    Qt::LeftButton,
                                    Qt::NoButton,
                                    Qt::ControlModifier);
    if (!expect(controller.handleEvent(view.viewport(), &controlClickRelease).value_or(false),
                "Control-left release without movement should finish a pan candidate.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Control-left click without movement should open the map selection context menu.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1 && preparedLineNumber == 77,
                "Control-left click without movement should prepare selection metadata before opening the context menu.")) {
        return 1;
    }

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    QContextMenuEvent trackpadContextMenu(QContextMenuEvent::Mouse,
                                          clickPosition,
                                          view.viewport()->mapToGlobal(clickPosition));
    if (!expect(controller.handleEvent(view.viewport(), &trackpadContextMenu).value_or(false),
                "Trackpad-style context menu event should be handled by the map viewport controller.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Trackpad-style context menu event should open the map selection context menu.")) {
        return 1;
    }
    if (!expect(hitItem->isSelected() && pendingClickLineNumber == 77,
                "Trackpad-style context menu event should select the map item under the cursor.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1 && preparedLineNumber == 77,
                "Trackpad-style context menu event should prepare selection metadata.")) {
        return 1;
    }

    const QPoint vertexClickPosition(90, 70);
    const QPointF vertexScenePosition = view.mapToScene(vertexClickPosition);
    const QRectF stableBounds(vertexScenePosition.x() - 100.0,
                              vertexScenePosition.y() - 100.0,
                              200.0,
                              200.0);
    auto *hiddenVertexItem = new MapEditableGeometryVertexItem(88,
                                                               QStringLiteral("line"),
                                                               7,
                                                               vertexScenePosition,
                                                               stableBounds,
                                                               stableBounds);
    hiddenVertexItem->setData(kMapSceneLineNumberRole, 88);
    hiddenVertexItem->setData(kMapSceneSelectionGatedRole, true);
    hiddenVertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineAnchor);
    hiddenVertexItem->setData(kMapSceneOwnerVertexRole, 7);
    hiddenVertexItem->setVisible(false);
    scene.addItem(hiddenVertexItem);

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    pendingClickSourceVertexIndex = -1;
    pendingClickGeometryKind.clear();
    QContextMenuEvent vertexTrackpadContextMenu(QContextMenuEvent::Mouse,
                                                vertexClickPosition,
                                                view.viewport()->mapToGlobal(vertexClickPosition));
    if (!expect(controller.handleEvent(view.viewport(), &vertexTrackpadContextMenu).value_or(false),
                "Trackpad-style context menu event on a gated line vertex should be handled.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Trackpad-style context menu event on a gated line vertex should open the context menu.")) {
        return 1;
    }
    if (!expect(hiddenVertexItem->isSelected() && pendingClickLineNumber == 88,
                "Trackpad-style context menu event should select the gated line vertex under the cursor.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1
                    && preparedLineNumber == 88
                    && preparedVertexIndex == 7
                    && preparedGeometryKind == QStringLiteral("line"),
                "Trackpad-style context menu event should prepare gated line vertex metadata.")) {
        return 1;
    }

    const QPoint pointClickPosition(130, 90);
    const QPointF pointScenePosition = view.mapToScene(pointClickPosition);
    const QRectF pointStableBounds(pointScenePosition.x() - 100.0,
                                   pointScenePosition.y() - 100.0,
                                   200.0,
                                   200.0);
    auto *nearbyVertexItem = new MapEditableGeometryVertexItem(98,
                                                               QStringLiteral("line"),
                                                               3,
                                                               pointScenePosition + QPointF(6.0, 0.0),
                                                               pointStableBounds,
                                                               pointStableBounds);
    nearbyVertexItem->setData(kMapSceneLineNumberRole, 98);
    nearbyVertexItem->setData(kMapSceneSelectionGatedRole, true);
    nearbyVertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineAnchor);
    nearbyVertexItem->setData(kMapSceneOwnerVertexRole, 3);
    scene.addItem(nearbyVertexItem);

    auto *nearbyPointItem = new MapEditablePointItem(99,
                                                     pointScenePosition,
                                                     pointStableBounds,
                                                     pointStableBounds);
    nearbyPointItem->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    nearbyPointItem->setData(kMapSceneLineNumberRole, 99);
    scene.addItem(nearbyPointItem);

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    pendingClickSourceVertexIndex = -1;
    pendingClickGeometryKind.clear();
    QContextMenuEvent pointNearVertexContextMenu(QContextMenuEvent::Mouse,
                                                 pointClickPosition,
                                                 view.viewport()->mapToGlobal(pointClickPosition));
    if (!expect(controller.handleEvent(view.viewport(), &pointNearVertexContextMenu).value_or(false),
                "Trackpad-style context menu event on a point near a line vertex should be handled.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Trackpad-style context menu event on a point near a line vertex should open the context menu.")) {
        return 1;
    }
    if (!expect(nearbyPointItem->isSelected() && !nearbyVertexItem->isSelected() && pendingClickLineNumber == 99,
                "Context menu selection should keep a directly hit point ahead of a nearby line vertex fallback.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1
                    && preparedLineNumber == 99
                    && preparedVertexIndex == -1
                    && preparedGeometryKind.isEmpty(),
                "Context menu selection on a point near a line vertex should prepare point metadata.")) {
        return 1;
    }

    return 0;
}

int runNearestPathWinsPrimaryClickTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool primaryPointerInteractionActive = false;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;
    int selectionSyncCalls = 0;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.handleInteractiveDrawClick = [](const QPointF &) {
        return false;
    };
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};
    context.syncMapSelectionFromScene = [&selectionSyncCalls, &pendingClickLineNumber]() {
        ++selectionSyncCalls;
        expect(pendingClickLineNumber == 91,
               "Selection sync after a path click should still see the pending clicked source line.");
    };

    MapEditorViewportInputController controller(context);

    QPainterPath wallPath;
    wallPath.moveTo(10.0, 56.0);
    wallPath.lineTo(90.0, 56.0);
    auto *wallItem = scene.addPath(wallPath, QPen(Qt::black, 8.0));
    wallItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    wallItem->setAcceptedMouseButtons(Qt::LeftButton);
    wallItem->setData(kMapSceneLineNumberRole, 601);
    wallItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeGeneric);
    wallItem->setSelected(true);

    QPainterPath contourPath;
    contourPath.moveTo(10.0, 40.0);
    contourPath.lineTo(90.0, 40.0);
    auto *contourItem = scene.addPath(contourPath, QPen(Qt::cyan, 2.0));
    contourItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    contourItem->setAcceptedMouseButtons(Qt::LeftButton);
    contourItem->setData(kMapSceneLineNumberRole, 91);
    contourItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineDetail);

    int emptySelectionUpdates = 0;
    QObject::connect(&scene, &QGraphicsScene::selectionChanged, [&scene, &emptySelectionUpdates]() {
        if (scene.selectedItems().isEmpty()) {
            ++emptySelectionUpdates;
        }
    });

    const QPoint clickPosition = view.mapFromScene(QPointF(50.0, 40.0));
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(clickPosition),
                      QPointF(view.viewport()->mapToGlobal(clickPosition)),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    if (!expect(controller.handleEvent(view.viewport(), &press).value_or(false),
                "Primary click on a distance-ranked path should be handled by the viewport input controller.")) {
        return 1;
    }
    if (!expect(contourItem->isSelected() && !wallItem->isSelected(),
                "Primary click should select the nearest path instead of an overlapping selected wall.")) {
        return 1;
    }
    if (!expect(pendingClickLineNumber == 91,
                "Primary click on the nearest contour should store the contour source line as pending selection.")) {
        return 1;
    }
    if (!expect(emptySelectionUpdates == 0,
                "Switching from one path to another should not emit an intermediate empty selection.")) {
        return 1;
    }
    if (!expect(selectionSyncCalls == 1,
                "Primary path click should explicitly sync map selection once after the atomic selection update.")) {
        return 1;
    }

    return 0;
}

int runHoveredPathWinsPrimaryClickTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool primaryPointerInteractionActive = false;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;
    int selectionSyncCalls = 0;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.handleInteractiveDrawClick = [](const QPointF &) {
        return false;
    };
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};
    context.syncMapSelectionFromScene = [&selectionSyncCalls, &pendingClickLineNumber]() {
        ++selectionSyncCalls;
        expect(pendingClickLineNumber == 91,
               "Selection sync after a highlighted path click should still see the pending highlighted source line.");
    };

    MapEditorViewportInputController controller(context);

    QPainterPath wallPath;
    wallPath.moveTo(10.0, 42.0);
    wallPath.lineTo(90.0, 42.0);
    auto *wallItem = scene.addPath(wallPath, QPen(Qt::black, 8.0));
    wallItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    wallItem->setAcceptedMouseButtons(Qt::LeftButton);
    wallItem->setData(kMapSceneLineNumberRole, 601);
    wallItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeGeneric);
    wallItem->setSelected(true);

    QPainterPath contourPath;
    contourPath.moveTo(10.0, 40.0);
    contourPath.lineTo(90.0, 40.0);
    auto *contourItem = scene.addPath(contourPath, QPen(Qt::cyan, 2.0));
    contourItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    contourItem->setAcceptedMouseButtons(Qt::LeftButton);
    contourItem->setData(kMapSceneLineNumberRole, 91);
    contourItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineDetail);
    contourItem->setData(kMapSceneInteractionHoverRole, true);

    const QPoint clickPosition = view.mapFromScene(QPointF(50.0, 41.0));
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(clickPosition),
                      QPointF(view.viewport()->mapToGlobal(clickPosition)),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    if (!expect(controller.handleEvent(view.viewport(), &press).value_or(false),
                "Primary click on a highlighted path should be handled by the viewport input controller.")) {
        return 1;
    }
    if (!expect(contourItem->isSelected() && !wallItem->isSelected(),
                "Primary click should select the highlighted path instead of recomputing a different target.")) {
        return 1;
    }
    if (!expect(pendingClickLineNumber == 91,
                "Primary click on a highlighted contour should store the contour source line as pending selection.")) {
        return 1;
    }
    if (!expect(selectionSyncCalls == 1,
                "Primary highlighted path click should explicitly sync map selection once after the atomic selection update.")) {
        return 1;
    }

    return 0;
}

int runVisibleVertexPressFallsThroughForDragTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool primaryPointerInteractionActive = false;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.handleInteractiveDrawClick = [](const QPointF &) {
        return false;
    };
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};

    MapEditorViewportInputController controller(context);

    QPainterPath wallPath;
    wallPath.moveTo(10.0, 40.0);
    wallPath.lineTo(90.0, 40.0);
    auto *wallItem = scene.addPath(wallPath, QPen(Qt::black, 8.0));
    wallItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    wallItem->setAcceptedMouseButtons(Qt::LeftButton);
    wallItem->setData(kMapSceneLineNumberRole, 601);
    wallItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeGeneric);
    wallItem->setSelected(true);

    const QPointF vertexScenePosition(50.0, 40.0);
    const QRectF stableBounds(vertexScenePosition.x() - 100.0,
                              vertexScenePosition.y() - 100.0,
                              200.0,
                              200.0);
    auto *vertexItem = new MapEditableGeometryVertexItem(601,
                                                         QStringLiteral("line"),
                                                         7,
                                                         vertexScenePosition,
                                                         stableBounds,
                                                         stableBounds);
    vertexItem->setRect(QRectF(-3.4, -3.4, 6.8, 6.8));
    vertexItem->setData(kMapSceneLineNumberRole, 601);
    vertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineAnchor);
    vertexItem->setData(kMapSceneOwnerVertexRole, 7);
    vertexItem->setVisible(true);
    scene.addItem(vertexItem);

    const QPoint clickPosition = view.mapFromScene(vertexScenePosition);
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(clickPosition),
                      QPointF(view.viewport()->mapToGlobal(clickPosition)),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    const std::optional<bool> handled = controller.handleEvent(view.viewport(), &press);
    if (!expect(!handled.has_value(),
                "Primary press on a visible vertex should fall through so the scene can start dragging it.")) {
        return 1;
    }
    if (!expect(pendingClickSelection
                    && pendingClickLineNumber == 601
                    && pendingClickSourceVertexIndex == 7
                    && pendingClickGeometryKind == QStringLiteral("line"),
                "Primary press on a visible vertex should preserve pending metadata for selection restoration.")) {
        return 1;
    }

    return 0;
}

int runNearVisibleVertexPressFallsThroughForDragTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool primaryPointerInteractionActive = false;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;
    int selectionSyncCalls = 0;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.handleInteractiveDrawClick = [](const QPointF &) {
        return false;
    };
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};
    context.syncMapSelectionFromScene = [&selectionSyncCalls]() {
        ++selectionSyncCalls;
    };

    MapEditorViewportInputController controller(context);

    QPainterPath wallPath;
    wallPath.moveTo(10.0, 40.0);
    wallPath.lineTo(90.0, 40.0);
    auto *wallItem = scene.addPath(wallPath, QPen(Qt::black, 12.0));
    wallItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    wallItem->setAcceptedMouseButtons(Qt::LeftButton);
    wallItem->setData(kMapSceneLineNumberRole, 601);
    wallItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeGeneric);
    wallItem->setSelected(true);

    const QPointF controlScenePosition(50.0, 40.0);
    const QRectF stableBounds(0.0, 0.0, 100.0, 80.0);
    auto *controlItem = new MapEditableGeometryVertexItem(601,
                                                          QStringLiteral("line control"),
                                                          2,
                                                          controlScenePosition,
                                                          stableBounds,
                                                          stableBounds);
    controlItem->setRect(QRectF(-2.4, -2.4, 4.8, 4.8));
    controlItem->setData(kMapSceneLineNumberRole, 601);
    controlItem->setData(kMapSceneSelectionGatedRole, true);
    controlItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControl);
    controlItem->setData(kMapSceneOwnerVertexRole, 1);
    scene.addItem(controlItem);

    const QPoint controlCenter = view.mapFromScene(controlScenePosition);
    const QPoint nearControlButOutsideSmallRect = controlCenter + QPoint(9, 0);
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(nearControlButOutsideSmallRect),
                      QPointF(view.viewport()->mapToGlobal(nearControlButOutsideSmallRect)),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    const std::optional<bool> handled = controller.handleEvent(view.viewport(), &press);
    if (!expect(!handled.has_value(),
                "A thick selected path must not steal press events near a visible Bezier control handle affordance.")) {
        return 1;
    }
    if (!expect(pendingClickSelection
                    && pendingClickLineNumber == 601
                    && pendingClickSourceVertexIndex == 1
                    && pendingClickGeometryKind == QStringLiteral("line"),
                "Primary press near a visible Bezier control should preserve pending metadata for its owner vertex.")) {
        return 1;
    }
    if (!expect(selectionSyncCalls == 0 && wallItem->isSelected(),
                "Near-handle press should fall through to QGraphicsItem drag handling instead of reselecting the path.")) {
        return 1;
    }

    return 0;
}

int runHoveredPathDoesNotStealVisibleVertexPressTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool primaryPointerInteractionActive = false;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.handleInteractiveDrawClick = [](const QPointF &) {
        return false;
    };
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};

    MapEditorViewportInputController controller(context);

    QPainterPath wallPath;
    wallPath.moveTo(10.0, 40.0);
    wallPath.lineTo(90.0, 40.0);
    auto *wallItem = scene.addPath(wallPath, QPen(Qt::black, 8.0));
    wallItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    wallItem->setAcceptedMouseButtons(Qt::LeftButton);
    wallItem->setData(kMapSceneLineNumberRole, 601);
    wallItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeGeneric);
    wallItem->setData(kMapSceneInteractionHoverRole, true);
    wallItem->setSelected(true);

    const QPointF vertexScenePosition(50.0, 40.0);
    const QRectF stableBounds(vertexScenePosition.x() - 100.0,
                              vertexScenePosition.y() - 100.0,
                              200.0,
                              200.0);
    auto *vertexItem = new MapEditableGeometryVertexItem(601,
                                                         QStringLiteral("line"),
                                                         7,
                                                         vertexScenePosition,
                                                         stableBounds,
                                                         stableBounds);
    vertexItem->setRect(QRectF(-3.4, -3.4, 6.8, 6.8));
    vertexItem->setData(kMapSceneLineNumberRole, 601);
    vertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineAnchor);
    vertexItem->setData(kMapSceneOwnerVertexRole, 7);
    vertexItem->setVisible(true);
    scene.addItem(vertexItem);

    const QPoint clickPosition = view.mapFromScene(vertexScenePosition);
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(clickPosition),
                      QPointF(view.viewport()->mapToGlobal(clickPosition)),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    const std::optional<bool> handled = controller.handleEvent(view.viewport(), &press);
    if (!expect(!handled.has_value(),
                "A highlighted path must not steal primary press events from its visible vertex handles.")) {
        return 1;
    }
    if (!expect(pendingClickSelection
                    && pendingClickLineNumber == 601
                    && pendingClickSourceVertexIndex == 7
                    && pendingClickGeometryKind == QStringLiteral("line"),
                "Primary press on a visible vertex ahead of a highlighted path should preserve pending metadata.")) {
        return 1;
    }

    return 0;
}

int runHoveredPathDoesNotStealLinePointHandlePressTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool primaryPointerInteractionActive = false;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.handleInteractiveDrawClick = [](const QPointF &) {
        return false;
    };
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};

    MapEditorViewportInputController controller(context);

    QPainterPath contourPath;
    contourPath.moveTo(10.0, 40.0);
    contourPath.lineTo(90.0, 40.0);
    auto *hoveredPathItem = scene.addPath(contourPath, QPen(Qt::cyan, 2.0));
    hoveredPathItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    hoveredPathItem->setAcceptedMouseButtons(Qt::LeftButton);
    hoveredPathItem->setData(kMapSceneLineNumberRole, 91);
    hoveredPathItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineDetail);
    hoveredPathItem->setData(kMapSceneInteractionHoverRole, true);

    const QPointF handleScenePosition(50.0, 40.0);
    auto *linePointHandle = scene.addRect(QRectF(handleScenePosition.x() - 5.0,
                                                handleScenePosition.y() - 5.0,
                                                10.0,
                                                10.0),
                                         QPen(Qt::red),
                                         QBrush(Qt::yellow));
    linePointHandle->setFlag(QGraphicsItem::ItemIsSelectable, true);
    linePointHandle->setAcceptedMouseButtons(Qt::LeftButton);
    linePointHandle->setData(kMapSceneLineNumberRole, 91);
    linePointHandle->setData(kMapSceneOwnerVertexRole, 4);
    linePointHandle->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControlConnector);

    const QPoint clickPosition = view.mapFromScene(handleScenePosition);
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(clickPosition),
                      QPointF(view.viewport()->mapToGlobal(clickPosition)),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    const std::optional<bool> handled = controller.handleEvent(view.viewport(), &press);
    if (!expect(!handled.has_value(),
                "A highlighted path must not steal primary press events from a visible line-point orientation handle.")) {
        return 1;
    }
    if (!expect(pendingClickSelection
                    && pendingClickLineNumber == 91
                    && pendingClickSourceVertexIndex == 4
                    && pendingClickGeometryKind == QStringLiteral("line"),
                "Primary press on a visible line-point handle should preserve pending metadata for its owner vertex.")) {
        return 1;
    }

    return 0;
}

int runSlopeLinePointHandleShapeExcludesAnchorAndConnectorTest()
{
    const QPointF anchorPreview(100.0, 100.0);
    constexpr qreal orientationDegrees = 35.0;
    constexpr qreal leftSize = 40.0;
    constexpr qreal previewScale = 1.0;
    MapLinePointSizeHandleItem handleItem(18,
                                          2,
                                          anchorPreview,
                                          orientationDegrees,
                                          leftSize,
                                          previewScale);
    if (!expect(!(handleItem.flags() & QGraphicsItem::ItemIsSelectable),
                "Slope line-point orientation handle should stay out of map selection hit-testing.")) {
        return 1;
    }

    const QPainterPath shape = handleItem.shape();
    if (!expect(!shape.contains(anchorPreview),
                "Slope line-point orientation handle hit shape must not include the anchor vertex.")) {
        return 1;
    }

    constexpr qreal pi = 3.14159265358979323846;
    const qreal radians = orientationDegrees * pi / 180.0;
    const qreal displayLength = qMax<qreal>(12.0, qMax<qreal>(0.1, leftSize) * previewScale);
    const QPointF arrowTip = anchorPreview + QPointF(std::sin(radians) * displayLength,
                                                     -std::cos(radians) * displayLength);
    const QPointF direction = (arrowTip - anchorPreview) / displayLength;
    const QPointF normal(-direction.y(), direction.x());
    const QPointF arrowBase = arrowTip - (direction * 9.0);
    const QPointF connectorMidpoint = anchorPreview + ((arrowTip - anchorPreview) * 0.5);
    if (!expect(!shape.contains(connectorMidpoint),
                "Slope line-point orientation handle hit shape must not include the connector segment.")) {
        return 1;
    }

    if (!expect(shape.contains(arrowTip),
                "Slope line-point orientation handle hit shape should include the arrow tip.")) {
        return 1;
    }

    const QPointF arrowHeadCenter = (arrowTip + arrowBase + (arrowBase + (normal * 6.0))) / 3.0;
    if (!expect(shape.contains(arrowHeadCenter),
                "Slope line-point orientation handle hit shape should include the arrow-head center.")) {
        return 1;
    }

    if (!expect(shape.contains(arrowBase + (normal * 4.0))
                    && shape.contains(arrowBase - (normal * 4.0)),
                "Slope line-point orientation handle hit shape should include both arrow-head flanks.")) {
        return 1;
    }

    return 0;
}

int runHoveredPathDoesNotStealSlopeOrientationHandlePressTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool primaryPointerInteractionActive = false;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.handleInteractiveDrawClick = [](const QPointF &) {
        return false;
    };
    context.refreshToolbarSummary = []() {};
    context.updateCommandSurfaceState = []() {};

    MapEditorViewportInputController controller(context);

    QPainterPath contourPath;
    contourPath.moveTo(10.0, 40.0);
    contourPath.lineTo(90.0, 40.0);
    auto *hoveredPathItem = scene.addPath(contourPath, QPen(Qt::cyan, 2.0));
    hoveredPathItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    hoveredPathItem->setAcceptedMouseButtons(Qt::LeftButton);
    hoveredPathItem->setData(kMapSceneLineNumberRole, 18);
    hoveredPathItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineDetail);
    hoveredPathItem->setData(kMapSceneInteractionHoverRole, true);

    const QPointF anchorPreview(50.0, 40.0);
    auto *handleItem = new MapLinePointSizeHandleItem(18,
                                                      2,
                                                      anchorPreview,
                                                      35.0,
                                                      40.0,
                                                      1.0);
    scene.addItem(handleItem);
    handleItem->setZValue(4.7);
    handleItem->setData(kMapSceneLineNumberRole, 18);
    handleItem->setData(kMapSceneOwnerVertexRole, 2);
    handleItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControlConnector);
    handleItem->setVisible(true);

    const QPoint clickPosition = view.mapFromScene(handleItem->boundingRect().center());
    QMouseEvent press(QEvent::MouseButtonPress,
                      QPointF(clickPosition),
                      QPointF(view.viewport()->mapToGlobal(clickPosition)),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    const std::optional<bool> handled = controller.handleEvent(view.viewport(), &press);
    if (!expect(!handled.has_value(),
                "A highlighted path must not steal primary press events from a slope orientation handle.")) {
        return 1;
    }
    if (!expect(pendingClickSelection
                    && pendingClickLineNumber == 18
                    && pendingClickSourceVertexIndex == 2
                    && pendingClickGeometryKind == QStringLiteral("line"),
                "Primary press on a slope orientation handle should preserve pending metadata for its owner vertex.")) {
        return 1;
    }

    return 0;
}

}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int rc = runBackspaceDeleteOnMapViewAndViewportTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSecondaryClickOpensContextMenuTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runNearestPathWinsPrimaryClickTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runHoveredPathWinsPrimaryClickTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runVisibleVertexPressFallsThroughForDragTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runNearVisibleVertexPressFallsThroughForDragTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runHoveredPathDoesNotStealVisibleVertexPressTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runHoveredPathDoesNotStealLinePointHandlePressTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSlopeLinePointHandleShapeExcludesAnchorAndConnectorTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runHoveredPathDoesNotStealSlopeOrientationHandlePressTest(); rc != 0) {
        return rc;
    }
    return runResizeAutoFitSuppressesCommandSurfaceUpdateTest();
}
