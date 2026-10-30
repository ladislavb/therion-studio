#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
class ProjectVersioningStore final
{
public:
    struct Checkpoint
    {
        QString id;
        QString label;
        QDateTime createdAt;
    };

    struct CheckpointResult
    {
        bool succeeded = false;
        QString checkpointId;
        QString error;
    };

    struct HistoryResult
    {
        bool succeeded = false;
        QVector<Checkpoint> checkpoints;
        QString error;
        QString currentCheckpointId;
    };

    struct RestoreResult
    {
        bool succeeded = false;
        QString checkpointId;
        QString safetyCheckpointId;
        QStringList addedFiles;
        QStringList changedFiles;
        QStringList removedFiles;
        QString error;
    };

    ProjectVersioningStore(QString projectRootPath,
                           QString historyDirectory,
                           QString gitExecutable = QStringLiteral("git"));

    CheckpointResult createCheckpoint(const QString &label);
    HistoryResult history() const;
    CheckpointResult removeHistory();
    RestoreResult restoreCheckpoint(const QString &checkpointId);

private:
    CheckpointResult createCheckpointLocked(const QString &label);
    QString projectRootPath_;
    QString historyDirectory_;
    QString gitExecutable_;
};
} // namespace TherionStudio
