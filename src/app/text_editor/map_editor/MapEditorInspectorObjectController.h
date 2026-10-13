#pragma once

#include "MapEditorInspectorObjectContext.h"

#include <QMap>
#include <QModelIndex>
#include <QPersistentModelIndex>
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
    void applyInspectorObjectVisibility();
    void hideAllObjects();
    void showAllObjects();
    void selectNextObject(bool forward = true);
    void selectPreviousObject();

private:
    QString tr(const char *text) const;
    void rebuildInspectorObjectLineIndex() const;
    void syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection, bool expandAncestors);
    QSet<int> collapsedScrapLineNumbers() const;
    void restoreInspectorObjectExpansion(const QSet<int> &collapsedScrapLines);
    void expandInspectorObjectAncestors(const QModelIndex &index);
    void focusInspectorObjectScrap(const QModelIndex &index);
    void collapseAllInspectorObjectScraps();

    MapEditorInspectorObjectContext context_;
    mutable QMap<int, QPersistentModelIndex> inspectorObjectIndexesByLine_;
    mutable const QStandardItemModel *indexedObjectsModel_ = nullptr;
    mutable bool inspectorObjectLineIndexValid_ = false;
};
}
