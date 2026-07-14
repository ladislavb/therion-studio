#include "../src/app/MainWindowSessionProjectService.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowSessionProjectServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void decidesMissingProjectRestore();
    void decidesRestorableProject();
};

void MainWindowSessionProjectServiceTest::decidesMissingProjectRestore()
{
    const auto emptyDecision = MainWindowSessionProjectService::decideProjectRestore(QString());
    QCOMPARE(emptyDecision.status, MainWindowSessionProjectService::ProjectRestoreStatus::NotRestored);

    const auto missingDecision = MainWindowSessionProjectService::decideProjectRestore(QStringLiteral("/definitely/missing/path"));
    QCOMPARE(missingDecision.status, MainWindowSessionProjectService::ProjectRestoreStatus::NotRestored);
}

void MainWindowSessionProjectServiceTest::decidesRestorableProject()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    const auto decision = MainWindowSessionProjectService::decideProjectRestore(tempDir.path());
    QCOMPARE(decision.status, MainWindowSessionProjectService::ProjectRestoreStatus::Restored);
    QCOMPARE(decision.projectPath, tempDir.path());
}

}

int runMainWindowSessionProjectServiceTest(int argc, char **argv)
{
    MainWindowSessionProjectServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowSessionProjectServiceTest.moc"
