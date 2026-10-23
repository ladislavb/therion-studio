#include "ProjectJoinRules.h"

#include <QMap>
#include <QPair>
#include <QStringList>

#include <algorithm>

namespace TherionStudio
{
namespace
{
// Distinct scraps of a join's resolved items, in item order.
QStringList joinScrapObjectIds(const ProjectJoinCommand &join)
{
    QStringList scraps;
    for (const ProjectJoinItem &item : join.items) {
        if (item.resolved && !item.scrapObjectId.isEmpty() && !scraps.contains(item.scrapObjectId)) {
            scraps.append(item.scrapObjectId);
        }
    }
    return scraps;
}
}

QVector<ProjectJoinRuleViolation> projectJoinRuleViolations(const QVector<ProjectJoinCommand> &joins)
{
    // Join commands by the unordered scrap pair they connect; QMap keeps the report order stable.
    QMap<QPair<QString, QString>, QVector<int>> joinIndexesByScrapPair;
    for (int joinIndex = 0; joinIndex < joins.size(); ++joinIndex) {
        const QStringList scraps = joinScrapObjectIds(joins.at(joinIndex));
        for (int first = 0; first < scraps.size(); ++first) {
            for (int second = first + 1; second < scraps.size(); ++second) {
                const QString &a = scraps.at(first);
                const QString &b = scraps.at(second);
                joinIndexesByScrapPair[a < b ? qMakePair(a, b) : qMakePair(b, a)].append(joinIndex);
            }
        }
    }

    QVector<ProjectJoinRuleViolation> violations;
    for (auto it = joinIndexesByScrapPair.cbegin(); it != joinIndexesByScrapPair.cend(); ++it) {
        ProjectJoinRuleViolation violation;
        violation.firstScrapObjectId = it.key().first;
        violation.secondScrapObjectId = it.key().second;
        int scrapJoins = 0;
        int pointLineJoins = 0;
        for (const int joinIndex : it.value()) {
            const ProjectJoinCommand &join = joins.at(joinIndex);
            violation.joins.append(join);
            if (join.mode == ProjectJoinMode::Scraps) {
                ++scrapJoins;
            } else {
                ++pointLineJoins;
            }
        }
        if (scrapJoins > 0 && pointLineJoins > 0) {
            violation.kind = ProjectJoinRuleViolationKind::MixedJoinKinds;
        } else if (scrapJoins > 1) {
            violation.kind = ProjectJoinRuleViolationKind::RepeatedScrapJoin;
        } else if (pointLineJoins == 1) {
            violation.kind = ProjectJoinRuleViolationKind::SinglePointLineJoin;
        } else {
            continue;
        }
        violations.append(violation);
    }
    return violations;
}

QVector<ProjectJoinRuleViolation> projectJoinRuleViolationsFor(const QVector<ProjectJoinRuleViolation> &violations,
                                                               const QVector<ProjectJoinCommand> &joins)
{
    const auto isListed = [&joins](const ProjectJoinCommand &violationJoin) {
        return std::any_of(joins.cbegin(), joins.cend(), [&violationJoin](const ProjectJoinCommand &join) {
            return join.lineNumber == violationJoin.lineNumber && join.sourceFile == violationJoin.sourceFile;
        });
    };
    QVector<ProjectJoinRuleViolation> matching;
    for (const ProjectJoinRuleViolation &violation : violations) {
        if (std::any_of(violation.joins.cbegin(), violation.joins.cend(), isListed)) {
            matching.append(violation);
        }
    }
    return matching;
}
}
