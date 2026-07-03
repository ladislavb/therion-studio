#include "ProjectFileDiscovery.h"

#include <QDir>

namespace TherionStudio
{
namespace
{
void collectFilesRecursive(const QString &directoryPath,
                           const QString &projectRootPath,
                           const ProjectFileDiscovery::FilePredicate &predicate,
                           QVector<ProjectDiscoveredFile> *files)
{
    if (files == nullptr) {
        return;
    }

    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isDir() || ProjectFileDiscovery::shouldSkipDirectory(directoryInfo)) {
        return;
    }

    const QFileInfoList entries = QDir(directoryInfo.absoluteFilePath()).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            collectFilesRecursive(entry.absoluteFilePath(), projectRootPath, predicate, files);
            continue;
        }

        if (!predicate || !predicate(entry)) {
            continue;
        }

        const QString filePath = ProjectFileDiscovery::canonicalOrAbsolutePath(entry.absoluteFilePath());
        QString relativePath = QDir(projectRootPath).relativeFilePath(filePath);
        if (relativePath.startsWith(QStringLiteral("../"))) {
            relativePath = filePath;
        }
        files->append({
            filePath,
            QDir::toNativeSeparators(relativePath),
        });
    }
}
}

QString ProjectFileDiscovery::canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

bool ProjectFileDiscovery::shouldSkipDirectory(const QFileInfo &info)
{
    const QString name = info.fileName();
    return name == QStringLiteral(".git")
        || name == QStringLiteral(".svn")
        || name == QStringLiteral(".hg")
        || name == QStringLiteral("CMakeFiles")
        || name == QStringLiteral("build")
        || name.startsWith(QStringLiteral("cmake-build"));
}

QVector<ProjectDiscoveredFile> ProjectFileDiscovery::collectFiles(const QString &projectRootPath,
                                                                  const FilePredicate &predicate)
{
    const QString normalizedProjectRootPath = canonicalOrAbsolutePath(projectRootPath);
    QVector<ProjectDiscoveredFile> files;
    if (normalizedProjectRootPath.trimmed().isEmpty() || !QDir(normalizedProjectRootPath).exists()) {
        return files;
    }

    collectFilesRecursive(normalizedProjectRootPath, normalizedProjectRootPath, predicate, &files);
    return files;
}

} // namespace TherionStudio
