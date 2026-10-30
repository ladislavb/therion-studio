#include "../src/app/ProjectVersioningStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

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

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

int runCheckpointCreatesIsolatedHistoryTest()
{
    QTemporaryDir temporaryDirectory;
    if (!expect(temporaryDirectory.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString projectRoot = temporaryDirectory.path() + QStringLiteral("/project");
    const QString historyDirectory = temporaryDirectory.path() + QStringLiteral("/history/project-id");
    if (!QDir().mkpath(projectRoot)) {
        return 1;
    }

    if (!writeFile(projectRoot + QStringLiteral("/survey.th"), QByteArrayLiteral("survey cave\nendsurvey\n"))
        || !QDir().mkpath(projectRoot + QStringLiteral("/outputs"))
        || !writeFile(projectRoot + QStringLiteral("/outputs/model.3d"), QByteArrayLiteral("generated model"))
        || !writeFile(projectRoot + QStringLiteral("/survey.lox"), QByteArrayLiteral("generated export"))) {
        return 1;
    }

    const QString existingGitDirectory = projectRoot + QStringLiteral("/.git");
    if (!QDir().mkpath(existingGitDirectory)) {
        return 1;
    }
    QFile marker(existingGitDirectory + QStringLiteral("/do-not-touch"));
    if (!marker.open(QIODevice::WriteOnly) || marker.write("existing project git state") < 0) {
        return 1;
    }
    marker.close();

    ProjectVersioningStore store(projectRoot, historyDirectory);
    const ProjectVersioningStore::CheckpointResult result =
        store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(result.succeeded, "Checkpoint creation should succeed for a valid project.")) {
        std::cerr << result.error.toStdString() << '\n';
        return 1;
    }
    if (!expect(!result.checkpointId.isEmpty(), "Checkpoint should have a stable identifier.")) {
        return 1;
    }

    QProcess listFiles;
    listFiles.start(QStringLiteral("git"),
                    {QStringLiteral("--git-dir=%1").arg(historyDirectory),
                     QStringLiteral("ls-tree"),
                     QStringLiteral("-r"),
                     QStringLiteral("--name-only"),
                     QStringLiteral("HEAD")});
    if (!expect(listFiles.waitForFinished(5000) && listFiles.exitCode() == 0,
                "The checkpoint tree should be readable.")) {
        return 1;
    }
    const QByteArray trackedFiles = listFiles.readAllStandardOutput();
    if (!expect(trackedFiles.contains("survey.th"), "Therion source files must be included in checkpoints.")
        || !expect(!trackedFiles.contains("outputs/model.3d"), "Generated output directories must be excluded.")
        || !expect(!trackedFiles.contains("survey.lox"), "Generated Therion exports must be excluded.")
        || !expect(!trackedFiles.contains(".git/do-not-touch"), "Project Git metadata must be excluded.")) {
        return 1;
    }

    QFile untouchedMarker(existingGitDirectory + QStringLiteral("/do-not-touch"));
    if (!expect(untouchedMarker.open(QIODevice::ReadOnly), "The project's existing Git metadata must remain present.")) {
        return 1;
    }
    if (!expect(untouchedMarker.readAll() == QByteArrayLiteral("existing project git state"),
                "The project's existing Git metadata must remain unchanged.")) {
        return 1;
    }

    const ProjectVersioningStore::HistoryResult history = store.history();
    if (!expect(history.succeeded, "Checkpoint history should be readable.")) {
        std::cerr << history.error.toStdString() << '\n';
        return 1;
    }
    if (!expect(history.checkpoints.size() == 1, "The isolated history should contain the new checkpoint.")) {
        return 1;
    }
    if (!expect(history.checkpoints.constFirst().id == result.checkpointId,
                "The created checkpoint should be discoverable through history.")) {
        return 1;
    }

    if (!writeFile(projectRoot + QStringLiteral("/survey.th"), QByteArrayLiteral("survey cave\nchanged\n"))) {
        return 1;
    }
    const ProjectVersioningStore::CheckpointResult secondCheckpoint =
        store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(secondCheckpoint.succeeded && secondCheckpoint.checkpointId != result.checkpointId,
                "A changed project should create a distinct checkpoint.")) {
        return 1;
    }
    const ProjectVersioningStore::CheckpointResult unchangedCheckpoint =
        store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(unchangedCheckpoint.succeeded && unchangedCheckpoint.checkpointId == secondCheckpoint.checkpointId,
                "An unchanged project should not create a duplicate checkpoint.")) {
        return 1;
    }

    const ProjectVersioningStore::HistoryResult updatedHistory = store.history();
    if (!expect(updatedHistory.succeeded && updatedHistory.checkpoints.size() == 2,
                "History should retain both distinct project states.")) {
        return 1;
    }
    QProcess readFirstVersion;
    readFirstVersion.start(QStringLiteral("git"),
                           {QStringLiteral("--git-dir=%1").arg(historyDirectory),
                            QStringLiteral("show"),
                            QStringLiteral("%1:survey.th").arg(result.checkpointId)});
    if (!expect(readFirstVersion.waitForFinished(5000) && readFirstVersion.exitCode() == 0
                    && readFirstVersion.readAllStandardOutput() == QByteArrayLiteral("survey cave\nendsurvey\n"),
                "Earlier checkpoint contents must remain recoverable.")) {
        return 1;
    }
    return 0;
}

