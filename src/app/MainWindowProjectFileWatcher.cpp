#include "MainWindow.h"

#include "../core/TherionFileTypes.h"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFileSystemWatcher>
#include <QSignalBlocker>

#include <utility>

namespace
{
bool shouldSkipProjectWatchDirectory(const QFileInfo &info)
{
    const QString name = info.fileName();
    return name == QStringLiteral(".git")
        || name == QStringLiteral(".svn")
        || name == QStringLiteral(".hg")
        || name == QStringLiteral("CMakeFiles")
        || name == QStringLiteral("build")
        || name.startsWith(QStringLiteral("cmake-build"));
}

QString canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

bool isProjectValidationFile(const QFileInfo &info)
{
    if (!info.isFile()) {
        return false;
    }
    if (TherionStudio::isTherionConfigFileName(info.fileName())) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2");
}

QString projectWatchSignatureForFile(const QFileInfo &info)
{
    if (!info.exists()) {
        return QStringLiteral("missing");
    }

    return QStringLiteral("file|%1|%2")
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch());
}

QString projectWatchSignatureForDirectory(const QString &directoryPath)
{
    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.exists() || !directoryInfo.isDir()) {
        return QStringLiteral("missing");
    }

    QStringList entrySignatures;
    const QFileInfoList entries = QDir(directoryInfo.absoluteFilePath()).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            if (!shouldSkipProjectWatchDirectory(entry)) {
                entrySignatures.append(QStringLiteral("dir|%1|%2")
                                           .arg(entry.fileName())
                                           .arg(entry.lastModified().toMSecsSinceEpoch()));
            }
            continue;
        }
        if (isProjectValidationFile(entry)) {
            entrySignatures.append(QStringLiteral("file|%1|%2|%3")
                                       .arg(entry.fileName())
                                       .arg(entry.size())
                                       .arg(entry.lastModified().toMSecsSinceEpoch()));
        }
    }

    return entrySignatures.join(QLatin1Char('\n'));
}

QString projectWatchSignature(const QString &path)
{
    const QFileInfo info(path);
    if (info.isDir()) {
        return projectWatchSignatureForDirectory(info.absoluteFilePath());
    }
    return projectWatchSignatureForFile(info);
}

void collectProjectWatchPaths(const QString &directoryPath, QStringList *directories, QStringList *files)
{
    if (directories == nullptr || files == nullptr) {
        return;
    }

    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isDir()
        || directoryInfo.isSymLink()
        || shouldSkipProjectWatchDirectory(directoryInfo)) {
        return;
    }

    directories->append(canonicalOrAbsolutePath(directoryInfo.absoluteFilePath()));

    const QFileInfoList entries = QDir(directoryInfo.absoluteFilePath()).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            collectProjectWatchPaths(entry.absoluteFilePath(), directories, files);
        } else if (isProjectValidationFile(entry)) {
            files->append(canonicalOrAbsolutePath(entry.absoluteFilePath()));
        }
    }
}
}

void MainWindow::rebuildProjectFileWatcher()
{
    if (projectFileWatcher_ == nullptr) {
        return;
    }

    const QSignalBlocker blocker(projectFileWatcher_);
    clearProjectFileWatcher();
    if (projectRootPath_.trimmed().isEmpty() || !QDir(projectRootPath_).exists()) {
        return;
    }

    QStringList directories;
    QStringList files;
    collectProjectWatchPaths(projectRootPath_, &directories, &files);
    directories.removeDuplicates();
    files.removeDuplicates();

    if (!directories.isEmpty()) {
        projectFileWatcher_->addPaths(directories);
    }
    if (!files.isEmpty()) {
        projectFileWatcher_->addPaths(files);
    }

    projectFileWatcherSignatures_.clear();
    for (const QString &directory : std::as_const(directories)) {
        projectFileWatcherSignatures_.insert(directory, projectWatchSignature(directory));
    }
    for (const QString &file : std::as_const(files)) {
        projectFileWatcherSignatures_.insert(file, projectWatchSignature(file));
    }
}

void MainWindow::clearProjectFileWatcher()
{
    if (projectFileWatcher_ == nullptr) {
        return;
    }

    const QStringList directories = projectFileWatcher_->directories();
    if (!directories.isEmpty()) {
        projectFileWatcher_->removePaths(directories);
    }

    const QStringList files = projectFileWatcher_->files();
    if (!files.isEmpty()) {
        projectFileWatcher_->removePaths(files);
    }
    projectFileWatcherSignatures_.clear();
}

void MainWindow::handleProjectDirectoryChanged(const QString &directoryPath)
{
    if (!isDocumentPathInsideOpenProject(directoryPath)) {
        return;
    }

    const QString normalizedPath = canonicalOrAbsolutePath(directoryPath);
    const QString previousSignature = projectFileWatcherSignatures_.value(normalizedPath);
    const QString currentSignature = projectWatchSignature(normalizedPath);
    if (previousSignature == currentSignature) {
        return;
    }
    projectFileWatcherSignatures_.insert(normalizedPath, currentSignature);

    rebuildProjectFileWatcher();
    requestProjectOutputsRefresh();
    requestProjectValidationForFileSystemChange(directoryPath);
}

void MainWindow::handleProjectFileChanged(const QString &filePath)
{
    if (!isDocumentPathInsideOpenProject(filePath)) {
        return;
    }

    const QString normalizedPath = canonicalOrAbsolutePath(filePath);
    const QString previousSignature = projectFileWatcherSignatures_.value(normalizedPath);
    const QString currentSignature = projectWatchSignature(normalizedPath);
    if (previousSignature == currentSignature) {
        return;
    }
    projectFileWatcherSignatures_.insert(normalizedPath, currentSignature);

    if (QFileInfo(normalizedPath).exists()
        && projectFileWatcher_ != nullptr
        && !projectFileWatcher_->files().contains(normalizedPath)) {
        projectFileWatcher_->addPath(normalizedPath);
    }

    requestProjectOutputsRefresh();
    requestProjectValidationForFileSystemChange(filePath);
}

void MainWindow::requestProjectValidationForFileSystemChange(const QString &changedPath)
{
    if (!isDocumentPathInsideOpenProject(changedPath)) {
        return;
    }

    requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::ProjectFilesChanged,
                             false);
}
