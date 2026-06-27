#include "../src/app/MainWindowRecentFilesService.h"

#include <QStringList>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowRecentFilesServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void normalizesProjectScopedPaths();
    void recordsOpenedFile();
    void keepsMaxRecentFileCount();
    void rejectsOutsideProjectFiles();
};
}

void MainWindowRecentFilesServiceTest::normalizesProjectScopedPaths()
{
    const QString projectPath = QStringLiteral("/tmp/project");
    const QStringList normalizedPaths =
        MainWindowRecentFilesService::normalizedRecentFilePaths(
            projectPath,
            {QString(),
             QStringLiteral("/tmp/project/a.th"),
             QStringLiteral("/tmp/project/a.th"),
             QStringLiteral("/tmp/project/maps/map.th2"),
             QStringLiteral("/tmp/other/outside.th")});

    const QStringList expectedPaths = {
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/a.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/maps/map.th2"))};
    QCOMPARE(normalizedPaths, expectedPaths);
}

void MainWindowRecentFilesServiceTest::recordsOpenedFile()
{
    const QString projectPath = QStringLiteral("/tmp/project");
    const QStringList currentPaths = {
        QStringLiteral("/tmp/project/a.th"),
        QStringLiteral("/tmp/project/b.th"),
        QStringLiteral("/tmp/project/c.th2")};

    const QStringList updatedPaths =
        MainWindowRecentFilesService::recordOpenedFile(projectPath,
                                                       currentPaths,
                                                       QStringLiteral("/tmp/project/b.th"));
    const QStringList expectedPaths = {
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/b.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/a.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/c.th2"))};
    QCOMPARE(updatedPaths, expectedPaths);
}

void MainWindowRecentFilesServiceTest::keepsMaxRecentFileCount()
{
    const QString projectPath = QStringLiteral("/tmp/project");
    const QStringList currentPaths = {
        QStringLiteral("/tmp/project/one.th"),
        QStringLiteral("/tmp/project/two.th"),
        QStringLiteral("/tmp/project/three.th"),
        QStringLiteral("/tmp/project/four.th"),
        QStringLiteral("/tmp/project/five.th"),
        QStringLiteral("/tmp/project/six.th"),
        QStringLiteral("/tmp/project/seven.th"),
        QStringLiteral("/tmp/project/eight.th"),
        QStringLiteral("/tmp/project/nine.th"),
        QStringLiteral("/tmp/project/ten.th")};

    const QStringList updatedPaths =
        MainWindowRecentFilesService::recordOpenedFile(projectPath,
                                                       currentPaths,
                                                       QStringLiteral("/tmp/project/eleven.th"));
    const QStringList expectedPaths = {
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/eleven.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/one.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/two.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/three.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/four.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/five.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/six.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/seven.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/eight.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/nine.th"))};
    QCOMPARE(updatedPaths, expectedPaths);
}

void MainWindowRecentFilesServiceTest::rejectsOutsideProjectFiles()
{
    const QStringList updatedPaths =
        MainWindowRecentFilesService::recordOpenedFile(QStringLiteral("/tmp/project"),
                                                       {QStringLiteral("/tmp/project/a.th")},
                                                       QStringLiteral("/tmp/outside.th"));
    const QStringList expectedPaths = {
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/a.th"))};
    QCOMPARE(updatedPaths, expectedPaths);
}

int runMainWindowRecentFilesServiceTest(int argc, char **argv)
{
    MainWindowRecentFilesServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowRecentFilesServiceTest.moc"
