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

private:
    quint64 currentGeneration_ = 0;
};
}
