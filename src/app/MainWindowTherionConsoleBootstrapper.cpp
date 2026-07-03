#include "MainWindowTherionConsoleBootstrapper.h"

#include "MainWindowTherionConsoleController.h"

#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

namespace TherionStudio
{
void MainWindowTherionConsoleBootstrapper::bootstrap(const BootstrapInput &input)
{
    if (input.consoleController != nullptr) {
        input.consoleController->bindWidgets(input.consoleView,
                                             input.therionRunButton,
                                             input.therionStopButton,
                                             input.therionWorkingDirectoryEdit,
                                             input.therionArgumentsEdit);
    }

    if (input.consoleSidebarPageLayout != nullptr && input.rootWidget != nullptr) {
        input.consoleSidebarPageLayout->addWidget(input.rootWidget, 1);
    }

    if (input.refreshTherionConfigDisplay) {
        input.refreshTherionConfigDisplay();
    }

    if (input.updateTherionRunnerState) {
        input.updateTherionRunnerState();
    }
}
}
