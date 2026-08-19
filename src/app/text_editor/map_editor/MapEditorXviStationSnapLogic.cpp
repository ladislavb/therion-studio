#include "MapEditorXviStationSnapLogic.h"

#include <QLineF>
#include <QSet>

namespace TherionStudio
{
std::optional<MapEditorXviStationSnapCandidate> mapEditorUniqueXviStationSnap(
    const QPointF &clickViewportPosition,
    const QVector<MapEditorXviStationSnapCandidate> &candidates,
    qreal radiusPixels)
{
    if (radiusPixels <= 0.0) {
        return std::nullopt;
    }

    QVector<MapEditorXviStationSnapCandidate> nearbyCandidates;
    QSet<QString> candidateKeys;
    for (const MapEditorXviStationSnapCandidate &candidate : candidates) {
        const QString stationName = candidate.name.trimmed();
        if (stationName.isEmpty()
            || QLineF(clickViewportPosition, candidate.viewportPosition).length() > radiusPixels) {
            continue;
        }

        const QString candidateKey = QStringLiteral("%1|%2|%3")
                                         .arg(stationName)
                                         .arg(qRound64(candidate.scenePosition.x() * 1000.0))
                                         .arg(qRound64(candidate.scenePosition.y() * 1000.0));
        if (!candidateKeys.contains(candidateKey)) {
            candidateKeys.insert(candidateKey);
            nearbyCandidates.append(MapEditorXviStationSnapCandidate{stationName,
                                                                       candidate.scenePosition,
                                                                       candidate.viewportPosition});
        }
    }

    return nearbyCandidates.size() == 1
        ? std::optional<MapEditorXviStationSnapCandidate>{nearbyCandidates.constFirst()}
        : std::nullopt;
}
}
