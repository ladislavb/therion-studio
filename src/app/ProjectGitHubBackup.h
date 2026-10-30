#pragma once

#include <QString>

namespace TherionStudio
{
// One-way backup of saved, included project files; never operates on the project's .git.
class ProjectGitHubBackup final
{
public:
    struct Result
    {
        bool succeeded = false;
        bool changed = false;
        QString commitId;
        QString error;
    };

    ProjectGitHubBackup(QString root, QString stateDirectory,
                        QString ghExecutable = QStringLiteral("gh"),
                        QString gitExecutable = QStringLiteral("git"));

    // Local snapshot comparison only: does not run gh or contact a remote.
    Result check(const QString &repositoryFullName) const;
    Result upload(const QString &repositoryFullName, const QString &commitMessage) const;

private:
    Result perform(const QString &repositoryFullName, const QString *commitMessage) const;
    QString root_;
    QString stateDirectory_;
    QString ghExecutable_;
    QString gitExecutable_;
};
} // namespace TherionStudio
