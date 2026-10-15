#include "../src/app/ProjectScanCacheService.h"
#include "../src/app/ProjectStructureScanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <memory>

using namespace TherionStudio;

namespace
{
bool writeTextFile(const QString &filePath, const QString &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    return file.write(contents.toUtf8()) == contents.toUtf8().size();
}

QString canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}
}

class ProjectStructureScannerTest final : public QObject
{
    Q_OBJECT

private slots:
    void scansFilesystemProject();
    void prefersInMemoryProjectContents();
    void reusesSharedProjectIndexCache();
    void reparsesOnlyChangedFilesBetweenScans();
    void publishesOnlyLatestPendingRequest();
    void teardownDoesNotPublishCompletedWorker();
};

void ProjectStructureScannerTest::scansFilesystemProject()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString rootFile = QDir(tempDir.path()).filePath(QStringLiteral("root.th"));
    QVERIFY(writeTextFile(rootFile,
                          QStringLiteral(
                              "survey scanner\n"
                              "  centreline\n"
                              "  endcentreline\n"
                              "endsurvey scanner\n")));

    ProjectStructureScanner scanner;
    scanner.setDebounceIntervalMs(0);

    ProjectStructureScanner::Result result;
    bool received = false;
    connect(&scanner,
            &ProjectStructureScanner::scanFinished,
            &scanner,
            [&](const ProjectStructureScanner::Result &nextResult) {
                result = nextResult;
                received = true;
            });

    scanner.requestScan(tempDir.path(), {});
    QTRY_VERIFY_WITH_TIMEOUT(received, 5000);
    QCOMPARE(result.requestSerial, quint64(1));
    QCOMPARE(result.projectRootPath, tempDir.path());
    QVERIFY(result.errorMessage.isEmpty());
    QVERIFY(result.entries.size() >= 2);

    const ProjectStructureEntry &surveyEntry = result.entries.constFirst();
    QCOMPARE(surveyEntry.category, QStringLiteral("Surveys"));
    QCOMPARE(surveyEntry.name, QStringLiteral("scanner"));
    QCOMPARE(canonicalOrAbsolutePath(surveyEntry.sourceFile), canonicalOrAbsolutePath(rootFile));
}

void ProjectStructureScannerTest::prefersInMemoryProjectContents()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString unsavedFilePath = QDir(tempDir.path()).filePath(QStringLiteral("unsaved.th"));
    QVERIFY(writeTextFile(unsavedFilePath,
                          QStringLiteral(
                              "survey stale\n"
                              "endsurvey stale\n")));

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(canonicalOrAbsolutePath(unsavedFilePath),
                            QStringLiteral(
                                "survey memory\n"
                                "  input map.th2\n"
                                "endsurvey memory\n"));

    ProjectStructureScanner scanner;
    scanner.setDebounceIntervalMs(0);

    ProjectStructureScanner::Result result;
    bool received = false;
    connect(&scanner,
            &ProjectStructureScanner::scanFinished,
            &scanner,
            [&](const ProjectStructureScanner::Result &nextResult) {
                result = nextResult;
                received = true;
            });

    scanner.requestScan(tempDir.path(), inMemoryContents);
    QTRY_VERIFY_WITH_TIMEOUT(received, 5000);
    QCOMPARE(result.requestSerial, quint64(1));
    QVERIFY(result.errorMessage.isEmpty());
    QVERIFY(!result.entries.isEmpty());

    bool foundMemorySurvey = false;
    for (const ProjectStructureEntry &entry : result.entries) {
        if (entry.category == QStringLiteral("Surveys")
            && entry.name == QStringLiteral("memory")
            && canonicalOrAbsolutePath(entry.sourceFile) == canonicalOrAbsolutePath(unsavedFilePath)) {
            foundMemorySurvey = true;
            break;
        }
    }
    QVERIFY(foundMemorySurvey);
}

