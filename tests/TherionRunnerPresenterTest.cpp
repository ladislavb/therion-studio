#include "../src/app/TherionRunnerLifecyclePresenter.h"
#include "../src/app/TherionRunnerStartResultPresenter.h"
#include "../src/app/TherionRunnerStartSuccessPresenter.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionRunnerPresenterTest : public QObject
{
    Q_OBJECT

private slots:
    void presentsStartResults();
    void presentsStartSuccess();
    void presentsLifecycleEvents();
};

void TherionRunnerPresenterTest::presentsStartResults()
{
    const TherionRunnerStartResultPresenter::Presentation started =
        TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode::Started,
                                                   QStringLiteral("therion"));
    QVERIFY(!started.isHandled);

    const TherionRunnerStartResultPresenter::Presentation alreadyRunning =
        TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode::AlreadyRunning,
                                                   QStringLiteral("therion"));
    QVERIFY(alreadyRunning.isHandled);
    QVERIFY(alreadyRunning.showStatusBarMessage);
    QVERIFY(alreadyRunning.updateStatusLabel);
    QCOMPARE(alreadyRunning.statusBarTimeoutMs, 3000);

    const TherionRunnerStartResultPresenter::Presentation missingWorkingDirectory =
        TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode::MissingWorkingDirectory,
                                                   QStringLiteral("therion"));
    QVERIFY(missingWorkingDirectory.showWarningDialog);
    QCOMPARE(missingWorkingDirectory.warningDialogMessage,
             QStringLiteral("Open a project or set a working directory before running Therion."));

    const TherionRunnerStartResultPresenter::Presentation missingExecutable =
        TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode::ExecutableNotFound,
                                                   QStringLiteral("/missing/therion"));
    QVERIFY(missingExecutable.showWarningDialog);
    QVERIFY(missingExecutable.updateStatusLabel);
    QVERIFY(missingExecutable.warningDialogMessage.contains(QStringLiteral("/missing/therion")));
}

void TherionRunnerPresenterTest::presentsStartSuccess()
{
    TherionRunnerService::StartResult startResult;
    startResult.code = TherionRunnerService::StartCode::Started;
    startResult.resolvedExecutablePath = QStringLiteral("/opt/therion/bin/therion");
    startResult.usedPlatformFallback = true;

    const TherionRunnerStartSuccessPresenter::Presentation fallbackPresentation =
        TherionRunnerStartSuccessPresenter::present(startResult,
                                                    QStringLiteral("-q survey.th"),
                                                    QStringLiteral("/project"));
    QCOMPARE(fallbackPresentation.consoleMessage,
             QStringLiteral("Running /opt/therion/bin/therion -q survey.th in /project"));
    QCOMPARE(fallbackPresentation.statusLabelMessage, QStringLiteral("Starting Therion..."));

    const TherionRunnerStartSuccessPresenter::Presentation explicitPresentation =
        TherionRunnerStartSuccessPresenter::present(startResult,
                                                    QString(),
                                                    QStringLiteral("/project"));
    QCOMPARE(explicitPresentation.statusLabelMessage, QStringLiteral("Starting Therion..."));
}

void TherionRunnerPresenterTest::presentsLifecycleEvents()
{
    const TherionRunnerLifecyclePresenter::StopPresentation notRunning =
        TherionRunnerLifecyclePresenter::presentStopRequest(false);
    QVERIFY(!notRunning.shouldStopProcess);
    QVERIFY(notRunning.shouldUpdateStatusLabel);
    QCOMPARE(notRunning.statusLabelMessage, QStringLiteral("Therion is not running."));

    const TherionRunnerLifecyclePresenter::StopPresentation running =
        TherionRunnerLifecyclePresenter::presentStopRequest(true);
    QVERIFY(running.shouldStopProcess);
    QVERIFY(running.shouldUpdateStatusLabel);
    QCOMPARE(running.statusLabelMessage, QStringLiteral("Stopping Therion..."));

    const TherionRunnerLifecyclePresenter::EventPresentation normalFinish =
        TherionRunnerLifecyclePresenter::presentFinished(7, QProcess::NormalExit);
    QVERIFY(!normalFinish.succeeded);
    QCOMPARE(normalFinish.statusText, QStringLiteral("Therion finished with exit code 7."));

    const TherionRunnerLifecyclePresenter::EventPresentation successfulFinish =
        TherionRunnerLifecyclePresenter::presentFinished(0, QProcess::NormalExit);
    QVERIFY(successfulFinish.succeeded);
    QCOMPARE(successfulFinish.statusText, QStringLiteral("Therion finished with exit code 0."));

    const TherionRunnerLifecyclePresenter::EventPresentation outputWriteError =
        TherionRunnerLifecyclePresenter::presentFinished(
            0,
            QProcess::NormalExit,
            QStringLiteral("/opt/homebrew/bin/therion: warning -- error writing /tmp/out.lox\n"));
    QVERIFY(!outputWriteError.succeeded);
    QCOMPARE(outputWriteError.statusText,
             QStringLiteral("Therion reported an output writing error despite exit code 0."));

    const TherionRunnerLifecyclePresenter::EventPresentation crashFinish =
        TherionRunnerLifecyclePresenter::presentFinished(1, QProcess::CrashExit);
    QVERIFY(!crashFinish.succeeded);
    QCOMPARE(crashFinish.statusText, QStringLiteral("Therion crashed while running."));

    const TherionRunnerLifecyclePresenter::EventPresentation error =
        TherionRunnerLifecyclePresenter::presentError(QStringLiteral("permission denied"));
    QCOMPARE(error.statusText, QStringLiteral("Therion runner error: permission denied"));
}
}

int runTherionRunnerPresenterTest(int argc, char **argv)
{
    TherionRunnerPresenterTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionRunnerPresenterTest.moc"
