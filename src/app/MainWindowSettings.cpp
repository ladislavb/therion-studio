#include "MainWindow.h"
#include "MainWindowSettingsDialog.h"
#include "ProjectVersioningService.h"
#include "ProjectVersioningNavigationService.h"
#include "ProjectVersioningSettings.h"
#include "../core/ISessionStore.h"
#include "../platform/ApplicationLanguageOverride.h"

#include <QStatusBar>
#include <QSettings>
#include <QStandardPaths>

void MainWindow::refreshProjectVersioning()
{
    if (projectRestoreInProgress_) {
        return;
    }
    if (projectVersioningService_ == nullptr) {
        projectVersioningService_ = new TherionStudio::ProjectVersioningService(this);
        connect(projectVersioningService_, &TherionStudio::ProjectVersioningService::checkpointFinished,
                this, [this](const QString &root, bool succeeded, const QString &, const QString &error) {
            if (root != projectRootPath_) {
                return;
            }
            statusBar()->showMessage(succeeded ? tr("Automatic local history check completed.")
                                              : tr("Automatic local history error: %1").arg(error),
                                     succeeded ? 5000 : 30000);
            if (succeeded && projectVersioningNavigation_ != nullptr) {
                projectVersioningNavigation_->refresh(false);
            }
        });
    }
    projectVersioningService_->configure(projectRootPath_);
    if (projectVersioningNavigation_ != nullptr) {
        QSettings settings;
        settings.sync();
        TherionStudio::ProjectVersioningSettings preferences(settings,
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        const auto result = projectRootPath_.isEmpty()
            ? TherionStudio::ProjectVersioningSettings::Result{} : preferences.projectSettings(projectRootPath_);
        projectVersioningNavigation_->configure(projectRootPath_, result.settings.historyDirectory,
                                                result.succeeded && result.settings.enabled);
        if (!projectRootPath_.isEmpty() && !result.succeeded) {
            qWarning("Could not configure backup navigation: %s", qPrintable(result.error));
            statusBar()->showMessage(tr("Automatic local history error: %1").arg(result.error), 30000);
        }
    }
}

void MainWindow::showSettingsDialog()
{
    showSettingsDialogAtPage(false);
}

void MainWindow::showBackupSettingsDialog()
{
    showSettingsDialogAtPage(true);
}

void MainWindow::showSettingsDialogAtPage(bool backups, bool github)
{
    if (sessionStore_ == nullptr) {
        return;
    }

    TherionStudio::MainWindowSettingsDialog::Settings initialSettings;
    initialSettings.projectRootPath = projectRootPath_;
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
    if (github) {
        dialog.showGitHubPage();
    } else if (backups) {
        dialog.showBackupPage();
    }
    connect(&dialog, &QDialog::finished, this, [this] {
        if (projectVersioningNavigation_ != nullptr) {
            projectVersioningNavigation_->refresh(false);
        }
    });
    connect(&dialog, &TherionStudio::MainWindowSettingsDialog::projectVersioningChanged,
            this, &MainWindow::refreshProjectVersioning);
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
