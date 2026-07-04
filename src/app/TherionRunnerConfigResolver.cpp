#include "TherionRunnerConfigResolver.h"

#include "../core/TherionFileTypes.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace TherionStudio
{
namespace
{
QString canonicalOrAbsoluteFilePath(const QFileInfo &fileInfo)
{
    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}

QString resolveDefaultConfigPath(const QString &baseDirectory)
{
    if (baseDirectory.isEmpty()) {
        return QString();
    }

    const QDir directory(baseDirectory);
    if (!directory.exists()) {
        return QString();
    }

    QFileInfoList configInfos = directory.entryInfoList(therionConfigNameFilters(),
                                                        QDir::Files,
                                                        QDir::Name | QDir::IgnoreCase);
    const QString projectName = QFileInfo(directory.absolutePath()).fileName();
    QFileInfoList preferredConfigInfos;
    for (const QFileInfo &configInfo : configInfos) {
        if (isPreferredRootTherionConfigFileName(configInfo.fileName(), projectName)) {
            preferredConfigInfos.append(configInfo);
        }
    }

    if (!preferredConfigInfos.isEmpty()) {
        std::sort(preferredConfigInfos.begin(),
                  preferredConfigInfos.end(),
                  [projectName](const QFileInfo &left, const QFileInfo &right) {
                      return preferredRootTherionConfigFilePriority(left.fileName(), projectName)
                          < preferredRootTherionConfigFilePriority(right.fileName(), projectName);
                  });
        return canonicalOrAbsoluteFilePath(preferredConfigInfos.first());
    }

    return configInfos.size() == 1 ? canonicalOrAbsoluteFilePath(configInfos.first()) : QString();
}
}

QString TherionRunnerConfigResolver::resolveWorkingDirectory(const QString &typedWorkingDirectory,
                                                             const QString &projectRootPath)
{
    if (!typedWorkingDirectory.trimmed().isEmpty()) {
        return QDir(typedWorkingDirectory).absolutePath();
    }

    if (!projectRootPath.trimmed().isEmpty()) {
        return QDir(projectRootPath).absolutePath();
    }

    return QString();
}

bool TherionRunnerConfigResolver::hasExplicitConfigArgument(const QStringList &arguments)
{
    for (const QString &rawToken : arguments) {
        const QString token = rawToken.trimmed();
        if (token == QStringLiteral("-c") || token == QStringLiteral("--config")) {
            return true;
        }
        if (token.startsWith(QStringLiteral("--config=")) || token.startsWith(QStringLiteral("-c="))) {
            return true;
        }
    }

    return false;
}

QString TherionRunnerConfigResolver::resolveConfigPath(const QStringList &arguments,
                                                       const QString &workingDirectory,
                                                       const QString &projectRootPath,
                                                       const QString &activeDocumentPath)
{
    for (int index = 0; index < arguments.size(); ++index) {
        const QString token = arguments.at(index).trimmed();
        if (token == QStringLiteral("-c") || token == QStringLiteral("--config")) {
            if (index + 1 < arguments.size()) {
                const QString resolvedPath = resolveCandidatePath(arguments.at(index + 1),
                                                                  workingDirectory,
                                                                  projectRootPath);
                if (!resolvedPath.isEmpty()) {
                    return resolvedPath;
                }
            }
            continue;
        }

        if (token.startsWith(QStringLiteral("--config="))) {
            const QString resolvedPath = resolveCandidatePath(token.mid(QStringLiteral("--config=").size()),
                                                              workingDirectory,
                                                              projectRootPath);
            if (!resolvedPath.isEmpty()) {
                return resolvedPath;
            }
            continue;
        }

        if (token.startsWith(QStringLiteral("-c="))) {
            const QString resolvedPath = resolveCandidatePath(token.mid(QStringLiteral("-c=").size()),
                                                              workingDirectory,
                                                              projectRootPath);
            if (!resolvedPath.isEmpty()) {
                return resolvedPath;
            }
        }
    }

    if (isTherionConfigFilePath(activeDocumentPath)) {
        QFileInfo activeDocumentInfo(activeDocumentPath);
        if (activeDocumentInfo.exists()) {
            return canonicalOrAbsoluteFilePath(activeDocumentInfo);
        }
    }

    const QString baseDirectory = workingDirectory.isEmpty() ? projectRootPath : workingDirectory;
    return resolveDefaultConfigPath(baseDirectory);
}

QString TherionRunnerConfigResolver::resolveCandidatePath(const QString &candidate,
                                                          const QString &workingDirectory,
                                                          const QString &projectRootPath)
{
    const QString trimmedCandidate = candidate.trimmed();
    if (trimmedCandidate.isEmpty()) {
        return QString();
    }

    QFileInfo candidateInfo(trimmedCandidate);
    QString absolutePath;
    if (candidateInfo.isAbsolute()) {
        absolutePath = candidateInfo.absoluteFilePath();
    } else {
        const QString baseDirectory = workingDirectory.isEmpty() ? projectRootPath : workingDirectory;
        if (baseDirectory.isEmpty()) {
            return QString();
        }
        absolutePath = QDir(baseDirectory).absoluteFilePath(trimmedCandidate);
    }

    QFileInfo resolvedInfo(absolutePath);
    if (!resolvedInfo.exists()) {
        return QString();
    }

    const QString canonicalPath = resolvedInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? resolvedInfo.absoluteFilePath() : canonicalPath;
}
}
