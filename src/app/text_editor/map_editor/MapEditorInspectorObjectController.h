#pragma once

#include "MapEditorInspectorObjectContext.h"
#include "MapEditorMoveToScrapPlanner.h"
#include "MapEditorScrapVisibilityMenu.h"

#include <QModelIndex>
#include <QSet>

namespace TherionStudio
{
class MapEditorInspectorObjectController final
{
public:
    explicit MapEditorInspectorObjectController(MapEditorInspectorObjectContext context);

    void rebuildInspectorObjectsTree();
    void configureInspectorObjectTreeColumns();
    QModelIndex findInspectorObjectIndexForLine(int lineNumber) const;
    void syncInspectorObjectSelectionToLine(int lineNumber);
    void syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection);
    void syncInspectorObjectSelectionToLineExpanding(int lineNumber, bool scrollToSelection);
    void setInspectorObjectCurrentIndex(const QModelIndex &index);
    void clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers = {});
    void handleInspectorObjectSelectionChanged(const QModelIndex &current);
    void handleInspectorObjectClicked(const QModelIndex &index);
    bool moveInspectorObject(const QModelIndex &sourceIndex, const QModelIndex &targetIndex, bool afterTarget);
    bool moveObjectToScrap(int lineNumber, int targetScrapLineNumber);
    [[nodiscard]] QVector<MapEditorMoveToScrapTarget> moveToScrapTargets(int lineNumber) const;
    void applyInspectorObjectVisibility();
    [[nodiscard]] int inspectorObjectLineNumberAt(const QPoint &viewportPosition) const;
    [[nodiscard]] MapEditorScrapVisibilityMenuState scrapVisibilityMenuState(int lineNumber) const;
    void setScrapVisibility(int lineNumber, MapEditorScrapVisibilityTarget target, MapEditorObjectVisibility visibility);
    [[nodiscard]] bool allObjectsShown() const;
    void showAllObjects();

private:
    QString tr(const char *text) const;
    [[nodiscard]] MapEditorObjectVisibilityLines visibilityLines() const;
    bool applyObjectMovePlan(const MapEditorObjectMovePlan &movePlan,
                             const QString &beforeText,
                             const QString &undoLabel);
    void syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection, bool expandAncestors);
    QSet<int> collapsedScrapLineNumbers() const;
    void restoreInspectorObjectExpansion(const QSet<int> &collapsedScrapLines);
    void expandInspectorObjectAncestors(const QModelIndex &index);
    void focusInspectorObjectScrap(const QModelIndex &index);
    void collapseAllInspectorObjectScraps();

    MapEditorInspectorObjectContext context_;
};
}
