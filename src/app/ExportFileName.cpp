#include "ExportFileName.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace TherionStudio
{
namespace
{
QString sanitizedName(QString value)
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    value.replace(QRegularExpression(QStringLiteral("-+")), QStringLiteral("-"));
    value = value.trimmed();
    while (value.startsWith(QLatin1Char('-')) || value.startsWith(QLatin1Char('.'))) {
        value.remove(0, 1);
    }
    while (value.endsWith(QLatin1Char('-')) || value.endsWith(QLatin1Char('.'))) {
        value.chop(1);
    }
    return value.isEmpty() ? QStringLiteral("project") : value;
}

QString projectNameFromDirectoryPath(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        return {};
    }
    return QDir(QDir::cleanPath(trimmedPath)).dirName();
}

QString projectNameFromFallbackPath(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        return {};
    }
    QFileInfo info(trimmedPath);
    if (info.isDir()) {
        return projectNameFromDirectoryPath(trimmedPath);
    }
    const QString parentName = info.absoluteDir().dirName();
    if (!parentName.isEmpty()) {
        return parentName;
    }
    return info.completeBaseName();
}
} // namespace

QString defaultExportFileName(const QString &kind,
                              const QString &projectRootPath,
                              const QString &fallbackPath,
                              const QString &extension,
                              const QDateTime &timestamp)
{
    const QString projectName =
        sanitizedName(!projectRootPath.trimmed().isEmpty() ? projectNameFromDirectoryPath(projectRootPath)
                                                           : projectNameFromFallbackPath(fallbackPath));
    const QString timestampText = timestamp.toLocalTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString cleanKind = sanitizedName(kind);
    const QString cleanExtension = extension.startsWith(QLatin1Char('.')) ? extension.mid(1) : extension;
    return QStringLiteral("therion-studio-%1-%2-%3.%4")
        .arg(cleanKind, projectName, timestampText, cleanExtension);
}

QString defaultArtifactExportFileName(const QString &artifactPath,
                                      const QString &fallbackKind,
                                      const QString &extension,
                                      const QDateTime &timestamp)
{
    const QString trimmedPath = artifactPath.trimmed();
    const QString baseName = trimmedPath.isEmpty() ? QString() : QFileInfo(trimmedPath).completeBaseName();
    const QString cleanBaseName = sanitizedName(baseName.isEmpty() ? fallbackKind : baseName);
    const QString timestampText = timestamp.toLocalTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString cleanExtension = extension.startsWith(QLatin1Char('.')) ? extension.mid(1) : extension;
    return QStringLiteral("%1-%2.%3").arg(cleanBaseName, timestampText, cleanExtension);
}

} // namespace TherionStudio
