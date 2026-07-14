#include "../src/app/ProjectFileWatchInventory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace TherionStudio;

namespace
{
bool writeFile(const QString &filePath, const QByteArray &contents = "x")
{
    QFile file(filePath);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

QString externalLinkFileName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("outside-link.lnk");
#else
    return QStringLiteral("outside-link.th");
#endif
}

bool createExternalLink(const QString &targetPath, const QString &linkPath)
{
    if (!QFile::link(targetPath, linkPath)) {
        return false;
    }
    if (QFileInfo(linkPath).isSymLink()) {
        return true;
    }
    QFile::remove(linkPath);
    return false;
}
}

class ProjectFileWatchInventoryTest final : public QObject
{
    Q_OBJECT

private slots:
    void collectsDeterministicTherionInventory();
    void excludesSymlinkedPathsOutsideProjectRoot();
    void collectsDeepWideTreeWithoutSkippedOrSymlinkedPaths();
    void reportsInvalidRoot();
};

void ProjectFileWatchInventoryTest::collectsDeterministicTherionInventory()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString rootPath = tempDir.path();
    QVERIFY(QDir(rootPath).mkpath(QStringLiteral("nested")));
    QVERIFY(QDir(rootPath).mkpath(QStringLiteral(".git")));
    QVERIFY(QDir(rootPath).mkpath(QStringLiteral("build")));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral("survey.th")), "survey"));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral("map.th2")), "map"));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral("thconfig")), "config"));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral("notes.txt")), "notes"));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral("nested/child.th")), "child"));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral(".git/hidden.th")), "hidden"));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral("build/generated.th")), "generated"));

    const ProjectFileWatchInventoryRequest request{rootPath};
    const ProjectFileWatchInventory first = ProjectFileWatchInventoryCollector::collect(request);
    const ProjectFileWatchInventory second = ProjectFileWatchInventoryCollector::collect(request);

    const QString normalizedRootPath = QFileInfo(rootPath).canonicalFilePath();
    QCOMPARE(first.projectRootPath, normalizedRootPath);
    QCOMPARE(first.directories,
             (QStringList{normalizedRootPath, QDir(normalizedRootPath).filePath(QStringLiteral("nested"))}));
    QCOMPARE(first.files,
             (QStringList{
                 QDir(normalizedRootPath).filePath(QStringLiteral("map.th2")),
                 QDir(normalizedRootPath).filePath(QStringLiteral("nested/child.th")),
                 QDir(normalizedRootPath).filePath(QStringLiteral("survey.th")),
                 QDir(normalizedRootPath).filePath(QStringLiteral("thconfig")),
             }));
    QCOMPARE(first.directories, second.directories);
    QCOMPARE(first.files, second.files);
    QCOMPARE(first.signatures, second.signatures);
    QVERIFY(first.discoveryErrors.isEmpty());
    QVERIFY(first.skippedPaths.contains(QDir(normalizedRootPath).filePath(QStringLiteral(".git"))));
    QVERIFY(first.skippedPaths.contains(QDir(normalizedRootPath).filePath(QStringLiteral("build"))));
    QCOMPARE(first.signatures.value(QDir(normalizedRootPath).filePath(QStringLiteral("survey.th"))),
             QStringLiteral("file|6|%1")
                 .arg(QFileInfo(QDir(normalizedRootPath).filePath(QStringLiteral("survey.th")))
                          .lastModified()
                          .toMSecsSinceEpoch()));
}

