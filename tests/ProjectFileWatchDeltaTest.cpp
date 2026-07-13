#include "../src/app/ProjectFileWatchDelta.h"

#include <QTest>

using namespace TherionStudio;

class ProjectFileWatchDeltaTest final : public QObject
{
    Q_OBJECT

private slots:
    void plansSortedDirectoryAndFileDeltas();
};

void ProjectFileWatchDeltaTest::plansSortedDirectoryAndFileDeltas()
{
    ProjectFileWatchInventory inventory;
    inventory.directories = {QStringLiteral("root"), QStringLiteral("root/new")};
    inventory.files = {QStringLiteral("root/a.th"), QStringLiteral("root/new/b.th")};

    const ProjectFileWatchDelta delta = ProjectFileWatchDeltaPlanner::plan(
        inventory,
        {QStringLiteral("root"), QStringLiteral("root/old")},
        {QStringLiteral("root/a.th"), QStringLiteral("root/old.th")});

    QCOMPARE(delta.directoriesToRemove, QStringList{QStringLiteral("root/old")});
    QCOMPARE(delta.directoriesToAdd, QStringList{QStringLiteral("root/new")});
    QCOMPARE(delta.filesToRemove, QStringList{QStringLiteral("root/old.th")});
    QCOMPARE(delta.filesToAdd, QStringList{QStringLiteral("root/new/b.th")});
}

int runProjectFileWatchDeltaTest(int argc, char **argv)
{
    ProjectFileWatchDeltaTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectFileWatchDeltaTest.moc"
