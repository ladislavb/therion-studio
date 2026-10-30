#include "MapEditorSceneGeneration.h"

namespace TherionStudio
{
quint64 MapEditorSceneGeneration::beginRefresh()
{
    return ++currentGeneration_;
}

quint64 MapEditorSceneGeneration::current() const
{
    return currentGeneration_;
}

bool MapEditorSceneGeneration::isCurrent(quint64 generation) const
{
    return currentGeneration_ == generation;
}

void MapEditorSceneGeneration::markCompleted(quint64 generation)
{
    if (generation == currentGeneration_) {
        completedGeneration_ = generation;
    }
}

bool MapEditorSceneGeneration::isSettled() const
{
    return completedGeneration_ == currentGeneration_;
}
}