void ProjectStructureScannerTest::reusesSharedProjectIndexCache()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString rootFile = QDir(tempDir.path()).filePath(QStringLiteral("root.th"));
    QVERIFY(writeTextFile(rootFile,
                          QStringLiteral(
                              "survey cached\n"
                              "  centreline\n"
                              "  endcentreline\n"
                              "endsurvey cached\n")));

    const auto cacheService = std::make_shared<ProjectScanCacheService>();
    ProjectStructureScanner firstScanner(cacheService);
    firstScanner.setDebounceIntervalMs(0);

    ProjectStructureScanner::Result firstResult;
    bool firstReceived = false;
    connect(&firstScanner,
            &ProjectStructureScanner::scanFinished,
            &firstScanner,
            [&](const ProjectStructureScanner::Result &result) {
                firstResult = result;
                firstReceived = true;
            });
    firstScanner.requestScan(tempDir.path(), {});
    QTRY_VERIFY_WITH_TIMEOUT(firstReceived, 5000);
    QVERIFY(!firstResult.projectIndexSnapshotCacheHit);
    QVERIFY(!firstResult.projectSourceSnapshotCacheHit);

    ProjectStructureScanner secondScanner(cacheService);
    secondScanner.setDebounceIntervalMs(0);

    ProjectStructureScanner::Result secondResult;
    bool secondReceived = false;
    connect(&secondScanner,
            &ProjectStructureScanner::scanFinished,
            &secondScanner,
            [&](const ProjectStructureScanner::Result &result) {
                secondResult = result;
                secondReceived = true;
            });
    secondScanner.requestScan(tempDir.path(), {});
    QTRY_VERIFY_WITH_TIMEOUT(secondReceived, 5000);
    QVERIFY(secondResult.projectIndexSnapshotCacheHit);
    QVERIFY(secondResult.projectSourceSnapshotCacheHit);
    QCOMPARE(secondResult.entries.size(), firstResult.entries.size());
}

void ProjectStructureScannerTest::reparsesOnlyChangedFilesBetweenScans()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString rootFile = QDir(tempDir.path()).filePath(QStringLiteral("root.th"));
    const QString childFile = QDir(tempDir.path()).filePath(QStringLiteral("child.th"));
    QVERIFY(writeTextFile(rootFile, QStringLiteral("survey top\n  input child.th\nendsurvey top\n")));
    QVERIFY(writeTextFile(childFile, QStringLiteral("survey first\nendsurvey first\n")));
    // The config sources only root.th, so the index never reads unused.th and needs no parse of it.
    QVERIFY(writeTextFile(QDir(tempDir.path()).filePath(QStringLiteral("thconfig")), QStringLiteral("source root.th\n")));
    QVERIFY(writeTextFile(QDir(tempDir.path()).filePath(QStringLiteral("unused.th")),
                          QStringLiteral("survey unused\nendsurvey unused\n")));

    const auto cacheService = std::make_shared<ProjectScanCacheService>();
    ProjectStructureScanner scanner(cacheService);
    scanner.setDebounceIntervalMs(0);
    ProjectStructureScanner::Result latest;
    int received = 0;
    connect(&scanner, &ProjectStructureScanner::scanFinished, &scanner, [&](const ProjectStructureScanner::Result &result) {
        latest = result;
        ++received;
    });
    const auto surveyNames = [&]() {
        QStringList names;
        for (const ProjectStructureEntry &entry : std::as_const(latest.entries)) {
            if (entry.kind == ProjectStructureEntryKind::Survey) {
                names.append(entry.name);
            }
        }
        return names;
    };

    scanner.requestScan(tempDir.path(), {});
    QTRY_COMPARE_WITH_TIMEOUT(received, 1, 5000);
    // thconfig, root.th, and child.th: the config is read to find the sources; unused.th is never parsed.
    QCOMPARE(latest.projectionCacheStats.logicalDocumentBuilds, 3);
    QVERIFY(surveyNames().contains(QStringLiteral("first")));
    QVERIFY(!surveyNames().contains(QStringLiteral("unused")));

    // A save changes one file; snapshot caches are invalidated, but the unchanged file keeps its parsed form.
    QVERIFY(writeTextFile(childFile, QStringLiteral("survey second\nendsurvey second\n")));
    cacheService->clearProjectSourceSnapshot();
    cacheService->clearProjectIndexSnapshot();
    scanner.requestScan(tempDir.path(), {});
    QTRY_COMPARE_WITH_TIMEOUT(received, 2, 5000);
    QVERIFY(!latest.projectIndexSnapshotCacheHit);
    QCOMPARE(latest.projectionCacheStats.logicalDocumentBuilds, 1);
    QCOMPARE(latest.projectionCacheStats.logicalDocumentHits, 2);
    QVERIFY(surveyNames().contains(QStringLiteral("second")));
    QVERIFY(!surveyNames().contains(QStringLiteral("first")));

    // Unsaved editor text is a different version of the file and is parsed, not served from the saved text.
    scanner.requestScan(tempDir.path(), {{canonicalOrAbsolutePath(childFile), QStringLiteral("survey third\nendsurvey third\n")}});
    QTRY_COMPARE_WITH_TIMEOUT(received, 3, 5000);
    QVERIFY(surveyNames().contains(QStringLiteral("third")));
}

