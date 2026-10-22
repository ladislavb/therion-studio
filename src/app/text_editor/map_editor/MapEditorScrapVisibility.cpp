#include "MapEditorScrapVisibility.h"

#include "../../../core/Th2GeometryProjection.h"

#include <algorithm>

namespace TherionStudio
{
namespace
{
bool isTargetScrap(const MapEditorScrapObjectLines &scrap, int scrapLineNumber, MapEditorScrapVisibilityTarget target)
{
    const bool thisScrap = scrap.scrapLineNumber == scrapLineNumber;
    return target == MapEditorScrapVisibilityTarget::ThisScrap ? thisScrap : !thisScrap;
}

void appendObjectLineInScrap(MapEditorScrapObjectLines *scrap,
                             const Th2SourceRange &scrapRange,
                             const Th2CommandObject &command)
{
    const int lineNumber = command.sourceRange.startLineNumber;
    if (lineNumber > scrapRange.startLineNumber && scrapRange.containsLine(lineNumber)) {
        scrap->lineNumbers.append(lineNumber);
    }
}
}

MapEditorObjectVisibility nextMapEditorObjectVisibility(MapEditorObjectVisibility visibility)
{
    switch (visibility) {
    case MapEditorObjectVisibility::Shown:
        return MapEditorObjectVisibility::Faded;
    case MapEditorObjectVisibility::Faded:
        return MapEditorObjectVisibility::Hidden;
    case MapEditorObjectVisibility::Hidden:
        break;
    }
    return MapEditorObjectVisibility::Shown;
}

MapEditorObjectVisibilityLines::MapEditorObjectVisibilityLines(QSet<int> *hiddenLines, QSet<int> *fadedLines)
    : hiddenLines_(hiddenLines),
      fadedLines_(fadedLines)
{
}

MapEditorObjectVisibility MapEditorObjectVisibilityLines::visibility(int lineNumber) const
{
    if (hiddenLines_ != nullptr && hiddenLines_->contains(lineNumber)) {
        return MapEditorObjectVisibility::Hidden;
    }
    if (fadedLines_ != nullptr && fadedLines_->contains(lineNumber)) {
        return MapEditorObjectVisibility::Faded;
    }
    return MapEditorObjectVisibility::Shown;
}

bool MapEditorObjectVisibilityLines::allShown() const
{
    return (hiddenLines_ == nullptr || hiddenLines_->isEmpty()) && (fadedLines_ == nullptr || fadedLines_->isEmpty());
}

void MapEditorObjectVisibilityLines::setVisibility(const QVector<int> &lineNumbers, MapEditorObjectVisibility visibility)
{
    for (const int lineNumber : lineNumbers) {
        remove(lineNumber);
        if (visibility == MapEditorObjectVisibility::Hidden && hiddenLines_ != nullptr) {
            hiddenLines_->insert(lineNumber);
        } else if (visibility == MapEditorObjectVisibility::Faded && fadedLines_ != nullptr) {
            fadedLines_->insert(lineNumber);
        }
    }
}

void MapEditorObjectVisibilityLines::remove(int lineNumber)
{
    if (hiddenLines_ != nullptr) {
        hiddenLines_->remove(lineNumber);
    }
    if (fadedLines_ != nullptr) {
        fadedLines_->remove(lineNumber);
    }
}

void MapEditorObjectVisibilityLines::clear()
{
    if (hiddenLines_ != nullptr) {
        hiddenLines_->clear();
    }
    if (fadedLines_ != nullptr) {
        fadedLines_->clear();
    }
}

void MapEditorObjectVisibilityLines::retainOnly(const QSet<int> &lineNumbers)
{
    for (QSet<int> *lines : {hiddenLines_, fadedLines_}) {
        if (lines == nullptr) {
            continue;
        }
        for (auto it = lines->begin(); it != lines->end();) {
            if (!lineNumbers.contains(*it)) {
                it = lines->erase(it);
            } else {
                ++it;
            }
        }
    }
}

QVector<MapEditorScrapObjectLines> mapEditorScrapObjectLines(const Th2GeometryProjection &projection)
{
    QVector<MapEditorScrapObjectLines> scraps;
    for (const Th2CommandObject &block : projection.blockObjects()) {
        if (block.kind != Th2GeometryObjectKind::Scrap || !block.sourceRange.isValid()) {
            continue;
        }

        MapEditorScrapObjectLines scrap;
        scrap.scrapLineNumber = block.sourceRange.startLineNumber;
        scrap.lineNumbers.append(scrap.scrapLineNumber);
        for (const Th2PointObject &point : projection.points()) {
            appendObjectLineInScrap(&scrap, block.sourceRange, point.command);
        }
        for (const Th2LineObject &line : projection.lines()) {
            appendObjectLineInScrap(&scrap, block.sourceRange, line.command);
        }
        for (const Th2AreaObject &area : projection.areas()) {
            appendObjectLineInScrap(&scrap, block.sourceRange, area.command);
        }
        std::sort(scrap.lineNumbers.begin() + 1, scrap.lineNumbers.end());
        scraps.append(scrap);
    }
    return scraps;
}

int mapEditorScrapLineForLine(const QVector<MapEditorScrapObjectLines> &scraps, int lineNumber)
{
    for (const MapEditorScrapObjectLines &scrap : scraps) {
        if (scrap.lineNumbers.contains(lineNumber)) {
            return scrap.scrapLineNumber;
        }
    }
    return 0;
}

std::optional<MapEditorObjectVisibility> mapEditorScrapsVisibility(const MapEditorObjectVisibilityLines &visibilityLines,
                                                                   const QVector<MapEditorScrapObjectLines> &scraps,
                                                                   int scrapLineNumber,
                                                                   MapEditorScrapVisibilityTarget target)
{
    std::optional<MapEditorObjectVisibility> shared;
    for (const MapEditorScrapObjectLines &scrap : scraps) {
        if (!isTargetScrap(scrap, scrapLineNumber, target)) {
            continue;
        }
        for (const int lineNumber : scrap.lineNumbers) {
            const MapEditorObjectVisibility visibility = visibilityLines.visibility(lineNumber);
            if (shared.has_value() && shared.value() != visibility) {
                return std::nullopt;
            }
            shared = visibility;
        }
    }
    return shared;
}

bool mapEditorHasOtherScraps(const QVector<MapEditorScrapObjectLines> &scraps, int scrapLineNumber)
{
    return std::any_of(scraps.cbegin(), scraps.cend(), [scrapLineNumber](const MapEditorScrapObjectLines &scrap) {
        return scrap.scrapLineNumber != scrapLineNumber;
    });
}

void setMapEditorScrapsVisibility(MapEditorObjectVisibilityLines &visibilityLines,
                                  const QVector<MapEditorScrapObjectLines> &scraps,
                                  int scrapLineNumber,
                                  MapEditorScrapVisibilityTarget target,
                                  MapEditorObjectVisibility visibility)
{
    for (const MapEditorScrapObjectLines &scrap : scraps) {
        if (isTargetScrap(scrap, scrapLineNumber, target)) {
            visibilityLines.setVisibility(scrap.lineNumbers, visibility);
        }
    }
}
}
