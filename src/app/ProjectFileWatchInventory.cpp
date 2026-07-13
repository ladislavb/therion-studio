#include "ProjectFileWatchInventory.h"

#include "ProjectFileDiscovery.h"
#include "../core/TherionFileTypes.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace TherionStudio
{
namespace
{
bool isProjectValidationFile(const QFileInfo &info)
{
    if (!info.isFile()) {
        return false;
    }
    if (isTherionConfigFileName(info.fileName())) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("th") || suffix == QStringLiteral("th2");
}

bool isPathInsideRoot(const QString &projectRootPath, const QString &path)
{
    const QString relativePath = QDir(projectRootPath).relativeFilePath(path);
    return relativePath != QStringLiteral("..")
        && !relativePath.startsWith(QStringLiteral("../"))
        && !QDir::isAbsolutePath(relativePath);
}

QString fileSignature(const QFileInfo &info)
{
    if (!info.exists()) {
        return QStringLiteral("missing");
    }
    return QStringLiteral("file|%1|%2")
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch());
}

QString directorySignature(const QString &directoryPath)
{
    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.exists() || !directoryInfo.isDir()) {
        return QStringLiteral("missing");
    }

    QStringList entrySignatures;
    const QFileInfoList entries = QDir(directoryInfo.absoluteFilePath()).entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isSymLink()) {
            continue;
        }
        if (entry.isDir()) {
            if (!ProjectFileDiscovery::shouldSkipDirectory(entry)) {
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

void collectDirectory(const QString &directoryPath,
                      const QString &projectRootPath,
                      ProjectFileWatchInventory *inventory,
                      QSet<QString> *visitedDirectories)
{
    Q_ASSERT(inventory != nullptr);
    Q_ASSERT(visitedDirectories != nullptr);

    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isDir()) {
        inventory->discoveryErrors.append(QStringLiteral("Unreadable project directory: %1")
                                              .arg(directoryInfo.absoluteFilePath()));
        return;
    }
    if (directoryInfo.isSymLink() || ProjectFileDiscovery::shouldSkipDirectory(directoryInfo)) {
        inventory->skippedPaths.append(directoryInfo.absoluteFilePath());
        return;
    }

    const QString normalizedDirectoryPath =
        ProjectFileDiscovery::canonicalOrAbsolutePath(directoryInfo.absoluteFilePath());
    if (!isPathInsideRoot(projectRootPath, normalizedDirectoryPath)) {
        inventory->skippedPaths.append(directoryInfo.absoluteFilePath());
        return;
    }
    if (visitedDirectories->contains(normalizedDirectoryPath)) {
        return;
    }
    visitedDirectories->insert(normalizedDirectoryPath);
    inventory->directories.append(normalizedDirectoryPath);
    inventory->signatures.insert(normalizedDirectoryPath, directorySignature(normalizedDirectoryPath));

    const QFileInfoList entries = QDir(directoryInfo.absoluteFilePath()).entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isSymLink()) {
            inventory->skippedPaths.append(entry.absoluteFilePath());
            continue;
        }
        if (entry.isDir()) {
            collectDirectory(entry.absoluteFilePath(), projectRootPath, inventory, visitedDirectories);
            continue;
        }
        if (!isProjectValidationFile(entry)) {
            continue;
        }

        const QString normalizedFilePath =
            ProjectFileDiscovery::canonicalOrAbsolutePath(entry.absoluteFilePath());
        if (!isPathInsideRoot(projectRootPath, normalizedFilePath)) {
            inventory->skippedPaths.append(entry.absoluteFilePath());
            continue;
        }
        inventory->files.append(normalizedFilePath);
        inventory->signatures.insert(normalizedFilePath, fileSignature(QFileInfo(normalizedFilePath)));
    }
}

void sortAndDedupe(QStringList *paths)
{
    Q_ASSERT(paths != nullptr);
    paths->removeDuplicates();
    paths->sort(Qt::CaseSensitive);
}
}

ProjectFileWatchInventory ProjectFileWatchInventoryCollector::collect(
    const ProjectFileWatchInventoryRequest &request)
{
    ProjectFileWatchInventory inventory;
    const QString requestedRootPath = request.projectRootPath.trimmed();
    if (requestedRootPath.isEmpty()) {
        return inventory;
    }

    inventory.projectRootPath = ProjectFileDiscovery::canonicalOrAbsolutePath(requestedRootPath);
    const QFileInfo rootInfo(inventory.projectRootPath);
    if (!rootInfo.isDir()) {
        inventory.discoveryErrors.append(QStringLiteral("Project root is not a directory: %1")
                                             .arg(requestedRootPath));
        return inventory;
    }

    QSet<QString> visitedDirectories;
    collectDirectory(inventory.projectRootPath,
                     inventory.projectRootPath,
                     &inventory,
                     &visitedDirectories);
    sortAndDedupe(&inventory.directories);
    sortAndDedupe(&inventory.files);
    sortAndDedupe(&inventory.skippedPaths);
    sortAndDedupe(&inventory.discoveryErrors);
    return inventory;
}

} // namespace TherionStudio
