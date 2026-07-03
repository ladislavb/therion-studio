#include "../src/app/ProjectOutputsScanner.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using TherionStudio::ProjectOutputsScanner;

namespace
{
bool writeFile(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write("test\n");
    return true;
}
}

class ProjectOutputsScannerTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsDuplicateNamesDistinctAndClassifiesArtifacts();
};

void ProjectOutputsScannerTest::keepsDuplicateNamesDistinctAndClassifiesArtifacts()
{
    QTemporaryDir projectDir;
    QVERIFY(projectDir.isValid());

    QVERIFY(writeFile(QDir(projectDir.path()).filePath(QStringLiteral("a/out/map.pdf"))));
    QVERIFY(writeFile(QDir(projectDir.path()).filePath(QStringLiteral("b/out/map.pdf"))));
    QVERIFY(writeFile(QDir(projectDir.path()).filePath(QStringLiteral("output/cave.lox"))));
    QVERIFY(writeFile(QDir(projectDir.path()).filePath(QStringLiteral("output/cave.3d"))));
    QVERIFY(writeFile(QDir(projectDir.path()).filePath(QStringLiteral("output/cave.sql"))));
    QVERIFY(writeFile(QDir(projectDir.path()).filePath(QStringLiteral("notes/readme.txt"))));

    ProjectOutputsScanner scanner;
    scanner.setDebounceIntervalMs(0);

    ProjectOutputsScanner::Result result;
    bool received = false;
    QEventLoop loop;
    connect(&scanner, &ProjectOutputsScanner::scanFinished, &loop, [&](const ProjectOutputsScanner::Result &nextResult) {
        result = nextResult;
        received = true;
        loop.quit();
    });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);

    scanner.requestScan(projectDir.path());
    loop.exec();
    QVERIFY(received);
    QVERIFY(result.errorMessage.isEmpty());
    QCOMPARE(result.artifacts.size(), 4);

    int modelCount = 0;
    int mapAtlasCount = 0;
    int databaseCount = 0;
    QStringList relativePaths;
    QHash<QString, QString> displayNamesByRelativePath;
    for (const ProjectOutputsScanner::Artifact &artifact : result.artifacts) {
        const QString relativePath = QDir::fromNativeSeparators(artifact.relativePath);
        relativePaths.append(relativePath);
        displayNamesByRelativePath.insert(relativePath, artifact.displayName);
        switch (artifact.kind) {
        case ProjectOutputsScanner::ArtifactKind::Model:
            ++modelCount;
            break;
        case ProjectOutputsScanner::ArtifactKind::MapAtlas:
            ++mapAtlasCount;
            break;
        case ProjectOutputsScanner::ArtifactKind::Database:
            ++databaseCount;
            break;
        }
    }

    QCOMPARE(modelCount, 1);
    QCOMPARE(mapAtlasCount, 2);
    QCOMPARE(databaseCount, 1);
    QVERIFY(relativePaths.contains(QStringLiteral("a/out/map.pdf")));
    QVERIFY(relativePaths.contains(QStringLiteral("b/out/map.pdf")));
    QVERIFY(!relativePaths.contains(QStringLiteral("output/cave.3d")));
    QCOMPARE(displayNamesByRelativePath.value(QStringLiteral("a/out/map.pdf")), QStringLiteral("map.pdf (a/out)"));
    QCOMPARE(displayNamesByRelativePath.value(QStringLiteral("b/out/map.pdf")), QStringLiteral("map.pdf (b/out)"));
    QCOMPARE(displayNamesByRelativePath.value(QStringLiteral("output/cave.lox")), QStringLiteral("cave.lox"));
    QCOMPARE(displayNamesByRelativePath.value(QStringLiteral("output/cave.sql")), QStringLiteral("cave.sql"));
}

int runProjectOutputsScannerTest(int argc, char **argv)
{
    ProjectOutputsScannerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectOutputsScannerTest.moc"
