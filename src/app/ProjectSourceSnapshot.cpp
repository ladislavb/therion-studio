#include "ProjectSourceSnapshot.h"

#include "../core/DocumentFile.h"
#include "../core/TherionFileTypes.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace TherionStudio
{
namespace
{
bool hasProjectSourceFileName(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (isTherionConfigFileName(info.fileName())) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2");
}

bool shouldSkipProjectSourceDirectory(const QFileInfo &info)
{
    const QString name = info.fileName();
    return name == QStringLiteral(".git")
        || name == QStringLiteral(".svn")
        || name == QStringLiteral(".hg")
        || name == QStringLiteral("CMakeFiles")
        || name == QStringLiteral("build")
        || name.startsWith(QStringLiteral("cmake-build"));
}

void collectProjectSourceFilePaths(const QString &directoryPath, QVector<QString> *filePaths)
{
    if (filePaths == nullptr) {
        return;
    }

    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isDir() || shouldSkipProjectSourceDirectory(directoryInfo)) {
        return;
    }

    const QFileInfoList entries = QDir(directoryPath).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            collectProjectSourceFilePaths(entry.absoluteFilePath(), filePaths);
        } else if (entry.isFile() && hasProjectSourceFileName(entry.absoluteFilePath())) {
            filePaths->append(entry.absoluteFilePath());
        }
    }
}

QHash<QString, QString> normalizedInMemoryContents(const QHash<QString, QString> &inMemoryContentsByPath)
{
    QHash<QString, QString> normalizedContents;
    QVector<QString> originalPaths;
    originalPaths.reserve(inMemoryContentsByPath.size());
    for (auto it = inMemoryContentsByPath.constBegin(); it != inMemoryContentsByPath.constEnd(); ++it) {
        originalPaths.append(it.key());
    }
    std::sort(originalPaths.begin(), originalPaths.end(), [](const QString &left, const QString &right) {
        const QString normalizedLeft = normalizeProjectSourcePath(left);
        const QString normalizedRight = normalizeProjectSourcePath(right);
        if (normalizedLeft == normalizedRight) {
            return left < right;
        }
        return normalizedLeft < normalizedRight;
    });

    for (const QString &originalPath : std::as_const(originalPaths)) {
        const QString normalizedPath = normalizeProjectSourcePath(originalPath);
        if (!normalizedPath.isEmpty()) {
            normalizedContents.insert(normalizedPath, inMemoryContentsByPath.value(originalPath));
        }
    }

    return normalizedContents;
}

QString encodeKeyPart(const QString &value)
{
    QString encoded = value;
    encoded.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    encoded.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    encoded.replace(QLatin1Char('='), QStringLiteral("\\="));
    return encoded;
}

}

QVector<QString> ProjectSourceSnapshot::knownFilePaths() const
{
    QVector<QString> filePaths;
    filePaths.reserve(documents.size());
    for (const ProjectSourceDocument &document : documents) {
        filePaths.append(document.normalizedPath);
    }
    return filePaths;
}

QString ProjectSourceRequestKey::stableKey() const
{
    QStringList parts;
    parts.reserve(3 + inMemoryDocuments.size());
    parts.append(QStringLiteral("root=") + encodeKeyPart(normalizedProjectRootPath));
    parts.append(QStringLiteral("config=") + encodeKeyPart(normalizedPreferredConfigPath));
    parts.append(QStringLiteral("policy=") + encodeKeyPart(filePolicyKey));

    for (const ProjectSourceInMemoryDocumentKey &document : inMemoryDocuments) {
        parts.append(QStringLiteral("memory=") + encodeKeyPart(document.normalizedPath)
                     + QStringLiteral("=") + QString::fromLatin1(document.contentHash.toHex()));
    }

    return parts.join(QLatin1Char('\n'));
}

QString normalizeProjectSourcePath(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        return {};
    }

    const QFileInfo info(trimmedPath);
    const QString canonicalPath = info.canonicalFilePath();
    if (!canonicalPath.isEmpty()) {
        return QDir::cleanPath(canonicalPath);
    }

    return QDir::cleanPath(info.absoluteFilePath());
}

QByteArray projectSourceContentHash(const QString &contents)
{
    return QCryptographicHash::hash(contents.toUtf8(), QCryptographicHash::Sha256);
}

QString projectSourceFilePolicyKey(qsizetype maximumTextBytes)
{
    return QStringLiteral("therion-project-sources-v1:max-text-bytes=%1").arg(maximumTextBytes);
}

