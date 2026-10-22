#include "MapEditorTab.h"

#include "MapEditorInspectorObjectController.h"
#include "MapEditorMoveToScrapMenu.h"
#include "MapEditorScrapVisibilityMenu.h"

#include <QMenu>
#include <QTreeView>

namespace TherionStudio
{
void MapEditorTab::addMapScrapVisibilityMenus(QMenu *menu, int lineNumber)
{
    const MapEditorScrapVisibilityMenuState state =
        MapEditorInspectorObjectController(inspectorObjectContext()).scrapVisibilityMenuState(lineNumber);
    if (!state.inScrap) {
        return;
    }

    menu->addSeparator();
    addMapEditorScrapVisibilityMenus(menu,
                                     state,
                                     [this, lineNumber](MapEditorScrapVisibilityTarget target,
                                                        MapEditorObjectVisibility visibility) {
                                         setMapScrapVisibility(lineNumber, target, visibility);
                                     });
}

void MapEditorTab::addMapMoveToScrapMenu(QMenu *menu, int lineNumber)
{
    const QVector<MapEditorMoveToScrapTarget> targets =
        MapEditorInspectorObjectController(inspectorObjectContext()).moveToScrapTargets(lineNumber);
    addMapEditorMoveToScrapMenu(menu, targets, [this, lineNumber](int targetScrapLineNumber) {
        moveMapObjectToScrap(lineNumber, targetScrapLineNumber);
    });
}

void MapEditorTab::moveMapObjectToScrap(int lineNumber, int targetScrapLineNumber)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).moveObjectToScrap(lineNumber, targetScrapLineNumber);
}

void MapEditorTab::showEmptyMapCanvasContextMenu(const QPoint &globalPosition)
{
    // Empty canvas right-clicks open nothing unless there is something to show again.
    if (MapEditorInspectorObjectController(inspectorObjectContext()).allObjectsShown()) {
        return;
    }

    QMenu *menu = createMapContextMenu();
    addMapEditorShowAllScrapsAction(menu, true, [this]() {
        showAllMapObjects();
    });
    menu->popup(globalPosition);
}

void MapEditorTab::showInspectorObjectContextMenu(const QPoint &position)
{
    const int lineNumber =
        MapEditorInspectorObjectController(inspectorObjectContext()).inspectorObjectLineNumberAt(position);
    if (lineNumber <= 0 || mapObjectsTree_ == nullptr) {
        return;
    }

    auto *menu = new QMenu(this);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    addMapScrapVisibilityMenus(menu, lineNumber);
    addMapMoveToScrapMenu(menu, lineNumber);
    if (menu->isEmpty()) {
        menu->deleteLater();
        return;
    }
    menu->popup(mapObjectsTree_->viewport()->mapToGlobal(position));
}

void MapEditorTab::setMapScrapVisibility(int lineNumber,
                                         MapEditorScrapVisibilityTarget target,
                                         MapEditorObjectVisibility visibility)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).setScrapVisibility(lineNumber, target, visibility);
}

void MapEditorTab::showAllMapObjects()
{
    MapEditorInspectorObjectController(inspectorObjectContext()).showAllObjects();
}
}
