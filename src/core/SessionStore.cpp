#include "SessionStore.h"

#include <QCryptographicHash>
#include <QSettings>

#include <memory>

namespace TherionStudio
{
namespace
{
const auto kLastProjectPathKey = QStringLiteral("session/lastProjectPath");
const auto kLastProjectParentDirectoryKey = QStringLiteral("session/lastProjectParentDirectory");
const auto kRecentProjectPathsKey = QStringLiteral("session/recentProjectPaths");
const auto kMainWindowGeometryKey = QStringLiteral("session/mainWindowGeometry");
const auto kMainWindowStateKey = QStringLiteral("session/mainWindowState");
const auto kOpenDocumentPathsKey = QStringLiteral("session/openDocumentPaths");
const auto kRecentFilePathsForProjectPrefix = QStringLiteral("session/recentFilePathsByProject/");
const auto kActiveDocumentPathKey = QStringLiteral("session/activeDocumentPath");
const auto kStructureNameOverridesKey = QStringLiteral("session/structureNameOverrides");
const auto kApplicationLanguageKey = QStringLiteral("settings/applicationLanguage");
const auto kDefaultTextEditorModeKey = QStringLiteral("settings/defaultTextEditorMode");
const auto kAutomaticProjectValidationEnabledKey = QStringLiteral("settings/automaticProjectValidationEnabled");
const auto kTroubleshootingLogsEnabledUntilUtcKey = QStringLiteral("settings/troubleshootingLogsEnabledUntilUtc");
const auto kTherionExecutablePathKey = QStringLiteral("session/therionExecutablePath");
const auto kTherionWorkingDirectoryKey = QStringLiteral("session/therionWorkingDirectory");
const auto kTherionArgumentsKey = QStringLiteral("session/therionArguments");
const auto kTherionRunTargetModeKey = QStringLiteral("session/therionRunTargetMode");
const auto kTherionTargetConfigPathKey = QStringLiteral("session/therionTargetConfigPath");
const auto kTherionMapTouchFriendlyControlsEnabledKey = QStringLiteral("session/therionMapTouchFriendlyControlsEnabled");
const auto kTherionMapMagnifierEnabledKey = QStringLiteral("session/therionMapMagnifierEnabled");
const auto kTherionMapObjectsAutoCollapseExpandScrapsEnabledKey =
    QStringLiteral("session/therionMapObjectsAutoCollapseExpandScrapsEnabled");
const auto kTherionMapRecentInsertTypeSubtypeHistoryKey =
    QStringLiteral("session/therionMapRecentInsertTypeSubtypeHistory");
const auto kTherionMapBackgroundLayersKey = QStringLiteral("session/therionMapBackgroundLayers");

void clearDeprecatedSessionOnlyKeys(QSettings &settings)
{
    settings.remove(kTherionArgumentsKey);
}

QString recentFilePathsKeyForProject(const QString &projectPath)
{
    const QByteArray projectHash =
        QCryptographicHash::hash(projectPath.toUtf8(), QCryptographicHash::Sha1).toHex();
    return kRecentFilePathsForProjectPrefix + QString::fromLatin1(projectHash);
}
}

SessionSettingsStore::SessionSettingsStore()
    : settings_(std::make_unique<QSettings>())
{
    clearDeprecatedSessionOnlyKeys(*settings_);
}

SessionSettingsStore::SessionSettingsStore(const QString &fileName, QSettings::Format format)
    : settings_(std::make_unique<QSettings>(fileName, format))
{
    clearDeprecatedSessionOnlyKeys(*settings_);
}

SessionSettingsStore::SessionSettingsStore(QSettings::Format format,
                                           QSettings::Scope scope,
                                           const QString &organization,
                                           const QString &application)
    : settings_(std::make_unique<QSettings>(format, scope, organization, application))
{
    clearDeprecatedSessionOnlyKeys(*settings_);
}

SessionSettingsStore::~SessionSettingsStore() = default;

SessionSettingsStore::SessionSettingsStore(SessionSettingsStore &&) noexcept = default;

SessionSettingsStore &SessionSettingsStore::operator=(SessionSettingsStore &&) noexcept = default;

QString SessionSettingsStore::lastProjectPath() const
{
    return settings_->value(kLastProjectPathKey).toString();
}

void SessionSettingsStore::setLastProjectPath(const QString &projectPath)
{
    settings_->setValue(kLastProjectPathKey, projectPath);
}

QString SessionSettingsStore::lastProjectParentDirectory() const
{
    return settings_->value(kLastProjectParentDirectoryKey).toString();
}

void SessionSettingsStore::setLastProjectParentDirectory(const QString &directoryPath)
{
    settings_->setValue(kLastProjectParentDirectoryKey, directoryPath);
}

QStringList SessionSettingsStore::recentProjectPaths() const
{
    const QStringList recentProjectPaths = settings_->value(kRecentProjectPathsKey).toStringList();
    if (!recentProjectPaths.isEmpty()) {
        return recentProjectPaths;
    }

    const QString lastProject = lastProjectPath();
    return lastProject.isEmpty() ? QStringList() : QStringList({lastProject});
}

void SessionSettingsStore::setRecentProjectPaths(const QStringList &projectPaths)
{
    settings_->setValue(kRecentProjectPathsKey, projectPaths);
}

QByteArray SessionSettingsStore::mainWindowGeometry() const
{
    return settings_->value(kMainWindowGeometryKey).toByteArray();
}

void SessionSettingsStore::setMainWindowGeometry(const QByteArray &geometry)
{
    settings_->setValue(kMainWindowGeometryKey, geometry);
}

QByteArray SessionSettingsStore::mainWindowState() const
{
    return settings_->value(kMainWindowStateKey).toByteArray();
}

void SessionSettingsStore::setMainWindowState(const QByteArray &state)
{
    settings_->setValue(kMainWindowStateKey, state);
}

QStringList SessionSettingsStore::openDocumentPaths() const
{
    return settings_->value(kOpenDocumentPathsKey).toStringList();
}

void SessionSettingsStore::setOpenDocumentPaths(const QStringList &documentPaths)
{
    settings_->setValue(kOpenDocumentPathsKey, documentPaths);
}

QStringList SessionSettingsStore::recentFilePathsForProject(const QString &projectPath) const
{
    if (projectPath.trimmed().isEmpty()) {
        return {};
    }

    return settings_->value(recentFilePathsKeyForProject(projectPath)).toStringList();
}

void SessionSettingsStore::setRecentFilePathsForProject(const QString &projectPath, const QStringList &filePaths)
{
    if (projectPath.trimmed().isEmpty()) {
        return;
    }

    settings_->setValue(recentFilePathsKeyForProject(projectPath), filePaths);
}

QString SessionSettingsStore::activeDocumentPath() const
{
    return settings_->value(kActiveDocumentPathKey).toString();
}

void SessionSettingsStore::setActiveDocumentPath(const QString &documentPath)
{
    settings_->setValue(kActiveDocumentPathKey, documentPath);
}

QString SessionSettingsStore::structureNameOverrides() const
{
    return settings_->value(kStructureNameOverridesKey).toString();
}

void SessionSettingsStore::setStructureNameOverrides(const QString &json)
{
    settings_->setValue(kStructureNameOverridesKey, json);
}

QString SessionSettingsStore::applicationLanguage() const
{
    return settings_->value(kApplicationLanguageKey, QStringLiteral("system")).toString();
}

void SessionSettingsStore::setApplicationLanguage(const QString &language)
{
    settings_->setValue(kApplicationLanguageKey, language);
}

QString SessionSettingsStore::defaultTextEditorMode() const
{
    return settings_->value(kDefaultTextEditorModeKey, QStringLiteral("raw")).toString();
}

void SessionSettingsStore::setDefaultTextEditorMode(const QString &mode)
{
    settings_->setValue(kDefaultTextEditorModeKey, mode);
}

bool SessionSettingsStore::automaticProjectValidationEnabled() const
{
    return settings_->value(kAutomaticProjectValidationEnabledKey, false).toBool();
}

void SessionSettingsStore::setAutomaticProjectValidationEnabled(bool enabled)
{
    settings_->setValue(kAutomaticProjectValidationEnabledKey, enabled);
}

QDateTime SessionSettingsStore::troubleshootingLogsEnabledUntilUtc() const
{
    const QString value = settings_->value(kTroubleshootingLogsEnabledUntilUtcKey).toString();
    if (value.trimmed().isEmpty()) {
        return {};
    }

    QDateTime enabledUntilUtc = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!enabledUntilUtc.isValid()) {
        enabledUntilUtc = QDateTime::fromString(value, Qt::ISODate);
    }
    return enabledUntilUtc.isValid() ? enabledUntilUtc.toUTC() : QDateTime();
}

