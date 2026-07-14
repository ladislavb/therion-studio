#include "MainWindowSessionRestoreUiFlowService.h"

#include <QCoreApplication>

namespace TherionStudio
{
QString MainWindowSessionRestoreUiFlowService::restoredProjectRootConsoleLine(const QString &projectPath)
{
    return QCoreApplication::translate("MainWindow", "Restored project root %1").arg(projectPath);
}
}