int runExistingGitRepositorySafetyTest()
{
    QTemporaryDir temporaryDirectory;
    if (!expect(temporaryDirectory.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString projectRoot = temporaryDirectory.path() + QStringLiteral("/project");
    const QString historyDirectory = temporaryDirectory.path() + QStringLiteral("/unrelated-bare-repository");
    if (!QDir().mkpath(projectRoot)
        || !writeFile(projectRoot + QStringLiteral("/survey.th"), QByteArrayLiteral("survey cave\nendsurvey\n"))) {
        return 1;
    }

    QProcess initializeRepository;
    initializeRepository.start(QStringLiteral("git"),
                               {QStringLiteral("init"),
                                QStringLiteral("--bare"),
                                QStringLiteral("--quiet"),
                                historyDirectory});
    if (!expect(initializeRepository.waitForFinished(5000) && initializeRepository.exitCode() == 0,
                "The unrelated bare repository fixture should be created.")) {
        return 1;
    }

    ProjectVersioningStore store(projectRoot, historyDirectory);
    const ProjectVersioningStore::CheckpointResult result =
        store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(!result.succeeded, "An unrelated bare repository must not be adopted as project history.")) {
        return 1;
    }

    QProcess inspectMarker;
    inspectMarker.start(QStringLiteral("git"),
                        {QStringLiteral("--git-dir=%1").arg(historyDirectory),
                         QStringLiteral("config"),
                         QStringLiteral("--local"),
                         QStringLiteral("--get"),
                         QStringLiteral("therionstudio.versioningStore")});
    if (!expect(inspectMarker.waitForFinished(5000) && inspectMarker.exitCode() != 0,
                "Rejecting an unrelated repository must not modify its configuration.")) {
        return 1;
    }

    const QString nestedHistoryDirectory = projectRoot + QStringLiteral("/history");
    ProjectVersioningStore nestedStore(projectRoot, nestedHistoryDirectory);
    const ProjectVersioningStore::CheckpointResult nestedResult =
        nestedStore.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(!nestedResult.succeeded && !QFileInfo::exists(nestedHistoryDirectory),
                "The history store must not be created inside the project tree.")) {
        return 1;
    }
    return 0;
}

