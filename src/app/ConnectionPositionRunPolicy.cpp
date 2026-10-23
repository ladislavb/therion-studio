#include "ConnectionPositionRunPolicy.h"

namespace TherionStudio
{
ConnectionPositionRunMode connectionPositionRunModeFromSetting(const QString &value)
{
    if (value == QStringLiteral("run")) {
        return ConnectionPositionRunMode::Run;
    }
    if (value == QStringLiteral("never")) {
        return ConnectionPositionRunMode::Never;
    }
    return ConnectionPositionRunMode::Ask;
}

QString connectionPositionRunModeSetting(ConnectionPositionRunMode mode)
{
    switch (mode) {
    case ConnectionPositionRunMode::Run:
        return QStringLiteral("run");
    case ConnectionPositionRunMode::Never:
        return QStringLiteral("never");
    case ConnectionPositionRunMode::Ask:
        break;
    }
    return QStringLiteral("ask");
}

ConnectionPositionRunAction connectionPositionRunAction(ConnectionPositionRunMode mode,
                                                        bool hasPositions,
                                                        bool runPositionsOutOfDate)
{
    if ((hasPositions && !runPositionsOutOfDate) || mode == ConnectionPositionRunMode::Never) {
        return ConnectionPositionRunAction::OpenNow;
    }
    return mode == ConnectionPositionRunMode::Run ? ConnectionPositionRunAction::Run
                                                  : ConnectionPositionRunAction::AskFirst;
}
}
