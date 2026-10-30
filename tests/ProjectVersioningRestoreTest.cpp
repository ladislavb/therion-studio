#include "../src/app/ProjectVersioningStore.h"

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QtTest>

using namespace TherionStudio;
using namespace Qt::StringLiterals;

namespace
{
bool writeFile(const QString &path, const QByteArray &bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class ProjectVersioningRestoreTest final : public QObject
{
    Q_OBJECT
private slots:
    void restoresWholeProjectAndCanReturnToSafetyCheckpoint()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString history = temporary.filePath(QStringLiteral("history"));
        QVERIFY(writeFile(root + "/survey.th"_L1, "old\r\n"));
        QVERIFY(writeFile(root + "/sub/map.th2"_L1, QByteArray("binary\0data", 11)));
        QVERIFY(writeFile(root + "/.git/marker"_L1, "project git"));
        QVERIFY(writeFile(root + "/output/export.lox"_L1, "export"));
        ProjectVersioningStore store(root, history);
        const auto first = store.createCheckpoint(QStringLiteral("First"));
        QVERIFY2(first.succeeded, qPrintable(first.error));
        QVERIFY(writeFile(root + "/survey.th"_L1, "new\r\n"));
        QVERIFY(QFile::remove(root + "/sub/map.th2"_L1));
        QVERIFY(writeFile(root + "/new.th"_L1, "new file"));
        const auto restored = store.restoreCheckpoint(first.checkpointId);
        QVERIFY2(restored.succeeded, qPrintable(restored.error));
        QCOMPARE(restored.addedFiles, QStringList{"sub/map.th2"_L1});
        QCOMPARE(restored.changedFiles, QStringList{"survey.th"_L1});
        QCOMPARE(restored.removedFiles, QStringList{"new.th"_L1});
        QCOMPARE(readFile(root + "/survey.th"_L1), QByteArray("old\r\n"));
        QCOMPARE(readFile(root + "/sub/map.th2"_L1), QByteArray("binary\0data", 11));
        QVERIFY(!QFileInfo::exists(root + "/new.th"_L1));
        QCOMPARE(readFile(root + "/.git/marker"_L1), QByteArray("project git"));
        QCOMPARE(readFile(root + "/output/export.lox"_L1), QByteArray("export"));
        QVERIFY(!restored.safetyCheckpointId.isEmpty());
        const auto forward = store.restoreCheckpoint(restored.safetyCheckpointId);
        QVERIFY2(forward.succeeded, qPrintable(forward.error));
        QCOMPARE(readFile(root + "/survey.th"_L1), QByteArray("new\r\n"));
        QCOMPARE(readFile(root + "/new.th"_L1), QByteArray("new file"));
        QVERIFY(!QFileInfo::exists(root + "/sub/map.th2"_L1));
        const auto unchanged = store.createCheckpoint(QStringLiteral("No changes"));
        QVERIFY2(unchanged.succeeded, qPrintable(unchanged.error));
        QCOMPARE(unchanged.checkpointId, store.history().checkpoints.first().id);
    }

    void refusesIgnoredCollisionInvalidRevisionAndBusyStore()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString history = temporary.filePath(QStringLiteral("history"));
        QVERIFY(writeFile(root + "/keep.th"_L1, "old"));
        ProjectVersioningStore store(root, history);
        const auto first = store.createCheckpoint(QStringLiteral("First"));
        QVERIFY(first.succeeded);
        QVERIFY(writeFile(root + "/.gitignore"_L1, "keep.th\n"));
        QVERIFY(writeFile(root + "/keep.th"_L1, "private"));
        const auto collision = store.restoreCheckpoint(first.checkpointId);
        QVERIFY(!collision.succeeded);
        QVERIFY(!collision.error.isEmpty());
        QCOMPARE(readFile(root + "/keep.th"_L1), QByteArray("private"));
        QVERIFY(!store.restoreCheckpoint(QStringLiteral("--all")).succeeded);
        QCOMPARE(readFile(root + "/keep.th"_L1), QByteArray("private"));
        QLockFile lock(history + ".lock"_L1);
        QVERIFY(lock.tryLock());
        QVERIFY(!store.restoreCheckpoint(first.checkpointId).succeeded);
    }

    void refusesSymlinkParentAndWrongProject()
    {
#ifdef Q_OS_UNIX
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString history = temporary.filePath(QStringLiteral("history"));
        const QString outside = temporary.filePath(QStringLiteral("outside"));
        QVERIFY(writeFile(root + "/sub/map.th2"_L1, "old"));
        QVERIFY(writeFile(outside + "/map.th2"_L1, "outside"));
        ProjectVersioningStore store(root, history);
        const auto first = store.createCheckpoint(QStringLiteral("First"));
        QVERIFY(first.succeeded);
        QVERIFY(QFile::remove(root + "/sub/map.th2"_L1));
        QVERIFY(QDir(root).rmdir("sub"_L1));
        QVERIFY(QFile::link(outside, root + "/sub"_L1));
        const auto result = store.restoreCheckpoint(first.checkpointId);
        QVERIFY(!result.succeeded);
        QCOMPARE(readFile(outside + "/map.th2"_L1), QByteArray("outside"));
        ProjectVersioningStore wrong(outside, history);
        QVERIFY(!wrong.restoreCheckpoint(first.checkpointId).succeeded);
#endif
    }

