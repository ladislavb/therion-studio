#include "TherionRunnerLifecyclePresenter.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace TherionStudio
{
namespace
{
bool containsTherionOutputWriteError(const QString &standardErrorText)
{
    static const QRegularExpression outputWriteErrorPattern(
        QStringLiteral("\\bwarning\\s+--\\s+error\\s+writing\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return outputWriteErrorPattern.match(standardErrorText).hasMatch();
}
}

TherionRunnerLifecyclePresenter::StopPresentation
TherionRunnerLifecyclePresenter::presentStopRequest(bool isRunning)
{
    StopPresentation result;

    if (!isRunning) {
        result.shouldUpdateStatusLabel = true;
        result.statusLabelMessage = QCoreApplication::translate("MainWindow", "Therion is not running.");
        return result;
    }

    result.shouldStopProcess = true;
    result.shouldUpdateStatusLabel = true;
    result.statusLabelMessage = QCoreApplication::translate("MainWindow", "Stopping Therion...");
    return result;
}

TherionRunnerLifecyclePresenter::EventPresentation
TherionRunnerLifecyclePresenter::presentFinished(int exitCode,
                                                 QProcess::ExitStatus exitStatus,
                                                 const QString &standardErrorText)
{
    EventPresentation result;
    if (exitStatus != QProcess::NormalExit) {
        result.statusText = QCoreApplication::translate("MainWindow", "Therion crashed while running.");
        return result;
    }

    if (containsTherionOutputWriteError(standardErrorText)) {
        result.statusText = QCoreApplication::translate(
                                "MainWindow",
                                "Therion reported an output writing error despite exit code %1.")
                                .arg(exitCode);
        return result;
    }

    result.succeeded = exitCode == 0;
    result.statusText = QCoreApplication::translate("MainWindow", "Therion finished with exit code %1.").arg(exitCode);
    return result;
}

TherionRunnerLifecyclePresenter::EventPresentation
TherionRunnerLifecyclePresenter::presentError(const QString &errorText)
{
    EventPresentation result;
    result.statusText = QCoreApplication::translate("MainWindow", "Therion runner error: %1").arg(errorText);
    return result;
}
}
