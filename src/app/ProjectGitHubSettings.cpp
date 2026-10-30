#include "ProjectGitHubSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

namespace TherionStudio
{
QString ProjectGitHubSettings::repository() const
{
    return values.value(QStringLiteral("owner")).toString() + QLatin1Char('/')
        + values.value(QStringLiteral("repository")).toString();
}

bool ProjectGitHubSettings::uploadEnabled() const
{
    static const QRegularExpression name(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9_][A-Za-z0-9_.-]{0,99}$"));
    return succeeded && exists && values.value(QStringLiteral("uploadEnabled")).toBool()
        && name.match(repository()).hasMatch();
}

bool ProjectGitHubSettings::promptOnClose() const
{
    return uploadEnabled() && values.value(QStringLiteral("promptOnClose")).toBool();
}

ProjectGitHubSettings ProjectGitHubSettings::read(const QString &projectRoot)
{
    ProjectGitHubSettings result;
    const QFileInfo root(projectRoot);
    if (projectRoot.isEmpty() || !root.isDir()) {
        result.error = QStringLiteral("Cannot access the project folder to store GitHub settings.");
        return result;
    }
    const QFileInfo info(QDir(root.canonicalFilePath()).filePath(QStringLiteral(".therion-studio-github.json")));
    if (info.isSymLink() || (info.exists() && !info.isFile())) {
        result.error = QStringLiteral("GitHub project settings must be a regular file, not a symbolic link.");
        return result;
    }
    result.exists = info.exists();
    if (result.exists) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            result.error = QStringLiteral("Could not read GitHub project settings: %1").arg(file.errorString());
            return result;
        }
        result.contents = file.read(16385);
        if (file.error() != QFileDevice::NoError || result.contents.size() > 16384) {
            result.error = QStringLiteral("GitHub project settings are unreadable or too large.");
            return result;
        }
        const auto document = QJsonDocument::fromJson(result.contents);
        result.values = document.object();
        const auto &object = result.values;
        const QStringList allowed{QStringLiteral("version"), QStringLiteral("owner"), QStringLiteral("repository"),
                                  QStringLiteral("uploadEnabled"), QStringLiteral("promptOnClose")};
        bool valid = document.isObject() && object.value(QStringLiteral("version")).toInt() == 1
            && object.value(QStringLiteral("owner")).isString()
            && object.value(QStringLiteral("repository")).isString()
            && object.value(QStringLiteral("owner")).toString().size() <= 39
            && object.value(QStringLiteral("repository")).toString().size() <= 100;
        for (auto it = object.begin(); it != object.end(); ++it) {
            valid &= allowed.contains(it.key());
        }
        for (const QString &key : {QStringLiteral("uploadEnabled"), QStringLiteral("promptOnClose")}) {
            valid &= !object.contains(key) || object.value(key).isBool();
        }
        if (!valid) {
            result.error = QStringLiteral("Invalid or unsupported GitHub project settings. The file was not changed.");
            return result;
        }
    }
    result.succeeded = true;
    return result;
}
}
