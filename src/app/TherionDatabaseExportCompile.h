#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QString>

#include <memory>

class QTemporaryDir;

namespace TherionStudio
{
// The config bytes unchanged, followed by an `export database -fmt sql` line writing to databasePath.
[[nodiscard]] QByteArray therionConfigWithDatabaseExport(const QByteArray &configText, const QString &databasePath);

// Files of a one-off compile that adds a database export without changing the user's config. The temporary config
// sits next to the user's so relative paths resolve the same; the database goes to a temporary folder and stays
// until the next such compile, so the compile figures can still be read after Therion finishes.
class TherionDatabaseExportCompile final
{
    Q_DECLARE_TR_FUNCTIONS(TherionDatabaseExportCompile)

public:
    TherionDatabaseExportCompile();
    ~TherionDatabaseExportCompile();

    TherionDatabaseExportCompile(const TherionDatabaseExportCompile &) = delete;
    TherionDatabaseExportCompile &operator=(const TherionDatabaseExportCompile &) = delete;

    // Writes the temporary config for configPath and returns its path, or an empty path and errorMessage.
    QString prepare(const QString &configPath, QString *errorMessage);
    // Removes the temporary config once Therion has finished or failed to start.
    void removeTemporaryConfig();

private:
    std::unique_ptr<QTemporaryDir> databaseDir_;
    QString temporaryConfigPath_;
};
}
