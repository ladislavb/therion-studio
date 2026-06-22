#pragma once

#include "../src/core/ISessionStore.h"

#include <QHash>

namespace TherionStudio
{
class FakeSessionStore final : public ISessionStore
{
public:
    QString lastProjectPath() const override { return lastProjectPath_; }
    void setLastProjectPath(const QString &projectPath) override { lastProjectPath_ = projectPath; }

    QString lastProjectParentDirectory() const override { return lastProjectParentDirectory_; }
    void setLastProjectParentDirectory(const QString &directoryPath) override { lastProjectParentDirectory_ = directoryPath; }

    QStringList recentProjectPaths() const override { return recentProjectPaths_; }
    void setRecentProjectPaths(const QStringList &projectPaths) override { recentProjectPaths_ = projectPaths; }

    QByteArray mainWindowGeometry() const override { return mainWindowGeometry_; }
    void setMainWindowGeometry(const QByteArray &geometry) override { mainWindowGeometry_ = geometry; }

    QByteArray mainWindowState() const override { return mainWindowState_; }
    void setMainWindowState(const QByteArray &state) override { mainWindowState_ = state; }

    QStringList openDocumentPaths() const override { return openDocumentPaths_; }
    void setOpenDocumentPaths(const QStringList &documentPaths) override { openDocumentPaths_ = documentPaths; }

    QStringList recentFilePathsForProject(const QString &projectPath) const override
    {
        return recentFilePathsByProject_.value(projectPath);
    }
    void setRecentFilePathsForProject(const QString &projectPath, const QStringList &filePaths) override
    {
        recentFilePathsByProject_.insert(projectPath, filePaths);
    }

    QString activeDocumentPath() const override { return activeDocumentPath_; }
    void setActiveDocumentPath(const QString &documentPath) override { activeDocumentPath_ = documentPath; }

    QString structureNameOverrides() const override { return structureNameOverrides_; }
    void setStructureNameOverrides(const QString &json) override { structureNameOverrides_ = json; }

    QString applicationLanguage() const override { return applicationLanguage_; }
    void setApplicationLanguage(const QString &language) override { applicationLanguage_ = language; }

    QString defaultTextEditorMode() const override { return defaultTextEditorMode_; }
    void setDefaultTextEditorMode(const QString &mode) override { defaultTextEditorMode_ = mode; }

    QString therionExecutablePath() const override { return therionExecutablePath_; }
    void setTherionExecutablePath(const QString &path) override { therionExecutablePath_ = path; }

    QString therionWorkingDirectory() const override { return therionWorkingDirectory_; }
    void setTherionWorkingDirectory(const QString &path) override { therionWorkingDirectory_ = path; }

    QString therionRunTargetMode() const override { return therionRunTargetMode_; }
    void setTherionRunTargetMode(const QString &mode) override { therionRunTargetMode_ = mode; }

    QString therionTargetConfigPath() const override { return therionTargetConfigPath_; }
    void setTherionTargetConfigPath(const QString &path) override { therionTargetConfigPath_ = path; }

    bool therionMapTouchFriendlyControlsEnabled() const override { return therionMapTouchFriendlyControlsEnabled_; }
    void setTherionMapTouchFriendlyControlsEnabled(bool enabled) override { therionMapTouchFriendlyControlsEnabled_ = enabled; }

    bool therionMapMagnifierEnabled() const override { return therionMapMagnifierEnabled_; }
    void setTherionMapMagnifierEnabled(bool enabled) override { therionMapMagnifierEnabled_ = enabled; }

    bool therionMapObjectsAutoCollapseExpandScrapsEnabled() const override
    {
        return therionMapObjectsAutoCollapseExpandScrapsEnabled_;
    }
    void setTherionMapObjectsAutoCollapseExpandScrapsEnabled(bool enabled) override
    {
        therionMapObjectsAutoCollapseExpandScrapsEnabled_ = enabled;
    }

    QString therionMapRecentInsertTypeSubtypeHistory() const override
    {
        return therionMapRecentInsertTypeSubtypeHistory_;
    }
    void setTherionMapRecentInsertTypeSubtypeHistory(const QString &json) override
    {
        therionMapRecentInsertTypeSubtypeHistory_ = json;
    }

    QString therionMapBackgroundLayers() const override { return therionMapBackgroundLayers_; }
    void setTherionMapBackgroundLayers(const QString &json) override { therionMapBackgroundLayers_ = json; }

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
    QString therionExecutablePath_;
    QString therionWorkingDirectory_;
    QString therionRunTargetMode_;
    QString therionTargetConfigPath_;
    bool therionMapTouchFriendlyControlsEnabled_ = false;
    bool therionMapMagnifierEnabled_ = true;
    bool therionMapObjectsAutoCollapseExpandScrapsEnabled_ = false;
    QString therionMapRecentInsertTypeSubtypeHistory_;
    QString therionMapBackgroundLayers_;
};
}
