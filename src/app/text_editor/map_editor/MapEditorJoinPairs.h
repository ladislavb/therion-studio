#pragma once

#include "../../../core/Th2CentrelineAlignment.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QVector>

namespace TherionStudio
{
class MapEditorJoinOutline;
class MapEditorJoinViewLink;
class MapEditorTab;

// One drawing joined to `tab`: the partner, the shared view link, and the partner's outline shown in `tab`.
struct MapEditorJoinPair
{
    QPointer<MapEditorTab> partner;
    QPointer<MapEditorJoinViewLink> link;
    QPointer<MapEditorJoinOutline> outline;
};

// Therion data for placing two drawings, and the stations each drawing's station points draw (by source line).
struct MapEditorJoinPlacement
{
    // Measured legs place each drawing as drawn; without them, or where a scrap has too few, its XVI does.
    const QVector<Th2MeasuredLeg> *legs = nullptr;
    // Computed positions give only the offset between drawings that do not share the join station.
    const QHash<QString, Th2StationPosition> *positions = nullptr;
    QHash<int, QString> firstStationKeysByLine;
    QHash<int, QString> secondStationKeysByLine;
    QString projection;
    // The two join station points draw one station (an equate), so they are put at the same point.
    bool sameStation = true;
};

enum class MapEditorJoinFailure
{
    None,
    // A drawing has too few measured legs between its station points and no usable XVI.
    NoFit,
    // The drawings share no station and computed positions give no offset (none yet, or extended).
    NoOffset,
};

struct MapEditorJoinResult
{
    MapEditorJoinFailure failure = MapEditorJoinFailure::None;
    Th2CentrelineFit firstFit;
    Th2CentrelineFit secondFit;
    // Placed by measured legs, but its XVI background has a different scale.
    bool firstOffBackground = false;
    bool secondOffBackground = false;

    [[nodiscard]] bool opened() const { return failure == MapEditorJoinFailure::None; }
};

// Aligns two open drawings, shows each as an outline in the other, links the two views, and zooms both onto the
// join. `firstJoinLine` and `secondJoinLine` are the join-station points, or for a join with no shared station
// any line inside each joined scrap. Each drawing is scaled and turned as drawn (see MapEditorJoinPlacement). The
// join station points are put at the same point, or apart by their computed offset when they are two stations;
// without join station points, the closest pair of station points by computed position is used. The views are
// centred halfway between the two stations used. With `separateSecondMap`, the second drawing's map is shown in its
// separate map window. Replaces any existing join between the same two editors. Changes nothing when the drawings
// cannot be placed.
MapEditorJoinResult openMapEditorJoinPair(MapEditorTab *first,
                                          int firstJoinLine,
                                          MapEditorTab *second,
                                          int secondJoinLine,
                                          bool separateSecondMap = false,
                                          const MapEditorJoinPlacement &placement = {});

[[nodiscard]] QVector<MapEditorJoinPair> mapEditorJoinPairsFor(MapEditorTab *tab);

void closeMapEditorJoinPair(MapEditorTab *first, MapEditorTab *second);

// True while `tab` is part of a join (joined views always move together); drives the link icon on its tab and map
// window.
[[nodiscard]] bool mapEditorHasJoinLink(MapEditorTab *tab);
}