int runNewIgnoreRuleRemovesPreviouslyIncludedFileTest()
{
    QTemporaryDir directory;
    const QString projectRoot = directory.path() + QStringLiteral("/project");
    const QString historyDirectory = directory.path() + QStringLiteral("/history");
    if (!directory.isValid() || !QDir().mkpath(projectRoot)
        || !writeFile(projectRoot + QStringLiteral("/private.th"), QByteArrayLiteral("private survey\n"))) {
        return 1;
    }

    ProjectVersioningStore store(projectRoot, historyDirectory);
    const auto first = store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(first.succeeded, "The initial checkpoint should succeed.")
        || !writeFile(projectRoot + QStringLiteral("/.gitignore"), QByteArrayLiteral("private.th\n"))) {
        return 1;
    }
    const auto second = store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(second.succeeded, "A changed ignore policy should produce a checkpoint.")) {
        return 1;
    }

    QProcess tree;
    tree.start(QStringLiteral("git"),
               {QStringLiteral("--git-dir=%1").arg(historyDirectory),
                QStringLiteral("ls-tree"), QStringLiteral("-r"),
                QStringLiteral("--name-only"), second.checkpointId});
    if (!expect(tree.waitForFinished(5000) && tree.exitCode() == 0,
                "The checkpoint tree should be readable.")) {
        return 1;
    }
    const QByteArray paths = tree.readAllStandardOutput();
    return expect(!paths.contains("private.th") && paths.contains(".gitignore"),
                  "New ignore rules must exclude files even when older checkpoints contain them.") ? 0 : 1;
}

int runInheritedGitIndexIsIgnoredTest()
{
    QTemporaryDir directory;
    const QString projectRoot = directory.path() + QStringLiteral("/project");
    const QString historyDirectory = directory.path() + QStringLiteral("/history");
    const QString unrelatedIndex = directory.path() + QStringLiteral("/unrelated-index");
    const QByteArray marker = QByteArrayLiteral("unrelated Git index must remain untouched");
    if (!directory.isValid() || !QDir().mkpath(projectRoot)
        || !writeFile(projectRoot + QStringLiteral("/survey.th"), QByteArrayLiteral("survey cave\n"))
        || !writeFile(unrelatedIndex, marker)) {
        return 1;
    }

    const bool hadIndex = qEnvironmentVariableIsSet("GIT_INDEX_FILE");
    const QByteArray originalIndex = qgetenv("GIT_INDEX_FILE");
    qputenv("GIT_INDEX_FILE", QFile::encodeName(unrelatedIndex));
    ProjectVersioningStore store(projectRoot, historyDirectory);
    const auto checkpoint = store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (hadIndex) {
        qputenv("GIT_INDEX_FILE", originalIndex);
    } else {
        qunsetenv("GIT_INDEX_FILE");
    }

    QFile file(unrelatedIndex);
    return expect(checkpoint.succeeded, "Inherited Git environment must not redirect checkpoint operations.")
        && expect(file.open(QIODevice::ReadOnly) && file.readAll() == marker,
                  "An inherited Git index must remain unchanged.") ? 0 : 1;
}

int runChangingProjectDoesNotPublishPartialCheckpointTest()
{
#ifdef Q_OS_UNIX
    QTemporaryDir directory;
    const QString projectRoot = directory.path() + QStringLiteral("/project");
    const QString historyDirectory = directory.path() + QStringLiteral("/history");
    const QString source = projectRoot + QStringLiteral("/survey.th");
    if (!directory.isValid() || !QDir().mkpath(projectRoot)
        || !writeFile(source, QByteArrayLiteral("original\n"))) {
        return 1;
    }
    ProjectVersioningStore store(projectRoot, historyDirectory);
    const auto initial = store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(initial.succeeded, "The stable initial checkpoint should succeed.")
        || !writeFile(source, QByteArrayLiteral("saved edit\n"))) {
        return 1;
    }

    const QString git = QStandardPaths::findExecutable(QStringLiteral("git"));
    const QString wrapperPath = directory.path() + QStringLiteral("/changing-git");
    const QByteArray wrapper =
        "#!/bin/sh\n\"" + QFile::encodeName(git) + "\" \"$@\"\nstatus=$?\n"
        "for argument in \"$@\"; do\n"
        "  if [ \"$argument\" = add ] && [ ! -e \"" + QFile::encodeName(directory.path())
        + "/changed\" ]; then\n"
        "    printf 'concurrent edit\\n' > \"" + QFile::encodeName(source) + "\"\n"
        "    touch \"" + QFile::encodeName(directory.path()) + "/changed\"\n"
        "  fi\n"
        "done\nexit \"$status\"\n";
    if (!writeFile(wrapperPath, wrapper)
        || !QFile::setPermissions(wrapperPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        return 1;
    }

    ProjectVersioningStore changingStore(projectRoot, historyDirectory, wrapperPath);
    const auto changed = changingStore.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    const auto history = store.history();
    if (!expect(!changed.succeeded && !changed.error.isEmpty(),
                "A project changed during capture must report failure.")
        || !expect(history.succeeded && history.checkpoints.size() == 1
                       && history.checkpoints.constFirst().id == initial.checkpointId,
                   "An incomplete candidate must never be published in checkpoint history.")) {
        return 1;
    }
    const auto retry = store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    return expect(retry.succeeded && store.history().checkpoints.size() == 2,
                  "A stable retry must succeed after a discarded candidate.") ? 0 : 1;
#else
    return 0;
#endif
}

