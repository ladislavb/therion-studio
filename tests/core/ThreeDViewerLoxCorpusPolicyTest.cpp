#include "ThreeDViewerLoxCorpusFixtures.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace
{
bool writeTemporaryFixture(const QString &rootPath, const QString &relativePath)
{
    const QString absolutePath = QDir(rootPath).absoluteFilePath(relativePath);
    if (!QDir().mkpath(QFileInfo(absolutePath).absolutePath())) {
        return false;
    }
    QFile file(absolutePath);
    return file.open(QIODevice::WriteOnly) && file.write("fixture") == 7;
}

class ThreeDViewerLoxCorpusPolicyTest : public QObject
{
    Q_OBJECT

private slots:
    void ignoresUnrelatedFiles();
    void resolvesPartialCorpusIndependently();
    void normalizesEquivalentRoots();
};

void ThreeDViewerLoxCorpusPolicyTest::ignoresUnrelatedFiles()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    QVERIFY(writeTemporaryFixture(corpus.path(), QStringLiteral("unrelated/project.lox")));

    const QVector<ResolvedThreeDViewerLoxCorpusFixture> resolved =
        resolveThreeDViewerLoxCorpusFixtures(corpus.path());

    QCOMPARE(resolved.size(), knownThreeDViewerLoxCorpusFixtures().size());
    for (const ResolvedThreeDViewerLoxCorpusFixture &fixture : resolved) {
        QVERIFY(!fixture.available);
    }
}

void ThreeDViewerLoxCorpusPolicyTest::resolvesPartialCorpusIndependently()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QVector<ThreeDViewerLoxCorpusFixture> knownFixtures = knownThreeDViewerLoxCorpusFixtures();
    const ThreeDViewerLoxCorpusFixture expectedFixture = knownFixtures.constLast();
    QVERIFY(writeTemporaryFixture(corpus.path(), expectedFixture.relativePath));

    const QVector<ResolvedThreeDViewerLoxCorpusFixture> resolved =
        resolveThreeDViewerLoxCorpusFixtures(corpus.path());

    int availableCount = 0;
    for (const ResolvedThreeDViewerLoxCorpusFixture &fixture : resolved) {
        if (!fixture.available) {
            continue;
        }
        ++availableCount;
        QCOMPARE(fixture.fixture.rowName, expectedFixture.rowName);
        QCOMPARE(fixture.fixture.relativePath, expectedFixture.relativePath);
        const QString normalizedRoot =
            QDir::fromNativeSeparators(normalizedThreeDViewerLoxCorpusRoot(corpus.path()));
        const QString normalizedFixturePath = QDir::fromNativeSeparators(fixture.absolutePath);
        QVERIFY(normalizedFixturePath.startsWith(normalizedRoot + QLatin1Char('/')));
    }
    QCOMPARE(availableCount, 1);
}

void ThreeDViewerLoxCorpusPolicyTest::normalizesEquivalentRoots()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const ThreeDViewerLoxCorpusFixture fixture = knownThreeDViewerLoxCorpusFixtures().first();
    QVERIFY(writeTemporaryFixture(corpus.path(), fixture.relativePath));

    const QString relativeRoot = QDir::current().relativeFilePath(corpus.path());
    const QVector<ResolvedThreeDViewerLoxCorpusFixture> fromAbsolute =
        resolveThreeDViewerLoxCorpusFixtures(corpus.path());
    const QVector<ResolvedThreeDViewerLoxCorpusFixture> fromRelative =
        resolveThreeDViewerLoxCorpusFixtures(relativeRoot);

    QCOMPARE(fromAbsolute.size(), fromRelative.size());
    for (qsizetype index = 0; index < fromAbsolute.size(); ++index) {
        QCOMPARE(fromAbsolute.at(index).fixture.relativePath, fromRelative.at(index).fixture.relativePath);
        QCOMPARE(fromAbsolute.at(index).absolutePath, fromRelative.at(index).absolutePath);
        QCOMPARE(fromAbsolute.at(index).available, fromRelative.at(index).available);
    }
}
}

int runThreeDViewerLoxCorpusPolicyTest(int argc, char **argv)
{
    ThreeDViewerLoxCorpusPolicyTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerLoxCorpusPolicyTest.moc"
