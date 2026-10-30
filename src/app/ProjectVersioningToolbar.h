#pragma once

#include "ProjectVersioningStore.h"

#include <QWidget>

class QMenu;
class QToolButton;
class QLabel;

namespace TherionStudio
{
class ProjectVersioningNavigationService;

class ProjectVersioningToolbar final : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectVersioningToolbar(ProjectVersioningNavigationService &service, QWidget *parent = nullptr);
    void restored(const QString &checkpointId);
    QString checkpointDescription(const QString &id) const;

signals:
    void restoreRequested(const QString &checkpointId);
    void settingsRequested();
    void githubBackupRequested();

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateControls();
    void rebuildMenu();
    ProjectVersioningNavigationService &service_;
    QVector<ProjectVersioningStore::Checkpoint> allCheckpoints_;
    QVector<ProjectVersioningStore::Checkpoint> checkpoints_;
    QString currentId_;
    QString error_;
    QToolButton *older_;
    QToolButton *newer_;
    QToolButton *choose_;
    QMenu *menu_;
    QLabel *status_;
};
}
