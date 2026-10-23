#include "ProjectJoinList.h"

#include "ProjectJoinRules.h"

#include <QHash>
#include <QPair>
#include <QSet>

#include <algorithm>
#include <tuple>

namespace TherionStudio
{
namespace
{
std::tuple<int, qreal, QString> projectionSortKey(const QString &projection)
{
    if (projection == QStringLiteral("plan")) {
        return {0, 0.0, projection};
    }
    if (projection == QStringLiteral("extended")) {
        return {1, 0.0, projection};
    }
    if (projection.startsWith(QStringLiteral("elevation"))) {
        bool ok = false;
        const qreal angle = projection.section(QLatin1Char(' '), 1, 1).toDouble(&ok);
        return {2, ok ? angle : -1.0, projection};
    }
    return {3, 0.0, projection};
}

ProjectJoinConnection connectionForJoin(const ProjectJoinCommand &join)
{
    if (join.mode == ProjectJoinMode::PointsAndLines) {
        return ProjectJoinConnection::PointLineJoin;
    }
    return join.count > 0 ? ProjectJoinConnection::CountedScrapJoin : ProjectJoinConnection::AutomaticScrapJoin;
}

ProjectJoinConnection combinedConnection(const QVector<ProjectJoinCommand> &joins)
{
    ProjectJoinConnection connection = ProjectJoinConnection::None;
    for (const ProjectJoinCommand &join : joins) {
        const ProjectJoinConnection joinConnection = connectionForJoin(join);
        if (connection != ProjectJoinConnection::None && connection != joinConnection) {
            return ProjectJoinConnection::MixedJoins;
        }
        connection = joinConnection;
    }
    return connection;
}

// Survey namespaces of a join's resolved items whose scraps are drawn in `projection`.
QSet<QString> joinNamespacesInProjection(const ProjectIndexSnapshot &snapshot,
                                         const ProjectJoinCommand &join,
                                         const QString &projection)
{
    QSet<QString> namespaces;
    for (const ProjectJoinItem &item : join.items) {
        if (item.resolved && snapshot.scrapProjectionByObjectId.value(item.scrapObjectId) == projection) {
            namespaces.insert(item.namespacePath.toLower());
        }
    }
    return namespaces;
}

QString namespacePairKey(QString first, QString second)
{
    first = first.toLower();
    second = second.toLower();
    return first < second ? first + QLatin1Char('|') + second : second + QLatin1Char('|') + first;
}

// A key for every pair of distinct namespaces in `namespaces`.
QStringList surveyPairKeys(const QStringList &namespaces)
{
    QStringList keys;
    for (int first = 0; first < namespaces.size(); ++first) {
        for (int second = first + 1; second < namespaces.size(); ++second) {
            if (namespaces.at(first).compare(namespaces.at(second), Qt::CaseInsensitive) != 0) {
                keys.append(namespacePairKey(namespaces.at(first), namespaces.at(second)));
            }
        }
    }
    return keys;
}

// Scraps in `projection` with station points, by lower-case survey namespace.
QHash<QString, QVector<ProjectStructureEntry>> scrapsWithStationsByNamespace(const ProjectIndexSnapshot &snapshot,
                                                                             const QString &projection)
{
    QHash<QString, QVector<ProjectStructureEntry>> scraps;
    for (const ProjectStructureEntry &entry : snapshot.entries) {
        if (entry.kind == ProjectStructureEntryKind::Scrap
            && snapshot.scrapProjectionByObjectId.value(entry.objectId) == projection
            && !snapshot.stationPointsByScrapObjectId.value(entry.objectId).isEmpty()) {
            scraps[entry.namespacePath.toLower()].append(entry);
        }
    }
    return scraps;
}

ProjectJoinSide joinSide(const ProjectIndexSnapshot &snapshot,
                         const ProjectEquateStation &station,
                         const QString &projection,
                         const QHash<QString, QVector<ProjectStructureEntry>> &surveyScraps)
{
    ProjectJoinSide side;
    side.station = station;
    if (!station.resolved) {
        side.status = ProjectJoinSideStatus::Unresolved;
        return side;
    }
    const QVector<ProjectStationDrawingPoint> points =
        snapshot.stationDrawingPointsByStationKey.value(station.stationKey);
    for (const ProjectStationDrawingPoint &point : points) {
        if (point.projection == projection) {
            side.status = ProjectJoinSideStatus::Drawn;
            side.drawing = point;
            return side;
        }
    }
    side.surveyScraps = surveyScraps.value(station.namespacePath.toLower());
    if (!side.surveyScraps.isEmpty()) {
        side.status = ProjectJoinSideStatus::SurveyDrawnWithoutStation;
    } else {
        side.status = points.isEmpty() ? ProjectJoinSideStatus::NotDrawn : ProjectJoinSideStatus::NotDrawnInProjection;
    }
    return side;
}
}

QStringList projectJoinProjections(const ProjectIndexSnapshot &snapshot)
{
    QSet<QString> projections;
    for (const QVector<ProjectStationDrawingPoint> &points : snapshot.stationDrawingPointsByStationKey) {
        for (const ProjectStationDrawingPoint &point : points) {
            projections.insert(point.projection);
        }
    }
    QStringList sorted(projections.cbegin(), projections.cend());
    std::sort(sorted.begin(), sorted.end(), [](const QString &left, const QString &right) {
        return projectionSortKey(left) < projectionSortKey(right);
    });
    return sorted;
}

QVector<ProjectJoinGroup> projectJoinGroups(const ProjectIndexSnapshot &snapshot, const QString &projection)
{
    // Cross-survey joins drawn in this projection, by the survey pairs they connect.
    QHash<QString, QVector<ProjectJoinCommand>> joinsBySurveyPair;
    QVector<QPair<ProjectJoinCommand, QSet<QString>>> crossSurveyJoins;
    for (const ProjectJoinCommand &join : snapshot.joinCommands) {
        const QSet<QString> namespaces = joinNamespacesInProjection(snapshot, join, projection);
        if (namespaces.size() < 2) {
            continue;
        }
        crossSurveyJoins.append({join, namespaces});
        for (const QString &pairKey : surveyPairKeys(QStringList(namespaces.cbegin(), namespaces.cend()))) {
            joinsBySurveyPair[pairKey].append(join);
        }
    }

    // Survey pairs linked by any equate or nosurvey leg, in any projection.
    QSet<QString> linkedSurveyPairs;
    for (const ProjectEquateLink &link : snapshot.equateLinks) {
        QStringList stationNamespaces;
        for (const ProjectEquateStation &station : link.stations) {
            stationNamespaces.append(station.namespacePath);
        }
        for (const QString &pairKey : surveyPairKeys(stationNamespaces)) {
            linkedSurveyPairs.insert(pairKey);
        }
    }

    const QHash<QString, QVector<ProjectStructureEntry>> surveyScraps = scrapsWithStationsByNamespace(snapshot, projection);
    QVector<ProjectJoinGroup> groups;
    groups.reserve(snapshot.equateLinks.size() + crossSurveyJoins.size());
    for (const ProjectEquateLink &link : snapshot.equateLinks) {
        ProjectJoinGroup group;
        group.link = link;
        for (int firstIndex = 0; firstIndex < link.stations.size(); ++firstIndex) {
            for (int secondIndex = firstIndex + 1; secondIndex < link.stations.size(); ++secondIndex) {
                const ProjectEquateStation &first = link.stations.at(firstIndex);
                const ProjectEquateStation &second = link.stations.at(secondIndex);
                if (first.namespacePath.compare(second.namespacePath, Qt::CaseInsensitive) == 0) {
                    continue;
                }
                ProjectJoinPair pair{joinSide(snapshot, first, projection, surveyScraps),
                                     joinSide(snapshot, second, projection, surveyScraps),
                                     projection};
                pair.linkKind = link.kind;
                pair.joins = joinsBySurveyPair.value(namespacePairKey(first.namespacePath, second.namespacePath));
                pair.connection = combinedConnection(pair.joins);
                pair.ruleViolations = projectJoinRuleViolationsFor(snapshot.joinRuleViolations, pair.joins);
                group.pairs.append(pair);
            }
        }
        groups.append(group);
    }

    QHash<QString, ProjectStructureEntry> entriesByObjectId;
    for (const ProjectStructureEntry &entry : snapshot.entries) {
        entriesByObjectId.insert(entry.objectId, entry);
    }
    for (const auto &[join, namespaces] : std::as_const(crossSurveyJoins)) {
        const QStringList pairKeys = surveyPairKeys(QStringList(namespaces.cbegin(), namespaces.cend()));
        if (std::any_of(pairKeys.cbegin(), pairKeys.cend(), [&linkedSurveyPairs](const QString &pairKey) {
                return linkedSurveyPairs.contains(pairKey);
            })) {
            continue;
        }
        ProjectJoinGroup group;
        group.joinOnly = join;
        group.joinOnlyConnection = connectionForJoin(join);
        group.joinOnlyRuleViolations = projectJoinRuleViolationsFor(snapshot.joinRuleViolations, {join});
        QSet<QString> seenScraps;
        for (const ProjectJoinItem &item : join.items) {
            if (item.resolved && !seenScraps.contains(item.scrapObjectId)) {
                seenScraps.insert(item.scrapObjectId);
                group.joinOnlyScraps.append(entriesByObjectId.value(item.scrapObjectId));
            }
        }
        groups.append(group);
    }
    return groups;
}

std::optional<ProjectStructureEntry> nearestSurveyScrap(const ProjectIndexSnapshot &snapshot,
                                                        const ProjectJoinSide &side,
                                                        const QHash<QString, Th2StationPosition> &positions)
{
    const auto station = positions.constFind(side.station.stationKey);
    if (station == positions.cend()) {
        return std::nullopt;
    }
    std::optional<ProjectStructureEntry> nearest;
    double nearestSquared = 0.0;
    for (const ProjectStructureEntry &scrap : side.surveyScraps) {
        for (const ProjectScrapStationPoint &point : snapshot.stationPointsByScrapObjectId.value(scrap.objectId)) {
            const auto position = positions.constFind(point.stationKey);
            if (position == positions.cend()) {
                continue;
            }
            const double dx = position->x - station->x;
            const double dy = position->y - station->y;
            const double dz = position->z - station->z;
            const double squared = dx * dx + dy * dy + dz * dz;
            if (!nearest.has_value() || squared < nearestSquared) {
                nearest = scrap;
                nearestSquared = squared;
            }
        }
    }
    return nearest;
}

QSet<QString> projectScrapsFittedByMeasuredLegs(const ProjectIndexSnapshot &snapshot, const QVector<Th2MeasuredLeg> &legs)
{
    QHash<QString, QVector<int>> legsByStationKey;
    for (int index = 0; index < legs.size(); ++index) {
        legsByStationKey[legs.at(index).fromStationKey].append(index);
    }
    QSet<QString> fitted;
    for (auto scrap = snapshot.stationPointsByScrapObjectId.cbegin(); scrap != snapshot.stationPointsByScrapObjectId.cend();
         ++scrap) {
        QVector<QPair<QString, QPointF>> drawnStations;
        QVector<Th2MeasuredLeg> scrapLegs;
        for (const ProjectScrapStationPoint &point : scrap.value()) {
            if (!point.position.has_value()) {
                continue;
            }
            drawnStations.append({point.stationKey, *point.position});
            for (const int index : legsByStationKey.value(point.stationKey)) {
                scrapLegs.append(legs.at(index));
            }
        }
        const QString projection = snapshot.scrapProjectionByObjectId.value(scrap.key());
        if (fitTh2StationPointsToMeasuredLegs(drawnStations, scrapLegs, projection).isValid()) {
            fitted.insert(scrap.key());
        }
    }
    return fitted;
}

ProjectConnectionNeed projectJoinPairNeed(const ProjectJoinPair &pair, const QSet<QString> *fittedScraps)
{
    if (!pair.canOpen()) {
        return ProjectConnectionNeed::None;
    }
    if (fittedScraps == nullptr) {
        return pair.needsComputedPositions() ? ProjectConnectionNeed::ComputedPositions : ProjectConnectionNeed::None;
    }
    // A stand-in scrap is only chosen when the connection opens, so only drawn sides are checked.
    for (const ProjectJoinSide *side : {&pair.first, &pair.second}) {
        if (side->drawing.has_value() && !fittedScraps->contains(side->drawing->scrapObjectId)) {
            return ProjectConnectionNeed::Approximate;
        }
    }
    return ProjectConnectionNeed::None;
}

ProjectConnectionNeed projectJoinOnlyNeed(const ProjectJoinGroup &group, const QSet<QString> *fittedScraps)
{
    if (group.joinOnlyScraps.size() != 2) {
        return ProjectConnectionNeed::None;
    }
    if (fittedScraps == nullptr) {
        return ProjectConnectionNeed::ComputedPositions;
    }
    for (const ProjectStructureEntry &scrap : group.joinOnlyScraps) {
        if (!fittedScraps->contains(scrap.objectId)) {
            return ProjectConnectionNeed::Approximate;
        }
    }
    return ProjectConnectionNeed::None;
}
}
