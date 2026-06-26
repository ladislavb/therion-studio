#pragma once

#include "MapEditorCanvasEditContext.h"
#include "MapEditorSceneSupport.h"
#include "../TextEditorSourceTransactionController.h"

#include <QPointF>
#include <QRectF>
#include <QtGlobal>
#include <QVector>

#include <functional>

class QGraphicsRectItem;

namespace TherionStudio
{
enum class DraftGeometryKind;
enum class MapEditorLineVertexInsertPlacement
{
    Before,
    After
};

class MapEditorCanvasEditController final
{
public:
    explicit MapEditorCanvasEditController(MapEditorCanvasEditContext context);

    void recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition);
    void recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible);
    void recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint);
    void recordLineAreaVertexMove(int lineNumber,
                                  const QString &kind,
                                  int vertexIndex,
                                  const QPointF &oldPoint,
                                  const QPointF &newPoint);
    void recordPointOrientationHandleChange(int lineNumber, qreal orientationDegrees);
    void recordLinePointLeftHandleChange(int lineNumber,
                                         int sourceVertexIndex,
                                         qreal orientationDegrees,
                                         qreal leftSize);
    void restorePointSelection(int lineNumber);
    void restoreLineAnchorSelection(int lineNumber, int sourceVertexIndex);
    TextEditorSourceTransactionResult recordSourceTextSnapshot(const QString &label,
                                                               const QString &beforeText,
                                                               const QString &afterText,
                                                               int insertedLineNumber);
    TextEditorSourceTransactionResult applySourceTextChangeWithSnapshot(
        const QString &label,
        const QString &beforeText,
        const QString &afterText,
        int insertedLineNumber,
        TextEditorSourceSelectionRestorePolicy selectionRestorePolicy =
            TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection,
        std::function<void()> selectionRestoreHook = {});
    TextEditorSourceTransactionResult applySourceTextChangeWithSnapshotDeferredProjection(
        const QString &label,
        const QString &beforeText,
        const QString &afterText,
        int insertedLineNumber,
        std::function<void()> projectionInvalidationHook,
        TextEditorSourceSelectionRestorePolicy selectionRestorePolicy =
            TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection,
        std::function<void()> selectionRestoreHook = {});
    TextEditorSourceTransactionResult applySourceEditsWithSnapshotDeferredProjection(
        const QString &label,
        const QString &beforeText,
        QVector<TherionSourceTextEdit> sourceEdits,
        int insertedLineNumber,
        std::function<void()> projectionInvalidationHook,
        TextEditorSourceSelectionRestorePolicy selectionRestorePolicy =
            TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection,
        std::function<void()> selectionRestoreHook = {});
    bool insertLineVertexFromSelection(MapEditorLineVertexInsertPlacement placement);
    bool insertLineVertexAtSelectionCoordinate();
    bool splitLineAtSelection();
    bool removeLineVertexFromSelection();
    bool toggleLineVertexSmoothFromSelection();
    bool setLineVertexSmoothForSelection(bool smooth);
    bool setLineVertexControlHandleForSelection(bool incoming, bool enabled);
    void recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition);
    void recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible);
    void recordDraftCompletion(QGraphicsRectItem *item,
                               const QString &label,
                               const QString &beforeText,
                               const QString &afterText,
                               int insertedLineNumber);
    QGraphicsRectItem *selectedDraftGeometryItem() const;
    QGraphicsRectItem *createDraftGeometryItem(DraftGeometryKind kind);
    void addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position);
    void removeDraftGeometryItem(QGraphicsRectItem *item);
    QVector<QPointF> sourceVerticesForDraft(const QGraphicsRectItem *item) const;
    QPointF previewToSourcePoint(const QPointF &previewPoint, const QRectF &sourceBounds, const QRectF &previewBounds) const;

private:
    QString tr(const char *text) const;
    void resetPendingClickSelection();
    void restoreLineVertexOwnerSelection(int lineNumber, int ownerIndex);

    MapEditorCanvasEditContext context_;
};
}
