#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
class ISessionStore
{
public:
    virtual ~ISessionStore() = default;

    virtual QString lastProjectPath() const = 0;
    virtual void setLastProjectPath(const QString &projectPath) = 0;

    virtual QString lastProjectParentDirectory() const = 0;
    virtual void setLastProjectParentDirectory(const QString &directoryPath) = 0;

    virtual QStringList recentProjectPaths() const = 0;
    virtual void setRecentProjectPaths(const QStringList &projectPaths) = 0;

    virtual QByteArray mainWindowGeometry() const = 0;
    virtual void setMainWindowGeometry(const QByteArray &geometry) = 0;

    virtual QByteArray mainWindowState() const = 0;
    virtual void setMainWindowState(const QByteArray &state) = 0;

    virtual QStringList openDocumentPaths() const = 0;
    virtual void setOpenDocumentPaths(const QStringList &documentPaths) = 0;

    virtual QStringList recentFilePathsForProject(const QString &projectPath) const = 0;
    virtual void setRecentFilePathsForProject(const QString &projectPath, const QStringList &filePaths) = 0;

    virtual QString activeDocumentPath() const = 0;
    virtual void setActiveDocumentPath(const QString &documentPath) = 0;

    virtual QString structureNameOverrides() const = 0;
    virtual void setStructureNameOverrides(const QString &json) = 0;

    virtual QString applicationLanguage() const = 0;
    virtual void setApplicationLanguage(const QString &language) = 0;

    virtual QString defaultTextEditorMode() const = 0;
    virtual void setDefaultTextEditorMode(const QString &mode) = 0;

    virtual QString therionExecutablePath() const = 0;
    virtual void setTherionExecutablePath(const QString &path) = 0;

    virtual QString therionWorkingDirectory() const = 0;
    virtual void setTherionWorkingDirectory(const QString &path) = 0;

    virtual QString therionRunTargetMode() const = 0;
    virtual void setTherionRunTargetMode(const QString &mode) = 0;

    virtual QString therionTargetConfigPath() const = 0;
    virtual void setTherionTargetConfigPath(const QString &path) = 0;

    virtual bool therionMapTouchFriendlyControlsEnabled() const = 0;
    virtual void setTherionMapTouchFriendlyControlsEnabled(bool enabled) = 0;

    virtual bool therionMapMagnifierEnabled() const = 0;
    virtual void setTherionMapMagnifierEnabled(bool enabled) = 0;

    virtual bool therionMapObjectsAutoCollapseExpandScrapsEnabled() const = 0;
    virtual void setTherionMapObjectsAutoCollapseExpandScrapsEnabled(bool enabled) = 0;

    virtual QString therionMapRecentInsertTypeSubtypeHistory() const = 0;
    virtual void setTherionMapRecentInsertTypeSubtypeHistory(const QString &json) = 0;

    virtual QString therionMapBackgroundLayers() const = 0;
    virtual void setTherionMapBackgroundLayers(const QString &json) = 0;
};
}
