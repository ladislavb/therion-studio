#include "TherionDatabaseExportCompile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace TherionStudio
{
namespace
{
const QString kTemporaryConfigFileName = QStringLiteral(".therion-studio-database.thconfig");
const QString kDatabaseFileName = QStringLiteral("database.sql");
}

QByteArray therionConfigWithDatabaseExport(const QByteArray &configText, const QString &databasePath)
{
    QByteArray config = configText;
    if (!config.isEmpty() && !config.endsWith('\n')) {
        config += '\n';
    }
    // Therion splits arguments on spaces, so the path is quoted; forward slashes work on every platform.
    config += QStringLiteral("export database -fmt sql -o \"%1\"\n").arg(QDir::fromNativeSeparators(databasePath)).toUtf8();
    return config;
}

TherionDatabaseExportCompile::TherionDatabaseExportCompile() = default;

TherionDatabaseExportCompile::~TherionDatabaseExportCompile()
{
    removeTemporaryConfig();
}

QString TherionDatabaseExportCompile::prepare(const QString &configPath, QString *errorMessage)
{
    removeTemporaryConfig();
    QFile config(configPath);
    if (!config.open(QIODevice::ReadOnly)) {
        *errorMessage = tr("Could not read %1.").arg(QDir::toNativeSeparators(configPath));
        return QString();
    }
    const QByteArray configText = config.readAll();

    auto databaseDir = std::make_unique<QTemporaryDir>();
    if (!databaseDir->isValid()) {
        *errorMessage = tr("Could not create a temporary folder for the database export.");
        return QString();
    }

    const QString temporaryConfigPath = QFileInfo(configPath).dir().filePath(kTemporaryConfigFileName);
    QFile temporaryConfig(temporaryConfigPath);
    const QByteArray text = therionConfigWithDatabaseExport(configText, databaseDir->filePath(kDatabaseFileName));
    if (!temporaryConfig.open(QIODevice::WriteOnly | QIODevice::Truncate) || temporaryConfig.write(text) != text.size()) {
        *errorMessage = tr("Could not write %1.").arg(QDir::toNativeSeparators(temporaryConfigPath));
        temporaryConfig.remove();
        return QString();
    }

    databaseDir_ = std::move(databaseDir);
    temporaryConfigPath_ = temporaryConfigPath;
    return temporaryConfigPath_;
}

void TherionDatabaseExportCompile::removeTemporaryConfig()
{
    if (!temporaryConfigPath_.isEmpty()) {
        QFile::remove(temporaryConfigPath_);
        temporaryConfigPath_.clear();
    }
}
}
