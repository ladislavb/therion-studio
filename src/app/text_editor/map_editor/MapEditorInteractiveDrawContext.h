#pragma once

#include "MapEditorInteractiveDrawLogic.h"
#include "MapEditorXviStationSnapLogic.h"
#include "../TextEditorSourceTransactionController.h"
#include "../../../core/TherionDocumentEditor.h"

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

class QGraphicsItem;
class QGraphicsPathItem;
class QGraphicsScene;
class QGraphicsView;

namespace TherionStudio
{
class TextEditorTab;

enum class MapEditorInteractiveDrawMode
{
    None,
    Point,
    Line,
    Freehand,
    Area,
    SmartArea
};

struct MapEditorInteractiveDrawContext
{
    TextEditorTab *textEditor = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsView *view = nullptr;
    QString *toolbarStatusNote = nullptr;
    bool *selectModeActive = nullptr;
    bool *commandApplyInProgress = nullptr;
    bool *panActive = nullptr;
    QVector<QPointF> *sourceVertices = nullptr;
    QVector<QPointF> *sceneVertices = nullptr;
    QVector<MapEditorInteractiveLineDraftVertex> *lineVertices = nullptr;
    bool *strokeActive = nullptr;
    bool *anchorPressActive = nullptr;
    QPointF *anchorPressScenePoint = nullptr;
    bool *anchorDragActive = nullptr;
    QPointF *anchorDragScenePoint = nullptr;
    bool *controlDragActive = nullptr;
    bool *hoverActive = nullptr;
    QPointF *hoverScenePoint = nullptr;
    bool *hoverSnapActive = nullptr;
    QPointF *hoverSnapScenePoint = nullptr;
    QVector<QPointF> *hoverSnapGuideScenePoints = nullptr;
    QGraphicsPathItem **previewPath = nullptr;
    QVector<QGraphicsItem *> *previewMarkers = nullptr;

    std::function<MapEditorInteractiveDrawMode()> drawMode;
    std::function<void(MapEditorInteractiveDrawMode)> setDrawMode;
    std::function<QString(const char *)> translate;
    std::function<void()> emitModeStatusChanged;
    std::function<QPointF(const QPointF &)> sourcePointFromScenePosition;
    std::function<std::optional<MapEditorXviStationSnapCandidate>(const QPointF &)> xviStationSnapAtScenePosition;
    std::function<TextEditorSourceTransactionResult(const QString &, const QString &, const QString &, int, std::function<void()>)>
        applySourceTextChangeWithSnapshot;
    std::function<TextEditorSourceTransactionResult(const QString &, const QString &, const QString &, int, std::function<void()>)>
        applySourceTextChangeWithSnapshotDeferredProjection;
    std::function<TherionDraftObjectOptions(const QString &)> draftObjectOptions;
    std::function<void(const QString &, const TherionDraftObjectOptions &)> recordCommittedDraftObjectOptions;
    std::function<std::optional<QRectF>()> initialAreaAdjustRectForDraftInsertion;
    std::function<QStringList()> lineCoordinateRowsForInteractiveDraft;
    std::function<QStringList()> areaCoordinateRowsForInteractiveDraft;
    std::function<void(const QPointF &, const std::optional<QPointF> &)> captureInteractiveLineAnchor;
    std::function<bool(const QPointF &)> previewSmartAreaAt;
    std::function<bool()> hasSmartAreaPreview;
    std::function<bool()> commitSmartAreaPreview;
    std::function<bool()> hasCompletableInteractiveDrawSession;
    std::function<void()> clearPendingInsertObject;
    std::function<void(int)> selectCommittedDraftObject;
    std::function<void()> refreshObjectDetailsPanel;
    std::function<void()> refreshToolbarSummary;
    std::function<void()> updateCommandSurfaceState;
    std::function<void()> updateHelpPanel;
};
}
