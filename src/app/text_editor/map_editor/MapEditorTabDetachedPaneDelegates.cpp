#include "MapEditorTab.h"

#include "../DocumentFileInspector.h"
#include "MapEditorDetachedPaneWindow.h"

#include <QFrame>
#include <QGraphicsView>
#include <QMainWindow>
#include <QSplitter>

#include "../TextEditorTab.h"

namespace TherionStudio
{
void MapEditorTab::refreshStatus()
{
    if (mapFileInspector_ != nullptr) {
        mapFileInspector_->refresh();
    }
    if (detachedPaneState_.window_ != nullptr) {
        detachedPaneState_.window_->setWindowTitle(tr("%1 — Map").arg(displayName()));
        detachedPaneState_.window_->setWindowFilePath(filePath());
        if (auto *window = dynamic_cast<MapEditorDetachedPaneWindow *>(detachedPaneState_.window_.data())) {
            window->setMapStatus(zoomPercent(), isInsertModeActive(), statusModeText());
        }
    }
}

QString MapEditorTab::displayPath() const
{
    return textEditor_->filePath();
}

void MapEditorTab::toggleMapPaneWindow()
{
    if (detachedPaneState_.detached_) {
        if (detachedPaneState_.window_ != nullptr) {
            detachedPaneState_.window_->close();
            return;
        }

        reattachMapPaneFromWindow();
        return;
    }

    detachMapPaneToWindow();
}

void MapEditorTab::detachMapPaneToWindow()
{
    if (detachedPaneState_.detached_ || mapPaneContainer_ == nullptr || splitter_ == nullptr) {
        return;
    }

    mapPaneContainer_->setParent(nullptr);

    auto *window = new MapEditorDetachedPaneWindow(this, this);
    window->setWindowTitle(tr("%1 — Map").arg(displayName()));
    window->setWindowFilePath(filePath());
    window->setWindowIcon(detachedPaneState_.windowIcon_);
    window->setMapPaneWidget(mapPaneContainer_);
    window->setCloseCallback([this]() { reattachMapPaneFromWindow(); });

    detachedPaneState_.window_ = window;
    detachedPaneState_.detached_ = true;
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->hide();
    }
    emit mapPaneDetachStateChanged(true);
    updateWorkspaceVisibility();

    const QSize mapSize = mapView_ != nullptr ? mapView_->size() : QSize();
    window->resize(mapSize.isValid() ? mapSize.expandedTo(QSize(900, 700)) : QSize(1200, 800));
    window->show();
    window->raise();
    window->activateWindow();
}

void MapEditorTab::reattachMapPaneFromWindow()
{
    if (!detachedPaneState_.detached_ || mapPaneContainer_ == nullptr || splitter_ == nullptr || detachedPaneState_.reattaching_) {
        return;
    }

    detachedPaneState_.reattaching_ = true;

    mapPaneContainer_->setParent(splitter_);
    splitter_->insertWidget(0, mapPaneContainer_);
    if (mapPaneTopSeparator_ != nullptr) {
        mapPaneTopSeparator_->setVisible(workspaceMode_ == WorkspaceMode::Visual);
    }

    detachedPaneState_.detached_ = false;
    detachedPaneState_.window_ = nullptr;
    emit mapPaneDetachStateChanged(false);
    updateWorkspaceVisibility();
    if (mapView_ != nullptr) {
        mapView_->setFocus(Qt::OtherFocusReason);
    }

    detachedPaneState_.reattaching_ = false;
}

void MapEditorTab::focusDetachedMapPaneWindow()
{
    if (detachedPaneState_.window_ == nullptr) {
        return;
    }

    detachedPaneState_.window_->showNormal();
    detachedPaneState_.window_->raise();
    detachedPaneState_.window_->activateWindow();
}

void MapEditorTab::setMapPaneWindowIcon(const QIcon &icon)
{
    detachedPaneState_.windowIcon_ = icon;
    if (detachedPaneState_.window_ != nullptr) {
        detachedPaneState_.window_->setWindowIcon(icon);
    }
}
}
