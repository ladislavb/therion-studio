#pragma once

#include <QVector>

#include <functional>

namespace TherionStudio
{
struct TherionSourceLogicalCommand;

struct MapEditorLogicalSourceContext
{
    std::function<QVector<TherionSourceLogicalCommand>()> logicalCommandsForCurrentDocument;
};
}
