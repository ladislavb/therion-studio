#pragma once

#include <QHash>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

class QElapsedTimer;
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;

namespace TherionStudio
{
class TextEditorTab;
struct TherionParsedLine;
struct TherionSourceLogicalCommand;

struct MapEditorSelectionContext
{
    TextEditorTab *textEditor = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsView *view = nullptr;
    QHash<int, QGraphicsItem *> *itemsByLine = nullptr;
    QSet<int> *hiddenObjectLines = nullptr;
    QSet<int> *suppressedAutoReselectLineNumbers = nullptr;
    bool *updatingSelection = nullptr;
    bool *autoFitEnabled = nullptr;
    bool *textNavigationInProgress = nullptr;
    int *lastCursorSyncedLine = nullptr;
    int *lastCursorSyncedColumn = nullptr;
    bool *pendingClickSelection = nullptr;
    QPointF *pendingClickScenePosition = nullptr;
    QElapsedTimer *pendingClickElapsed = nullptr;
    int *pendingClickLineNumber = nullptr;
    int *pendingClickSourceVertexIndex = nullptr;
    QString *pendingClickGeometryKind = nullptr;
    int *sceneRefreshSelectionLineNumber = nullptr;
    int *sceneRefreshSelectionVertexIndex = nullptr;
    QString *sceneRefreshSelectionGeometryKind = nullptr;
    int *selectedObjectLineNumber = nullptr;
    int *selectedObjectVertexIndex = nullptr;
    int *selectedLineSegmentStartVertexIndex = nullptr;
    int *selectedLineSegmentEndVertexIndex = nullptr;
    QString *selectedObjectKind = nullptr;
    std::optional<QPointF> *selectedObjectCoordinate = nullptr;

    std::function<int()> currentLineNumber;
    std::function<QVector<TherionParsedLine>()> parsedLinesForCurrentDocument;
    std::function<QVector<TherionSourceLogicalCommand>()> logicalCommandsForCurrentDocument;
    std::function<QPointF(const QPointF &)> sourcePointFromScenePosition;
    std::function<void()> updateCommandSurfaceState;
    std::function<void()> updateHelpPanel;
    std::function<void()> refreshObjectDetailsPanel;
    std::function<void()> clearInspectorObjectSelection;
    std::function<void(int)> syncInspectorObjectSelectionToLineExpanding;
};
}
