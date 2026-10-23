#include "MapEditorJoinPairs.h"

#include "MapEditorJoinOutline.h"
#include "MapEditorJoinViewLink.h"
#include "MapEditorTab.h"

#include <QWidget>

namespace TherionStudio
{
namespace
{
// Width of cave shown across the first view when a join opens.
constexpr qreal kJoinViewWidthMetres = 30.0;

MapEditorJoinViewLink *joinLinkBetween(MapEditorTab *first, MapEditorTab *second)
{
    for (MapEditorTab *owner : {first, second}) {
        for (MapEditorJoinViewLink *link : owner->findChildren<MapEditorJoinViewLink *>(Qt::FindDirectChildrenOnly)) {
            if (link->connects(first, second)) {
                return link;
            }
        }
    }
    return nullptr;
}

// Measured leg fit, else the XVI fit; `offBackground` is set when the two disagree in scale.
Th2CentrelineFit drawingFit(MapEditorTab *tab,
                            int lineNumber,
                            const QHash<int, QString> &stationKeysByLine,
                            const MapEditorJoinPlacement &placement,
                            bool *offBackground)
{
    const Th2CentrelineFit xviFit = tab->centrelineFitForSourceLine(lineNumber);
    if (placement.legs != nullptr) {
        const Th2CentrelineFit legFit =
            tab->measuredLegFitForSourceLine(lineNumber, stationKeysByLine, *placement.legs, placement.projection);
        if (legFit.isValid()) {
            *offBackground = xviFit.isValid() && th2FitScalesDisagree(legFit, xviFit);
            return legFit;
        }
    }
    return xviFit;
}

// Where the two drawings are linked, or nothing when they share no station and no offset is known.
std::optional<Th2JoinAnchor> joinAnchor(MapEditorTab *first,
                                        int firstJoinLine,
                                        MapEditorTab *second,
                                        int secondJoinLine,
                                        const MapEditorJoinPlacement &placement)
{
    const std::optional<QPointF> firstJoin = first->stationPointSourcePosition(firstJoinLine);
    const std::optional<QPointF> secondJoin = second->stationPointSourcePosition(secondJoinLine);
    if (firstJoin.has_value() && secondJoin.has_value() && placement.sameStation) {
        return Th2JoinAnchor{*firstJoin, *secondJoin, QPointF()};
    }
    if (placement.positions == nullptr) {
        return std::nullopt;
    }
    if (firstJoin.has_value() && secondJoin.has_value()) {
        const std::optional<QPointF> offset =
            computedStationOffsetMetres(placement.firstStationKeysByLine.value(firstJoinLine),
                                        placement.secondStationKeysByLine.value(secondJoinLine),
                                        *placement.positions,
                                        placement.projection);
        return offset.has_value() ? std::optional<Th2JoinAnchor>(Th2JoinAnchor{*firstJoin, *secondJoin, *offset})
                                  : std::nullopt;
    }
    return closestComputedStationPair(first->scrapStationPointsForSourceLine(firstJoinLine),
                                      placement.firstStationKeysByLine,
                                      second->scrapStationPointsForSourceLine(secondJoinLine),
                                      placement.secondStationKeysByLine,
                                      *placement.positions,
                                      placement.projection);
}
}

MapEditorJoinResult openMapEditorJoinPair(MapEditorTab *first,
                                          int firstJoinLine,
                                          MapEditorTab *second,
                                          int secondJoinLine,
                                          bool separateSecondMap,
                                          const MapEditorJoinPlacement &placement)
{
    MapEditorJoinResult result;
    if (first == nullptr || second == nullptr || first == second) {
        result.failure = MapEditorJoinFailure::NoFit;
        return result;
    }
    result.firstFit =
        drawingFit(first, firstJoinLine, placement.firstStationKeysByLine, placement, &result.firstOffBackground);
    result.secondFit =
        drawingFit(second, secondJoinLine, placement.secondStationKeysByLine, placement, &result.secondOffBackground);
    if (!result.firstFit.isValid() || !result.secondFit.isValid()) {
        result.failure = MapEditorJoinFailure::NoFit;
        return result;
    }
    const std::optional<Th2JoinAnchor> anchor = joinAnchor(first, firstJoinLine, second, secondJoinLine, placement);
    if (!anchor.has_value()) {
        result.failure = MapEditorJoinFailure::NoOffset;
        return result;
    }
    const Th2SimilarityTransform secondToFirst = joinPartnerToThisTransform(
        result.firstFit, anchor->thisPoint, result.secondFit, anchor->partnerPoint, anchor->partnerOffsetMetres);
    // Look at where the drawings meet: halfway between the two anchor stations (one point for an equate).
    const QPointF viewCentre = (anchor->thisPoint + secondToFirst.map(anchor->partnerPoint)) / 2.0;

    closeMapEditorJoinPair(first, second);
    // Separate before aligning; the link follows any viewport resize while the window is laid out.
    if (separateSecondMap && !second->isMapPaneDetached()) {
        second->toggleMapPaneWindow();
    }
    MapEditorJoinOutline::attach(first, second, secondToFirst);
    MapEditorJoinOutline::attach(second, first, secondToFirst.inverted());

    auto *link = new MapEditorJoinViewLink(first, second, secondToFirst, first);
    QObject::connect(second, &QObject::destroyed, link, &QObject::deleteLater);
    const qreal firstUnitsAcross = kJoinViewWidthMetres / result.firstFit.drawingToMetres.scale();
    const int viewWidthPixels = qMax(1, first->mapViewportPixelWidth());
    link->showFirstAt(viewCentre, viewWidthPixels / firstUnitsAcross);
    return result;
}

QVector<MapEditorJoinPair> mapEditorJoinPairsFor(MapEditorTab *tab)
{
    QVector<MapEditorJoinPair> pairs;
    if (tab == nullptr) {
        return pairs;
    }
    // Every join shows an outline in both drawings, so the outlines name the partners.
    for (MapEditorJoinOutline *outline : tab->findChildren<MapEditorJoinOutline *>(Qt::FindDirectChildrenOnly)) {
        MapEditorTab *partner = outline->partner();
        if (partner != nullptr) {
            pairs.append(MapEditorJoinPair{partner, joinLinkBetween(tab, partner), outline});
        }
    }
    return pairs;
}

bool mapEditorHasJoinLink(MapEditorTab *tab)
{
    for (const MapEditorJoinPair &pair : mapEditorJoinPairsFor(tab)) {
        if (pair.link != nullptr) {
            return true;
        }
    }
    return false;
}

void closeMapEditorJoinPair(MapEditorTab *first, MapEditorTab *second)
{
    if (first == nullptr || second == nullptr) {
        return;
    }
    delete joinLinkBetween(first, second);
    delete MapEditorJoinOutline::find(first, second);
    delete MapEditorJoinOutline::find(second, first);
}
}
