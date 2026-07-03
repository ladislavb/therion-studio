#pragma once

#include <QProcess>
#include <QString>

namespace TherionStudio
{
class TherionRunnerLifecyclePresenter final
{
public:
    struct StopPresentation
    {
        bool shouldStopProcess = false;
        bool shouldUpdateStatusLabel = false;
        QString statusLabelMessage;
    };

    struct EventPresentation
    {
        QString statusText;
        bool succeeded = false;
    };

    static StopPresentation presentStopRequest(bool isRunning);
    static EventPresentation presentFinished(int exitCode,
                                             QProcess::ExitStatus exitStatus,
                                             const QString &standardErrorText = QString());
    static EventPresentation presentError(const QString &errorText);
};
}
