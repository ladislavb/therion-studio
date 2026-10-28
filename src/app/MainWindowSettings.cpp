#include "MainWindow.h"
#include "MainWindowSettingsDialog.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "../core/ISessionStore.h"
#include "../platform/ApplicationLanguageOverride.h"

#include <QDateTime>

void MainWindow::showSettingsDialog()
{
    if (sessionStore_ == nullptr) {
        return;
    }

    TherionStudio::MainWindowSettingsDialog::Settings initialSettings;
    const QString nativeApplicationLanguage =
        TherionStudio::Platform::applicationLanguageOverride();
    initialSettings.applicationLanguage = nativeApplicationLanguage != QStringLiteral("system")
        ? nativeApplicationLanguage
        : sessionStore_->applicationLanguage();
    initialSettings.therionExecutablePath = sessionStore_->therionExecutablePath();
    initialSettings.defaultTextEditorMode = sessionStore_->defaultTextEditorMode();
    initialSettings.mapPointerCoordinatesInStatusBarEnabled =
        sessionStore_->therionMapPointerCoordinatesInStatusBarEnabled();
    initialSettings.mapXviStationNamesInStatusBarEnabled =
        sessionStore_->therionMapXviStationNamesInStatusBarEnabled();
    initialSettings.automaticProjectValidationEnabled =
        sessionStore_->automaticProjectValidationEnabled();
    initialSettings.troubleshootingLogsEnabled =
        sessionStore_->troubleshootingLogsEnabledUntilUtc().isValid()
        && sessionStore_->troubleshootingLogsEnabledUntilUtc() > QDateTime::currentDateTimeUtc();

    TherionStudio::MainWindowSettingsDialog dialog(initialSettings, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const TherionStudio::MainWindowSettingsDialog::Settings updatedSettings = dialog.settings();
    const QString updatedApplicationLanguage =
        TherionStudio::Platform::normalizeApplicationLanguageSetting(
            updatedSettings.applicationLanguage);
    const bool languageChanged =
        updatedApplicationLanguage
        != TherionStudio::Platform::normalizeApplicationLanguageSetting(
            initialSettings.applicationLanguage);
    const bool troubleshootingLogsChanged =
        updatedSettings.troubleshootingLogsEnabled != initialSettings.troubleshootingLogsEnabled;

    sessionStore_->setApplicationLanguage(updatedApplicationLanguage);
    TherionStudio::Platform::setApplicationLanguageOverride(updatedApplicationLanguage);
    sessionStore_->setTherionExecutablePath(updatedSettings.therionExecutablePath);
    sessionStore_->setDefaultTextEditorMode(updatedSettings.defaultTextEditorMode);
    sessionStore_->setTherionMapPointerCoordinatesInStatusBarEnabled(
        updatedSettings.mapPointerCoordinatesInStatusBarEnabled);
    sessionStore_->setTherionMapXviStationNamesInStatusBarEnabled(
        updatedSettings.mapXviStationNamesInStatusBarEnabled);
    forEachOpenMapEditorTab([](TherionStudio::MapEditorTab *mapTab) { mapTab->refreshPointerStatusOptions(); });
    refreshDocumentStatusWidgets();
    sessionStore_->setAutomaticProjectValidationEnabled(
        updatedSettings.automaticProjectValidationEnabled);
    sessionStore_->setTroubleshootingLogsEnabledUntilUtc(
        updatedSettings.troubleshootingLogsEnabled
            ? QDateTime::currentDateTimeUtc().addSecs(24 * 60 * 60)
            : QDateTime());
    refreshTherionConfigDisplay();
    updateProjectValidationStatusMessage();

    if (languageChanged) {
        offerApplicationRestart(tr("Language changes will take effect after restarting Therion Studio."));
    }
    if (troubleshootingLogsChanged) {
        offerApplicationRestart(tr("Troubleshooting log changes will take effect after restarting Therion Studio."));
    }
}