void SessionSettingsStore::setTroubleshootingLogsEnabledUntilUtc(const QDateTime &enabledUntilUtc)
{
    if (!enabledUntilUtc.isValid()) {
        settings_->remove(kTroubleshootingLogsEnabledUntilUtcKey);
        return;
    }
    settings_->setValue(kTroubleshootingLogsEnabledUntilUtcKey,
                        enabledUntilUtc.toUTC().toString(Qt::ISODateWithMs));
}

QString SessionSettingsStore::therionExecutablePath() const
{
    return settings_->value(kTherionExecutablePathKey).toString();
}

void SessionSettingsStore::setTherionExecutablePath(const QString &path)
{
    settings_->setValue(kTherionExecutablePathKey, path);
}

QString SessionSettingsStore::therionWorkingDirectory() const
{
    return settings_->value(kTherionWorkingDirectoryKey).toString();
}

void SessionSettingsStore::setTherionWorkingDirectory(const QString &path)
{
    settings_->setValue(kTherionWorkingDirectoryKey, path);
}

QString SessionSettingsStore::therionRunTargetMode() const
{
    return settings_->value(kTherionRunTargetModeKey, QStringLiteral("project")).toString();
}

void SessionSettingsStore::setTherionRunTargetMode(const QString &mode)
{
    settings_->setValue(kTherionRunTargetModeKey, mode);
}