    void interruptedRestoreRequiresSafetyAndCanRecover()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString history = temporary.filePath(QStringLiteral("history"));
        QVERIFY(writeFile(root + "/survey.th"_L1, "old"));
        ProjectVersioningStore store(root, history);
        const auto first = store.createCheckpoint(QStringLiteral("First"));
        QVERIFY(first.succeeded);
        QVERIFY(writeFile(root + "/survey.th"_L1, "before interruption"));
        const auto safety = store.createCheckpoint(QStringLiteral("Safety checkpoint before restore"));
        QVERIFY(safety.succeeded);
        QVERIFY(writeFile(history + "/restore-pending"_L1, safety.checkpointId.toLatin1() + '\n'));
        QVERIFY(writeFile(root + "/survey.th"_L1, "partial restore"));
        QVERIFY(!store.createCheckpoint(QStringLiteral("Automatic checkpoint")).succeeded);
        QVERIFY(!store.restoreCheckpoint(first.checkpointId).succeeded);
        const auto recovered = store.restoreCheckpoint(safety.checkpointId);
        QVERIFY2(recovered.succeeded, qPrintable(recovered.error));
        QCOMPARE(readFile(root + "/survey.th"_L1), QByteArray("before interruption"));
        QVERIFY(!QFileInfo::exists(history + "/restore-pending"_L1));
        QCOMPARE(store.history().currentCheckpointId, safety.checkpointId);
    }

    void failedPublishRollsBackEveryAppliedFile()
    {
#ifdef Q_OS_UNIX
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString history = temporary.filePath(QStringLiteral("history"));
        QVERIFY(writeFile(root + "/survey.th"_L1, "old"));
        QVERIFY(writeFile(root + "/removed.th"_L1, "restore me"));
        ProjectVersioningStore store(root, history);
        const auto first = store.createCheckpoint(QStringLiteral("First"));
        QVERIFY(first.succeeded);
        QVERIFY(writeFile(root + "/survey.th"_L1, "new"));
        QVERIFY(QFile::remove(root + "/removed.th"_L1));
        QVERIFY(writeFile(root + "/added.th"_L1, "keep me"));
        const QString wrapper = temporary.filePath(QStringLiteral("fail-publish-git"));
        const QByteArray script = "#!/bin/sh\nfor arg in \"$@\"; do\n"
            "case \"$arg\" in 'Restored checkpoint '*) echo 'simulated publish failure' >&2; exit 1;; esac\n"
            "done\nexec \"" + QFile::encodeName(QStandardPaths::findExecutable(QStringLiteral("git"))) + "\" \"$@\"\n";
        QVERIFY(writeFile(wrapper, script));
        QVERIFY(QFile::setPermissions(wrapper, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        ProjectVersioningStore failing(root, history, wrapper);
        const auto failed = failing.restoreCheckpoint(first.checkpointId);
        QVERIFY(!failed.succeeded);
        QVERIFY(failed.error.contains(QStringLiteral("simulated publish failure")));
        QCOMPARE(readFile(root + "/survey.th"_L1), QByteArray("new"));
        QCOMPARE(readFile(root + "/added.th"_L1), QByteArray("keep me"));
        QVERIFY(!QFileInfo::exists(root + "/removed.th"_L1));
        QVERIFY(!QFileInfo::exists(history + "/restore-pending"_L1));
        QVERIFY(store.createCheckpoint(QStringLiteral("Unchanged")).succeeded);
        QVERIFY(writeFile(history + "/restore-pending"_L1, first.checkpointId.toLatin1() + '\n'));
        QVERIFY(!failing.restoreCheckpoint(first.checkpointId).succeeded);
        QVERIFY(QFileInfo::exists(history + "/restore-pending"_L1));
        QVERIFY(!store.createCheckpoint(QStringLiteral("Still blocked")).succeeded);
        const auto recovered = store.restoreCheckpoint(first.checkpointId);
        QVERIFY2(recovered.succeeded, qPrintable(recovered.error));
        QCOMPARE(readFile(root + "/survey.th"_L1), QByteArray("old"));
        QVERIFY(!QFileInfo::exists(history + "/restore-pending"_L1));
#endif
    }

    void restoresExecutableFlag()
    {
#ifdef Q_OS_UNIX
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString history = temporary.filePath(QStringLiteral("history"));
        const QString path = root + "/script.sh"_L1;
        QVERIFY(writeFile(path, "#!/bin/sh\n"));
        const auto ordinary = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
        QVERIFY(QFile::setPermissions(path, ordinary));
        ProjectVersioningStore store(root, history);
        const auto first = store.createCheckpoint(QStringLiteral("Non-executable"));
        QVERIFY(first.succeeded);
        QVERIFY(QFile::setPermissions(path, ordinary | QFileDevice::ExeOwner));
        const auto second = store.createCheckpoint(QStringLiteral("Executable"));
        QVERIFY(second.succeeded);
        QVERIFY(second.checkpointId != first.checkpointId);
        const auto restored = store.restoreCheckpoint(first.checkpointId);
        QVERIFY2(restored.succeeded, qPrintable(restored.error));
        QVERIFY(!QFileInfo(path).permissions().testFlag(QFileDevice::ExeOwner));
        const auto executable = store.restoreCheckpoint(second.checkpointId);
        QVERIFY2(executable.succeeded, qPrintable(executable.error));
        QVERIFY(QFileInfo(path).permissions().testFlag(QFileDevice::ExeOwner));
#endif
    }
};

QTEST_GUILESS_MAIN(ProjectVersioningRestoreTest)
#include "ProjectVersioningRestoreTest.moc"
