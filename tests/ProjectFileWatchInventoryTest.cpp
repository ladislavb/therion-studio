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
}

class ProjectFileWatchInventoryTest final : public QObject
{
    Q_OBJECT

private slots:
    void collectsDeterministicTherionInventory();
    void excludesSymlinkedPathsOutsideProjectRoot();
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
    const QString symlinkPath = QDir(projectDir.path()).filePath(QStringLiteral("outside-link.th"));
    if (!QFile::link(externalFilePath, symlinkPath)) {
        QSKIP("The platform does not permit the test symlink.");
    }
    if (!QFileInfo(symlinkPath).isSymLink()) {
        QSKIP("QFile::link did not create a symbolic link on this platform.");
    }
    QVERIFY(writeFile(externalFilePath));

    const ProjectFileWatchInventory inventory =
        ProjectFileWatchInventoryCollector::collect({projectDir.path()});
    QVERIFY(!inventory.files.contains(QFileInfo(externalFilePath).canonicalFilePath()));
    QVERIFY(!inventory.files.contains(symlinkPath));
    QVERIFY(inventory.skippedPaths.contains(
        QDir(inventory.projectRootPath).filePath(QStringLiteral("outside-link.th"))));
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
