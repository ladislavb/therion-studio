#include "../src/app/ProjectStructureScanner.h"
#include "../src/app/ProjectScanCacheService.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSemaphore>
#include <QScopeGuard>
#include <QTimer>

#include <atomic>
#include <functional>
#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

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

struct ScanWaitResult
{
    bool received = false;
    ProjectStructureScanner::Result result;
};

ScanWaitResult waitForScan(ProjectStructureScanner &scanner)
{
    ScanWaitResult waitResult;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);

    QObject::connect(&scanner,
                     &ProjectStructureScanner::scanFinished,
                     &loop,
                     [&](const ProjectStructureScanner::Result &result) {
                         waitResult.received = true;
                         waitResult.result = result;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();
    return waitResult;
}

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    return condition();
}

int runFilesystemScanTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString rootFile = QDir(tempDir.path()).filePath(QStringLiteral("root.th"));
    if (!expect(writeTextFile(rootFile,
                              QStringLiteral(
                                  "survey scanner\n"
                                  "  centreline\n"
                                  "  endcentreline\n"
                                  "endsurvey scanner\n")),
                "Temporary Therion source file could not be written.")) {
        return 1;
    }

    ProjectStructureScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), {});

    const ScanWaitResult waitResult = waitForScan(scanner);
    if (!expect(waitResult.received, "ProjectStructureScanner did not emit scanFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.requestSerial == 1, "First scan request serial should be 1.")) {
        return 1;
    }
    if (!expect(waitResult.result.projectRootPath == tempDir.path(), "Scan result project root path mismatch.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Filesystem scan should not report an error.")) {
        return 1;
    }
    if (!expect(waitResult.result.entries.size() >= 2, "Filesystem scan should find survey and centreline entries.")) {
        return 1;
    }

    const ProjectStructureEntry &surveyEntry = waitResult.result.entries.at(0);
    if (!expect(surveyEntry.category == QStringLiteral("Surveys")
                    && surveyEntry.name == QStringLiteral("scanner")
                    && canonicalOrAbsolutePath(surveyEntry.sourceFile) == canonicalOrAbsolutePath(rootFile),
                "Filesystem scan survey entry is incorrect.")) {
        return 1;
    }

    return 0;
}

int runInMemoryScanTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString unsavedFilePath = QDir(tempDir.path()).filePath(QStringLiteral("unsaved.th"));
    if (!expect(writeTextFile(unsavedFilePath,
                              QStringLiteral(
                                  "survey stale\n"
                                  "endsurvey stale\n")),
                "Temporary on-disk source placeholder could not be written.")) {
        return 1;
    }

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(canonicalOrAbsolutePath(unsavedFilePath),
                            QStringLiteral(
                                "survey memory\n"
                                "  input map.th2\n"
                                "endsurvey memory\n"));

    ProjectStructureScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), inMemoryContents);

    const ScanWaitResult waitResult = waitForScan(scanner);
    if (!expect(waitResult.received, "In-memory scan did not emit scanFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.requestSerial == 1, "In-memory first scan request serial should be 1.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "In-memory scan should not report an error.")) {
        return 1;
    }
    if (!expect(!waitResult.result.entries.isEmpty(), "In-memory scan should find entries.")) {
        return 1;
    }

    bool foundMemorySurvey = false;
    for (const ProjectStructureEntry &entry : waitResult.result.entries) {
        if (entry.category == QStringLiteral("Surveys")
            && entry.name == QStringLiteral("memory")
            && canonicalOrAbsolutePath(entry.sourceFile) == canonicalOrAbsolutePath(unsavedFilePath)) {
            foundMemorySurvey = true;
            break;
        }
    }
    if (!expect(foundMemorySurvey, "In-memory scan survey entry is incorrect.")) {
        return 1;
    }

    return 0;
}

int runSharedProjectIndexCacheTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString rootFile = QDir(tempDir.path()).filePath(QStringLiteral("root.th"));
    if (!expect(writeTextFile(rootFile,
                              QStringLiteral(
                                  "survey cached\n"
                                  "  centreline\n"
                                  "  endcentreline\n"
                                  "endsurvey cached\n")),
                "Temporary cached Therion source file could not be written.")) {
        return 1;
    }

    const auto cacheService = std::make_shared<ProjectScanCacheService>();
    ProjectStructureScanner firstScanner(cacheService);
    firstScanner.setDebounceIntervalMs(0);
    firstScanner.requestScan(tempDir.path(), {});

    const ScanWaitResult firstResult = waitForScan(firstScanner);
    if (!expect(firstResult.received, "First shared-cache structure scan did not finish.")) {
        return 1;
    }
    if (!expect(!firstResult.result.projectIndexSnapshotCacheHit,
                "First structure scan should build the project index snapshot.")) {
        return 1;
    }
    if (!expect(!firstResult.result.projectSourceSnapshotCacheHit,
                "First structure scan should collect the project source snapshot.")) {
        return 1;
    }

    ProjectStructureScanner secondScanner(cacheService);
    secondScanner.setDebounceIntervalMs(0);
    secondScanner.requestScan(tempDir.path(), {});

    const ScanWaitResult secondResult = waitForScan(secondScanner);
    if (!expect(secondResult.received, "Second shared-cache structure scan did not finish.")) {
        return 1;
    }
    if (!expect(secondResult.result.projectIndexSnapshotCacheHit,
                "Second structure scan should reuse the shared project index snapshot cache.")) {
        return 1;
    }
    if (!expect(secondResult.result.projectSourceSnapshotCacheHit,
                "Second structure scan should reuse the shared project source snapshot cache.")) {
        return 1;
    }
    if (!expect(secondResult.result.entries.size() == firstResult.result.entries.size(),
                "Cached structure scan should preserve structure entries.")) {
        return 1;
    }

    return 0;
}

int runLatestRequestSupersessionTest()
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
    QObject::connect(&scanner,
                     &ProjectStructureScanner::scanFinished,
                     &scanner,
                     [&](const ProjectStructureScanner::Result &result) {
                         publishedResults.append(result);
                     });

    scanner.requestScan(QStringLiteral("project-a"), {});
    if (!expect(waitUntil([&]() { return firstScanStarted.available() == 1; }),
                "First deterministic structure scan did not start.")) {
        return 1;
    }
    firstScanStarted.acquire();

    scanner.requestScan(QStringLiteral("project-b"), {});
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    scanner.requestScan(QStringLiteral("project-c"), {});
    releaseFirstScan.release();
    releaseFirstScanGuard.dismiss();

    if (!expect(waitUntil([&]() { return publishedResults.size() == 1; }),
                "Latest structure scan result was not published.")) {
        return 1;
    }
    if (!expect(scanCallCount.load() == 2,
                "Only the running and latest pending structure requests should execute.")) {
        return 1;
    }
    if (!expect(publishedResults.constFirst().requestSerial == 3
                    && publishedResults.constFirst().projectRootPath == QStringLiteral("project-c"),
                "Structure scanner should publish only the latest accepted request.")) {
        return 1;
    }
    if (!expect(publishedResults.constFirst().errorMessage.isEmpty(),
                "A superseded structure error must not replace the latest result.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runFilesystemScanTest() != 0) {
        return 1;
    }
    if (runInMemoryScanTest() != 0) {
        return 1;
    }
    if (runSharedProjectIndexCacheTest() != 0) {
        return 1;
    }
    if (runLatestRequestSupersessionTest() != 0) {
        return 1;
    }

    return 0;
}
