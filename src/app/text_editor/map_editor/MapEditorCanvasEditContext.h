#pragma once

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtGlobal>
#include <QVector>

#include <functional>
#include <optional>

class QObject;
class QGraphicsItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QUndoStack;

namespace TherionStudio
{
class TextEditorTab;
struct TherionSourceLogicalCommand;

struct MapEditorCanvasEditContext
{
    QObject *callbackContext = nullptr;
    TextEditorTab *textEditor = nullptr;
    QGraphicsScene *scene = nullptr;
    QUndoStack *undoStack = nullptr;
    QHash<int, QGraphicsItem *> *itemsByLine = nullptr;
    QHash<QString, QGraphicsItem *> *vertexItemsByKey = nullptr;
    QVector<QGraphicsRectItem *> *draftGeometryItems = nullptr;
    QString *toolbarStatusNote = nullptr;
    bool *commandApplyInProgress = nullptr;
    quint64 *lineVertexSelectionRestoreGeneration = nullptr;
    bool *updatingSelection = nullptr;
    bool *pendingClickSelection = nullptr;
    int *pendingClickLineNumber = nullptr;
    int *pendingClickSourceVertexIndex = nullptr;
    QString *pendingClickGeometryKind = nullptr;
    int *selectedObjectLineNumber = nullptr;
    int *selectedObjectVertexIndex = nullptr;
    QString *selectedObjectKind = nullptr;
    std::optional<QPointF> *selectedObjectCoordinate = nullptr;
    int *nextDraftGeometryId = nullptr;

    std::function<QString(const char *)> translate;
    std::function<void()> markSourceChangeOriginatedFromMapTransaction;
    std::function<void()> refreshToolbarSummary;
    std::function<void()> flushPendingSceneRefreshAfterCommand;
    std::function<void()> discardPendingSceneRefreshAfterCommand;
    std::function<QPointF(const QPointF &)> sourcePointFromScenePosition;
    std::function<void()> updateGeometrySelectionPresentation;
    std::function<void()> updateCommandSurfaceState;
    std::function<void()> updateHelpPanel;
    std::function<void()> refreshObjectDetailsPanel;
    std::function<QRectF()> mapPreviewBounds;
    std::function<QRectF()> mapSourceBoundsForCurrentDocument;
    std::function<QVector<TherionSourceLogicalCommand>()> logicalCommandsForCurrentDocument;
    std::function<void(int)> restorePointSelectionLater;
    std::function<void(int, int)> restoreLineAnchorSelectionLater;
    std::function<bool(int, int, bool)> beginLineExtensionFromSelection;
    std::function<void(QGraphicsRectItem *, const QPointF &, const QPointF &)> recordDraftMove;
    std::function<void(QGraphicsRectItem *, bool, bool)> recordDraftVisibility;
};
}
