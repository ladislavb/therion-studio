#pragma once

#include "ThreeDViewerSceneModel.h"

#include <QHash>
#include <QStringList>

#include <optional>

namespace TherionStudio
{
struct ScrapAltitudeStations
{
    enum class SourceState { Unavailable, AmbiguousOutputs, LoadFailed, Loaded };
    QHash<QString, double> heights;
    QString entranceReference;
    std::optional<double> entranceHeightMetres;
    double minimumHeightMetres = 0;
    double maximumHeightMetres = 0;
    SourceState sourceState = SourceState::Unavailable;
    int outputCount = 0;
};

struct ScrapAltitudeValue
{
    std::optional<double> relativeHeightMetres;
    int mappedStations = 0;
    double palettePosition = 0.5;
};

[[nodiscard]] ScrapAltitudeStations scrapAltitudeStations(const ThreeDViewerSceneModel &scene);
[[nodiscard]] ScrapAltitudeValue scrapAltitudeForReferences(
    const QString &identity, const QStringList &references, const ScrapAltitudeStations &stations);
}
