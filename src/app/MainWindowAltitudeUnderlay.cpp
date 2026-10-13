#include "MainWindow.h"

#include "text_editor/map_editor/MapEditorTab.h"
#include "../core/ISessionStore.h"

#include <QTabWidget>

void MainWindow::initializeAltitudeUnderlay()
{
    projectAltitudeService_ = new TherionStudio::ProjectAltitudeService(this);
    const auto appearance = sessionStore_->therionMapAppearance();
    projectAltitudeService_->setEnabled(sessionStore_->therionMapAltitudeUnderlayEnabled()
        || appearance.xviCenterlineColorMode == TherionStudio::MapAppearanceSettings::XviCenterlineColorMode::ByAltitude);
    connect(projectAltitudeService_, &TherionStudio::ProjectAltitudeService::stationsChanged,
            this, &MainWindow::updateMapAltitudeUnderlays);
    connect(projectOutputsScanner_, &TherionStudio::ProjectOutputsScanner::scanFinished,
            this, [this](const TherionStudio::ProjectOutputsScanner::Result &result) {
        if (projectOutputsScanner_->isLatestRequestResult(result)
            && result.projectRootPath == projectRootPath_) {
            projectAltitudeService_->acceptOutputs(result);
        }
    });
}

void MainWindow::configureMapAltitudeUnderlay(TherionStudio::MapEditorTab *tab)
{
    tab->setMapAppearance(sessionStore_->therionMapAppearance());
    tab->setAltitudeUnderlayData(sessionStore_->therionMapAltitudeUnderlayEnabled(),
        altitudeProjectIndex_.projectRootPath == projectRootPath_
            ? altitudeProjectIndex_ : TherionStudio::ProjectIndexSnapshot{},
        projectAltitudeService_->projectRootPath() == projectRootPath_
            ? projectAltitudeService_->stations() : TherionStudio::ScrapAltitudeStations{});
}

void MainWindow::updateMapAppearance()
{
    const auto appearance = sessionStore_->therionMapAppearance();
    auto tabs = detachedMapEditorTabs();
    for (int i = 0; i < editorTabs_->count(); ++i) {
        if (auto *tab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(i))) {
            tabs.append(tab);
        }
    }
    for (auto *tab : tabs) {
        tab->setMapAppearance(appearance);
    }
}

void MainWindow::updateMapAltitudeUnderlays()
{
    const bool enabled = sessionStore_->therionMapAltitudeUnderlayEnabled();
    const auto appearance = sessionStore_->therionMapAppearance();
    projectAltitudeService_->setEnabled(enabled
        || appearance.xviCenterlineColorMode == TherionStudio::MapAppearanceSettings::XviCenterlineColorMode::ByAltitude);
    const auto index = altitudeProjectIndex_.projectRootPath == projectRootPath_
        ? altitudeProjectIndex_ : TherionStudio::ProjectIndexSnapshot{};
    const auto stations = projectAltitudeService_->projectRootPath() == projectRootPath_
        ? projectAltitudeService_->stations() : TherionStudio::ScrapAltitudeStations{};
    auto tabs = detachedMapEditorTabs();
    for (int i = 0; i < editorTabs_->count(); ++i) {
        if (auto *tab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(i))) {
            tabs.append(tab);
        }
    }
    for (auto *tab : tabs) {
        tab->setAltitudeUnderlayData(enabled, index, stations);
    }
}
