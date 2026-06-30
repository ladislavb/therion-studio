#include "../src/app/TherionRunnerService.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <iostream>

using namespace TherionStudio;

namespace
{
constexpr const char *kChildModeArgument = "--therion-runner-service-child";
constexpr const char *kWaitForStdinChildModeArgument = "--therion-runner-service-wait-for-stdin-child";

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runChildProcess()
{
    std::cout << "runner-child-stdout\n";
    std::cout.flush();
    std::cerr << "runner-child-stderr\n";
    std::cerr.flush();
    QThread::msleep(250);
    return 0;
}

int runWaitForStdinChildProcess()
{
    std::cout << "Press ENTER to Exit!\n";
    std::cout.flush();
    char ignored = 0;
    while (std::cin.get(ignored)) {
    }
    return 0;
}

struct FinishedResult
{
    bool finished = false;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
};

FinishedResult waitForRunFinished(TherionRunnerService &service)
{
    FinishedResult result;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);

    QObject::connect(&service,
                     &TherionRunnerService::runFinished,
                     &loop,
                     [&](int exitCode, QProcess::ExitStatus exitStatus) {
                         result.finished = true;
                         result.exitCode = exitCode;
                         result.exitStatus = exitStatus;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();
    return result;
}

int runValidationResultTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary working directory creation failed.")) {
        return 1;
    }

    TherionRunnerService service;

    const TherionRunnerService::StartResult missingWorkingDirectory =
        service.start(QStringLiteral("therion"), QString(), {});
    if (!expect(missingWorkingDirectory.code == TherionRunnerService::StartCode::MissingWorkingDirectory,
                "Blank working directory should return MissingWorkingDirectory.")) {
        return 1;
    }

    const TherionRunnerService::StartResult missingDirectory =
        service.start(QStringLiteral("therion"), tempDir.path() + QStringLiteral("/missing"), {});
    if (!expect(missingDirectory.code == TherionRunnerService::StartCode::WorkingDirectoryNotFound,
                "Missing working directory should return WorkingDirectoryNotFound.")) {
        return 1;
    }

    const TherionRunnerService::StartResult missingExecutable =
        service.start(tempDir.path() + QStringLiteral("/missing-therion"), tempDir.path(), {});
    if (!expect(missingExecutable.code == TherionRunnerService::StartCode::ExecutableNotFound,
                "Missing executable should return ExecutableNotFound.")) {
        return 1;
    }

    return 0;
}

int runSuccessfulProcessTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary working directory creation failed.")) {
        return 1;
    }

    TherionRunnerService service;
    QString standardOutput;
    QString standardError;
    bool sawRunningState = false;
    bool sawStoppedState = false;

    QObject::connect(&service, &TherionRunnerService::standardOutputReady, [&](const QString &output) {
        standardOutput += output;
    });
    QObject::connect(&service, &TherionRunnerService::standardErrorReady, [&](const QString &output) {
        standardError += output;
    });
    QObject::connect(&service, &TherionRunnerService::runningStateChanged, [&](bool running) {
        if (running) {
            sawRunningState = true;
        } else {
            sawStoppedState = true;
        }
    });

    const QString executablePath = QCoreApplication::applicationFilePath();
    const TherionRunnerService::StartResult startResult =
        service.start(executablePath, tempDir.path(), {QString::fromLatin1(kChildModeArgument)});
    if (!expect(startResult.code == TherionRunnerService::StartCode::Started,
                "Valid executable and working directory should start the process.")) {
        return 1;
    }
    if (!expect(QFileInfo(startResult.resolvedExecutablePath).absoluteFilePath()
                    == QFileInfo(executablePath).absoluteFilePath(),
                "Started process should report the resolved executable path.")) {
        return 1;
    }

    const TherionRunnerService::StartResult alreadyRunning =
        service.start(executablePath, tempDir.path(), {QString::fromLatin1(kChildModeArgument)});
    if (!expect(alreadyRunning.code == TherionRunnerService::StartCode::AlreadyRunning,
                "Concurrent start attempts should return AlreadyRunning.")) {
        return 1;
    }

    const FinishedResult finished = waitForRunFinished(service);
    if (!expect(finished.finished, "Started process did not finish before timeout.")) {
        service.stop();
        return 1;
    }
    if (!expect(finished.exitCode == 0 && finished.exitStatus == QProcess::NormalExit,
                "Child process should finish normally with exit code 0.")) {
        return 1;
    }
    if (!expect(standardOutput.contains(QStringLiteral("runner-child-stdout")),
                "Child stdout should be forwarded through standardOutputReady.")) {
        return 1;
    }
    if (!expect(standardError.contains(QStringLiteral("runner-child-stderr")),
                "Child stderr should be forwarded through standardErrorReady.")) {
        return 1;
    }
    if (!expect(sawRunningState && sawStoppedState,
                "Service should emit running and stopped state changes.")) {
        return 1;
    }
    if (!expect(!service.isRunning(), "Service should not be running after child completion.")) {
        return 1;
    }

    return 0;
}

int runInteractivePromptDoesNotHangTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary working directory creation failed.")) {
        return 1;
    }

    TherionRunnerService service;
    QString standardOutput;
    QObject::connect(&service, &TherionRunnerService::standardOutputReady, [&](const QString &output) {
        standardOutput += output;
    });

    const QString executablePath = QCoreApplication::applicationFilePath();
    const TherionRunnerService::StartResult startResult =
        service.start(executablePath, tempDir.path(), {QString::fromLatin1(kWaitForStdinChildModeArgument)});
    if (!expect(startResult.code == TherionRunnerService::StartCode::Started,
                "Prompting child process should start.")) {
        return 1;
    }

    const FinishedResult finished = waitForRunFinished(service);
    if (!expect(finished.finished,
                "Runner should close child stdin so interactive Press ENTER prompts do not hang.")) {
        service.stop();
        return 1;
    }
    if (!expect(finished.exitCode == 0 && finished.exitStatus == QProcess::NormalExit,
                "Prompting child process should finish normally after stdin closes.")) {
        return 1;
    }
    if (!expect(standardOutput.contains(QStringLiteral("Press ENTER to Exit!")),
                "Prompting child stdout should be forwarded before process exit.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QString::fromLatin1(kChildModeArgument))) {
        return runChildProcess();
    }
    if (app.arguments().contains(QString::fromLatin1(kWaitForStdinChildModeArgument))) {
        return runWaitForStdinChildProcess();
    }

    if (runValidationResultTest() != 0) {
        return 1;
    }
    if (runSuccessfulProcessTest() != 0) {
        return 1;
    }
    if (runInteractivePromptDoesNotHangTest() != 0) {
        return 1;
    }

    return 0;
}
