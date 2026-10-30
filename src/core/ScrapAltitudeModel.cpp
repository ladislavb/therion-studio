#include "ScrapAltitudeModel.h"

#include <QCryptographicHash>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{
ScrapAltitudeStations scrapAltitudeStations(const ThreeDViewerSceneModel &scene)
{
    ScrapAltitudeStations result;
    result.sourceState = ScrapAltitudeStations::SourceState::Loaded;
    QSet<QString> ambiguous;
    QHash<QString, double> entrances;
    for (const auto &station : scene.stations) {
        if (station.name.isEmpty() || !std::isfinite(station.position.z)) {
            continue;
        }
        // ProjectStructureIndex returns normalized lookup keys, not LOX display names.
        const QString name = scene.stationQualifiedName(station).trimmed().toLower();
        if (result.heights.contains(name)) {
            ambiguous.insert(name);
        }
        result.heights.insert(name, station.position.z);
        if (station.entrance) {
            entrances.insert(name, station.position.z);
        }
    }
    for (const QString &name : ambiguous) {
        result.heights.remove(name);
        entrances.remove(name);
    }
    if (!entrances.isEmpty()) {
        QStringList names = entrances.keys();
        names.sort(Qt::CaseSensitive);
        result.entranceReference = names.first();
        result.entranceHeightMetres = entrances.value(names.first());
    }
    if (!result.heights.isEmpty()) {
        const auto bounds = std::minmax_element(result.heights.cbegin(), result.heights.cend());
        result.minimumHeightMetres = *bounds.first;
        result.maximumHeightMetres = *bounds.second;
    }
    return result;
}

ScrapAltitudeValue scrapAltitudeForReferences(
    const QString &identity, const QStringList &references, const ScrapAltitudeStations &stations)
{
    ScrapAltitudeValue result;
    double mean = 0;
    const QSet<QString> unique(references.cbegin(), references.cend());
    for (const auto &reference : unique) {
        const auto height = stations.heights.constFind(reference);
        if (height == stations.heights.cend()) {
            continue;
        }
        ++result.mappedStations;
        mean += (*height - mean) / result.mappedStations;
    }
    if (!unique.isEmpty() && result.mappedStations == unique.size()
        && stations.entranceHeightMetres.has_value()) {
        result.relativeHeightMetres = mean - *stations.entranceHeightMetres;
        const double range = stations.maximumHeightMetres - stations.minimumHeightMetres;
        result.palettePosition = range > 0
            ? std::clamp((mean - stations.minimumHeightMetres) / range, 0.0, 1.0)
            : 0.5;
    } else {
        const QByteArray digest = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256);
        const auto first = static_cast<unsigned char>(digest[0]);
        const auto second = static_cast<unsigned char>(digest[1]);
        result.palettePosition = (first * 256.0 + second) / 65535.0;
    }
    return result;
}
}
