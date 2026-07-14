#include "../src/app/MainWindowSessionRestoreUiFlowService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowSessionRestoreUiFlowServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void presentsConsoleLines();
};

void MainWindowSessionRestoreUiFlowServiceTest::presentsConsoleLines()
{
    const QString restored = MainWindowSessionRestoreUiFlowService::restoredProjectRootConsoleLine(
        QStringLiteral("/tmp/project"));
    QCOMPARE(restored, QStringLiteral("Restored project root /tmp/project"));
}
}

int runMainWindowSessionRestoreUiFlowServiceTest(int argc, char **argv)
{
    MainWindowSessionRestoreUiFlowServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowSessionRestoreUiFlowServiceTest.moc"
