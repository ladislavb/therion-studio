#include "../PlatformPathResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace TherionStudio::Platform
{
namespace
{
QString therionExeFromRegistryKey(const QString &keyPath)
{
    QSettings reg(keyPath, QSettings::NativeFormat);
    for (const QString &valueName : {QStringLiteral("InstallDir"),
                                     QStringLiteral("Path"),
                                     QStringLiteral("")}) {
        const QString v = reg.value(valueName).toString().trimmed();
        if (v.isEmpty()) {
            continue;
        }
        // Value may be a directory or a direct path to the exe
        const QString asExe = QDir(v).absoluteFilePath(QStringLiteral("therion.exe"));
        const QFileInfo asExeInfo(asExe);
        if (asExeInfo.exists() && asExeInfo.isFile() && asExeInfo.isExecutable()) {
            return asExe;
        }
        const QFileInfo vInfo(v);
        if (vInfo.exists() && vInfo.isFile() && vInfo.isExecutable()) {
            return v;
        }
    }
    return {};
}

QString therionExeFromUninstallKey(const QString &keyPath)
{
    QSettings reg(keyPath, QSettings::NativeFormat);
    const QString dir = reg.value(QStringLiteral("InstallLocation")).toString().trimmed();
    if (dir.isEmpty()) {
        return {};
    }
    const QString exe = QDir(dir).absoluteFilePath(QStringLiteral("therion.exe"));
    const QFileInfo exeInfo(exe);
    return (exeInfo.exists() && exeInfo.isFile() && exeInfo.isExecutable()) ? exeInfo.absoluteFilePath() : QString();
}

QString therionExeFromRegistry()
{
    // Direct Therion registry keys (try both 64-bit and 32-bit hives)
    const QStringList appKeys = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Therion"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Therion"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\speleo.sk\\Therion"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\speleo.sk\\Therion"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Therion"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\speleo.sk\\Therion"),
    };
    for (const QString &key : appKeys) {
        const QString found = therionExeFromRegistryKey(key);
        if (!found.isEmpty()) {
            return found;
        }
    }

    // Uninstall registry entries written by common Windows installers
    const QStringList uninstallKeys = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Therion"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Therion"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Therion"),
    };
    for (const QString &key : uninstallKeys) {
        const QString found = therionExeFromUninstallKey(key);
        if (!found.isEmpty()) {
            return found;
        }
    }

    return {};
}
} // namespace

QStringList therionExecutableCandidates()
{
    QStringList candidates;

    // Registry lookup: fastest and most reliable when an installer has run
    const QString fromRegistry = therionExeFromRegistry();
    if (!fromRegistry.isEmpty()) {
        candidates.append(fromRegistry);
    }

    // Common installation directories on C: and D: drives
    for (const QString &drive : {QStringLiteral("C:"), QStringLiteral("D:")}) {
        for (const QString &programDir :
             {QStringLiteral("Program Files"), QStringLiteral("Program Files (x86)")}) {
            candidates.append(
                QStringLiteral("%1/%2/Therion/therion.exe").arg(drive, programDir));
        }
    }

    return candidates;
}

QStringList externalToolSearchPathCandidates()
{
    return {};
}
} // namespace TherionStudio::Platform
