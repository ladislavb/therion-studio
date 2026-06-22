#include "TherionExecutableDetector.h"

#include "../platform/PlatformPathResolver.h"

#include <QFileInfo>
#include <QStandardPaths>

namespace TherionStudio
{
QString TherionExecutableDetector::detect()
{
    // Search PATH first (works on all platforms, including Homebrew on macOS and package
    // managers on Linux; unlikely but possible on Windows too)
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("therion"));
    if (!onPath.isEmpty()) {
        return onPath;
    }

    // Fall back to platform-specific candidate locations (registry, common install dirs, etc.)
    for (const QString &candidate : TherionStudio::Platform::therionExecutableCandidates()) {
        if (candidate.isEmpty()) {
            continue;
        }
        if (fi.exists() && fi.isFile() && fi.isExecutable()) {
            return fi.absoluteFilePath();
        }
    }

    return {};
}
}
