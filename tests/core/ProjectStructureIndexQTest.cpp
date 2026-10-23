#include "../../src/core/ProjectStructureIndex.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool writeTextFile(const QString &filePath, const QString &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    const QByteArray encodedContents = contents.toUtf8();
    return file.write(encodedContents) == encodedContents.size();
}
}

class ProjectStructureIndexQTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesScrapStationNamePrefixAndSuffix()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        QVERIFY(projectDir.mkpath(QStringLiteral("maps")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input maps/map.th2\n"
                                  "  centerline\n"
                                  "    data normal from to length compass clino\n"
                                  "    pre1suf pre2suf 1 0 0\n"
                                  "    pre3 pre4 1 0 0\n"
                                  "    1 1 1 0 0\n"
                                  "  endcenterline\n"
                                  "endsurvey cave\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                              QStringLiteral(
                                  "scrap s -station-names pre suf\n"
                                  "point 0 0 station -name 1\n"
                                  "point 1 1 station -name 2\n"
                                  "point 2 2 station -name missing\n"
                                  "endscrap\n"
                                  "scrap s2 -station-names pre []\n"
                                  "point 3 3 station -name 3\n"
                                  "endscrap\n"
                                  "scrap s3 -station-names \"\" @cave\n"
                                  "point 4 4 station -name 1@cave\n"
                                  "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QStringList unknownStationReferences;
        for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
            if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownStationReference) {
                unknownStationReferences.append(diagnostic.referencedName);
            }
        }

        QVERIFY(!unknownStationReferences.contains(QStringLiteral("1")));
        QVERIFY(!unknownStationReferences.contains(QStringLiteral("2")));
        QVERIFY(!unknownStationReferences.contains(QStringLiteral("3")));
        QCOMPARE(unknownStationReferences.count(QStringLiteral("missing")), 1);
        QCOMPARE(unknownStationReferences.count(QStringLiteral("1@cave")), 1);
    }

    void reportsMapItemsThatDoNotExistWhateverTheirName()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        QVERIFY(projectDir.mkpath(QStringLiteral("maps")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey top\n"
                                  "  survey branch\n"
                                  "  endsurvey branch\n"
                                  "  input maps/map.th2\n"
                                  "  map plan\n"
                                  "    plan1\n"
                                  "    PlanMissing2\n"
                                  "  endmap\n"
                                  "  map whole\n"
                                  "    plan\n"
                                  "    WholeMissing\n"
                                  "  endmap\n"
                                  "  map bySurvey\n"
                                  "    branch\n"
                                  "  endmap\n"
                                  "  map suffixed\n"
                                  "    missing.s\n"
                                  "  endmap\n"
                                  "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                              QStringLiteral(
                                  "scrap plan1\n"
                                  "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QStringList unknownMapItems;
        QStringList unknownMapScraps;
        for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
            QVERIFY2(diagnostic.kind != ProjectIndexDiagnosticKind::MixedMapAndScrapReferences,
                     "A missing map item should not count as map or scrap content.");
            if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapItemReference) {
                unknownMapItems.append(QStringLiteral("%1:%2").arg(diagnostic.referencedName).arg(diagnostic.lineNumber));
            } else if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapScrapReference) {
                unknownMapScraps.append(diagnostic.referencedName);
            }
        }

        unknownMapItems.sort();
        QCOMPARE(unknownMapItems, QStringList({QStringLiteral("PlanMissing2:7"), QStringLiteral("WholeMissing:11")}));
        QCOMPARE(unknownMapScraps, QStringList({QStringLiteral("missing.s")}));
    }

    void reportsMapContentTherionRejects()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        QVERIFY(projectDir.mkpath(QStringLiteral("maps")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey top\n"
                                  "  survey branch\n"
                                  "  endsurvey branch\n"
                                  "  input maps/map.th2\n"
                                  "  map mixed\n"
                                  "    plan1\n"
                                  "    branch\n"
                                  "  endmap\n"
                                  "  map objects\n"
                                  "    plan1\n"
                                  "    wall1\n"
                                  "  endmap\n"
                                  "  map empty\n"
                                  "  endmap\n"
                                  "  map previews\n"
                                  "    preview above mixed\n"
                                  "  endmap\n"
                                  "  map syntax\n"
                                  "    plan1 extra\n"
                                  "    plan1 [1 x m] above\n"
                                  "    plan1 [1 2 m] Above\n"
                                  "    preview none mixed\n"
                                  "  endmap\n"
                                  "  map surveyOption -survey nowhere\n"
                                  "    plan1\n"
                                  "  endmap\n"
                                  "  map good -survey branch\n"
                                  "    plan1 [1 2 m] none\n"
                                  "    preview below mixed\n"
                                  "  endmap\n"
                                  "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                              QStringLiteral(
                                  "scrap plan1\n"
                                  "line wall -id wall1\n"
                                  "  0 0\n"
                                  "  1 1\n"
                                  "endline\n"
                                  "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QStringList findings = diagnosticSummaries(snapshot.diagnostics);
        findings.sort();
        QStringList expected = {
            QStringLiteral("mixed:branch:7"),
            QStringLiteral("invalid-object:wall1:11"),
            QStringLiteral("empty:empty:13"),
            QStringLiteral("only-previews:previews:15"),
            QStringLiteral("invalid-item:plan1:19"),
            QStringLiteral("invalid-shift:[1 x m]:20"),
            QStringLiteral("preview-type:Above:21"),
            QStringLiteral("preview-type:none:22"),
            QStringLiteral("survey:nowhere:24"),
        };
        expected.sort();
        QCOMPARE(findings, expected);
    }

    void readsMapItemShiftsOnlyWithMapsOffset()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("thconfig")),
                              QStringLiteral(
                                  "source root.th\n"
                                  "maps-offset off\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey top\n"
                                  "  input map.th2\n"
                                  "  map plan\n"
                                  "    plan1 [1 x m] Above\n"
                                  "    plan1 extra\n"
                                  "  endmap\n"
                                  "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("map.th2")),
                              QStringLiteral(
                                  "scrap plan1\n"
                                  "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QCOMPARE(diagnosticSummaries(snapshot.diagnostics), QStringList({QStringLiteral("invalid-item:plan1:5")}));
    }

    void reportsMapProjectionsAndRecursionTherionRejects()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey top\n"
                                  "  input map.th2\n"
                                  "  map mP\n"
                                  "    sP\n"
                                  "  endmap\n"
                                  "  map mE\n"
                                  "    sE\n"
                                  "  endmap\n"
                                  "  map scraps\n"
                                  "    sP\n"
                                  "    sE\n"
                                  "  endmap\n"
                                  "  map explicit -projection extended\n"
                                  "    sP\n"
                                  "  endmap\n"
                                  "  map maps\n"
                                  "    mP\n"
                                  "    mE\n"
                                  "  endmap\n"
                                  "  map self\n"
                                  "    self\n"
                                  "  endmap\n"
                                  "  map loopA\n"
                                  "    loopB\n"
                                  "  endmap\n"
                                  "  map loopB\n"
                                  "    loopA\n"
                                  "  endmap\n"
                                  "  map angles\n"
                                  "    sV\n"
                                  "    sV2\n"
                                  "  endmap\n"
                                  "  map indexed\n"
                                  "    sP\n"
                                  "    sA\n"
                                  "  endmap\n"
                                  "  map alias -proj plan\n"
                                  "    sP\n"
                                  "    sDefault\n"
                                  "  endmap\n"
                                  "  map units -projection [elevation 315 deg]\n"
                                  "    sV\n"
                                  "  endmap\n"
                                  "  map mixed\n"
                                  "    sP\n"
                                  "    mE\n"
                                  "  endmap\n"
                                  "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("map.th2")),
                              QStringLiteral(
                                  "scrap sP -projection plan\n"
                                  "endscrap\n"
                                  "scrap sDefault\n"
                                  "endscrap\n"
                                  "scrap sE -proj extended\n"
                                  "endscrap\n"
                                  "scrap sV -projection [elevation 315]\n"
                                  "endscrap\n"
                                  "scrap sV2 -projection [elevation 315.0]\n"
                                  "endscrap\n"
                                  "scrap sA -projection plan:a\n"
                                  "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QStringList findings = diagnosticSummaries(snapshot.diagnostics);
        // Which map of the loop is reported depends on where the walk starts; it is reported once.
        const int loopFindings = findings.removeAll(QStringLiteral("recursive:loopA:27"))
            + findings.removeAll(QStringLiteral("recursive:loopB:24"));
        QCOMPARE(loopFindings, 1);
        findings.sort();
        QStringList expected = {
            QStringLiteral("scrap-projection:sE:11"),
            QStringLiteral("scrap-projection:sP:14"),
            QStringLiteral("map-projection:mE:18"),
            QStringLiteral("recursive:self:21"),
            QStringLiteral("scrap-projection:sV2:31"),
            QStringLiteral("scrap-projection:sA:35"),
            QStringLiteral("mixed:mE:46"),
        };
        expected.sort();
        QCOMPARE(findings, expected);
    }

    void reportsShiftsSurveysAndPreviewsTherionRejects()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey top\n"
                                  "  survey branch\n"
                                  "  endsurvey branch\n"
                                  "  input map.th2\n"
                                  "  map mP\n"
                                  "    sP\n"
                                  "  endmap\n"
                                  "  map mE\n"
                                  "    sE\n"
                                  "  endmap\n"
                                  "  map firstShift\n"
                                  "    sP [1 2 m] above\n"
                                  "  endmap\n"
                                  "  map laterShift\n"
                                  "    sP\n"
                                  "    sP2 [0 0 m] none\n"
                                  "  endmap\n"
                                  "  map surveyFirst\n"
                                  "    branch\n"
                                  "  endmap\n"
                                  "  map surveyPlan -projection plan\n"
                                  "    branch\n"
                                  "  endmap\n"
                                  "  map surveyNone -projection none\n"
                                  "    branch\n"
                                  "  endmap\n"
                                  "  map surveyLater\n"
                                  "    mP\n"
                                  "    branch\n"
                                  "  endmap\n"
                                  "  map mN\n"
                                  "    sN\n"
                                  "  endmap\n"
                                  "  map surveyAfterNone\n"
                                  "    mN\n"
                                  "    branch\n"
                                  "  endmap\n"
                                  "  map previews\n"
                                  "    sP\n"
                                  "    preview above nothing\n"
                                  "    preview above sE\n"
                                  "    preview above branch\n"
                                  "    preview above mE\n"
                                  "    preview below mP\n"
                                  "  endmap\n"
                                  "  map shiftedMaps\n"
                                  "    mP [1 2 m] above\n"
                                  "    mP [3 4 m] below\n"
                                  "  endmap\n"
                                  "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("map.th2")),
                              QStringLiteral(
                                  "scrap sP -projection plan\n"
                                  "endscrap\n"
                                  "scrap sP2\n"
                                  "endscrap\n"
                                  "scrap sE -projection extended\n"
                                  "endscrap\n"
                                  "scrap sN -projection none\n"
                                  "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QStringList findings = diagnosticSummaries(snapshot.diagnostics);
        findings.sort();
        QStringList expected = {
            QStringLiteral("shifted:sP2:16"),
            QStringLiteral("no-projection:branch:19"),
            QStringLiteral("survey-projection:branch:25"),
            QStringLiteral("survey-projection:branch:36"),
            QStringLiteral("unknown-item:nothing:40"),
            QStringLiteral("preview-not-map:sE:41"),
            QStringLiteral("preview-not-map:branch:42"),
            QStringLiteral("map-projection:mE:43"),
        };
        expected.sort();
        QCOMPARE(findings, expected);
    }

private:
    static QStringList diagnosticSummaries(const QVector<ProjectIndexDiagnostic> &diagnostics)
    {
        static const QHash<ProjectIndexDiagnosticKind, QString> labels = {
            {ProjectIndexDiagnosticKind::MixedMapAndScrapReferences, QStringLiteral("mixed")},
            {ProjectIndexDiagnosticKind::InvalidMapObject, QStringLiteral("invalid-object")},
            {ProjectIndexDiagnosticKind::EmptyMap, QStringLiteral("empty")},
            {ProjectIndexDiagnosticKind::MapWithOnlyPreviews, QStringLiteral("only-previews")},
            {ProjectIndexDiagnosticKind::InvalidMapItem, QStringLiteral("invalid-item")},
            {ProjectIndexDiagnosticKind::InvalidMapShift, QStringLiteral("invalid-shift")},
            {ProjectIndexDiagnosticKind::UnsupportedMapPreviewType, QStringLiteral("preview-type")},
            {ProjectIndexDiagnosticKind::InvalidMapSurveyReference, QStringLiteral("survey")},
            {ProjectIndexDiagnosticKind::RecursiveMapReference, QStringLiteral("recursive")},
            {ProjectIndexDiagnosticKind::IncompatibleScrapProjection, QStringLiteral("scrap-projection")},
            {ProjectIndexDiagnosticKind::IncompatibleMapProjection, QStringLiteral("map-projection")},
            {ProjectIndexDiagnosticKind::ShiftedScrap, QStringLiteral("shifted")},
            {ProjectIndexDiagnosticKind::NoProjectionForSurvey, QStringLiteral("no-projection")},
            {ProjectIndexDiagnosticKind::UnsupportedSurveyProjection, QStringLiteral("survey-projection")},
            {ProjectIndexDiagnosticKind::PreviewNotMap, QStringLiteral("preview-not-map")},
            {ProjectIndexDiagnosticKind::UnknownMapItemReference, QStringLiteral("unknown-item")},
        };
        QStringList summaries;
        for (const ProjectIndexDiagnostic &diagnostic : diagnostics) {
            summaries.append(QStringLiteral("%1:%2:%3")
                                 .arg(labels.value(diagnostic.kind, QStringLiteral("other")),
                                      diagnostic.referencedName)
                                 .arg(diagnostic.lineNumber));
        }
        return summaries;
    }
};

int runProjectStructureIndexQTest(int argc, char **argv)
{
    ProjectStructureIndexQTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectStructureIndexQTest.moc"
