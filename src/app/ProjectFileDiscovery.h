#pragma once

#include <QFileInfo>
#include <QString>
#include <QVector>

#include <functional>

namespace TherionStudio
{

struct ProjectDiscoveredFile
{
    QString filePath;
    QString relativePath;
};

class ProjectFileDiscovery final
{
public:
    using FilePredicate = std::function<bool(const QFileInfo &)>;

    static QString canonicalOrAbsolutePath(const QString &path);
    static bool shouldSkipDirectory(const QFileInfo &info);
    static QVector<ProjectDiscoveredFile> collectFiles(const QString &projectRootPath,
                                                       const FilePredicate &predicate);
};

} // namespace TherionStudio
