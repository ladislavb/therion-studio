#include "MapEditorMoveToScrapPlanner.h"

#include "MapEditorAreaReferenceResolver.h"
#include "MapEditorScrapVisibility.h"
#include "../../../core/Th2GeometryProjection.h"

#include <QCoreApplication>

#include <algorithm>

namespace TherionStudio
{
namespace
{
const MapEditorScrapObjectLines *scrapWithLine(const QVector<MapEditorScrapObjectLines> &scraps, int scrapLineNumber)
{
    const auto it = std::find_if(scraps.cbegin(), scraps.cend(), [scrapLineNumber](const MapEditorScrapObjectLines &scrap) {
        return scrap.scrapLineNumber == scrapLineNumber;
    });
    return it != scraps.cend() ? &*it : nullptr;
}

bool isAreaInScrap(const MapEditorAreaReference &reference, const MapEditorScrapObjectLines &scrap)
{
    return scrap.lineNumbers.contains(reference.areaLineNumber);
}

QVector<int> borderLinesInScrap(const Th2GeometryProjection &projection,
                                int areaLineNumber,
                                const MapEditorScrapObjectLines &scrap)
{
    QVector<int> borderLines;
    for (const int borderLine : mapEditorBorderLineNumbersForArea(projection, areaLineNumber)) {
        if (scrap.lineNumbers.contains(borderLine)) {
            borderLines.append(borderLine);
        }
    }
    return borderLines;
}

// Why the object cannot move to the target scrap, or empty when it can; fills the lines that move with it.
QString moveBlockedReason(const Th2GeometryProjection &projection,
                          const QVector<MapEditorScrapObjectLines> &scraps,
                          int objectLineNumber,
                          int targetScrapLineNumber,
                          QVector<int> *movedLineNumbers)
{
    const int sourceScrapLineNumber = mapEditorScrapLineForLine(scraps, objectLineNumber);
    const MapEditorScrapObjectLines *sourceScrap = scrapWithLine(scraps, sourceScrapLineNumber);
    if (sourceScrap == nullptr || objectLineNumber == sourceScrapLineNumber) {
        return QCoreApplication::translate(
            "TherionStudio::MapEditorMoveToScrapPlanner",
            "Only points, lines and areas inside a scrap can be moved to another scrap.");
    }
    const MapEditorScrapObjectLines *targetScrap = scrapWithLine(scraps, targetScrapLineNumber);
    if (targetScrap == nullptr || targetScrap == sourceScrap) {
        return QCoreApplication::translate("TherionStudio::MapEditorMoveToScrapPlanner",
                                           "Choose a different scrap in this drawing.");
    }

    QVector<int> group{objectLineNumber};
    if (projection.lineAtLineNumber(objectLineNumber) != nullptr) {
        // An area whose only border is this line moves with it; an area with other borders would be split.
        for (const MapEditorAreaReference &reference :
             mapEditorAreaReferencesForBorderLine(projection, objectLineNumber)) {
            if (!isAreaInScrap(reference, *sourceScrap) || group.contains(reference.areaLineNumber)) {
                continue;
            }
            if (borderLinesInScrap(projection, reference.areaLineNumber, *sourceScrap).size() > 1) {
                return QCoreApplication::translate(
                    "TherionStudio::MapEditorMoveToScrapPlanner",
                    "This line is one of several borders of an area. Move the area instead.");
            }
            group.append(reference.areaLineNumber);
        }
    } else if (projection.areaAtLineNumber(objectLineNumber) != nullptr) {
        for (const int borderLine : borderLinesInScrap(projection, objectLineNumber, *sourceScrap)) {
            const QVector<MapEditorAreaReference> references =
                mapEditorAreaReferencesForBorderLine(projection, borderLine);
            const bool sharedBorder =
                std::any_of(references.cbegin(),
                            references.cend(),
                            [sourceScrap, objectLineNumber](const MapEditorAreaReference &reference) {
                                return reference.areaLineNumber != objectLineNumber
                                    && isAreaInScrap(reference, *sourceScrap);
                            });
            if (sharedBorder) {
                return QCoreApplication::translate("TherionStudio::MapEditorMoveToScrapPlanner",
                                                   "A border line of this area is also used by another area.");
            }
            group.append(borderLine);
        }
    }

    if (movedLineNumbers != nullptr) {
        *movedLineNumbers = group;
    }
    return QString();
}
}

MapEditorObjectMovePlan planMapEditorMoveToScrap(const QString &text, int objectLineNumber, int targetScrapLineNumber)
{
    const Th2GeometryProjection projection = Th2GeometryProjection::fromText(text);
    QVector<int> movedLineNumbers;
    const QString blockedReason = moveBlockedReason(projection,
                                                    mapEditorScrapObjectLines(projection),
                                                    objectLineNumber,
                                                    targetScrapLineNumber,
                                                    &movedLineNumbers);
    if (!blockedReason.isEmpty()) {
        MapEditorObjectMovePlan plan;
        plan.errorMessage = blockedReason;
        return plan;
    }
    return MapEditorObjectMovePlanner::planMoveIntoScrap(text,
                                                         movedLineNumbers,
                                                         objectLineNumber,
                                                         targetScrapLineNumber);
}

QVector<MapEditorMoveToScrapTarget> mapEditorMoveToScrapTargets(const QString &text, int objectLineNumber)
{
    QVector<MapEditorMoveToScrapTarget> targets;
    const Th2GeometryProjection projection = Th2GeometryProjection::fromText(text);
    const QVector<MapEditorScrapObjectLines> scraps = mapEditorScrapObjectLines(projection);
    const int sourceScrapLineNumber = mapEditorScrapLineForLine(scraps, objectLineNumber);
    if (sourceScrapLineNumber <= 0 || objectLineNumber == sourceScrapLineNumber) {
        return targets;
    }

    for (const MapEditorScrapObjectLines &scrap : scraps) {
        if (scrap.scrapLineNumber == sourceScrapLineNumber) {
            continue;
        }
        const Th2CommandObject *scrapCommand =
            projection.commandForObjectRef(projection.objectRefAtLineNumber(scrap.scrapLineNumber));
        targets.append(MapEditorMoveToScrapTarget{
            scrap.scrapLineNumber,
            scrapCommand != nullptr ? scrapCommand->type : QString(),
            moveBlockedReason(projection, scraps, objectLineNumber, scrap.scrapLineNumber, nullptr),
        });
    }
    return targets;
}
}
