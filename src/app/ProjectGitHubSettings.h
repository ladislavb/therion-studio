#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace TherionStudio
{
struct ProjectGitHubSettings
{
    bool succeeded = false;
    bool exists = false;
    QJsonObject values;
    QByteArray contents;
    QString error;

    QString repository() const;
    bool uploadEnabled() const;
    bool promptOnClose() const;
    static ProjectGitHubSettings read(const QString &projectRoot);
};
}
