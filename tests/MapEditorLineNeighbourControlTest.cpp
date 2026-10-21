#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorSelectionController.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QtTest/QtTest>

#include <cmath>

using namespace TherionStudio;

namespace
{
constexpr int kLineNumber = 4;
const QRectF kSourceBounds(0.0, -100.0, 100.0, 100.0);
const QRectF kPreviewBounds(0.0, 0.0, 100.0, 100.0);

MapEditableGeometryVertexItem *addLineItem(QGraphicsScene *scene,
                                           const QString &geometryKind,
                                           int subtype,
                                           int sourceVertexIndex,
                                           int ownerVertexIndex,
                                           std::optional<int> neighbourVertexIndex)
{
    auto *item = new MapEditableGeometryVertexItem(kLineNumber,
                                                   geometryKind,
                                                   sourceVertexIndex,
                                                   QPointF(sourceVertexIndex, -50.0),
                                                   kSourceBounds,
                                                   kPreviewBounds);
    item->setData(kMapSceneLineNumberRole, kLineNumber);
    item->setData(kMapSceneSelectionGatedRole, true);
    item->setData(kMapSceneSelectionSubtypeRole, subtype);
    item->setData(kMapSceneOwnerVertexRole, ownerVertexIndex);
    if (neighbourVertexIndex.has_value()) {
        item->setData(kMapSceneSegmentNeighbourVertexRole, neighbourVertexIndex.value());
    }
    item->setVisible(false);
    scene->addItem(item);
    return item;
}

// Scene state and selection context, as the map editor tab owns them.
struct SelectionFixture
{
    QGraphicsScene scene;
    QGraphicsView view;
    QHash<int, QGraphicsItem *> itemsByLine;
    QSet<int> hiddenObjectLines;
    QSet<int> suppressedAutoReselectLineNumbers;
    bool updatingSelection = false;
    bool autoFitEnabled = false;
    bool textNavigationInProgress = false;
    int lastCursorSyncedLine = 0;
    int lastCursorSyncedColumn = 0;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;
    int sceneRefreshSelectionLineNumber = 0;
    int sceneRefreshSelectionVertexIndex = -1;
    QString sceneRefreshSelectionGeometryKind;
    int selectedObjectLineNumber = kLineNumber;
    int selectedObjectVertexIndex = -1;
    int selectedLineSegmentStartVertexIndex = -1;
    int selectedLineSegmentEndVertexIndex = -1;
    QString selectedObjectKind = QStringLiteral("line");
    std::optional<QPointF> selectedObjectCoordinate;

    // Three points (anchors 10, 20, 30) with both handles facing each segment.
    MapEditableGeometryVertexItem *anchors[3] = {};
    MapEditableGeometryVertexItem *firstOutgoing = nullptr;
    MapEditableGeometryVertexItem *middleIncoming = nullptr;
    MapEditableGeometryVertexItem *middleOutgoing = nullptr;
    MapEditableGeometryVertexItem *lastIncoming = nullptr;
    QGraphicsLineItem *firstOutgoingConnector = nullptr;

    SelectionFixture()
    {
        view.setScene(&scene);
        for (int order = 0; order < 3; ++order) {
            const int anchorIndex = (order + 1) * 10;
            anchors[order] = addLineItem(&scene, QStringLiteral("line"), kMapSceneSelectionSubtypeLineAnchor,
                                         anchorIndex, anchorIndex, std::nullopt);
        }
        const QString control = QStringLiteral("line control");
        const int subtype = kMapSceneSelectionSubtypeLineControl;
        firstOutgoing = addLineItem(&scene, control, subtype, 11, 10, 20);
        middleIncoming = addLineItem(&scene, control, subtype, 19, 20, 10);
        middleOutgoing = addLineItem(&scene, control, subtype, 21, 20, 30);
        lastIncoming = addLineItem(&scene, control, subtype, 29, 30, 20);

        firstOutgoingConnector = scene.addLine(QLineF(0.0, 0.0, 5.0, 0.0));
        firstOutgoingConnector->setData(kMapSceneLineNumberRole, kLineNumber);
        firstOutgoingConnector->setData(kMapSceneSelectionGatedRole, true);
        firstOutgoingConnector->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineControlConnector);
        firstOutgoingConnector->setData(kMapSceneOwnerVertexRole, 10);
        firstOutgoingConnector->setData(kMapSceneSegmentNeighbourVertexRole, 20);
        firstOutgoingConnector->setVisible(false);
    }

    void select(const QList<QGraphicsItem *> &items)
    {
        scene.clearSelection();
        for (QGraphicsItem *item : items) {
            item->setVisible(true); // hidden items cannot be selected
            item->setSelected(true);
        }
        updatePresentation();
    }

