#pragma once

#include "ProjectVersioningStore.h"

#include <QObject>

#include <memory>

class QSettings;
class QTimer;
template<typename T> class QFutureWatcher;

namespace TherionStudio
{
class ProjectVersioningService final : public QObject
{
    Q_OBJECT
public:
    explicit ProjectVersioningService(QObject *parent = nullptr);
    ProjectVersioningService(QSettings &settings, QString applicationDataDirectory,
                             QObject *parent = nullptr);
    ~ProjectVersioningService() override;

    void configure(const QString &projectRootPath);
    void checkNow();

signals:
    void checkpointFinished(const QString &projectRootPath, bool succeeded,
                            const QString &checkpointId, const QString &error);

private:
    void initialize();

    QString projectRootPath_;
    QString activeRoot_;
    QString applicationDataDirectory_;
    std::unique_ptr<QSettings> ownedSettings_;
    QSettings *settings_ = nullptr;
    QTimer *timer_ = nullptr;
    QFutureWatcher<ProjectVersioningStore::CheckpointResult> *watcher_ = nullptr;
};
} // namespace TherionStudio
