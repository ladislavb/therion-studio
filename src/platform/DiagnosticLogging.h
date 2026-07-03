#pragma once

#include <QString>

namespace TherionStudio
{

QString diagnosticLogDirectoryPath();
QString diagnosticLogFilePath();
bool diagnosticLoggingEnabled();
bool clearDiagnosticLogs(QString *errorMessage = nullptr);
void initializeDiagnosticLogging(bool enableFromPreference = false);

} // namespace TherionStudio
