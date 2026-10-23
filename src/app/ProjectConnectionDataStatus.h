#pragma once

#include "ProjectStationPositionService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QString>

namespace TherionStudio
{
// The line above the Connections list: where station positions come from and whether they are current.
class ProjectConnectionDataStatus final
{
    Q_DECLARE_TR_FUNCTIONS(ProjectConnectionDataStatus)

public:
    struct State
    {
        ProjectStationPositionService::DataSource source = ProjectStationPositionService::DataSource::None;
        QDateTime takenAt;
        bool outOfDate = false;
        bool running = false;
        // Without a target config the index cannot tell which drawings Therion reads.
        bool noTargetConfig = false;
        // The latest compile finished without writing a database export.
        bool compileWroteNoDatabase = false;
        // A connection opens when the run ends.
        bool openPending = false;
    };

    // `now` decides whether the date is shown with the time.
    [[nodiscard]] static QString text(const State &state, const QDateTime &now);
};
}
