#include "ProjectConnectionDataStatus.h"

#include <QLocale>

namespace TherionStudio
{
namespace
{
QString takenAtText(const QDateTime &takenAt, const QDateTime &now)
{
    const QLocale locale;
    return takenAt.date() == now.date() ? locale.toString(takenAt.time(), QLocale::ShortFormat)
                                        : locale.toString(takenAt, QLocale::ShortFormat);
}
}

QString ProjectConnectionDataStatus::text(const State &state, const QDateTime &now)
{
    if (state.noTargetConfig) {
        return tr("No target config selected, so the drawings Therion reads are unknown.");
    }
    if (state.running) {
        return state.openPending
            ? tr("Computing station positions with Therion; the connection opens when they are ready.")
            : tr("Computing station positions with Therion...");
    }
    const QString at = takenAtText(state.takenAt, now);
    switch (state.source) {
    case ProjectStationPositionService::DataSource::Compile:
        return state.outOfDate ? tr("Positions from the compile at %1. Sources changed since; compile to update them.").arg(at)
                               : tr("Positions from the compile at %1.").arg(at);
    case ProjectStationPositionService::DataSource::Run:
        return state.outOfDate ? tr("Positions from a Therion run at %1. Sources changed since.").arg(at)
                               : tr("Positions from a Therion run at %1.").arg(at);
    case ProjectStationPositionService::DataSource::None:
        break;
    }
    if (state.compileWroteNoDatabase) {
        return tr("The last compile wrote no database export. Add \"export database -fmt sql\" to the thconfig, or open "
                  "a connection that needs station positions to run Therion once.");
    }
    return tr("No station positions. Add \"export database -fmt sql\" to the thconfig and compile.");
}
}
