#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace TherionStudio
{
constexpr qsizetype kDefaultMaximumProjectSourceTextBytes = 4 * 1024 * 1024;

enum class ProjectSourceDocumentOrigin
{
    Filesystem,
    InMemoryOverride,
    InMemoryOnly,
};

struct ProjectSourceInMemoryDocumentKey
{
    QString normalizedPath;
    QByteArray contentHash;

    bool operator==(const ProjectSourceInMemoryDocumentKey &other) const = default;
};

struct ProjectSourceRequestKey
{
    QString normalizedProjectRootPath;
    QString normalizedPreferredConfigPath;
    QString filePolicyKey;
    QVector<ProjectSourceInMemoryDocumentKey> inMemoryDocuments;

    [[nodiscard]] QString stableKey() const;
    bool operator==(const ProjectSourceRequestKey &other) const = default;
};

struct ProjectSourceDocument
{
    QString normalizedPath;
    QString text;
    ProjectSourceDocumentOrigin origin = ProjectSourceDocumentOrigin::Filesystem;
    qint64 filesystemSizeBytes = -1;
    bool textLoaded = true;

    bool operator==(const ProjectSourceDocument &other) const = default;
};

struct ProjectSourceSnapshot
{
    ProjectSourceRequestKey requestKey;
    QVector<ProjectSourceDocument> documents;

    [[nodiscard]] QVector<QString> knownFilePaths() const;
};

[[nodiscard]] QString normalizeProjectSourcePath(const QString &path);

[[nodiscard]] QByteArray projectSourceContentHash(const QString &contents);

[[nodiscard]] ProjectSourceRequestKey projectSourceRequestKey(
    const QString &projectRootPath,
    const QString &preferredConfigPath,
    const QHash<QString, QString> &inMemoryContentsByPath,
    const QString &filePolicyKey = QString());

[[nodiscard]] ProjectSourceSnapshot collectProjectSourceSnapshot(
    const QString &projectRootPath,
    const QString &preferredConfigPath,
    const QHash<QString, QString> &inMemoryContentsByPath,
    qsizetype maximumTextBytes = kDefaultMaximumProjectSourceTextBytes);
}
