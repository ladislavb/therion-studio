#include "ProjectVersioningToolbar.h"

#include "LucideIconFactory.h"
#include "ProjectVersioningNavigationService.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

namespace TherionStudio
{
ProjectVersioningToolbar::ProjectVersioningToolbar(ProjectVersioningNavigationService &service, QWidget *parent)
    : QWidget(parent)
    , service_(service)
{
    setObjectName(QStringLiteral("projectBackupPanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(6);
    auto *title = new QLabel(tr("Backups and restore"), this);
    auto font = title->font();
    font.setBold(true);
    title->setFont(font);
    layout->addWidget(title);
    auto *scope = new QLabel(tr("Backups apply to the whole project, not only this file."), this);
    scope->setWordWrap(true);
    scope->setTextFormat(Qt::PlainText);
    layout->addWidget(scope);
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("projectBackupStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status_);
    auto *navigation = new QHBoxLayout;
    navigation->setSpacing(4);
    layout->addLayout(navigation);
    const auto button = [&](const QString &name, const QString &text, const QString &icon, const QString &tooltip) {
        auto *tool = new QToolButton(this);
        tool->setObjectName(name);
        tool->setText(text);
        tool->setToolTip(tooltip);
        tool->setAccessibleName(tooltip);
        tool->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        tool->setIconSize(QSize(16, 16));
        tool->setIcon(themedLucideIcon(icon, tool->palette(), 16, tool->devicePixelRatioF()));
        tool->setProperty("lucideIconName", icon);
        tool->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        tool->setMinimumHeight(28);
        return tool;
    };
    older_ = button(QStringLiteral("olderProjectBackup"), tr("Older"), QStringLiteral("rotate-ccw"),
                    tr("Restore the previous project backup (project undo)"));
    newer_ = button(QStringLiteral("newerProjectBackup"), tr("Newer"), QStringLiteral("rotate-cw"),
                    tr("Restore the next project backup (project redo)"));
    navigation->addWidget(older_);
    navigation->addWidget(newer_);
    choose_ = button(QStringLiteral("chooseProjectBackup"), tr("Choose backup"), QStringLiteral("folder-tree"),
                     tr("Choose a project backup by date and time"));
    layout->addWidget(choose_);
    auto *settings = button(QStringLiteral("projectBackupSettings"), tr("Backup settings..."), QStringLiteral("cog"),
                            tr("Backup settings..."));
    auto *settingsRow = new QHBoxLayout;
    auto *github = button(QStringLiteral("githubProjectBackup"), tr("GitHub"), QStringLiteral("github-backup"),
                          tr("Upload a project backup to GitHub"));
    github->setToolButtonStyle(Qt::ToolButtonIconOnly);
    github->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    github->setMinimumWidth(32);
    settingsRow->addWidget(settings, 1);
    settingsRow->addWidget(github);
    layout->addLayout(settingsRow);
    connect(github, &QToolButton::clicked, this, &ProjectVersioningToolbar::githubBackupRequested);
    connect(settings, &QToolButton::clicked, this, &ProjectVersioningToolbar::settingsRequested);
    menu_ = new QMenu(choose_);
    choose_->setMenu(menu_);
    choose_->setPopupMode(QToolButton::InstantPopup);
    connect(menu_, &QMenu::aboutToShow, this, [this] {
        rebuildMenu();
        service_.refresh();
    });
    connect(older_, &QToolButton::clicked, this, [this] {
        for (int i = 0; i + 1 < checkpoints_.size(); ++i) {
            if (checkpoints_.at(i).id == currentId_) {
                emit restoreRequested(checkpoints_.at(i + 1).id);
                return;
            }
        }
    });
    connect(newer_, &QToolButton::clicked, this, [this] {
        for (int i = 1; i < checkpoints_.size(); ++i) {
            if (checkpoints_.at(i).id == currentId_) {
                emit restoreRequested(checkpoints_.at(i - 1).id);
                return;
            }
        }
    });
    connect(&service_, &ProjectVersioningNavigationService::historyReady, this,
            [this](const ProjectVersioningStore::HistoryResult &result, bool preserve) {
        allCheckpoints_ = result.checkpoints;
        checkpoints_.clear();
        for (const auto &checkpoint : result.checkpoints) {
            if (!checkpoint.label.startsWith(QStringLiteral("Restored checkpoint "))
                || checkpoint.id == result.currentCheckpointId || (preserve && checkpoint.id == currentId_)) {
                checkpoints_.append(checkpoint);
            }
        }
        error_ = result.error;
        bool found = false;
        for (const auto &checkpoint : allCheckpoints_) {
            found |= checkpoint.id == currentId_;
        }
        if (!preserve || !found) {
            currentId_ = result.currentCheckpointId;
            if (currentId_.isEmpty() && !checkpoints_.isEmpty()) {
                currentId_ = checkpoints_.first().id;
            }
        }
        updateControls();
        if (menu_->isVisible()) {
            rebuildMenu();
        }
    });
    connect(&service_, &ProjectVersioningNavigationService::busyChanged, this, [this] { updateControls(); });
    updateControls();
}

QString ProjectVersioningToolbar::checkpointDescription(const QString &id) const
{
    for (const auto &checkpoint : allCheckpoints_) {
        if (checkpoint.id != id) {
            continue;
        }
        QString label = checkpoint.label;
        if (label == QStringLiteral("Manual checkpoint")) {
            label = tr("Manual checkpoint");
        } else if (label == QStringLiteral("Automatic checkpoint")) {
            label = tr("Automatic checkpoint");
        } else if (label == QStringLiteral("Safety checkpoint before restore")) {
            label = tr("Safety checkpoint before restore");
        } else if (label.startsWith(QStringLiteral("Restored checkpoint "))) {
            label = tr("Restored checkpoint %1").arg(label.mid(20, 12));
        }
        return tr("%1 - %2 [%3]").arg(QLocale().toString(checkpoint.createdAt.toLocalTime(), QLocale::ShortFormat),
                                    label, id.left(12));
    }
    return id.left(12);
}

void ProjectVersioningToolbar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        for (auto *button : findChildren<QToolButton *>()) {
            button->setIcon(themedLucideIcon(button->property("lucideIconName").toString(),
                                           button->palette(), 16, button->devicePixelRatioF()));
        }
    }
}

void ProjectVersioningToolbar::restored(const QString &checkpointId)
{
    currentId_ = checkpointId;
    service_.refresh(true);
}

void ProjectVersioningToolbar::updateControls()
{
    const bool ready = service_.isEnabled() && !service_.isBusy() && error_.isEmpty();
    int index = -1;
    for (int i = 0; i < checkpoints_.size(); ++i) {
        if (checkpoints_.at(i).id == currentId_) {
            index = i;
            break;
        }
    }
    older_->setEnabled(ready && index >= 0 && index + 1 < checkpoints_.size());
    newer_->setEnabled(ready && index > 0);
    choose_->setEnabled(!service_.isBusy());
    status_->setText(!error_.isEmpty() ? error_
        : !service_.isEnabled() ? tr("Local project history is disabled.")
        : service_.isBusy() ? tr("Loading project backups...")
        : currentId_.isEmpty() ? tr("No backup selected")
        : tr("Current: %1").arg(checkpointDescription(currentId_)));
    setToolTip(!error_.isEmpty() ? error_
        : !service_.isEnabled() ? tr("Enable local project history in File > Settings > Backups and versioning.")
                               : checkpointDescription(currentId_));
}

void ProjectVersioningToolbar::rebuildMenu()
{
    menu_->clear();
    auto *current = menu_->addAction(tr("Current: %1").arg(
        currentId_.isEmpty() ? tr("No backup selected") : checkpointDescription(currentId_)));
    current->setEnabled(false);
    if (!error_.isEmpty()) {
        auto *error = menu_->addAction(error_);
        error->setEnabled(false);
    }
    menu_->addSeparator();
    for (const auto &checkpoint : checkpoints_) {
        auto *action = menu_->addAction(checkpointDescription(checkpoint.id));
        action->setCheckable(true);
        action->setChecked(checkpoint.id == currentId_);
        action->setEnabled(service_.isEnabled() && !service_.isBusy());
        connect(action, &QAction::triggered, this, [this, id = checkpoint.id] { emit restoreRequested(id); });
    }
    menu_->addSeparator();
    auto *settings = menu_->addAction(tr("Backup settings..."));
    connect(settings, &QAction::triggered, this, &ProjectVersioningToolbar::settingsRequested);
}
}
