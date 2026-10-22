#pragma once

#include "MapEditorMoveToScrapPlanner.h"

#include <functional>

class QMenu;

namespace TherionStudio
{
// Adds a `Move to Scrap` submenu listing the target scraps; blocked targets are disabled with the reason as tooltip.
void addMapEditorMoveToScrapMenu(QMenu *menu,
                                 const QVector<MapEditorMoveToScrapTarget> &targets,
                                 const std::function<void(int targetScrapLineNumber)> &move);
}
