#pragma once

#include "ProjectStructureIndex.h"

#include <QVector>

namespace TherionStudio
{
// Checks `join` commands against the rules for one pair of scraps: the join kinds are exclusive, a scrap join
// (automatic or `-count N`) is a single command, and point/line joins need at least two commands (a passage needs
// both walls joined).
[[nodiscard]] QVector<ProjectJoinRuleViolation> projectJoinRuleViolations(const QVector<ProjectJoinCommand> &joins);

// Violations that involve any of `joins`.
[[nodiscard]] QVector<ProjectJoinRuleViolation> projectJoinRuleViolationsFor(
    const QVector<ProjectJoinRuleViolation> &violations,
    const QVector<ProjectJoinCommand> &joins);
}