int runCheckpointPreservesExactBytesTest()
{
    QTemporaryDir directory;
    const QString projectRoot = directory.path() + QStringLiteral("/project");
    const QString historyDirectory = directory.path() + QStringLiteral("/history");
    const QByteArray source = QByteArrayLiteral("survey cave\r\nendsurvey\r\n");
    if (!directory.isValid() || !QDir().mkpath(projectRoot)
        || !writeFile(projectRoot + QStringLiteral("/survey.th"), source)
        || !writeFile(projectRoot + QStringLiteral("/.gitattributes"), QByteArrayLiteral("*.th text eol=lf\n"))) {
        return 1;
    }

    ProjectVersioningStore store(projectRoot, historyDirectory);
    const auto checkpoint = store.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(checkpoint.succeeded, "A project with Git attributes should be checkpointed.")) {
        return 1;
    }
    QProcess readSource;
    readSource.start(QStringLiteral("git"),
                     {QStringLiteral("--git-dir=%1").arg(historyDirectory),
                      QStringLiteral("show"), checkpoint.checkpointId + QStringLiteral(":survey.th")});
    return expect(readSource.waitForFinished(5000) && readSource.exitCode() == 0
                      && readSource.readAllStandardOutput() == source,
                  "Checkpoint contents must retain the exact on-disk bytes without Git attribute conversions.") ? 0 : 1;
}

int runHistoryStoreCannotBeSharedAcrossProjectsTest()
{
    QTemporaryDir directory;
    const QString firstRoot = directory.path() + QStringLiteral("/first");
    const QString secondRoot = directory.path() + QStringLiteral("/second");
    const QString historyDirectory = directory.path() + QStringLiteral("/history");
    if (!directory.isValid() || !QDir().mkpath(firstRoot) || !QDir().mkpath(secondRoot)
        || !writeFile(firstRoot + QStringLiteral("/first.th"), QByteArrayLiteral("first project\n"))
        || !writeFile(secondRoot + QStringLiteral("/second.th"), QByteArrayLiteral("second project\n"))) {
        return 1;
    }
    ProjectVersioningStore first(firstRoot, historyDirectory);
    const auto initial = first.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(initial.succeeded, "The first project should initialize its history store.")) {
        return 1;
    }
    ProjectVersioningStore second(secondRoot, historyDirectory);
    const auto rejected = second.createCheckpoint(QStringLiteral("Automatic checkpoint"));
    if (!expect(!rejected.succeeded && !rejected.error.isEmpty(),
                "A history store bound to another project must reject writes.")
        || !expect(!second.history().succeeded,
                   "A history store bound to another project must reject history reads.")) {
        return 1;
    }
    const auto history = first.history();
    return expect(history.succeeded && history.checkpoints.size() == 1
                      && history.checkpoints.constFirst().id == initial.checkpointId,
                  "Rejecting another project must leave the existing history unchanged.") ? 0 : 1;
}

