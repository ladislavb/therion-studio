#include "../src/app/ProjectVersioningSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runOptInPersistsAcrossRestartTest()
{
    QTemporaryDir directory;
    const QString root = directory.path() + QStringLiteral("/project");
    const QString otherRoot = directory.path() + QStringLiteral("/other");
    const QString dataRoot = directory.path() + QStringLiteral("/app-data");
    const QString settingsPath = directory.path() + QStringLiteral("/settings.ini");
    if (!directory.isValid() || !QDir().mkpath(root) || !QDir().mkpath(otherRoot)) {
        return 1;
    }

    QString projectId;
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        ProjectVersioningSettings preferences(settings, dataRoot);
        const auto initial = preferences.projectSettings(root);
        if (!expect(initial.succeeded && !initial.settings.enabled && initial.settings.projectId.isEmpty(),
                    "Local history must start disabled without allocating a project ID.")
            || !expect(settings.allKeys().isEmpty() && !QFileInfo::exists(dataRoot),
                       "Reading default-off settings must not create persistent state or a history store.")) {
            return 1;
        }

        const auto enabled = preferences.setEnabled(root, true);
        if (!expect(enabled.succeeded && enabled.settings.enabled && !enabled.settings.projectId.isEmpty(),
                    "Opting in must assign an application-generated project ID.")
            || !expect(!QFileInfo::exists(enabled.settings.historyDirectory),
                       "Enabling preferences alone must not initialize a Git repository.")) {
            return 1;
        }
        projectId = enabled.settings.projectId;
        const auto other = preferences.projectSettings(otherRoot);
        if (!expect(other.succeeded && !other.settings.enabled && other.settings.projectId.isEmpty(),
                    "Enabling one project must leave other projects disabled.")) {
            return 1;
        }
    }

    QSettings settings(settingsPath, QSettings::IniFormat);
    ProjectVersioningSettings restarted(settings, dataRoot);
    const auto persisted = restarted.projectSettings(root);
    if (!expect(persisted.succeeded && persisted.settings.enabled && persisted.settings.projectId == projectId,
                "A project must retain its opt-in and stable ID across application restarts.")) {
        return 1;
    }
    if (!expect(!persisted.settings.automaticEnabled && persisted.settings.intervalMinutes == 5,
                "Automatic history must default to off with a five-minute interval.")) {
        return 1;
    }
    const auto automatic = restarted.setAutomatic(root, true, 7);
    if (!expect(automatic.succeeded && automatic.settings.automaticEnabled
                    && automatic.settings.intervalMinutes == 7,
                "Automatic history interval and opt-in must persist per project.")) {
        return 1;
    }
    if (!expect(!restarted.setAutomatic(root, true, 0).succeeded,
                "Invalid automatic intervals must fail explicitly.")) {
        return 1;
    }
    const auto disabled = restarted.setEnabled(root, false);
    const auto enabledAgain = restarted.setEnabled(root, true);
    if (!expect(disabled.succeeded && !disabled.settings.enabled && disabled.settings.projectId == projectId
                    && enabledAgain.succeeded && enabledAgain.settings.projectId == projectId,
                "Disabling and reenabling must retain the existing history mapping.")
        || !expect(QDir(root).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty(),
                   "Versioning preferences must not write into the project tree.")) {
        return 1;
    }
    return 0;
}
}

int main()
{
    return runOptInPersistsAcrossRestartTest();
}
