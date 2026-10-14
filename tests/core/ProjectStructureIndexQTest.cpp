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

    void resolvesObjectsThroughNamespaceOffSurveyName()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QDir projectDir(tempDir.path());
        // `ent` shares the `cave` namespace, but Therion still names its objects `sp1@ent.cave`.
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral("survey cave\n"
                                             "survey ent -namespace off\n"
                                             "  input ent.th2\n"
                                             "  map entm\n"
                                             "    sp1\n"
                                             "  endmap\n"
                                             "endsurvey ent\n"
                                             "survey b\n"
                                             "  input b.th2\n"
                                             "endsurvey b\n"
                                             "map cavem\n"
                                             "  entm@ent\n"
                                             "endmap\n"
                                             "join sp1@ent sb@b\n"
                                             "join sp1 sb@b\n"
                                             "join nope@ent sb@b\n"
                                             "endsurvey cave\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("ent.th2")),
                              QStringLiteral("scrap sp1\nendscrap\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("b.th2")),
                              QStringLiteral("scrap sb\nendscrap\n")));

        QString errorMessage;
        const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        // `sp1@ent` and the unqualified `sp1` resolve; only the genuinely missing object is reported.
        QStringList unknownJoins;
        QStringList unknownMapReferences;
        for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
            if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownJoinReference) {
                unknownJoins.append(diagnostic.referencedName);
            } else if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapReference
                       || diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapScrapReference) {
                unknownMapReferences.append(diagnostic.referencedName);
            }
        }
        QCOMPARE(unknownJoins, QStringList({QStringLiteral("nope@ent")}));
        QVERIFY(unknownMapReferences.isEmpty());

        // The map composed through the namespace-off survey name gains its child map.
        QString caveMapKey;
        QString entranceMapKey;
        for (const ProjectStructureEntry &entry : snapshot.entries) {
            if (entry.kind == ProjectStructureEntryKind::Map && entry.name == QStringLiteral("cavem")) {
                caveMapKey = ProjectStructureIndex::structureEntryNodeKey(entry);
            } else if (entry.kind == ProjectStructureEntryKind::Map && entry.name == QStringLiteral("entm")) {
                entranceMapKey = ProjectStructureIndex::structureEntryNodeKey(entry);
            }
        }
        QVERIFY(!caveMapKey.isEmpty() && !entranceMapKey.isEmpty());
        QVERIFY(snapshot.mapChildReferencesByMapKey.value(caveMapKey).contains(entranceMapKey));
    }
};

int runProjectStructureIndexQTest(int argc, char **argv)
{
    ProjectStructureIndexQTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectStructureIndexQTest.moc"
