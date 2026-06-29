#pragma once

#include "ISessionStore.h"

#include <QHash>
#include <QSettings>

#include <memory>

namespace TherionStudio
{
class SessionSettingsStore final : public ISessionStore
{
public:
    SessionSettingsStore();
    explicit SessionSettingsStore(const QString &fileName, QSettings::Format format = QSettings::IniFormat);
    SessionSettingsStore(QSettings::Format format,
                         QSettings::Scope scope,
                         const QString &organization,
                         const QString &application);
    ~SessionSettingsStore() override;

    SessionSettingsStore(const SessionSettingsStore &) = delete;
    SessionSettingsStore &operator=(const SessionSettingsStore &) = delete;
    SessionSettingsStore(SessionSettingsStore &&) noexcept;
    SessionSettingsStore &operator=(SessionSettingsStore &&) noexcept;

    QString lastProjectPath() const override;
    void setLastProjectPath(const QString &projectPath) override;

    QString lastProjectParentDirectory() const override;
    void setLastProjectParentDirectory(const QString &directoryPath) override;

    QStringList recentProjectPaths() const override;
    void setRecentProjectPaths(const QStringList &projectPaths) override;

    QByteArray mainWindowGeometry() const override;
    void setMainWindowGeometry(const QByteArray &geometry) override;

    QByteArray mainWindowState() const override;
    void setMainWindowState(const QByteArray &state) override;

    QStringList openDocumentPaths() const override;
    void setOpenDocumentPaths(const QStringList &documentPaths) override;

    QStringList recentFilePathsForProject(const QString &projectPath) const override;
    void setRecentFilePathsForProject(const QString &projectPath, const QStringList &filePaths) override;

    QString activeDocumentPath() const override;
    void setActiveDocumentPath(const QString &documentPath) override;

    QString structureNameOverrides() const override;
    void setStructureNameOverrides(const QString &json) override;

    QString applicationLanguage() const override;
    void setApplicationLanguage(const QString &language) override;

    QString defaultTextEditorMode() const override;
    void setDefaultTextEditorMode(const QString &mode) override;

    bool automaticProjectValidationEnabled() const override;
    void setAutomaticProjectValidationEnabled(bool enabled) override;

    QDateTime troubleshootingLogsEnabledUntilUtc() const override;
    void setTroubleshootingLogsEnabledUntilUtc(const QDateTime &enabledUntilUtc) override;

    QString therionExecutablePath() const override;
    void setTherionExecutablePath(const QString &path) override;

    QString therionWorkingDirectory() const override;
    void setTherionWorkingDirectory(const QString &path) override;

    QString therionRunTargetMode() const override;
    void setTherionRunTargetMode(const QString &mode) override;

    QString therionTargetConfigPath() const override;
    void setTherionTargetConfigPath(const QString &path) override;

    bool therionMapTouchFriendlyControlsEnabled() const override;
    void setTherionMapTouchFriendlyControlsEnabled(bool enabled) override;

    bool therionMapMagnifierEnabled() const override;
    void setTherionMapMagnifierEnabled(bool enabled) override;

    bool therionMapObjectsAutoCollapseExpandScrapsEnabled() const override;
    void setTherionMapObjectsAutoCollapseExpandScrapsEnabled(bool enabled) override;

    QString therionMapRecentInsertTypeSubtypeHistory() const override;
    void setTherionMapRecentInsertTypeSubtypeHistory(const QString &json) override;

    QString therionMapBackgroundLayers() const override;
    void setTherionMapBackgroundLayers(const QString &json) override;

private:
    std::unique_ptr<QSettings> settings_;
};

class InMemorySessionStore final : public ISessionStore
{
public:
    QString lastProjectPath() const override;
    void setLastProjectPath(const QString &projectPath) override;

    QString lastProjectParentDirectory() const override;
    void setLastProjectParentDirectory(const QString &directoryPath) override;

    QStringList recentProjectPaths() const override;
    void setRecentProjectPaths(const QStringList &projectPaths) override;

    QByteArray mainWindowGeometry() const override;
    void setMainWindowGeometry(const QByteArray &geometry) override;

    QByteArray mainWindowState() const override;
    void setMainWindowState(const QByteArray &state) override;

    QStringList openDocumentPaths() const override;
    void setOpenDocumentPaths(const QStringList &documentPaths) override;

    QStringList recentFilePathsForProject(const QString &projectPath) const override;
    void setRecentFilePathsForProject(const QString &projectPath, const QStringList &filePaths) override;

    QString activeDocumentPath() const override;
    void setActiveDocumentPath(const QString &documentPath) override;

    QString structureNameOverrides() const override;
    void setStructureNameOverrides(const QString &json) override;

    QString applicationLanguage() const override;
    void setApplicationLanguage(const QString &language) override;

    QString defaultTextEditorMode() const override;
    void setDefaultTextEditorMode(const QString &mode) override;

    bool automaticProjectValidationEnabled() const override;
    void setAutomaticProjectValidationEnabled(bool enabled) override;

    QDateTime troubleshootingLogsEnabledUntilUtc() const override;
    void setTroubleshootingLogsEnabledUntilUtc(const QDateTime &enabledUntilUtc) override;

    QString therionExecutablePath() const override;
    void setTherionExecutablePath(const QString &path) override;

    QString therionWorkingDirectory() const override;
    void setTherionWorkingDirectory(const QString &path) override;

    QString therionRunTargetMode() const override;
    void setTherionRunTargetMode(const QString &mode) override;

    QString therionTargetConfigPath() const override;
    void setTherionTargetConfigPath(const QString &path) override;

    bool therionMapTouchFriendlyControlsEnabled() const override;
    void setTherionMapTouchFriendlyControlsEnabled(bool enabled) override;

    bool therionMapMagnifierEnabled() const override;
    void setTherionMapMagnifierEnabled(bool enabled) override;

    bool therionMapObjectsAutoCollapseExpandScrapsEnabled() const override;
    void setTherionMapObjectsAutoCollapseExpandScrapsEnabled(bool enabled) override;

    QString therionMapRecentInsertTypeSubtypeHistory() const override;
    void setTherionMapRecentInsertTypeSubtypeHistory(const QString &json) override;

    QString therionMapBackgroundLayers() const override;
    void setTherionMapBackgroundLayers(const QString &json) override;

private:
    QString lastProjectPath_;
    QString lastProjectParentDirectory_;
    QStringList recentProjectPaths_;
    QByteArray mainWindowGeometry_;
    QByteArray mainWindowState_;
    QStringList openDocumentPaths_;
    QHash<QString, QStringList> recentFilePathsByProject_;
    QString activeDocumentPath_;
    QString structureNameOverrides_;
    QString applicationLanguage_ = QStringLiteral("system");
    QString defaultTextEditorMode_ = QStringLiteral("raw");
    bool automaticProjectValidationEnabled_ = false;
    QDateTime troubleshootingLogsEnabledUntilUtc_;
    QString therionExecutablePath_;
    QString therionWorkingDirectory_;
    QString therionRunTargetMode_ = QStringLiteral("project");
    QString therionTargetConfigPath_;
    bool therionMapTouchFriendlyControlsEnabled_ = false;
    bool therionMapMagnifierEnabled_ = true;
    bool therionMapObjectsAutoCollapseExpandScrapsEnabled_ = false;
    QString therionMapRecentInsertTypeSubtypeHistory_;
    QString therionMapBackgroundLayers_;
};
}
