#include "../src/app/text_editor/map_editor/MapEditorViewportInputController.h"

#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QTest>

using namespace TherionStudio;

namespace
{
class MapEditorViewportTabletInputTest final : public QObject
{
    Q_OBJECT

private slots:
    void tabletLineClickCapturesSingleVertexAndSuppressesSyntheticMouse();
    void recentTabletInputDoesNotSuppressMouseReleaseCompletion();
};

void MapEditorViewportTabletInputTest::tabletLineClickCapturesSingleVertexAndSuppressesSyntheticMouse()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool selectModeActive = false;
    bool primaryPointerInteractionActive = false;
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
    QVector<QPointF> interactiveDrawSourceVertices;
    QVector<QPointF> interactiveDrawSceneVertices;
    QVector<MapEditorInteractiveLineDraftVertex> interactiveDrawLineVertices;
    QDateTime lastTabletInteractionUtc;
    int previewUpdates = 0;
    int toolbarRefreshes = 0;
    int commandSurfaceUpdates = 0;
    int capturedAnchors = 0;
    QPointF capturedAnchorScenePoint;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.selectModeActive = &selectModeActive;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.lastTabletInteractionUtc = &lastTabletInteractionUtc;
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
    context.drawMode = []() {
        return MapEditorInteractiveDrawMode::Line;
    };
    context.sourcePointFromScenePosition = [](const QPointF &scenePoint) {
        return scenePoint;
    };
    context.updateInteractiveDrawPreview = [&previewUpdates]() {
        ++previewUpdates;
    };
    context.refreshToolbarSummary = [&toolbarRefreshes]() {
        ++toolbarRefreshes;
    };
    context.updateCommandSurfaceState = [&commandSurfaceUpdates]() {
        ++commandSurfaceUpdates;
    };
    context.interactiveLineControlAt = [](const QPointF &, qreal) {
        return std::optional<MapEditorInteractiveLineControlHandleRef>();
    };
    context.handleInteractiveDrawClick = [](const QPointF &) {
        return false;
    };
    context.captureInteractiveLineAnchor = [&capturedAnchors, &capturedAnchorScenePoint](
                                               const QPointF &anchorScenePoint,
                                               const std::optional<QPointF> &) {
        ++capturedAnchors;
        capturedAnchorScenePoint = anchorScenePoint;
    };

    MapEditorViewportInputController controller(context);
    const QPoint viewportPosition(80, 60);
    const QPointF globalPosition = view.viewport()->mapToGlobal(viewportPosition);
    const QPointingDevice tabletDevice(QStringLiteral("test stylus"),
                                       1,
                                       QInputDevice::DeviceType::Stylus,
                                       QPointingDevice::PointerType::Pen,
                                       QInputDevice::Capability::Position | QInputDevice::Capability::Pressure,
                                       1,
                                       1);

    QTabletEvent tabletPress(QEvent::TabletPress,
                             &tabletDevice,
                             QPointF(viewportPosition),
                             globalPosition,
                             0.7,
                             0.0F,
                             0.0F,
                             0.0,
                             0.0,
                             0.0F,
                             Qt::NoModifier,
                             Qt::LeftButton,
                             Qt::LeftButton);
    QVERIFY(controller.handleEvent(view.viewport(), &tabletPress).value_or(false));
    QVERIFY(interactiveDrawAnchorPressActive);

    QTabletEvent tabletRelease(QEvent::TabletRelease,
                               &tabletDevice,
                               QPointF(viewportPosition),
                               globalPosition,
                               0.0,
                               0.0F,
                               0.0F,
                               0.0,
                               0.0,
                               0.0F,
                               Qt::NoModifier,
                               Qt::LeftButton,
                               Qt::NoButton);
    QVERIFY(controller.handleEvent(view.viewport(), &tabletRelease).value_or(false));
    QCOMPARE(capturedAnchors, 1);
    QCOMPARE(capturedAnchorScenePoint, view.mapToScene(viewportPosition));
    QVERIFY(lastTabletInteractionUtc.isValid());

