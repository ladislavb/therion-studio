#include "MapEditorSceneInteractionIndex.h"
#include "MapEditorSceneItems.h"

#include "MapEditorSceneInternals.h"

#include <QGraphicsItem>
#include <QGraphicsScene>

namespace TherionStudio
{
const MapEditorSceneInteractionIndex::ItemsByLine &MapEditorSceneInteractionIndex::itemsByLine(
    QGraphicsScene *scene, quint64 generation)
{
    if (valid_ && indexedScene_ == scene && indexedGeneration_ == generation) {
        return itemsByLine_;
    }

    itemsByLine_.clear();
    stationLinesByScrap_.clear();
    stationScrapByLine_.clear();
    if (scene != nullptr) {
        // Once per scene generation, not per pointer move.
        ++mapEditorPerformanceCounters().sceneRevisionIndexRebuilds;
        for (QGraphicsItem *item : unsortedSceneItems(scene)) {
            if (item == nullptr) {
                continue;
            }
            const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
            if (lineNumber > 0) {
                itemsByLine_[lineNumber].append(item);
                const int scrapLine = item->data(kMapSceneStationScrapRole).toInt();
                if (scrapLine > 0 && !stationScrapByLine_.contains(lineNumber)) {
                    stationScrapByLine_.insert(lineNumber, scrapLine);
                    stationLinesByScrap_[scrapLine].append(lineNumber);
                }
            }
        }
    }
    indexedScene_ = scene;
    indexedGeneration_ = generation;
    valid_ = true;
    return itemsByLine_;
}

const QHash<int, QVector<int>> &MapEditorSceneInteractionIndex::stationLinesByScrap(QGraphicsScene *scene,
                                                                                   quint64 generation)
{
    itemsByLine(scene, generation);
    return stationLinesByScrap_;
}

int MapEditorSceneInteractionIndex::stationScrapLine(QGraphicsScene *scene, quint64 generation, int stationLine)
{
    itemsByLine(scene, generation);
    return stationScrapByLine_.value(stationLine, 0);
}

void MapEditorSceneInteractionIndex::invalidate()
{
    itemsByLine_.clear();
    stationLinesByScrap_.clear();
    stationScrapByLine_.clear();
    indexedScene_ = nullptr;
    indexedGeneration_ = 0;
    valid_ = false;
    presentedSelectionLines_.clear();
    presentationScene_ = nullptr;
    presentationGeneration_ = 0;
    presentationValid_ = false;
}

bool MapEditorSceneInteractionIndex::hasSelectionPresentation(QGraphicsScene *scene, quint64 generation) const
{
    return presentationValid_ && scene != nullptr && presentationScene_ == scene
        && presentationGeneration_ == generation;
}

const QSet<int> &MapEditorSceneInteractionIndex::presentedSelectionLines() const
{
    return presentedSelectionLines_;
}

void MapEditorSceneInteractionIndex::recordSelectionPresentation(QGraphicsScene *scene, quint64 generation,
                                                                 const QSet<int> &lines)
{
    presentedSelectionLines_ = lines;
    presentationScene_ = scene;
    presentationGeneration_ = generation;
    presentationValid_ = scene != nullptr;
}
}
