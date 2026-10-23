#include "../src/app/TherionCompileResult.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("x") == 1;
}

class TherionCompileResultTest : public QObject
{
    Q_OBJECT

private slots:
    void findsWrittenFilesInEachMessageForm();
    void skipsXtherionFileMessage();
    void detectsXtherionFileMessage();
    void recordsDatabaseAndXtherionFilesOfTheCompile();
    void ignoresFilesTheCompileDidNotReportWriting();
    void abandonedRunKeepsTheLatestResult();
    void clearForgetsTheLatestResult();
    void sourceSavedAfterTheCompileStartedIsOutOfDate();
    void compileArgumentsAddXtherionOptionOnce();
    void detectsXtherionFileMessageAfterReadingDots();
};

void TherionCompileResultTest::detectsXtherionFileMessageAfterReadingDots()
{
    QVERIFY(therionWroteXtherionData(QStringLiteral("configuration file: thconfig\nreading ...writing xtherion file ... done\n")));
}

void TherionCompileResultTest::compileArgumentsAddXtherionOptionOnce()
{
    QCOMPARE(therionCompileArguments({QStringLiteral("thconfig")}),
             QStringList({QStringLiteral("-x"), QStringLiteral("thconfig")}));
    QCOMPARE(therionCompileArguments({}), QStringList{QStringLiteral("-x")});
    const QStringList userArguments{QStringLiteral("-q"), QStringLiteral("-x"), QStringLiteral("thconfig")};
    QCOMPARE(therionCompileArguments(userArguments), userArguments);
}

void TherionCompileResultTest::findsWrittenFilesInEachMessageForm()
{
    const QDir workingDir(QStringLiteral("/project"));
    const QString output = QStringLiteral(
        "writing cave.sql ... done\n"
        "writing output/plan.pdf ...\r\n"
        "writing model.lox ...\n"
        "\n\nwriting debug.3d\n"
        "writing /abs/cave map.svg ... done\n");

    const QStringList paths = therionWrittenFilePaths(output, workingDir.absolutePath());
    QCOMPARE(paths,
             QStringList({workingDir.filePath(QStringLiteral("cave.sql")),
                          workingDir.filePath(QStringLiteral("output/plan.pdf")),
                          workingDir.filePath(QStringLiteral("model.lox")),
                          workingDir.filePath(QStringLiteral("debug.3d")),
                          QStringLiteral("/abs/cave map.svg")}));
}

void TherionCompileResultTest::skipsXtherionFileMessage()
{
    const QString output = QStringLiteral(
        "writing xtherion file ... done\n"
        "writing xtherion file -- .xtherion.dat\n");
    QVERIFY(therionWrittenFilePaths(output, QStringLiteral("/project")).isEmpty());
}

void TherionCompileResultTest::detectsXtherionFileMessage()
{
    QVERIFY(therionWroteXtherionData(QStringLiteral("reading ... done\nwriting xtherion file ... done\n")));
    QVERIFY(!therionWroteXtherionData(QStringLiteral("writing cave.sql ... done\n")));
}

void TherionCompileResultTest::recordsDatabaseAndXtherionFilesOfTheCompile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QDir workingDir(tempDir.path());
    QVERIFY(writeFile(workingDir.filePath(QStringLiteral("cave.sql"))));
    QVERIFY(writeFile(workingDir.filePath(QStringLiteral("plan.pdf"))));
    QVERIFY(writeFile(workingDir.filePath(QStringLiteral(".xtherion.dat"))));

    TherionCompileResultRecorder recorder;
    QSignalSpy changed(&recorder, &TherionCompileResultRecorder::latestChanged);
    recorder.begin(workingDir.filePath(QStringLiteral("thconfig")), workingDir.absolutePath());
    recorder.appendStandardOutput(QStringLiteral("writing cave.s"));
    recorder.appendStandardOutput(QStringLiteral("ql ... done\nwriting plan.pdf ... done\n"));
    recorder.appendStandardOutput(QStringLiteral("writing xtherion file ... done\n"));
    recorder.finish(true);

    QCOMPARE(changed.count(), 1);
    const TherionCompileResult &result = recorder.latest();
    QVERIFY(result.isValid());
    QVERIFY(result.succeeded);
    QCOMPARE(result.configPath, workingDir.filePath(QStringLiteral("thconfig")));
    // Paths are canonical, as for compiler source links.
    QCOMPARE(result.databasePaths,
             QStringList({QFileInfo(workingDir.filePath(QStringLiteral("cave.sql"))).canonicalFilePath()}));
    QCOMPARE(result.xtherionDataPath,
             QFileInfo(workingDir.filePath(QStringLiteral(".xtherion.dat"))).canonicalFilePath());
}

void TherionCompileResultTest::ignoresFilesTheCompileDidNotReportWriting()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QDir workingDir(tempDir.path());
    // Left over from earlier compiles.
    QVERIFY(writeFile(workingDir.filePath(QStringLiteral("old.sql"))));
    QVERIFY(writeFile(workingDir.filePath(QStringLiteral(".xtherion.dat"))));

    TherionCompileResultRecorder recorder;
    recorder.begin(workingDir.filePath(QStringLiteral("thconfig")), workingDir.absolutePath());
    recorder.appendStandardOutput(QStringLiteral("writing missing.sql ... done\n"));
    recorder.finish(false);

    const TherionCompileResult &result = recorder.latest();
    QVERIFY(result.isValid());
    QVERIFY(!result.succeeded);
    QVERIFY(result.databasePaths.isEmpty());
    QVERIFY(result.xtherionDataPath.isEmpty());
}

void TherionCompileResultTest::abandonedRunKeepsTheLatestResult()
{
    TherionCompileResultRecorder recorder;
    recorder.begin(QStringLiteral("/project/thconfig"), QStringLiteral("/project"));
    recorder.finish(true);
    const QDateTime finishedAt = recorder.latest().finishedAt;

    QSignalSpy changed(&recorder, &TherionCompileResultRecorder::latestChanged);
    recorder.begin(QStringLiteral("/project/thconfig"), QStringLiteral("/project"));
    recorder.abandon();
    recorder.finish(true);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(recorder.latest().finishedAt, finishedAt);
}

void TherionCompileResultTest::clearForgetsTheLatestResult()
{
    TherionCompileResultRecorder recorder;
    recorder.begin(QStringLiteral("/project/thconfig"), QStringLiteral("/project"));
    recorder.finish(true);

    QSignalSpy changed(&recorder, &TherionCompileResultRecorder::latestChanged);
    recorder.clear();
    QCOMPARE(changed.count(), 1);
    QVERIFY(!recorder.latest().isValid());
    recorder.clear();
    QCOMPARE(changed.count(), 1);
}

void TherionCompileResultTest::sourceSavedAfterTheCompileStartedIsOutOfDate()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString source = QDir(tempDir.path()).filePath(QStringLiteral("cave.th"));
    QVERIFY(writeFile(source));
    const QDateTime saved = QFileInfo(source).lastModified();

    TherionCompileResult result;
    result.startedAt = saved.addSecs(10);
    result.finishedAt = saved.addSecs(20);
    QVERIFY(!therionCompileResultIsOutOfDate(result, {source}));

    result.startedAt = saved.addSecs(-10);
    QVERIFY(therionCompileResultIsOutOfDate(result, {source}));

    QVERIFY(therionCompileResultIsOutOfDate(TherionCompileResult(), {}));
}
}

int runTherionCompileResultTest(int argc, char **argv)
{
    TherionCompileResultTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionCompileResultTest.moc"