QString SessionSettingsStore::therionTargetConfigPath() const
{
    return settings_->value(kTherionTargetConfigPathKey).toString();
}

void SessionSettingsStore::setTherionTargetConfigPath(const QString &path)
{
    settings_->setValue(kTherionTargetConfigPathKey, path);
}

bool SessionSettingsStore::therionMapTouchFriendlyControlsEnabled() const
{
    return settings_->value(kTherionMapTouchFriendlyControlsEnabledKey, false).toBool();
}

void SessionSettingsStore::setTherionMapTouchFriendlyControlsEnabled(bool enabled)
{
    settings_->setValue(kTherionMapTouchFriendlyControlsEnabledKey, enabled);
}

bool SessionSettingsStore::therionMapMagnifierEnabled() const
{
    return settings_->value(kTherionMapMagnifierEnabledKey, true).toBool();
}

void SessionSettingsStore::setTherionMapMagnifierEnabled(bool enabled)
{
    settings_->setValue(kTherionMapMagnifierEnabledKey, enabled);
}

bool SessionSettingsStore::therionMapObjectsAutoCollapseExpandScrapsEnabled() const
{
    return settings_->value(kTherionMapObjectsAutoCollapseExpandScrapsEnabledKey, false).toBool();
}

void SessionSettingsStore::setTherionMapObjectsAutoCollapseExpandScrapsEnabled(bool enabled)
{
    settings_->setValue(kTherionMapObjectsAutoCollapseExpandScrapsEnabledKey, enabled);
}

QString SessionSettingsStore::therionMapRecentInsertTypeSubtypeHistory() const
{
    return settings_->value(kTherionMapRecentInsertTypeSubtypeHistoryKey).toString();
}

void SessionSettingsStore::setTherionMapRecentInsertTypeSubtypeHistory(const QString &json)
{
    settings_->setValue(kTherionMapRecentInsertTypeSubtypeHistoryKey, json);
}

QString SessionSettingsStore::therionMapBackgroundLayers() const
{
    return settings_->value(kTherionMapBackgroundLayersKey).toString();
}

void SessionSettingsStore::setTherionMapBackgroundLayers(const QString &json)
{
    settings_->setValue(kTherionMapBackgroundLayersKey, json);
}

QString InMemorySessionStore::lastProjectPath() const
{
    return lastProjectPath_;
}

void InMemorySessionStore::setLastProjectPath(const QString &projectPath)
{
    lastProjectPath_ = projectPath;
}

QString InMemorySessionStore::lastProjectParentDirectory() const
{
    return lastProjectParentDirectory_;
}

void InMemorySessionStore::setLastProjectParentDirectory(const QString &directoryPath)
{
    lastProjectParentDirectory_ = directoryPath;
}

QStringList InMemorySessionStore::recentProjectPaths() const
{
    return recentProjectPaths_;
}

void InMemorySessionStore::setRecentProjectPaths(const QStringList &projectPaths)
{
    recentProjectPaths_ = projectPaths;
}

QByteArray InMemorySessionStore::mainWindowGeometry() const
{
    return mainWindowGeometry_;
}

void InMemorySessionStore::setMainWindowGeometry(const QByteArray &geometry)
{
    mainWindowGeometry_ = geometry;
}

QByteArray InMemorySessionStore::mainWindowState() const
{
    return mainWindowState_;
}

void InMemorySessionStore::setMainWindowState(const QByteArray &state)
{
    mainWindowState_ = state;
}

QStringList InMemorySessionStore::openDocumentPaths() const
{
    return openDocumentPaths_;
}

void InMemorySessionStore::setOpenDocumentPaths(const QStringList &documentPaths)
{
    openDocumentPaths_ = documentPaths;
}