int runNestedRepositoryCannotBecomeAnIncompleteBackupTest()
{
    QTemporaryDir directory;
    const QString projectRoot = directory.path() + QStringLiteral("/project");
    const QString nested = projectRoot + QStringLiteral("/nested");
    const QString nestedHistory = directory.path() + QStringLiteral("/nested-history");
    const QString historyDirectory = directory.path() + QStringLiteral("/history");
    if (!directory.isValid() || !QDir().mkpath(nested)
        || !writeFile(nested + QStringLiteral("/survey.th"), QByteArrayLiteral("nested survey\n"))) {
        return 1;
    }
    QProcess initialize;
    initialize.start(QStringLiteral("git"),
                     {QStringLiteral("init"), QStringLiteral("--quiet"), nested});
    if (!initialize.waitForFinished(5000) || initialize.exitCode() != 0) {
        return 1;
    }
    QProcess commit;
    commit.start(QStringLiteral("git"),
                 {QStringLiteral("-C"), nested,
                  QStringLiteral("-c"), QStringLiteral("user.name=Test"),
                  QStringLiteral("-c"), QStringLiteral("user.email=test@example.invalid"),
                  QStringLiteral("-c"), QStringLiteral("commit.gpgsign=false"),
                  QStringLiteral("-c"), QStringLiteral("core.hooksPath=%1").arg(nestedHistory),
                  QStringLiteral("commit"), QStringLiteral("--allow-empty"),
                  QStringLiteral("--quiet"), QStringLiteral("-m"), QStringLiteral("Fixture")});
    if (!commit.waitForFinished(5000) || commit.exitCode() != 0) {
        return 1;
    }
    ProjectVersioningStore store(projectRoot, historyDirectory);
    const auto result = store.createCheckpoint(QStringLiteral("Manual checkpoint"));
    return expect(!result.succeeded && !result.error.isEmpty(),
                  "Nested Git repositories must not silently become gitlinks instead of backed-up contents.") ? 0 : 1;
}

int runHistoryRemovalPreservesProjectAndAllowsRestartTest()
{
    QTemporaryDir directory;
    const QString root = directory.path() + QStringLiteral("/project");
    const QString historyPath = directory.path() + QStringLiteral("/history");
    if (!directory.isValid() || !QDir().mkpath(root)
        || !writeFile(root + QStringLiteral("/survey.th"), QByteArrayLiteral("keep source\n"))) {
        return 1;
    }
    ProjectVersioningStore store(root, historyPath);
    const auto first = store.createCheckpoint(QStringLiteral("Manual checkpoint"));
    if (!expect(first.succeeded, "History should exist before removal.")) {
        return 1;
    }
    ProjectVersioningStore wrongProject(directory.path(), historyPath);
    if (!expect(!wrongProject.removeHistory().succeeded,
                "History removal must reject an unrelated project root.")) {
        return 1;
    }
    const auto removed = store.removeHistory();
    QFile source(root + QStringLiteral("/survey.th"));
    if (!expect(removed.succeeded && !QFileInfo::exists(historyPath)
                    && store.history().checkpoints.isEmpty(),
                "Explicit removal should delete the isolated local history.")
        || !expect(source.open(QIODevice::ReadOnly) && source.readAll() == QByteArrayLiteral("keep source\n"),
                   "Deleting history must not touch project source files.")) {
        return 1;
    }
    return expect(store.createCheckpoint(QStringLiteral("Manual checkpoint")).succeeded,
                  "A new history must be possible after explicit removal.") ? 0 : 1;
}
}

int main()
{
    return runCheckpointCreatesIsolatedHistoryTest()
        || runExistingGitRepositorySafetyTest()
        || runNewIgnoreRuleRemovesPreviouslyIncludedFileTest()
        || runInheritedGitIndexIsIgnoredTest()
        || runChangingProjectDoesNotPublishPartialCheckpointTest()
        || runCheckpointPreservesExactBytesTest()
        || runHistoryStoreCannotBeSharedAcrossProjectsTest()
        || runNestedRepositoryCannotBecomeAnIncompleteBackupTest()
        || runHistoryRemovalPreservesProjectAndAllowsRestartTest();
}
