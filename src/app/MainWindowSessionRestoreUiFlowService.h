#pragma once

#include "MainWindowSessionProjectService.h"

#include <QString>

namespace TherionStudio
{
class MainWindowSessionRestoreUiFlowService final
{
public:
    static QString restoredProjectRootConsoleLine(const QString &projectPath);
};
}