    void updatePresentation()
    {
        MapEditorSelectionContext context{
            .textEditor = nullptr,
            .scene = &scene,
            .view = &view,
            .itemsByLine = &itemsByLine,
            .hiddenObjectLines = &hiddenObjectLines,
            .suppressedAutoReselectLineNumbers = &suppressedAutoReselectLineNumbers,
            .updatingSelection = &updatingSelection,
            .autoFitEnabled = &autoFitEnabled,
            .textNavigationInProgress = &textNavigationInProgress,
            .lastCursorSyncedLine = &lastCursorSyncedLine,
            .lastCursorSyncedColumn = &lastCursorSyncedColumn,
            .pendingClickSelection = &pendingClickSelection,
            .pendingClickScenePosition = &pendingClickScenePosition,
            .pendingClickElapsed = &pendingClickElapsed,
            .pendingClickLineNumber = &pendingClickLineNumber,
            .pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex,
            .pendingClickGeometryKind = &pendingClickGeometryKind,
            .sceneRefreshSelectionLineNumber = &sceneRefreshSelectionLineNumber,
            .sceneRefreshSelectionVertexIndex = &sceneRefreshSelectionVertexIndex,
            .sceneRefreshSelectionGeometryKind = &sceneRefreshSelectionGeometryKind,
            .selectedObjectLineNumber = &selectedObjectLineNumber,
            .selectedObjectVertexIndex = &selectedObjectVertexIndex,
            .selectedLineSegmentStartVertexIndex = &selectedLineSegmentStartVertexIndex,
            .selectedLineSegmentEndVertexIndex = &selectedLineSegmentEndVertexIndex,
            .selectedObjectKind = &selectedObjectKind,
            .selectedObjectCoordinate = &selectedObjectCoordinate,
            .currentLineNumber = []() {
                return kLineNumber;
            },
            .parsedLinesForCurrentDocument = []() {
                return QVector<TherionParsedLine>{};
            },
            .sourcePointFromScenePosition = [](const QPointF &point) {
                return point;
            },
            .updateCommandSurfaceState = []() {},
            .updateHelpPanel = []() {},
            .refreshObjectDetailsPanel = []() {},
            .clearInspectorObjectSelection = []() {},
            .syncInspectorObjectSelectionToLineExpanding = [](int) {},
        };
        MapEditorSelectionController(context).updateGeometrySelectionPresentation();
    }
};

bool isNeighbour(const QGraphicsItem *item)
{
    return item->data(kMapSceneNeighbourControlRole).toBool();
}

void sendSceneMouseEvent(QGraphicsScene *scene,
                         QEvent::Type type,
                         const QPointF &scenePosition,
                         const QPointF &pressScenePosition,
                         Qt::MouseButtons buttons)
{
    QGraphicsSceneMouseEvent event(type);
    event.setScenePos(scenePosition);
    event.setLastScenePos(scenePosition);
    event.setButtonDownScenePos(Qt::LeftButton, pressScenePosition);
    event.setButton(Qt::LeftButton);
    event.setButtons(buttons);
    QApplication::sendEvent(scene, &event);
}

MapGeometryFeature smoothLineFeature()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;
    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 200.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 200.5); // about 3 degrees off line, still smooth
    vertex.outgoingSourceVertexIndex = 12;
    lineFeature.lineVertices.append(vertex);
    return lineFeature;
}

class MapEditorLineNeighbourControlTest final : public QObject
{
    Q_OBJECT

private slots:
    void showsNeighbourHandlesForOneSelectedPoint();
    void showsOnlyOneNeighbourAtLineEnd();
    void showsNoNeighbourHandlesForSeveralPoints();
    void keepsPointSelectedWhileNeighbourHandleIsPicked();
    void dragsNeighbourHandleAlongItsDirection();
    void lengthOnlyDragLeavesOppositeHandle();
    void directionChangeStillMirrorsSmoothHandle();
};

void MapEditorLineNeighbourControlTest::showsNeighbourHandlesForOneSelectedPoint()
{
    SelectionFixture fixture;
    fixture.select({fixture.anchors[1]});

    QVERIFY(fixture.middleIncoming->isVisible() && !isNeighbour(fixture.middleIncoming));
    QVERIFY(fixture.middleOutgoing->isVisible() && !isNeighbour(fixture.middleOutgoing));
    QVERIFY(fixture.firstOutgoing->isVisible() && isNeighbour(fixture.firstOutgoing));
    QVERIFY(fixture.lastIncoming->isVisible() && isNeighbour(fixture.lastIncoming));
    QVERIFY(fixture.firstOutgoingConnector->isVisible() && isNeighbour(fixture.firstOutgoingConnector));
    QCOMPARE(fixture.firstOutgoingConnector->pen().color(), mapEditorLineNeighbourControlConnectorColor());
}

