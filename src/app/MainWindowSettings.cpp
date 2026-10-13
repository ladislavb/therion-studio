#include "MainWindow.h"
#include "MainWindowSettingsDialog.h"
#include "../core/ISessionStore.h"
#include "../platform/ApplicationLanguageOverride.h"

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
    initialSettings.mapAltitudeUnderlayEnabled = sessionStore_->therionMapAltitudeUnderlayEnabled();
    initialSettings.mapAppearance = sessionStore_->therionMapAppearance();
    initialSettings.automaticProjectValidationEnabled =
        sessionStore_->automaticProjectValidationEnabled();
    initialSettings.troubleshootingLogsEnabled =
        sessionStore_->troubleshootingLogsEnabledUntilUtc().isValid()
        && sessionStore_->troubleshootingLogsEnabledUntilUtc() > QDateTime::currentDateTimeUtc();

    TherionStudio::MainWindowSettingsDialog dialog(initialSettings, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto updatedSettings = dialog.settings();
    const QString updatedApplicationLanguage =
        TherionStudio::Platform::normalizeApplicationLanguageSetting(updatedSettings.applicationLanguage);
    const bool languageChanged = updatedApplicationLanguage
        != TherionStudio::Platform::normalizeApplicationLanguageSetting(initialSettings.applicationLanguage);
    const bool troubleshootingLogsChanged =
        updatedSettings.troubleshootingLogsEnabled != initialSettings.troubleshootingLogsEnabled;

    sessionStore_->setApplicationLanguage(updatedApplicationLanguage);
    TherionStudio::Platform::setApplicationLanguageOverride(updatedApplicationLanguage);
    sessionStore_->setTherionExecutablePath(updatedSettings.therionExecutablePath);
    sessionStore_->setDefaultTextEditorMode(updatedSettings.defaultTextEditorMode);
    sessionStore_->setTherionMapAltitudeUnderlayEnabled(updatedSettings.mapAltitudeUnderlayEnabled);
    sessionStore_->setTherionMapAppearance(updatedSettings.mapAppearance);
    updateMapAppearance();
    updateMapAltitudeUnderlays();
    sessionStore_->setAutomaticProjectValidationEnabled(updatedSettings.automaticProjectValidationEnabled);
    sessionStore_->setTroubleshootingLogsEnabledUntilUtc(updatedSettings.troubleshootingLogsEnabled
        ? QDateTime::currentDateTimeUtc().addSecs(24 * 60 * 60) : QDateTime());
    refreshTherionConfigDisplay();
    updateProjectValidationStatusMessage();

    if (languageChanged) {
        offerApplicationRestart(tr("Language changes will take effect after restarting Therion Studio."));
    }
    if (troubleshootingLogsChanged) {
        offerApplicationRestart(tr("Troubleshooting log changes will take effect after restarting Therion Studio."));
    }
}
