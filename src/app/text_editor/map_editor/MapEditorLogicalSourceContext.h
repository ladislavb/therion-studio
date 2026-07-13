#pragma once

#include <QVector>

#include <functional>

#include "../../../core/Th2GeometryProjection.h"

namespace TherionStudio
{
struct TherionSourceLogicalCommand;

struct MapEditorLogicalSourceContext
{
    std::function<QVector<TherionSourceLogicalCommand>()> logicalCommandsForCurrentDocument;
    std::function<Th2GeometryProjection()> geometryProjectionForCurrentDocument;
};
}
