#pragma once

#include <QString>

class QSettings;

namespace TherionStudio
{
class ProjectVersioningSettings final
{
public:
    struct Settings
    {
        bool enabled = false;
        bool automaticEnabled = false;
        int intervalMinutes = 5;
        QString projectId;
        QString historyDirectory;
    };

    struct Result
    {
        bool succeeded = false;
        Settings settings;
        QString error;
    };

    ProjectVersioningSettings(QSettings &settings, QString applicationDataDirectory);

    Result projectSettings(const QString &projectRootPath) const;
    Result setEnabled(const QString &projectRootPath, bool enabled);
    Result setAutomatic(const QString &projectRootPath, bool enabled, int intervalMinutes);

private:
    QSettings &settings_;
    QString applicationDataDirectory_;
};
} // namespace TherionStudio
