#pragma once

#include "../core/ProjectJoinList.h"

#include <QString>

#include <optional>

namespace TherionStudio
{
// The setting for opening a connection that needs station positions when there are no current ones.
enum class ConnectionPositionRunMode
{
    Ask,
    Run,
    Never,
};

// Stored as "ask", "run" or "never"; anything else reads as Ask.
[[nodiscard]] ConnectionPositionRunMode connectionPositionRunModeFromSetting(const QString &value);
[[nodiscard]] QString connectionPositionRunModeSetting(ConnectionPositionRunMode mode);

enum class ConnectionPositionRunAction
{
    // Open with the positions there are, current, out of date or none.
    OpenNow,
    AskFirst,
    Run,
};

// Positions from a compile are kept until the next compile, so only missing positions or out-of-date positions from
// a run lead to a run.
[[nodiscard]] ConnectionPositionRunAction connectionPositionRunAction(ConnectionPositionRunMode mode,
                                                                      bool hasPositions,
                                                                      bool runPositionsOutOfDate);

// A connection waiting for a Therion run: opened when it finishes, without alignment when it fails or is cancelled.
struct ProjectConnectionPendingOpen
{
    std::optional<ProjectJoinPair> pair;
    std::optional<ProjectJoinGroup> joinOnly;
    bool separateWindow = false;
};
}
