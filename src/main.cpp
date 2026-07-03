#include "app/MainWindow.h"
#include "core/CommandCatalogStore.h"
#include "core/SessionStore.h"
#include "platform/ApplicationBootstrap.h"
#include "platform/DiagnosticLogging.h"

#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>

#include <memory>

int main(int argc, char *argv[])
{
    QElapsedTimer startupTimer;
    startupTimer.start();

    QApplication application(argc, argv);
    const qint64 applicationCreatedMs = startupTimer.elapsed();
    const TherionStudio::ApplicationStartupState startupState = TherionStudio::initializeApplicationBootstrap(application);
    (void)startupState;
    const qint64 applicationBootstrapMs = startupTimer.elapsed();
    auto sessionStore = std::make_unique<TherionStudio::SessionSettingsStore>();
    const QDateTime troubleshootingLogsEnabledUntilUtc =
        sessionStore->troubleshootingLogsEnabledUntilUtc();
    const bool troubleshootingLogsEnabled = troubleshootingLogsEnabledUntilUtc.isValid()
        && troubleshootingLogsEnabledUntilUtc > QDateTime::currentDateTimeUtc();
    if (troubleshootingLogsEnabledUntilUtc.isValid() && !troubleshootingLogsEnabled) {
        sessionStore->setTroubleshootingLogsEnabledUntilUtc(QDateTime());
    }
    TherionStudio::initializeDiagnosticLogging(troubleshootingLogsEnabled);
    if (TherionStudio::diagnosticLoggingEnabled()) {
        qInfo("startup-timing phase=diagnostic-logging elapsed_ms=%lld application_ms=%lld bootstrap_ms=%lld",
              static_cast<long long>(startupTimer.elapsed()),
              static_cast<long long>(applicationCreatedMs),
              static_cast<long long>(applicationBootstrapMs));
    }

    auto *window = new MainWindow(std::move(sessionStore), TherionStudio::CommandCatalogStore());
    if (TherionStudio::diagnosticLoggingEnabled()) {
        qInfo("startup-timing phase=main-window-constructed elapsed_ms=%lld",
              static_cast<long long>(startupTimer.elapsed()));
    }
    window->show();
    if (TherionStudio::diagnosticLoggingEnabled()) {
        qInfo("startup-timing phase=main-window-shown elapsed_ms=%lld",
              static_cast<long long>(startupTimer.elapsed()));
        QTimer::singleShot(0, window, [startupTimer]() {
            if (TherionStudio::diagnosticLoggingEnabled()) {
                qInfo("startup-timing phase=event-loop-ready elapsed_ms=%lld",
                      static_cast<long long>(startupTimer.elapsed()));
            }
        });
    }

    return application.exec();
}
