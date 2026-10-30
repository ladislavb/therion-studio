#pragma once

#include <QtGlobal>

namespace TherionStudio
{
class MapEditorSceneGeneration final
{
public:
    quint64 beginRefresh();
    quint64 current() const;
    bool isCurrent(quint64 generation) const;
    // Scene items may be partially removed/added between beginRefresh() and
    // markCompleted(); generation-keyed caches must not be built in that window.
    void markCompleted(quint64 generation);
    bool isSettled() const;

private:
    quint64 currentGeneration_ = 0;
    quint64 completedGeneration_ = 0;
};
}