ProjectSourceRequestKey projectSourceRequestKey(const QString &projectRootPath,
                                                const QString &preferredConfigPath,
                                                const QHash<QString, QString> &inMemoryContentsByPath,
                                                const QString &filePolicyKey)
{
    ProjectSourceRequestKey key;
    key.normalizedProjectRootPath = normalizeProjectSourcePath(projectRootPath);
    key.normalizedPreferredConfigPath = normalizeProjectSourcePath(preferredConfigPath);
    key.filePolicyKey = filePolicyKey;
    key.inMemoryDocuments.reserve(inMemoryContentsByPath.size());

    for (auto it = inMemoryContentsByPath.cbegin(); it != inMemoryContentsByPath.cend(); ++it) {
        key.inMemoryDocuments.push_back(
            {normalizeProjectSourcePath(it.key()), projectSourceContentHash(it.value())});
    }

    std::sort(key.inMemoryDocuments.begin(),
              key.inMemoryDocuments.end(),
              [](const ProjectSourceInMemoryDocumentKey &left,
                 const ProjectSourceInMemoryDocumentKey &right) {
                  if (left.normalizedPath == right.normalizedPath) {
                      return left.contentHash < right.contentHash;
                  }
                  return left.normalizedPath < right.normalizedPath;
              });

    return key;
}

ProjectSourceSnapshot collectProjectSourceSnapshot(const QString &projectRootPath,
                                                   const QString &preferredConfigPath,
                                                   const QHash<QString, QString> &inMemoryContentsByPath,
                                                   qsizetype maximumTextBytes)
{
    ProjectSourceSnapshot snapshot;
    snapshot.requestKey = projectSourceRequestKey(projectRootPath,
                                                  preferredConfigPath,
                                                  inMemoryContentsByPath,
                                                  projectSourceFilePolicyKey(maximumTextBytes));

    const QString normalizedProjectRootPath = normalizeProjectSourcePath(projectRootPath);
    if (normalizedProjectRootPath.isEmpty() || !QFileInfo(normalizedProjectRootPath).isDir()) {
        return snapshot;
    }

    const QHash<QString, QString> memoryContents = normalizedInMemoryContents(inMemoryContentsByPath);

    QVector<QString> candidateFilePaths;
    collectProjectSourceFilePaths(normalizedProjectRootPath, &candidateFilePaths);
    std::sort(candidateFilePaths.begin(), candidateFilePaths.end(), [](const QString &left, const QString &right) {
        return normalizeProjectSourcePath(left).toLower() < normalizeProjectSourcePath(right).toLower();
    });

    QSet<QString> collectedPaths;
    for (const QString &candidateFilePath : std::as_const(candidateFilePaths)) {
        const QString normalizedPath = normalizeProjectSourcePath(candidateFilePath);
        if (normalizedPath.isEmpty() || collectedPaths.contains(normalizedPath)) {
            continue;
        }
        collectedPaths.insert(normalizedPath);

        const auto memoryIt = memoryContents.constFind(normalizedPath);
        if (memoryIt != memoryContents.constEnd()) {
            snapshot.documents.append({normalizedPath,
                                       *memoryIt,
                                       ProjectSourceDocumentOrigin::InMemoryOverride,
                                       QFileInfo(normalizedPath).size(),
                                       true});
            continue;
        }

        const QFileInfo info(normalizedPath);
        ProjectSourceDocument document;
        document.normalizedPath = normalizedPath;
        document.origin = ProjectSourceDocumentOrigin::Filesystem;
        document.filesystemSizeBytes = info.size();
        if (maximumTextBytes >= 0 && info.size() > maximumTextBytes) {
            document.textLoaded = false;
        } else if (!DocumentFile::readTextFile(normalizedPath, &document.text, nullptr, nullptr, nullptr)) {
            document.textLoaded = false;
        }
        snapshot.documents.append(document);
    }

    QVector<QString> memoryPaths;
    memoryPaths.reserve(memoryContents.size());
    for (auto it = memoryContents.constBegin(); it != memoryContents.constEnd(); ++it) {
        memoryPaths.append(it.key());
    }
    std::sort(memoryPaths.begin(), memoryPaths.end(), [](const QString &left, const QString &right) {
        return left.toLower() < right.toLower();
    });

    for (const QString &memoryPath : std::as_const(memoryPaths)) {
        if (collectedPaths.contains(memoryPath) || !hasProjectSourceFileName(memoryPath)) {
            continue;
        }
        collectedPaths.insert(memoryPath);
        snapshot.documents.append({memoryPath,
                                   memoryContents.value(memoryPath),
                                   ProjectSourceDocumentOrigin::InMemoryOnly,
                                   -1,
                                   true});
    }

    return snapshot;
}

ProjectStructureIndexSourceSet projectStructureIndexSourceSet(const ProjectSourceSnapshot &snapshot)
{
    ProjectStructureIndexSourceSet sourceSet;
    sourceSet.projectRootPath = snapshot.requestKey.normalizedProjectRootPath;
    sourceSet.preferredConfigPath = snapshot.requestKey.normalizedPreferredConfigPath;
    sourceSet.sources.reserve(snapshot.documents.size());
    for (const ProjectSourceDocument &document : snapshot.documents) {
        sourceSet.sources.append({document.normalizedPath,
                                  document.text,
                                  document.textLoaded,
                                  nullptr});
    }
    return sourceSet;
}
}
