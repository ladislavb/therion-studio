#pragma once

#include "ProjectVersioningStore.h"

#include <QObject>

template<typename T> class QFutureWatcher;

namespace TherionStudio
{
class ProjectVersioningNavigationService final : public QObject
{
    Q_OBJECT
public:
    explicit ProjectVersioningNavigationService(QObject *parent = nullptr);
    void configure(const QString &root, const QString &historyDirectory, bool enabled);
    void refresh(bool preserveSelection = true);
    void restore(const QString &checkpointId);
    bool isBusy() const;
    bool isEnabled() const;

signals:
    void historyReady(const TherionStudio::ProjectVersioningStore::HistoryResult &result, bool preserveSelection);
    void restoreFinished(const TherionStudio::ProjectVersioningStore::RestoreResult &result);
    void busyChanged(bool busy);

private:
    QString root_;
    QString historyDirectory_;
    QString requestedRoot_;
    bool enabled_ = false;
    bool preserveSelection_ = true;
    bool refreshPending_ = false;
    QFutureWatcher<ProjectVersioningStore::HistoryResult> *history_;
    QFutureWatcher<ProjectVersioningStore::RestoreResult> *restore_;
};
}