QStringList InMemorySessionStore::recentFilePathsForProject(const QString &projectPath) const
{
    return recentFilePathsByProject_.value(projectPath);
}

void InMemorySessionStore::setRecentFilePathsForProject(const QString &projectPath, const QStringList &filePaths)
{
    if (projectPath.trimmed().isEmpty()) {
        return;
    }

    recentFilePathsByProject_.insert(projectPath, filePaths);
}

QString InMemorySessionStore::activeDocumentPath() const
{
    return activeDocumentPath_;
}

void InMemorySessionStore::setActiveDocumentPath(const QString &documentPath)
{
    activeDocumentPath_ = documentPath;
}

QString InMemorySessionStore::structureNameOverrides() const
{
    return structureNameOverrides_;
}

void InMemorySessionStore::setStructureNameOverrides(const QString &json)
{
    structureNameOverrides_ = json;
}

QString InMemorySessionStore::applicationLanguage() const
{
    return applicationLanguage_;
}

void InMemorySessionStore::setApplicationLanguage(const QString &language)
{
    applicationLanguage_ = language;
}

QString InMemorySessionStore::defaultTextEditorMode() const
{
    return defaultTextEditorMode_;
}

void InMemorySessionStore::setDefaultTextEditorMode(const QString &mode)
{
    defaultTextEditorMode_ = mode;
}

bool InMemorySessionStore::automaticProjectValidationEnabled() const
{
    return automaticProjectValidationEnabled_;
}

void InMemorySessionStore::setAutomaticProjectValidationEnabled(bool enabled)
{
    automaticProjectValidationEnabled_ = enabled;
}

QDateTime InMemorySessionStore::troubleshootingLogsEnabledUntilUtc() const
{
    return troubleshootingLogsEnabledUntilUtc_;
}

void InMemorySessionStore::setTroubleshootingLogsEnabledUntilUtc(const QDateTime &enabledUntilUtc)
{
    troubleshootingLogsEnabledUntilUtc_ = enabledUntilUtc.isValid() ? enabledUntilUtc.toUTC() : QDateTime();
}

QString InMemorySessionStore::therionExecutablePath() const
{
    return therionExecutablePath_;
}

void InMemorySessionStore::setTherionExecutablePath(const QString &path)
{
    therionExecutablePath_ = path;
}

QString InMemorySessionStore::therionWorkingDirectory() const
{
    return therionWorkingDirectory_;
}

void InMemorySessionStore::setTherionWorkingDirectory(const QString &path)
{
    therionWorkingDirectory_ = path;
}

QString InMemorySessionStore::therionRunTargetMode() const
{
    return therionRunTargetMode_;
}

void InMemorySessionStore::setTherionRunTargetMode(const QString &mode)
{
    therionRunTargetMode_ = mode;
}

QString InMemorySessionStore::therionTargetConfigPath() const
{
    return therionTargetConfigPath_;
}

void InMemorySessionStore::setTherionTargetConfigPath(const QString &path)
{
    therionTargetConfigPath_ = path;
}

bool InMemorySessionStore::therionMapTouchFriendlyControlsEnabled() const
{
    return therionMapTouchFriendlyControlsEnabled_;
}

void InMemorySessionStore::setTherionMapTouchFriendlyControlsEnabled(bool enabled)
{
    therionMapTouchFriendlyControlsEnabled_ = enabled;
}

bool InMemorySessionStore::therionMapMagnifierEnabled() const
{
    return therionMapMagnifierEnabled_;
}

void InMemorySessionStore::setTherionMapMagnifierEnabled(bool enabled)
{
    therionMapMagnifierEnabled_ = enabled;
}

bool InMemorySessionStore::therionMapObjectsAutoCollapseExpandScrapsEnabled() const
{
    return therionMapObjectsAutoCollapseExpandScrapsEnabled_;
}

void InMemorySessionStore::setTherionMapObjectsAutoCollapseExpandScrapsEnabled(bool enabled)
{
    therionMapObjectsAutoCollapseExpandScrapsEnabled_ = enabled;
}

QString InMemorySessionStore::therionMapRecentInsertTypeSubtypeHistory() const
{
    return therionMapRecentInsertTypeSubtypeHistory_;
}

void InMemorySessionStore::setTherionMapRecentInsertTypeSubtypeHistory(const QString &json)
{
    therionMapRecentInsertTypeSubtypeHistory_ = json;
}

QString InMemorySessionStore::therionMapBackgroundLayers() const
{
    return therionMapBackgroundLayers_;
}

void InMemorySessionStore::setTherionMapBackgroundLayers(const QString &json)
{
    therionMapBackgroundLayers_ = json;
}

}