void MapEditorLineNeighbourControlTest::showsOnlyOneNeighbourAtLineEnd()
{
    SelectionFixture fixture;
    fixture.select({fixture.anchors[1]});
    fixture.select({fixture.anchors[0]});

    QVERIFY(fixture.firstOutgoing->isVisible() && !isNeighbour(fixture.firstOutgoing));
    QVERIFY(fixture.middleIncoming->isVisible() && isNeighbour(fixture.middleIncoming));
    QVERIFY(!fixture.middleOutgoing->isVisible() && !isNeighbour(fixture.middleOutgoing));
    QVERIFY(!fixture.lastIncoming->isVisible() && !isNeighbour(fixture.lastIncoming));
    QVERIFY(fixture.firstOutgoingConnector->isVisible() && !isNeighbour(fixture.firstOutgoingConnector));
    QVERIFY(fixture.firstOutgoingConnector->pen().color() != mapEditorLineNeighbourControlConnectorColor());
}

void MapEditorLineNeighbourControlTest::showsNoNeighbourHandlesForSeveralPoints()
{
    SelectionFixture fixture;
    fixture.select({fixture.anchors[0], fixture.anchors[2]});

    QVERIFY(fixture.firstOutgoing->isVisible() && !isNeighbour(fixture.firstOutgoing));
    QVERIFY(fixture.lastIncoming->isVisible() && !isNeighbour(fixture.lastIncoming));
    QVERIFY(!fixture.middleIncoming->isVisible());
    QVERIFY(!fixture.middleOutgoing->isVisible());
}

void MapEditorLineNeighbourControlTest::keepsPointSelectedWhileNeighbourHandleIsPicked()
{
    SelectionFixture fixture;
    fixture.select({fixture.anchors[1]});
    fixture.select({fixture.firstOutgoing});

    QCOMPARE(mapSceneSelectionOwnerVertex(fixture.firstOutgoing), 20);
    QVERIFY(fixture.firstOutgoing->isVisible() && isNeighbour(fixture.firstOutgoing));
    QVERIFY(fixture.middleIncoming->isVisible() && !isNeighbour(fixture.middleIncoming));
    QVERIFY(fixture.middleOutgoing->isVisible() && !isNeighbour(fixture.middleOutgoing));
    QVERIFY(fixture.lastIncoming->isVisible() && isNeighbour(fixture.lastIncoming));
}

void MapEditorLineNeighbourControlTest::dragsNeighbourHandleAlongItsDirection()
{
    QGraphicsScene scene;
    auto *control = new MapEditableGeometryVertexItem(kLineNumber,
                                                      QStringLiteral("line control"),
                                                      11,
                                                      QPointF(60.0, -50.0),
                                                      kSourceBounds,
                                                      kPreviewBounds);
    control->setLineControlAnchorSourcePoint(QPointF(50.0, -50.0));
    control->setData(kMapSceneNeighbourControlRole, true);
    scene.addItem(control);

    QPointF committedOld;
    QPointF committedNew;
    control->setMoveCommittedCallback([&](int, const QString &, int, const QPointF &oldPoint, const QPointF &newPoint) {
        committedOld = oldPoint;
        committedNew = newPoint;
    });

    const QPointF press = control->scenePos();
    const QPointF moved = press + QPointF(10.0, 7.0);
    sendSceneMouseEvent(&scene, QEvent::GraphicsSceneMousePress, press, press, Qt::LeftButton);
    sendSceneMouseEvent(&scene, QEvent::GraphicsSceneMouseMove, moved, press, Qt::LeftButton);
    sendSceneMouseEvent(&scene, QEvent::GraphicsSceneMouseRelease, moved, press, Qt::NoButton);

    QVERIFY(std::abs(committedOld.x() - 60.0) < 1e-6 && std::abs(committedOld.y() + 50.0) < 1e-6);
    QVERIFY(committedNew.x() > 60.5);
    QVERIFY(std::abs(committedNew.y() + 50.0) < 1e-6);
}

void MapEditorLineNeighbourControlTest::lengthOnlyDragLeavesOppositeHandle()
{
    const QVector<MapLineSecondaryMove> moves =
        collectLineSecondaryMovesForVertexDrag(smoothLineFeature(), 11, QPointF(90.0, 200.0), QPointF(80.0, 200.0));
    QVERIFY(moves.isEmpty());
}

void MapEditorLineNeighbourControlTest::directionChangeStillMirrorsSmoothHandle()
{
    const QVector<MapLineSecondaryMove> moves =
        collectLineSecondaryMovesForVertexDrag(smoothLineFeature(), 11, QPointF(90.0, 200.0), QPointF(90.0, 190.0));
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves.first().sourceVertexIndex, 12);
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MapEditorLineNeighbourControlTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorLineNeighbourControlTest.moc"