void ProjectStructureScannerTest::publishesOnlyLatestPendingRequest()
{
    QSemaphore firstScanStarted;
    QSemaphore releaseFirstScan;
    std::atomic_int scanCallCount = 0;

    ProjectStructureScanner scanner(
        [&](const QString &projectRootPath,
            const QString &,
            const QHash<QString, QString> &,
            quint64 requestSerial) {
            const int callNumber = ++scanCallCount;
            ProjectStructureScanner::Result result;
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

    QVector<ProjectStructureScanner::Result> publishedResults;
    connect(&scanner,
            &ProjectStructureScanner::scanFinished,
            &scanner,
            [&](const ProjectStructureScanner::Result &result) {
                publishedResults.append(result);
            });

    scanner.requestScan(QStringLiteral("same-project"), {});
    QTRY_COMPARE_WITH_TIMEOUT(firstScanStarted.available(), 1, 2000);
    firstScanStarted.acquire();

    scanner.requestScan(QStringLiteral("same-project"), {});
    ProjectStructureScanner::Result firstIdentity;
    firstIdentity.requestSerial = 1;
    firstIdentity.projectRootPath = QStringLiteral("same-project");
    QVERIFY(!scanner.isLatestRequestResult(firstIdentity));
    ProjectStructureScanner::Result secondIdentity;
    secondIdentity.requestSerial = 2;
    secondIdentity.projectRootPath = QStringLiteral("same-project");
    QVERIFY(scanner.isLatestRequestResult(secondIdentity));
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    scanner.requestScan(QStringLiteral("project-c"), {});
    QVERIFY(!scanner.isLatestRequestResult(secondIdentity));
    releaseFirstScan.release();
    releaseFirstScanGuard.dismiss();

    QTRY_COMPARE_WITH_TIMEOUT(publishedResults.size(), 1, 2000);
    QCOMPARE(scanCallCount.load(), 2);
    QCOMPARE(publishedResults.constFirst().requestSerial, quint64(3));
    QCOMPARE(publishedResults.constFirst().projectRootPath, QStringLiteral("project-c"));
    QVERIFY(publishedResults.constFirst().errorMessage.isEmpty());
}

void ProjectStructureScannerTest::teardownDoesNotPublishCompletedWorker()
{
    QSemaphore workerStarted;
    QSemaphore releaseWorker;
    QSemaphore workerFinished;
    int publishedResultCount = 0;

    auto scanner = std::make_unique<ProjectStructureScanner>(
        [&](const QString &projectRootPath,
            const QString &,
            const QHash<QString, QString> &,
            quint64 requestSerial) {
            workerStarted.release();
            releaseWorker.acquire();
            ProjectStructureScanner::Result result;
            result.requestSerial = requestSerial;
            result.projectRootPath = projectRootPath;
            workerFinished.release();
            return result;
        });
    auto releaseWorkerGuard = qScopeGuard([&]() { releaseWorker.release(); });
    scanner->setDebounceIntervalMs(0);
    connect(scanner.get(),
            &ProjectStructureScanner::scanFinished,
            scanner.get(),
            [&](const ProjectStructureScanner::Result &) {
                ++publishedResultCount;
            });

    scanner->requestScan(QStringLiteral("project"), {});
    QTRY_COMPARE_WITH_TIMEOUT(workerStarted.available(), 1, 2000);
    workerStarted.acquire();
    releaseWorker.release();
    releaseWorkerGuard.dismiss();
    QVERIFY(workerFinished.tryAcquire(1, 2000));
    scanner.reset();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(publishedResultCount, 0);
}

int runProjectStructureScannerTest(int argc, char **argv)
{
    ProjectStructureScannerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectStructureScannerTest.moc"
