#include "../src/app/ProjectOutputsScanner.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSemaphore>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <atomic>

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
    void emptyProjectRootReportsOpenProjectState();
    void publishesOnlyLatestPendingRequest();
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

void ProjectOutputsScannerTest::emptyProjectRootReportsOpenProjectState()
{
    ProjectOutputsScanner scanner;
    scanner.setDebounceIntervalMs(0);

    ProjectOutputsScanner::Result result;
    bool received = false;
    connect(&scanner,
            &ProjectOutputsScanner::scanFinished,
            &scanner,
            [&](const ProjectOutputsScanner::Result &nextResult) {
                result = nextResult;
                received = true;
            });

    scanner.requestScan(QString());
    QTRY_VERIFY_WITH_TIMEOUT(received, 2000);
    QCOMPARE(result.requestSerial, quint64(1));
    QVERIFY(result.projectRootPath.isEmpty());
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(result.artifacts.isEmpty());
}

void ProjectOutputsScannerTest::publishesOnlyLatestPendingRequest()
{
    QSemaphore firstScanStarted;
    QSemaphore releaseFirstScan;
    std::atomic_int scanCallCount = 0;

    ProjectOutputsScanner scanner([&](const QString &projectRootPath, quint64 requestSerial) {
        const int callNumber = ++scanCallCount;
        ProjectOutputsScanner::Result result;
        result.requestSerial = requestSerial;
        result.projectRootPath = projectRootPath;
        if (callNumber == 1) {
            firstScanStarted.release();
            releaseFirstScan.acquire();
            result.errorMessage = QStringLiteral("superseded error");
        }
        return result;
    });
    auto releaseFirstScanGuard = qScopeGuard([&]() { releaseFirstScan.release(); });
    scanner.setDebounceIntervalMs(0);

    QVector<ProjectOutputsScanner::Result> publishedResults;
    connect(&scanner,
            &ProjectOutputsScanner::scanFinished,
            &scanner,
            [&](const ProjectOutputsScanner::Result &result) {
                publishedResults.append(result);
            });

    scanner.requestScan(QStringLiteral("same-project"));
    QTRY_COMPARE_WITH_TIMEOUT(firstScanStarted.available(), 1, 2000);
    firstScanStarted.acquire();

    scanner.requestScan(QStringLiteral("same-project"));
    ProjectOutputsScanner::Result firstIdentity;
    firstIdentity.requestSerial = 1;
    firstIdentity.projectRootPath = QStringLiteral("same-project");
    QVERIFY(!scanner.isLatestRequestResult(firstIdentity));
    ProjectOutputsScanner::Result secondIdentity;
    secondIdentity.requestSerial = 2;
    secondIdentity.projectRootPath = QStringLiteral("same-project");
    QVERIFY(scanner.isLatestRequestResult(secondIdentity));
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    scanner.requestScan(QStringLiteral("project-c"));
    QVERIFY(!scanner.isLatestRequestResult(secondIdentity));
    releaseFirstScan.release();
    releaseFirstScanGuard.dismiss();

    QTRY_COMPARE_WITH_TIMEOUT(publishedResults.size(), 1, 2000);
    QCOMPARE(scanCallCount.load(), 2);
    QCOMPARE(publishedResults.constFirst().requestSerial, quint64(3));
    QCOMPARE(publishedResults.constFirst().projectRootPath, QStringLiteral("project-c"));
    QVERIFY(publishedResults.constFirst().errorMessage.isEmpty());
}

int runProjectOutputsScannerTest(int argc, char **argv)
{
    ProjectOutputsScannerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectOutputsScannerTest.moc"
