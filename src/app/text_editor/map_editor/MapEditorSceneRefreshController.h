#pragma once

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include <QVector>

#include <functional>

#include "MapEditorLogicalSourceContext.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneGeneration.h"

class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;
class QObject;
class QUndoStack;

namespace TherionStudio
{
struct TherionParsedLine;
struct MapEditorObjectStyleCatalog;

struct MapEditorSceneRefreshMetrics
{
    bool usedProjectionSnapshot = false;
    bool projectionSnapshotWasReused = false;
    bool usedSourceFallback = false;
    int beforeItemCount = 0;
    int afterItemCount = 0;
    qsizetype entryCount = 0;
    qsizetype geometryFeatureCount = 0;
    qint64 projectionLookupMs = 0;
    qint64 sourceFallbackMs = 0;
    qint64 featureCollectionMs = 0;
    qint64 sceneClearMs = 0;
    qint64 renderMs = 0;
    qint64 backgroundRestoreMs = 0;
    qint64 selectionMs = 0;
    qint64 presentationMs = 0;
    qint64 viewportMs = 0;
    qint64 finalUiMs = 0;
    qint64 totalMs = 0;
};

struct MapEditorSceneRefreshContext
{
    QObject *sceneParent = nullptr;
    QObject *selectionConnectionContext = nullptr;
    QGraphicsScene **scene = nullptr;
    QGraphicsView *view = nullptr;
    QUndoStack *undoStack = nullptr;
    QHash<int, QGraphicsItem *> *itemsByLine = nullptr;
    QHash<QString, QGraphicsItem *> *vertexItemsByKey = nullptr;
    bool *commandApplyInProgress = nullptr;
    bool *sceneRefreshPending = nullptr;
    bool *autoFitEnabled = nullptr;
    bool *fitBackgroundRequested = nullptr;
    MapEditorSceneGeneration *sceneGeneration = nullptr;
    const MapEditorOrientationApplicabilityByCommand *orientationApplicabilityByCommand = nullptr;
    const MapEditorObjectStyleCatalog *styleCatalog = nullptr;
    MapEditorLogicalSourceContext logicalSource;

    std::function<QString()> documentText;
    std::function<QVector<TherionParsedLine>()> parsedLinesForCurrentDocument;
    std::function<int()> currentLineNumber;
    std::function<int()> currentColumnNumber;
    std::function<int()> sceneRefreshSelectionLineNumber;
    std::function<int()> sceneRefreshSelectionVertexIndex;
    std::function<QString()> sceneRefreshSelectionKind;
    std::function<QString()> filePath;
    std::function<void()> handleSceneSelectionChanged;
    std::function<void()> updateCommandSurfaceState;
    std::function<void()> clearMapScene;
    std::function<QRectF()> mapSourceBoundsForCurrentDocument;
    std::function<QRectF()> mapBackgroundFitBounds;
    std::function<void(int, const QPointF &, const QPointF &)> recordCardMove;
    std::function<void(int, bool, bool)> recordCardVisibility;
    std::function<void(int, const QPointF &, const QPointF &)> recordPointGeometryMove;
    std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> recordLineAreaVertexMove;
    std::function<void(int, qreal)> recordPointOrientationHandleChange;
    std::function<void(int, int, qreal, qreal)> recordLinePointLeftHandleChange;
    std::function<void()> restoreBackgroundImageItems;
    std::function<void()> reprojectMetadataBackgroundLayersForCurrentDocument;
    std::function<void()> restoreDraftGeometryItems;
    std::function<void(int)> restorePointSelection;
    std::function<void(int, int)> restoreLineAnchorSelection;
    std::function<void(int, bool)> selectMapLine;
    std::function<void()> applyInspectorObjectVisibility;
    std::function<void()> updateGeometrySelectionPresentation;
    std::function<void(bool)> fitMapToView;
    std::function<void()> syncZoomFactorFromView;
    std::function<void()> updateInteractiveDrawPreview;
    std::function<void()> refreshStatus;
    std::function<void()> refreshObjectDetailsPanel;
    std::function<void()> updateHelpPanel;
    std::function<void(quint64)> recordSceneProjectionRefreshCompleted;
    std::function<void(const MapEditorSceneRefreshMetrics &)> recordSceneRefreshMetrics;
};

class MapEditorSceneRefreshController final
{
public:
    explicit MapEditorSceneRefreshController(MapEditorSceneRefreshContext context);

    void buildMapScene();
    void refreshMapScene();
    void refreshMapScenePreservingUndoStack();
    void flushPendingMapSceneRefreshAfterCommand();

private:
    QGraphicsScene *scene() const;
    void updateSceneRectForBackgroundBounds();
    const MapEditorOrientationApplicabilityByCommand &orientationApplicabilityByCommand() const;
    void refreshMapScenePreservingUndoStack(bool preserveViewport);

    MapEditorSceneRefreshContext context_;
};
}
