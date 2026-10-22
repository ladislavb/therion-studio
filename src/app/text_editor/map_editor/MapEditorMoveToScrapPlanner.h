#pragma once

#include "MapEditorObjectMovePlanner.h"

#include <QString>
#include <QVector>

namespace TherionStudio
{
struct MapEditorMoveToScrapTarget
{
    int scrapLineNumber = 0;
    QString scrapId;
    // Empty when the move is allowed, otherwise why it is not.
    QString blockedReason;
};

// Plans moving the point, line or area at objectLineNumber to the end of another scrap in the same drawing.
// An area takes its border lines with it, and a line that is an area's only border takes that area with it. The move
// is refused when it would split an area border reference (a line that is one of several borders of an area, or a
// border shared with another area).
[[nodiscard]] MapEditorObjectMovePlan planMapEditorMoveToScrap(const QString &text,
                                                               int objectLineNumber,
                                                               int targetScrapLineNumber);

// The other scraps the object could move to, in source order. Empty for scraps and objects outside any scrap.
[[nodiscard]] QVector<MapEditorMoveToScrapTarget> mapEditorMoveToScrapTargets(const QString &text,
                                                                               int objectLineNumber);
}
