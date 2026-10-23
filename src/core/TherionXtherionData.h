#pragma once

#include <QString>
#include <QVector>

namespace TherionStudio
{
// One survey of the survey tree in Therion's .xtherion.dat (therion -x).
struct TherionXtherionSurvey
{
    QString name;
    // Innermost to outermost, as child.parent.
    QString fullName;
    QString title;
    // Lines separated by '\n', exactly as xtherion shows them.
    QString statistics;
};

// Surveys of the xth_cp_data_tree_insert lines. The unnamed top-level node is skipped.
[[nodiscard]] QVector<TherionXtherionSurvey> parseTherionXtherionSurveys(const QString &text);
}
