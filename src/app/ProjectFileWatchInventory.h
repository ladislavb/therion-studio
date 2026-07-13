#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace TherionStudio
{

struct ProjectFileWatchInventoryRequest
{
    QString projectRootPath;
};

struct ProjectFileWatchInventory
{
    QString projectRootPath;
    QStringList directories;
    QStringList files;
    QHash<QString, QString> signatures;
    QStringList skippedPaths;
    QStringList discoveryErrors;
};

class ProjectFileWatchInventoryCollector final
{
public:
    static ProjectFileWatchInventory collect(const ProjectFileWatchInventoryRequest &request);
};

} // namespace TherionStudio
