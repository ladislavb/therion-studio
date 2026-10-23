#include "../../src/core/ProjectEquateLinkScanner.h"
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

    void indexesJoinCommandsAndNoSurveyLegs()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QDir projectDir(tempDir.path());
        const QString centreline = QStringLiteral("  centreline\n"
                                                  "    data normal from to length compass clino\n"
                                                  "    1 2 1 0 0\n"
                                                  "  endcentreline\n");
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral("survey top\n"
                                             "survey a\n"
                                             "  input a.th2\n")
                                  + centreline
                                  + QStringLiteral("endsurvey a\n"
                                                   "survey b\n"
                                                   "  input b.th2\n")
                                  + centreline
                                  + QStringLiteral("endsurvey b\n"
                                                   "centreline\n"
                                                   "  data nosurvey from to\n"
                                                   "  2@a 1@b\n"
                                                   "  2@a 1@a\n"
                                                   "endcentreline\n"
                                                   "join sa@a sb@b -count 2\n"
                                                   "join w1@a:end w2@b:0\n"
                                                   "join sa@a sa2@a\n"
                                                   "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("a.th2")),
                              QStringLiteral("scrap sa -projection plan\n"
                                             "point 0 0 station -name 2\n"
                                             "line wall -id w1\n"
                                             "  0 0\n"
                                             "  10 0\n"
                                             "endline\n"
                                             "endscrap\n"
                                             "scrap sa2 -projection plan\n"
                                             "point 0 0 station -name 1\n"
                                             "endscrap\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("b.th2")),
                              QStringLiteral("scrap sb -projection plan\n"
                                             "point 0 0 station -name 1\n"
                                             "line wall -id w2\n"
                                             "  0 0\n"
                                             "  5 5\n"
                                             "endline\n"
                                             "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        // The cross-survey nosurvey leg is a link like an equate; the same-survey leg is not.
        QCOMPARE(snapshot.equateLinks.size(), 1);
        const ProjectEquateLink &leg = snapshot.equateLinks.first();
        QCOMPARE(leg.kind, ProjectStationLinkKind::NoSurvey);
        QCOMPARE(leg.stations.at(0).reference, QStringLiteral("2@a"));
        QCOMPARE(leg.stations.at(1).reference, QStringLiteral("1@b"));
        QVERIFY(leg.stations.at(0).resolved && leg.stations.at(1).resolved);
        const QVector<ProjectStationDrawingPoint> stationA =
            snapshot.stationDrawingPointsByStationKey.value(leg.stations.at(0).stationKey);
        const QVector<ProjectStationDrawingPoint> stationB =
            snapshot.stationDrawingPointsByStationKey.value(leg.stations.at(1).stationKey);
        QCOMPARE(stationA.size(), 1);
        QCOMPARE(stationB.size(), 1);
        QVERIFY(!stationA.first().scrapObjectId.isEmpty());
        // Every station point of every scrap is kept for fitting, equated or not (sa2 draws 1@a only).
        QCOMPARE(snapshot.stationPointsByScrapObjectId.size(), 3);
        QCOMPARE(snapshot.stationPointsByScrapObjectId.value(stationA.first().scrapObjectId).size(), 1);
        QCOMPARE(snapshot.stationPointsByScrapObjectId.value(stationA.first().scrapObjectId).first().stationKey,
                 leg.stations.at(0).stationKey);
        // Therion's SQL export names the station's survey innermost first, like the index.
        QCOMPARE(ProjectStructureIndex::stationKey(QStringLiteral("2"), QStringLiteral("A.Top")),
                 leg.stations.at(0).stationKey);
        QVERIFY(stationA.first().scrapObjectId != stationB.first().scrapObjectId);

        QCOMPARE(snapshot.joinCommands.size(), 3);
        const ProjectJoinCommand &scrapJoin = snapshot.joinCommands.at(0);
        QCOMPARE(scrapJoin.mode, ProjectJoinMode::Scraps);
        QCOMPARE(scrapJoin.count, 2);
        QCOMPARE(scrapJoin.items.size(), 2);
        QCOMPARE(scrapJoin.items.at(0).kind, ProjectStructureEntryKind::Scrap);
        // Join items name the same scraps that draw the linked stations, so equates and joins can be matched.
        QCOMPARE(scrapJoin.items.at(0).scrapObjectId, stationA.first().scrapObjectId);
        QCOMPARE(scrapJoin.items.at(1).scrapObjectId, stationB.first().scrapObjectId);

        const ProjectJoinCommand &lineJoin = snapshot.joinCommands.at(1);
        QCOMPARE(lineJoin.mode, ProjectJoinMode::PointsAndLines);
        QCOMPARE(lineJoin.count, 0);
        QCOMPARE(lineJoin.items.at(0).kind, ProjectStructureEntryKind::Line);
        QCOMPARE(lineJoin.items.at(0).linePointMark, QStringLiteral("end"));
        QCOMPARE(lineJoin.items.at(1).linePointMark, QStringLiteral("0"));
        QCOMPARE(lineJoin.items.at(0).scrapObjectId, stationA.first().scrapObjectId);
        QCOMPARE(lineJoin.items.at(1).scrapObjectId, stationB.first().scrapObjectId);

        const ProjectJoinCommand &sameSurveyJoin = snapshot.joinCommands.at(2);
        QCOMPARE(sameSurveyJoin.mode, ProjectJoinMode::Scraps);
        QCOMPARE(sameSurveyJoin.items.at(0).namespacePath, sameSurveyJoin.items.at(1).namespacePath);

        // sa and sb are joined both as scraps and by walls: one warning on each join line, at the `join` keyword.
        QCOMPARE(snapshot.joinRuleViolations.size(), 1);
        QVector<ProjectIndexDiagnostic> ruleDiagnostics;
        for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
            if (diagnostic.kind == ProjectIndexDiagnosticKind::MixedJoinKinds) {
                ruleDiagnostics.append(diagnostic);
            }
        }
        QCOMPARE(ruleDiagnostics.size(), 2);
        QCOMPARE(ruleDiagnostics.at(0).lineNumber, scrapJoin.lineNumber);
        QCOMPARE(ruleDiagnostics.at(1).lineNumber, lineJoin.lineNumber);
        QCOMPARE(ruleDiagnostics.at(0).columnNumber, 1);
        QCOMPARE(ruleDiagnostics.at(0).columnLength, 4);
        QCOMPARE(ruleDiagnostics.at(0).referencedName, QStringLiteral("sa ~ sb"));
        QCOMPARE(ruleDiagnostics.at(0).candidateCount, 2);
    }

    void normalizesScrapProjectionNames()
    {
        QCOMPARE(normalizedScrapProjection({}), QStringLiteral("plan"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("Plan").split(QLatin1Char(' '))), QStringLiteral("plan"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("elevation  315").split(QLatin1Char(' '))), QStringLiteral("elevation 315"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("elevation 90 deg").split(QLatin1Char(' '))), QStringLiteral("elevation 90"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("elevation 100 grad").split(QLatin1Char(' '))), QStringLiteral("elevation 100 grad"));
        QCOMPARE(normalizedScrapProjection(QStringLiteral("elevation").split(QLatin1Char(' '))), QStringLiteral("elevation"));
    }

    void indexesCrossSurveyEquateLinksAndStationDrawings()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        const QString centreline = QStringLiteral(
            "  centreline\n"
            "    data normal from to length compass clino\n"
            "    %1 %2 1 0 0\n"
            "%3"
            "  endcentreline\n");
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral("survey top\n"
                                             "survey a\n"
                                             "  input a.th2\n")
                                  + centreline.arg(QStringLiteral("1"), QStringLiteral("2"),
                                                   QStringLiteral("    equate 1 2\n"))
                                  + QStringLiteral("endsurvey a\n"
                                                   "survey b\n"
                                                   "  input b.th2\n")
                                  + centreline.arg(QStringLiteral("1"), QStringLiteral("2"), QString())
                                  + QStringLiteral("endsurvey b\n"
                                                   "survey e -namespace off\n"
                                                   "  input e.th2\n")
                                  + centreline.arg(QStringLiteral("9"), QStringLiteral("10"), QString())
                                  + QStringLiteral("endsurvey e\n"
                                                   "equate 2@a 1@b\n"
                                                   "#equate 1@a 2@b\n"
                                                   "equate 9 1@a 2@b\n"
                                                   "endsurvey top\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("a.th2")),
                              QStringLiteral("scrap sa -projection plan\n"
                                             "point 0 0 station -name 2\n"
                                             "point 5 0 station -name 1\n"
                                             "endscrap\n"
                                             "scrap sae -projection [elevation 315]\n"
                                             "point 0 0 station -name 2\n"
                                             "endscrap\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("b.th2")),
                              QStringLiteral("scrap sb\n"
                                             "point 0 0 station -name 1\n"
                                             "point 1 1 station -name 2\n"
                                             "endscrap\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("e.th2")),
                              QStringLiteral("scrap se -projection plan\n"
                                             "point 0 0 station -name 9\n"
                                             "point 1 1 station -name 10\n"
                                             "endscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot =
            ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        // The same-survey and commented-out equates are not cross-survey links.
        QCOMPARE(snapshot.equateLinks.size(), 2);
        const ProjectEquateLink &pair = snapshot.equateLinks.at(0);
        QCOMPARE(pair.lineNumber, 24);
        QCOMPARE(pair.stations.size(), 2);
        QCOMPARE(pair.stations.at(0).reference, QStringLiteral("2@a"));
        QCOMPARE(pair.stations.at(0).namespacePath, QStringLiteral("a.top"));
        QVERIFY(pair.stations.at(0).resolved);
        QVERIFY(pair.stations.at(1).resolved);

        const ProjectEquateLink &group = snapshot.equateLinks.at(1);
        QCOMPARE(group.stations.size(), 3);
        QCOMPARE(group.stations.at(0).reference, QStringLiteral("9"));
        QCOMPARE(group.stations.at(0).namespacePath, QStringLiteral("top"));
        QVERIFY(group.stations.at(0).resolved);

        const QVector<ProjectStationDrawingPoint> stationTwoA =
            snapshot.stationDrawingPointsByStationKey.value(pair.stations.at(0).stationKey);
        QCOMPARE(stationTwoA.size(), 2);
        QCOMPARE(stationTwoA.at(0).scrapName, QStringLiteral("sa"));
        QCOMPARE(stationTwoA.at(0).surveyName, QStringLiteral("a"));
        QCOMPARE(stationTwoA.at(0).projection, QStringLiteral("plan"));
        QCOMPARE(stationTwoA.at(0).lineNumber, 2);
        QCOMPARE(stationTwoA.at(1).projection, QStringLiteral("elevation 315"));

        const QVector<ProjectStationDrawingPoint> stationOneB =
            snapshot.stationDrawingPointsByStationKey.value(pair.stations.at(1).stationKey);
        QCOMPARE(stationOneB.size(), 1);
        QCOMPARE(stationOneB.at(0).projection, QStringLiteral("plan"));

        const QVector<ProjectStationDrawingPoint> stationNine =
            snapshot.stationDrawingPointsByStationKey.value(group.stations.at(0).stationKey);
        QCOMPARE(stationNine.size(), 1);
        QCOMPARE(stationNine.at(0).surveyName, QStringLiteral("e"));

        // Station 10 is drawn but not equated, so it is not indexed as a join drawing point.
        for (const QVector<ProjectStationDrawingPoint> &points : snapshot.stationDrawingPointsByStationKey) {
            for (const ProjectStationDrawingPoint &point : points) {
                QVERIFY(!(point.scrapName == QStringLiteral("se") && point.lineNumber == 3));
            }
        }
    }
};

int runProjectStructureIndexQTest(int argc, char **argv)
{
    ProjectStructureIndexQTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectStructureIndexQTest.moc"
