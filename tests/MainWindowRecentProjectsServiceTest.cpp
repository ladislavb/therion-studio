#include "../src/app/MainWindowRecentProjectsService.h"

#include <QStringList>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowRecentProjectsServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void recordsOpenedProject();
    void keepsMaxRecentProjectCount();
    void normalizesRecentProjects();
};
}

void MainWindowRecentProjectsServiceTest::recordsOpenedProject()
{
    const QStringList currentPaths = {
        QStringLiteral("/tmp/alpha"),
        QStringLiteral("/tmp/beta"),
        QStringLiteral("/tmp/gamma")};

    const QStringList updatedPaths =
        MainWindowRecentProjectsService::recordOpenedProject(currentPaths,
                                                             QStringLiteral("/tmp/beta"));

    const QStringList expectedPaths = {
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/beta")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/alpha")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/gamma"))};
    QCOMPARE(updatedPaths, expectedPaths);
}

void MainWindowRecentProjectsServiceTest::keepsMaxRecentProjectCount()
{
    const QStringList currentPaths = {
        QStringLiteral("/tmp/one"),
        QStringLiteral("/tmp/two"),
        QStringLiteral("/tmp/three"),
        QStringLiteral("/tmp/four"),
        QStringLiteral("/tmp/five")};

    const QStringList updatedPaths =
        MainWindowRecentProjectsService::recordOpenedProject(currentPaths,
                                                             QStringLiteral("/tmp/six"));

    const QStringList expectedPaths = {
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/six")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/one")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/two")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/three")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/four"))};
    QCOMPARE(updatedPaths, expectedPaths);
}

void MainWindowRecentProjectsServiceTest::normalizesRecentProjects()
{
    const QStringList normalizedPaths =
        MainWindowRecentProjectsService::normalizedRecentProjectPaths({
            QString(),
            QStringLiteral("  /tmp/project/../project  "),
            QStringLiteral("/tmp/project"),
            QStringLiteral("/tmp/other")});

    const QStringList expectedPaths = {
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/project")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/other"))};
    QCOMPARE(normalizedPaths, expectedPaths);
}

int runMainWindowRecentProjectsServiceTest(int argc, char **argv)
{
    MainWindowRecentProjectsServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowRecentProjectsServiceTest.moc"
