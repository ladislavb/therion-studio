#include "../PlatformPathResolver.h"

namespace TherionStudio::Platform
{
QStringList therionExecutableCandidates()
{
    // QStandardPaths::findExecutable("therion") in TherionExecutableDetector already covers PATH-based installs.
    // These candidates cover common absolute locations (including standard prefixes) as a fallback.
    return {
        QStringLiteral("/usr/bin/therion"),
        QStringLiteral("/usr/local/bin/therion"),
        QStringLiteral("/usr/local/sbin/therion"),
        QStringLiteral("/opt/therion/bin/therion"),
    };
}

QStringList externalToolSearchPathCandidates()
{
    return {
        QStringLiteral("/usr/local/bin"),
        QStringLiteral("/usr/local/sbin"),
        QStringLiteral("/usr/bin"),
        QStringLiteral("/usr/sbin"),
        QStringLiteral("/bin"),
        QStringLiteral("/sbin"),
    };
}
} // namespace TherionStudio::Platform
