#include "ProjectVersioningSettings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QUuid>

#include <utility>

namespace TherionStudio
{
namespace
{
QString projectKey(const QString &projectRootPath)
{
    const QByteArray digest =
        QCryptographicHash::hash(projectRootPath.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("projectVersioning/%1/").arg(QString::fromLatin1(digest));
}

QString canonicalProjectRoot(const QString &path)
{
    const QFileInfo info(path);
    return path.trimmed().isEmpty() || !info.isDir() ? QString() : info.canonicalFilePath();
}
}

ProjectVersioningSettings::ProjectVersioningSettings(QSettings &settings,
                                                     QString applicationDataDirectory)
    : settings_(settings)
    , applicationDataDirectory_(std::move(applicationDataDirectory))
{
}

ProjectVersioningSettings::Result ProjectVersioningSettings::projectSettings(const QString &projectRootPath) const
{
    Result result;
    const QString root = canonicalProjectRoot(projectRootPath);
    if (root.isEmpty()) {
        result.error = QStringLiteral("Select an existing project root before configuring local history.");
        return result;
    }
    if (applicationDataDirectory_.isEmpty() || !QDir::isAbsolutePath(applicationDataDirectory_)) {
        result.error = QStringLiteral("An absolute application data directory is required for local history.");
        return result;
    }
    if (settings_.status() != QSettings::NoError) {
        result.error = QStringLiteral("Project versioning settings could not be read.");
        return result;
    }

    const QString prefix = projectKey(root);
    result.settings.enabled = settings_.value(prefix + QStringLiteral("enabled"), false).toBool();
    result.settings.automaticEnabled = settings_.value(prefix + QStringLiteral("automaticEnabled"), false).toBool();
    bool intervalOk = false;
    result.settings.intervalMinutes =
        settings_.value(prefix + QStringLiteral("intervalMinutes"), 5).toInt(&intervalOk);
    if (!intervalOk || result.settings.intervalMinutes < 1 || result.settings.intervalMinutes > 1440) {
        result.error = QStringLiteral("The automatic history interval must be between 1 and 1440 minutes.");
        return result;
    }
    result.settings.projectId = settings_.value(prefix + QStringLiteral("projectId")).toString();
    if (!result.settings.projectId.isEmpty()) {
        const QUuid id(result.settings.projectId);
        if (id.isNull() || id.toString(QUuid::WithoutBraces) != result.settings.projectId) {
            result.error = QStringLiteral("The saved local history project ID is invalid.");
            return result;
        }
        result.settings.historyDirectory = QDir(applicationDataDirectory_).filePath(
            QStringLiteral("project-history/%1.git").arg(result.settings.projectId));
    } else if (result.settings.enabled) {
        result.error = QStringLiteral("Local history is enabled but its project ID is missing.");
        return result;
    }
    result.succeeded = true;
    return result;
}

ProjectVersioningSettings::Result ProjectVersioningSettings::setEnabled(const QString &projectRootPath,
                                                                        bool enabled)
{
    Result result = projectSettings(projectRootPath);
    if (!result.succeeded || result.settings.enabled == enabled) {
        return result;
    }
    const QString prefix = projectKey(canonicalProjectRoot(projectRootPath));
    if (enabled && result.settings.projectId.isEmpty()) {
        result.settings.projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings_.setValue(prefix + QStringLiteral("projectId"), result.settings.projectId);
    }
    settings_.setValue(prefix + QStringLiteral("enabled"), enabled);
    settings_.sync();
    if (settings_.status() != QSettings::NoError) {
        result.succeeded = false;
        result.error = QStringLiteral("Project versioning settings could not be saved.");
        return result;
    }
    return projectSettings(projectRootPath);
}

ProjectVersioningSettings::Result ProjectVersioningSettings::setAutomatic(
    const QString &projectRootPath, bool enabled, int intervalMinutes)
{
    Result result = projectSettings(projectRootPath);
    if (!result.succeeded) {
        return result;
    }
    if (intervalMinutes < 1 || intervalMinutes > 1440) {
        result.succeeded = false;
        result.error = QStringLiteral("The automatic history interval must be between 1 and 1440 minutes.");
        return result;
    }
    const QString prefix = projectKey(canonicalProjectRoot(projectRootPath));
    settings_.setValue(prefix + QStringLiteral("automaticEnabled"), enabled);
    settings_.setValue(prefix + QStringLiteral("intervalMinutes"), intervalMinutes);
    settings_.sync();
    if (settings_.status() != QSettings::NoError) {
        result.succeeded = false;
        result.error = QStringLiteral("Automatic history settings could not be saved.");
        return result;
    }
    return projectSettings(projectRootPath);
}
} // namespace TherionStudio
