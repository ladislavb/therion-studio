#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QVector>

struct ThreeDViewerLoxCorpusFixture
{
    QString rowName;
    QString relativePath;
    bool expectedNestedSurvey = false;
    bool expectedMesh = false;
    bool expectedSurfaceShot = false;
    bool expectedDuplicateShot = false;
    bool expectedSplayShot = false;
    bool expectedStationFlag = false;
};

struct ResolvedThreeDViewerLoxCorpusFixture
{
    ThreeDViewerLoxCorpusFixture fixture;
    QString absolutePath;
    bool available = false;
};

inline QVector<ThreeDViewerLoxCorpusFixture> knownThreeDViewerLoxCorpusFixtures()
{
    return {
        {QStringLiteral("1303"),
         QStringLiteral("babice/01_zadni_pole/1303_dvanactka/_output/1303.lox"),
         true,
         true,
         false,
         false,
         true,
         false},
        {QStringLiteral("1303-1974"),
         QStringLiteral("babice/01_zadni_pole/1303_dvanactka/_output/1303_1974.lox"),
         true,
         true,
         false,
         false,
         false,
         false},
        {QStringLiteral("1318"),
         QStringLiteral("babice/01_zadni_pole/1318_vetrna_propast/_output/1318.lox"),
         true,
         true,
         false,
         true,
         false,
         false},
        {QStringLiteral("1319"),
         QStringLiteral("babice/01_zadni_pole/1319_devitka/_output/1319.lox"),
         true,
         true,
         false,
         false,
         false,
         false},
        {QStringLiteral("zadni-pole"),
         QStringLiteral("babice/01_zadni_pole/_output/zadni_pole.lox"),
         true,
         true,
         true,
         true,
         true,
         true},
        {QStringLiteral("1313"),
         QStringLiteral("babice/03_skalky/1313_babicka/_output/1313.lox"),
         true,
         true,
         false,
         false,
         false,
         false},
        {QStringLiteral("1313-II"),
         QStringLiteral("babice/03_skalky/1313_babicka_II/_output/1313_II.lox"),
         true,
         true,
         false,
         false,
         false,
         false},
        {QStringLiteral("skalky"),
         QStringLiteral("babice/03_skalky/_output/skalky.lox"),
         true,
         true,
         false,
         false,
         false,
         false},
        {QStringLiteral("babice"),
         QStringLiteral("babice/_output/babice.lox"),
         true,
         true,
         true,
         true,
         false,
         true},
        {QStringLiteral("1302"),
         QStringLiteral("clopy/_output/1302.lox"),
         true,
         true,
         false,
         false,
         false,
         false},
    };
}

inline QString normalizedThreeDViewerLoxCorpusRoot(const QString &rootPath)
{
    const QDir root(rootPath);
    const QString canonicalPath = QFileInfo(root.absolutePath()).canonicalFilePath();
    return QDir::cleanPath(canonicalPath.isEmpty() ? root.absolutePath() : canonicalPath);
}

inline QVector<ResolvedThreeDViewerLoxCorpusFixture> resolveThreeDViewerLoxCorpusFixtures(const QString &rootPath)
{
    const QDir root(normalizedThreeDViewerLoxCorpusRoot(rootPath));
    QVector<ResolvedThreeDViewerLoxCorpusFixture> resolved;
    const QVector<ThreeDViewerLoxCorpusFixture> fixtures = knownThreeDViewerLoxCorpusFixtures();
    resolved.reserve(fixtures.size());
    for (const ThreeDViewerLoxCorpusFixture &fixture : fixtures) {
        const QString absolutePath = QDir::cleanPath(root.absoluteFilePath(fixture.relativePath));
        resolved.push_back({fixture, absolutePath, QFileInfo(absolutePath).isFile()});
    }
    return resolved;
}