void ProjectFileWatchInventoryTest::excludesSymlinkedPathsOutsideProjectRoot()
{
    QTemporaryDir projectDir;
    QTemporaryDir externalDir;
    QVERIFY(projectDir.isValid());
    QVERIFY(externalDir.isValid());
    const QString externalFilePath = QDir(externalDir.path()).filePath(QStringLiteral("outside.th"));
    const QString symlinkPath = QDir(projectDir.path()).filePath(externalLinkFileName());
    QVERIFY(writeFile(externalFilePath));
    if (!createExternalLink(externalFilePath, symlinkPath)) {
        QSKIP("The platform does not permit the test symlink.");
    }
    if (!QFileInfo(symlinkPath).isSymLink()) {
        QSKIP("QFile::link did not create a symbolic link on this platform.");
    }

    const ProjectFileWatchInventory inventory =
        ProjectFileWatchInventoryCollector::collect({projectDir.path()});
    const QString normalizedSymlinkPath =
        QDir(inventory.projectRootPath).filePath(externalLinkFileName());
    QVERIFY(!inventory.files.contains(QFileInfo(externalFilePath).canonicalFilePath()));
    QVERIFY(!inventory.files.contains(normalizedSymlinkPath));
    QVERIFY(inventory.skippedPaths.contains(normalizedSymlinkPath));
}

void ProjectFileWatchInventoryTest::collectsDeepWideTreeWithoutSkippedOrSymlinkedPaths()
{
    QTemporaryDir projectDir;
    QTemporaryDir externalDir;
    QVERIFY(projectDir.isValid());
    QVERIFY(externalDir.isValid());
    const QString rootPath = projectDir.path();
    const QString externalLinkName = externalLinkFileName();
    constexpr int branchCount = 12;
    constexpr int levelsPerBranch = 4;
    for (int branch = 0; branch < branchCount; ++branch) {
        QString directoryPath = QDir(rootPath).filePath(QStringLiteral("branch-%1").arg(branch));
        QVERIFY(QDir().mkpath(directoryPath));
        QVERIFY(writeFile(QDir(directoryPath).filePath(QStringLiteral("survey.th"))));
        for (int level = 0; level < levelsPerBranch; ++level) {
            directoryPath = QDir(directoryPath).filePath(QStringLiteral("level-%1").arg(level));
            QVERIFY(QDir().mkpath(directoryPath));
            QVERIFY(writeFile(QDir(directoryPath).filePath(QStringLiteral("map.th2"))));
        }
    }
    QVERIFY(QDir(rootPath).mkpath(QStringLiteral(".git")));
    QVERIFY(QDir(rootPath).mkpath(QStringLiteral("build")));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral(".git/ignored.th"))));
    QVERIFY(writeFile(QDir(rootPath).filePath(QStringLiteral("build/generated.th"))));

    const QString externalFilePath = QDir(externalDir.path()).filePath(QStringLiteral("external.th"));
    const QString symlinkPath = QDir(rootPath).filePath(externalLinkName);
    QVERIFY(writeFile(externalFilePath));
    createExternalLink(externalFilePath, symlinkPath);

    const ProjectFileWatchInventory inventory = ProjectFileWatchInventoryCollector::collect({rootPath});
    QCOMPARE(inventory.directories.size(), 1 + branchCount * (1 + levelsPerBranch));
    QCOMPARE(inventory.files.size(), branchCount * (1 + levelsPerBranch));
    QVERIFY(inventory.discoveryErrors.isEmpty());
    for (const QString &path : inventory.directories + inventory.files) {
        QVERIFY(!path.contains(QStringLiteral("/.git/")));
        QVERIFY(!path.contains(QStringLiteral("/build/")));
        QVERIFY(!path.endsWith(externalLinkName));
    }
}

void ProjectFileWatchInventoryTest::reportsInvalidRoot()
{
    const ProjectFileWatchInventory inventory =
        ProjectFileWatchInventoryCollector::collect({QStringLiteral("missing-project-root")});
    QVERIFY(inventory.directories.isEmpty());
    QVERIFY(inventory.files.isEmpty());
    QCOMPARE(inventory.discoveryErrors.size(), 1);
}

int runProjectFileWatchInventoryTest(int argc, char **argv)
{
    ProjectFileWatchInventoryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectFileWatchInventoryTest.moc"
