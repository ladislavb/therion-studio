#pragma once

#include "MapEditorScrapVisibility.h"

#include <functional>
#include <optional>

class QMenu;

namespace TherionStudio
{
struct MapEditorScrapVisibilityMenuState
{
    bool inScrap = false;
    bool hasOtherScraps = false;
    std::optional<MapEditorObjectVisibility> thisScrap;
    std::optional<MapEditorObjectVisibility> otherScraps;
};

using MapEditorScrapVisibilityApply = std::function<void(MapEditorScrapVisibilityTarget, MapEditorObjectVisibility)>;

// Adds the `This Scrap` and `Other Scraps` submenus shared by the map canvas and Objects view context menus.
void addMapEditorScrapVisibilityMenus(QMenu *menu,
                                      const MapEditorScrapVisibilityMenuState &state,
                                      const MapEditorScrapVisibilityApply &apply);

// Adds `Show All Scraps`, enabled only when something is faded or hidden.
void addMapEditorShowAllScrapsAction(QMenu *menu, bool enabled, const std::function<void()> &showAll);
}
