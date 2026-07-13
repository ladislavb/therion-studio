#include "MainWindow.h"

#include "ProjectFileDiscovery.h"
#include "ProjectFileWatchDelta.h"
#include "ProjectScanCacheService.h"

#include <QDebug>
#include <QFileSystemWatcher>
#include <QSignalBlocker>

namespace
{
QStringList watchedPaths(const QFileSystemWatcher *watcher)
{
    if (watcher == nullptr) {
        return {};
    }
    QStringList paths = watcher->directories();
    paths.append(watcher->files());
    paths.removeDuplicates();
    paths.sort(Qt::CaseSensitive);
    return paths;
}
}

void MainWindow::rebuildProjectFileWatcher()
{
    if (projectFileWatcher_ == nullptr) {
        return;
    }
    if (projectRootPath_.trimmed().isEmpty()) {
        clearProjectFileWatcher();
        return;
    }
    projectFileWatchInventoryService_->requestInventory(projectRootPath_);
}

void MainWindow::clearProjectFileWatcher()
{
    if (projectFileWatcher_ == nullptr) {
        return;
    }

    const QSignalBlocker blocker(projectFileWatcher_);
    const QStringList paths = watchedPaths(projectFileWatcher_);
    if (!paths.isEmpty()) {
        projectFileWatcher_->removePaths(paths);
    }
    projectFileWatcherSignatures_.clear();
    projectFileWatcherFailedPaths_.clear();
    projectFileWatcherInventoryRootPath_.clear();
}

void MainWindow::handleProjectFileWatchInventoryFinished(
    const TherionStudio::ProjectFileWatchInventoryService::Result &result)
{
    if (projectFileWatcher_ == nullptr || projectRootPath_.trimmed().isEmpty()
        || !projectFileWatchInventoryService_->isLatestRequestResult(result)) {
        return;
    }

    const QString currentRootPath = TherionStudio::ProjectFileDiscovery::canonicalOrAbsolutePath(projectRootPath_);
    if (result.projectRootPath != currentRootPath) {
        return;
    }

    const bool sameInventoryRoot = projectFileWatcherInventoryRootPath_ == result.projectRootPath;
    const QHash<QString, QString> previousSignatures = projectFileWatcherSignatures_;
    const QSignalBlocker blocker(projectFileWatcher_);
    const TherionStudio::ProjectFileWatchDelta delta = TherionStudio::ProjectFileWatchDeltaPlanner::plan(
        result.inventory, projectFileWatcher_->directories(), projectFileWatcher_->files());

    if (!delta.directoriesToRemove.isEmpty()) {
        projectFileWatcher_->removePaths(delta.directoriesToRemove);
    }
    if (!delta.filesToRemove.isEmpty()) {
        projectFileWatcher_->removePaths(delta.filesToRemove);
    }

    projectFileWatcherFailedPaths_.clear();
    if (!delta.directoriesToAdd.isEmpty()) {
        projectFileWatcherFailedPaths_.append(projectFileWatcher_->addPaths(delta.directoriesToAdd));
    }
    if (!delta.filesToAdd.isEmpty()) {
        projectFileWatcherFailedPaths_.append(projectFileWatcher_->addPaths(delta.filesToAdd));
    }
    projectFileWatcherFailedPaths_.removeDuplicates();
    projectFileWatcherFailedPaths_.sort(Qt::CaseSensitive);
    if (!projectFileWatcherFailedPaths_.isEmpty()) {
        qWarning().noquote() << "Project file watcher could not watch"
                             << projectFileWatcherFailedPaths_.size() << "path(s):"
                             << projectFileWatcherFailedPaths_.mid(0, 5).join(QStringLiteral(", "));
    }

    const QStringList activePaths = watchedPaths(projectFileWatcher_);
    projectFileWatcherSignatures_.clear();
    for (const QString &path : activePaths) {
        const auto signature = result.inventory.signatures.constFind(path);
        if (signature != result.inventory.signatures.cend()) {
            projectFileWatcherSignatures_.insert(path, *signature);
        }
    }
    projectFileWatcherInventoryRootPath_ = result.projectRootPath;

    if (sameInventoryRoot && previousSignatures != projectFileWatcherSignatures_) {
        handleProjectFileSystemMutation(result.projectRootPath);
    }
}

void MainWindow::handleProjectDirectoryChanged(const QString &directoryPath)
{
    if (!isDocumentPathInsideOpenProject(directoryPath)) {
        return;
    }
    rebuildProjectFileWatcher();
}

void MainWindow::handleProjectFileChanged(const QString &filePath)
{
    if (!isDocumentPathInsideOpenProject(filePath)) {
        return;
    }
    rebuildProjectFileWatcher();
}

void MainWindow::invalidateProjectScanCache()
{
    if (projectScanCacheService_ == nullptr) {
        return;
    }

    projectScanCacheService_->clearProjectSourceSnapshot();
    projectScanCacheService_->clearProjectIndexSnapshot();
}

void MainWindow::handleProjectFileSystemMutation(const QString &changedPath, const QString &previousPath)
{
    const bool changedInsideProject = isDocumentPathInsideOpenProject(changedPath);
    const bool previousInsideProject = !previousPath.isEmpty() && isDocumentPathInsideOpenProject(previousPath);
    if (!changedInsideProject && !previousInsideProject) {
        return;
    }

    invalidateProjectScanCache();
    clearMissingTherionTargetConfig();
    refreshTherionConfigDisplay();
    rebuildStructureSidebar();
    rebuildMapObjectsTree();
    requestProjectOutputsRefresh();
    requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::ProjectFilesChanged,
                             false);
}
