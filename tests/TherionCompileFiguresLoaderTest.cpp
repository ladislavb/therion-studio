#include "../src/app/TherionCompileFiguresLoader.h"

#include <QSignalSpy>
#include <memory>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
const QString kSurveyLine = QStringLiteral(
    "xth_cp_data_tree_insert 18 16 2 inner inner.cave \"Inner\" \"Inner\" \"length: 15.00m\"\n");

TherionCompileResult compileResult(const QString &xtherionDataPath, bool succeeded)
{
    TherionCompileResult result;
    result.configPath = QStringLiteral("thconfig");
    result.startedAt = QDateTime::currentDateTime();
    result.finishedAt = result.startedAt;
    result.succeeded = succeeded;
    result.xtherionDataPath = xtherionDataPath;
    return result;
}

class TherionCompileFiguresLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void loadsSurveysOfGoodCompile();
    void keepsFiguresWhenLaterCompileFails();
    void keepsFiguresWhenLaterCompileWritesNoTree();
    void staysUnavailableAfterFirstCompileFails();
    void clearsOnInvalidResult();
    void leavesDatabaseUnavailableWithoutExport();

private:
    QString writeDataFile(const QString &name, const QString &text);
    void loadAndWait(TherionCompileFiguresLoader &loader, const TherionCompileResult &result);

    std::unique_ptr<QTemporaryDir> dir_;
};

void TherionCompileFiguresLoaderTest::init()
{
    dir_ = std::make_unique<QTemporaryDir>();
    QVERIFY(dir_->isValid());
}

QString TherionCompileFiguresLoaderTest::writeDataFile(const QString &name, const QString &text)
{
    const QString path = dir_->filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(text.toUtf8());
    return path;
}

void TherionCompileFiguresLoaderTest::loadAndWait(TherionCompileFiguresLoader &loader,
                                                    const TherionCompileResult &result)
{
    QSignalSpy spy(&loader, &TherionCompileFiguresLoader::figuresChanged);
    loader.updateFromCompile(result);
    QVERIFY(spy.count() > 0 || spy.wait());
}

void TherionCompileFiguresLoaderTest::loadsSurveysOfGoodCompile()
{
    TherionCompileFiguresLoader loader;
    const TherionCompileResult result = compileResult(writeDataFile(QStringLiteral("a.dat"), kSurveyLine), true);
    loadAndWait(loader, result);

    const TherionCompileFigures &statistics = loader.figures();
    QVERIFY(statistics.isAvailable());
    QVERIFY(!statistics.laterCompileFailed);
    QCOMPARE(statistics.compile.startedAt, result.startedAt);
    QCOMPARE(statistics.surveysByFullName.value(QStringLiteral("inner.cave")).title, QStringLiteral("Inner"));
}

void TherionCompileFiguresLoaderTest::keepsFiguresWhenLaterCompileFails()
{
    TherionCompileFiguresLoader loader;
    loadAndWait(loader, compileResult(writeDataFile(QStringLiteral("a.dat"), kSurveyLine), true));
    loadAndWait(loader, compileResult(QString(), false));

    QVERIFY(loader.figures().isAvailable());
    QVERIFY(loader.figures().laterCompileFailed);
    QVERIFY(loader.figures().surveysByFullName.contains(QStringLiteral("inner.cave")));
}

void TherionCompileFiguresLoaderTest::keepsFiguresWhenLaterCompileWritesNoTree()
{
    TherionCompileFiguresLoader loader;
    loadAndWait(loader, compileResult(writeDataFile(QStringLiteral("a.dat"), kSurveyLine), true));
    loadAndWait(loader, compileResult(writeDataFile(QStringLiteral("b.dat"), QStringLiteral("xth_cp_data_tree_create\n")), true));

    QVERIFY(loader.figures().laterCompileFailed);
    QVERIFY(loader.figures().surveysByFullName.contains(QStringLiteral("inner.cave")));
}

void TherionCompileFiguresLoaderTest::staysUnavailableAfterFirstCompileFails()
{
    TherionCompileFiguresLoader loader;
    QSignalSpy spy(&loader, &TherionCompileFiguresLoader::figuresChanged);
    loader.updateFromCompile(compileResult(QString(), false));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!loader.figures().isAvailable());
}

void TherionCompileFiguresLoaderTest::clearsOnInvalidResult()
{
    TherionCompileFiguresLoader loader;
    loadAndWait(loader, compileResult(writeDataFile(QStringLiteral("a.dat"), kSurveyLine), true));
    loadAndWait(loader, TherionCompileResult());

    QVERIFY(!loader.figures().isAvailable());
    QVERIFY(loader.figures().surveysByFullName.isEmpty());
}
void TherionCompileFiguresLoaderTest::leavesDatabaseUnavailableWithoutExport()
{
    TherionCompileFiguresLoader loader;
    TherionCompileResult result = compileResult(writeDataFile(QStringLiteral("a.dat"), kSurveyLine), true);
    result.databasePaths = {dir_->filePath(QStringLiteral("missing.sql"))};
    loadAndWait(loader, result);

    QVERIFY(loader.figures().isAvailable());
    QVERIFY(!loader.figures().database.isAvailable());
}
}

int runTherionCompileFiguresLoaderTest(int argc, char **argv)
{
    TherionCompileFiguresLoaderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionCompileFiguresLoaderTest.moc"
