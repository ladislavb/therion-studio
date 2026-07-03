#include "../src/app/TherionRunnerOutputLinker.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionRunnerOutputLinkerTest : public QObject
{
    Q_OBJECT

private slots:
    void linksRelativeCompilerSourceLocations();
    void detectsCompilerErrors();
    void detectsCompilerWarnings();
    void ignoresLinesWithoutSourceLineNumbers();
    void roundTripsSourceLocationUrls();
};

void TherionRunnerOutputLinkerTest::linksRelativeCompilerSourceLocations()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QDir workingDir(tempDir.path());
    const QString output =
        QStringLiteral("[stderr] /opt/homebrew/bin/therion: error --\n"
                       "02_clopy_a_okoli/1302_ve_clopech/data/clopy01.th2 [64] -- orientation not valid with type wall\n");

    const QVector<TherionRunnerOutputLinker::Link> links =
        TherionRunnerOutputLinker::sourceLinksForText(output, workingDir.absolutePath());
    QCOMPARE(links.size(), 1);
    QCOMPARE(output.mid(links.first().start, links.first().length),
             QStringLiteral("02_clopy_a_okoli/1302_ve_clopech/data/clopy01.th2"));
    QCOMPARE(links.first().location.lineNumber, 64);
    QCOMPARE(links.first().location.path,
             workingDir.filePath(QStringLiteral("02_clopy_a_okoli/1302_ve_clopech/data/clopy01.th2")));
}

void TherionRunnerOutputLinkerTest::detectsCompilerErrors()
{
    QVERIFY(TherionRunnerOutputLinker::containsCompilerError(
        QStringLiteral("/opt/homebrew/bin/therion: error --\n"
                       "survey.th2 [64] -- orientation not valid with type wall\n")));
    QVERIFY(!TherionRunnerOutputLinker::containsCompilerError(
        QStringLiteral("/opt/homebrew/bin/therion: warning -- error writing /tmp/out.lox\n")));
    QVERIFY(!TherionRunnerOutputLinker::containsCompilerError(
        QStringLiteral("checking optional fonts ... NOT INSTALLED\n")));
}

void TherionRunnerOutputLinkerTest::detectsCompilerWarnings()
{
    QVERIFY(TherionRunnerOutputLinker::containsCompilerWarning(
        QStringLiteral("[Warning: scrap outline intersects itself in scrap clopy01@1302]\n")));
    QVERIFY(TherionRunnerOutputLinker::containsCompilerWarning(
        QStringLiteral("/opt/homebrew/bin/therion: warning -- error writing /tmp/out.lox\n")));
    QVERIFY(!TherionRunnerOutputLinker::containsCompilerWarning(
        QStringLiteral("checking optional fonts ... NOT INSTALLED\n")));
}

void TherionRunnerOutputLinkerTest::ignoresLinesWithoutSourceLineNumbers()
{
    const QString output =
        QStringLiteral("configuration file: /project/babice.thconfig\n"
                       "reading source files ...\n"
                       "done\n");
    QVERIFY(TherionRunnerOutputLinker::sourceLinksForText(output, QStringLiteral("/project")).isEmpty());
}

void TherionRunnerOutputLinkerTest::roundTripsSourceLocationUrls()
{
    TherionRunnerOutputLinker::SourceLocation location;
    location.path = QStringLiteral("/project/cave map/scrap.th2");
    location.lineNumber = 12;

    const QUrl url = TherionRunnerOutputLinker::urlForSourceLocation(location);
    const TherionRunnerOutputLinker::SourceLocation decoded =
        TherionRunnerOutputLinker::sourceLocationFromUrl(url);
    QCOMPARE(decoded.path, location.path);
    QCOMPARE(decoded.lineNumber, location.lineNumber);
}
}

int runTherionRunnerOutputLinkerTest(int argc, char **argv)
{
    TherionRunnerOutputLinkerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionRunnerOutputLinkerTest.moc"
