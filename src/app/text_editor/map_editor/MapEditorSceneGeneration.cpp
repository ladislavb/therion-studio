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
}