    QMouseEvent syntheticMousePress(QEvent::MouseButtonPress,
                                    QPointF(viewportPosition),
                                    globalPosition,
                                    Qt::LeftButton,
                                    Qt::LeftButton,
                                    Qt::NoModifier);
    QVERIFY(controller.handleEvent(view.viewport(), &syntheticMousePress).value_or(false));
    QCOMPARE(capturedAnchors, 1);
    QVERIFY(!interactiveDrawAnchorPressActive);
    QVERIFY(previewUpdates > 0);
    QVERIFY(toolbarRefreshes > 0);
    QVERIFY(commandSurfaceUpdates > 0);
}

void MapEditorViewportTabletInputTest::recentTabletInputDoesNotSuppressMouseReleaseCompletion()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);
    view.show();

    QString toolbarStatus;
    bool selectModeActive = false;
    bool primaryPointerInteractionActive = false;
    bool interactiveDrawStrokeActive = true;
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
    QVector<QPointF> interactiveDrawSourceVertices{QPointF(1.0, 2.0), QPointF(3.0, 4.0)};
    QVector<QPointF> interactiveDrawSceneVertices{QPointF(1.0, 2.0), QPointF(3.0, 4.0)};
    QVector<MapEditorInteractiveLineDraftVertex> interactiveDrawLineVertices;
    QDateTime lastTabletInteractionUtc = QDateTime::currentDateTimeUtc();
    int committedStrokes = 0;
    int clearedSessions = 0;
    int helpUpdates = 0;
    int toolbarRefreshes = 0;
    int commandSurfaceUpdates = 0;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.selectModeActive = &selectModeActive;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.lastTabletInteractionUtc = &lastTabletInteractionUtc;
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
    context.drawMode = []() {
        return MapEditorInteractiveDrawMode::Freehand;
    };
    context.sourcePointFromScenePosition = [](const QPointF &scenePoint) {
        return scenePoint;
    };
    context.clearInteractiveDrawSession = [&clearedSessions,
                                           &interactiveDrawStrokeActive,
                                           &interactiveDrawSourceVertices,
                                           &interactiveDrawSceneVertices](bool) {
        ++clearedSessions;
        interactiveDrawStrokeActive = false;
        interactiveDrawSourceVertices.clear();
        interactiveDrawSceneVertices.clear();
    };
    context.commitInteractiveDrawVertices = [&committedStrokes](const QString &type,
                                                                const QVector<QPointF> &vertices,
                                                                const QString &undoLabel) -> bool {
        if (type != QStringLiteral("line")
            || undoLabel != QStringLiteral("freehand line")
            || vertices.size() < 2) {
            return false;
        }
        ++committedStrokes;
        return true;
    };
    context.updateHelpPanel = [&helpUpdates]() {
        ++helpUpdates;
    };
    context.refreshToolbarSummary = [&toolbarRefreshes]() {
        ++toolbarRefreshes;
    };
    context.updateCommandSurfaceState = [&commandSurfaceUpdates]() {
        ++commandSurfaceUpdates;
    };

    MapEditorViewportInputController controller(context);
    const QPoint viewportPosition(90, 70);
    const QPointF globalPosition = view.viewport()->mapToGlobal(viewportPosition);
    QMouseEvent mouseRelease(QEvent::MouseButtonRelease,
                             QPointF(viewportPosition),
                             globalPosition,
                             Qt::LeftButton,
                             Qt::NoButton,
                             Qt::NoModifier);

    QVERIFY(controller.handleEvent(view.viewport(), &mouseRelease).value_or(false));
    QCOMPARE(committedStrokes, 1);
    QCOMPARE(clearedSessions, 1);
    QCOMPARE(helpUpdates, 1);
    QVERIFY(toolbarRefreshes > 0);
    QVERIFY(commandSurfaceUpdates > 0);
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MapEditorViewportTabletInputTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorViewportTabletInputTest.moc"
