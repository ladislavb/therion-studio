#include "app/MainWindow.h"
#include "core/CommandCatalogStore.h"
#include "core/SessionStore.h"
#include "platform/ApplicationBootstrap.h"
#include "platform/DiagnosticLogging.h"

#include <QApplication>
#include <QDateTime>

#include <memory>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const TherionStudio::ApplicationStartupState startupState = TherionStudio::initializeApplicationBootstrap(application);
    (void)startupState;
    auto sessionStore = std::make_unique<TherionStudio::SessionSettingsStore>();
    const QDateTime troubleshootingLogsEnabledUntilUtc =
        sessionStore->troubleshootingLogsEnabledUntilUtc();
    const bool troubleshootingLogsEnabled = troubleshootingLogsEnabledUntilUtc.isValid()
        && troubleshootingLogsEnabledUntilUtc > QDateTime::currentDateTimeUtc();
    if (troubleshootingLogsEnabledUntilUtc.isValid() && !troubleshootingLogsEnabled) {
        sessionStore->setTroubleshootingLogsEnabledUntilUtc(QDateTime());
    }
    TherionStudio::initializeDiagnosticLogging(troubleshootingLogsEnabled);

    auto *window = new MainWindow(std::move(sessionStore), TherionStudio::CommandCatalogStore());
    window->show();

    return application.exec();
}
