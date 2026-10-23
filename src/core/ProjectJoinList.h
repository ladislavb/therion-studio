#pragma once

#include "ProjectStructureIndex.h"
#include "Th2CentrelineAlignment.h"

#include <QStringList>
#include <QVector>

#include <optional>

namespace TherionStudio
{
enum class ProjectJoinSideStatus
{
    Drawn,
    Unresolved,
    NotDrawn,
    NotDrawnInProjection,
    // The station has no point in this projection, but its survey has scraps there; with computed station positions
    // the nearest of them stands in (see nearestSurveyScrap).
    SurveyDrawnWithoutStation,
};

struct ProjectJoinSide
{
    ProjectEquateStation station;
    ProjectJoinSideStatus status = ProjectJoinSideStatus::NotDrawn;
    std::optional<ProjectStationDrawingPoint> drawing;
    // For SurveyDrawnWithoutStation: the survey's scraps in this projection that have station points.
    QVector<ProjectStructureEntry> surveyScraps;
};

// How Therion `join` commands connect the drawings of two surveys in one projection.
enum class ProjectJoinConnection
{
    None,
    // A scrap join without `-count`: Therion joins the closest scrap ends.
    AutomaticScrapJoin,
    // A scrap join with `-count N`.
    CountedScrapJoin,
    // Joins of points and lines.
    PointLineJoin,
    // More than one kind between the same surveys; an input error only between the same pair of scraps
    // (see ProjectJoinRules).
    MixedJoins,
};

// Two stations of one equate that belong to different surveys, seen in one projection.
struct ProjectJoinPair
{
    ProjectJoinSide first;
    ProjectJoinSide second;
    QString projection;
    // A `nosurvey` pair draws two stations apart; an equate pair draws one station.
    ProjectStationLinkKind linkKind = ProjectStationLinkKind::Equate;
    ProjectJoinConnection connection = ProjectJoinConnection::None;
    QVector<ProjectJoinCommand> joins;
    // Broken join rules among `joins`; the row shows a warning marker.
    QVector<ProjectJoinRuleViolation> ruleViolations;

    [[nodiscard]] bool isAvailable() const
    {
        return first.status == ProjectJoinSideStatus::Drawn && second.status == ProjectJoinSideStatus::Drawn;
    }
    // Available, or a side's station is not drawn but its survey's nearest scrap can stand in. Rows without a shared
    // station point cannot be aligned in extended, so those are not openable there.
    [[nodiscard]] bool canOpen() const
    {
        const auto placeable = [](const ProjectJoinSide &side) {
            return side.status == ProjectJoinSideStatus::Drawn
                || side.status == ProjectJoinSideStatus::SurveyDrawnWithoutStation;
        };
        return isAvailable()
            || (placeable(first) && placeable(second) && projection != QStringLiteral("extended"));
    }
    // Only computed station positions place it: a side's station is not drawn, or the drawings draw two stations.
    [[nodiscard]] bool needsComputedPositions() const
    {
        return !isAvailable() || linkKind != ProjectStationLinkKind::Equate;
    }
};

struct ProjectJoinGroup
{
    ProjectEquateLink link;
    QVector<ProjectJoinPair> pairs;
    // Set instead of `link` for a cross-survey `join` with no equate or `nosurvey` leg between its surveys.
    std::optional<ProjectJoinCommand> joinOnly;
    ProjectJoinConnection joinOnlyConnection = ProjectJoinConnection::None;
    QVector<ProjectStructureEntry> joinOnlyScraps;
    QVector<ProjectJoinRuleViolation> joinOnlyRuleViolations;
};

// Projections that draw at least one equated station: plan, extended, then elevations by angle, then others.
[[nodiscard]] QStringList projectJoinProjections(const ProjectIndexSnapshot &snapshot);

// One group per cross-survey equate or `nosurvey` leg (a multi-station equate yields one pair per two surveys), each
// pair classified by the `join` commands between its surveys in `projection`, followed by one group per join-only
// connection in `projection`.
[[nodiscard]] QVector<ProjectJoinGroup> projectJoinGroups(const ProjectIndexSnapshot &snapshot,
                                                          const QString &projection);

// What a connection row needs to line up fully; the row shows it as a symbol.
enum class ProjectConnectionNeed
{
    None,
    // Opens, but a drawing is placed by its XVI: its scrap has too few measured legs between station points.
    Approximate,
    // Cannot be lined up without computed station positions.
    ComputedPositions,
};

// Scraps whose station points fit the measured legs (see fitTh2StationPointsToMeasuredLegs), each in its own
// projection. Legs are looked up by station, so the cost follows the project's size once, not per scrap.
[[nodiscard]] QSet<QString> projectScrapsFittedByMeasuredLegs(const ProjectIndexSnapshot &snapshot,
                                                              const QVector<Th2MeasuredLeg> &legs);

// `fittedScraps` comes with the station positions (projectScrapsFittedByMeasuredLegs), or is null without them.
// Rows that cannot be opened need nothing.
[[nodiscard]] ProjectConnectionNeed projectJoinPairNeed(const ProjectJoinPair &pair, const QSet<QString> *fittedScraps);
[[nodiscard]] ProjectConnectionNeed projectJoinOnlyNeed(const ProjectJoinGroup &group, const QSet<QString> *fittedScraps);

// Of a SurveyDrawnWithoutStation side's scraps, the one with a station point nearest its station by computed
// position; none when the positions do not cover them.
[[nodiscard]] std::optional<ProjectStructureEntry> nearestSurveyScrap(const ProjectIndexSnapshot &snapshot,
                                                                      const ProjectJoinSide &side,
                                                                      const QHash<QString, Th2StationPosition> &positions);
}
