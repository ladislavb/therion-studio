#include "../src/app/ProjectSourceSnapshot.h"

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
    const QByteArray bytes = contents.toUtf8();
    return file.write(bytes) == bytes.size();
}
}

class ProjectSourceSnapshotTest final : public QObject
{
    Q_OBJECT

private slots:
    void equivalentPathFormsShareRequestKey();
    void inMemoryHashChangesRequestKey();
    void inMemoryOrderDoesNotAffectRequestKey();
    void preferredConfigAffectsRequestKey();
    void policyKeyAffectsRequestKey();
    void collectorReadsSortedProjectSourceFiles();
    void collectorUsesInMemoryOverrideText();
    void collectorIncludesInMemoryOnlySources();
    void collectorRetainsOversizedKnownFileWithoutText();
};

void ProjectSourceSnapshotTest::equivalentPathFormsShareRequestKey()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    QDir projectDir(tempDir.path());
    const QString sourcePath = projectDir.filePath(QStringLiteral("survey.th"));
    const QString configPath = projectDir.filePath(QStringLiteral("thconfig"));
    QVERIFY2(writeTextFile(sourcePath, QStringLiteral("survey a\nendsurvey a\n")),
             "Temporary source file could not be written.");
    QVERIFY2(writeTextFile(configPath, QStringLiteral("source survey.th\n")),
             "Temporary config file could not be written.");

    QHash<QString, QString> firstContents;
    firstContents.insert(sourcePath, QStringLiteral("survey memory\nendsurvey memory\n"));

    QHash<QString, QString> secondContents;
    secondContents.insert(projectDir.filePath(QStringLiteral("./survey.th")),
                          QStringLiteral("survey memory\nendsurvey memory\n"));

    const ProjectSourceRequestKey firstKey =
        projectSourceRequestKey(tempDir.path(), configPath, firstContents);
    const ProjectSourceRequestKey secondKey = projectSourceRequestKey(projectDir.filePath(QStringLiteral(".")),
                                                                      projectDir.filePath(QStringLiteral("./thconfig")),
                                                                      secondContents);

    QCOMPARE(firstKey, secondKey);
    QCOMPARE(firstKey.stableKey(), secondKey.stableKey());
}

void ProjectSourceSnapshotTest::inMemoryHashChangesRequestKey()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    const QString sourcePath = QDir(tempDir.path()).filePath(QStringLiteral("survey.th"));
    QVERIFY2(writeTextFile(sourcePath, QStringLiteral("survey stale\nendsurvey stale\n")),
             "Temporary source file could not be written.");

    QHash<QString, QString> firstContents;
    firstContents.insert(sourcePath, QStringLiteral("survey first\nendsurvey first\n"));

    QHash<QString, QString> secondContents;
    secondContents.insert(sourcePath, QStringLiteral("survey second\nendsurvey second\n"));

    const ProjectSourceRequestKey firstKey = projectSourceRequestKey(tempDir.path(), {}, firstContents);
    const ProjectSourceRequestKey secondKey = projectSourceRequestKey(tempDir.path(), {}, secondContents);

    QVERIFY2(firstKey.stableKey() != secondKey.stableKey(),
             "Changing in-memory source contents should change the request key.");
}

void ProjectSourceSnapshotTest::inMemoryOrderDoesNotAffectRequestKey()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    QDir projectDir(tempDir.path());
    const QString firstPath = projectDir.filePath(QStringLiteral("a.th"));
    const QString secondPath = projectDir.filePath(QStringLiteral("b.th"));
    QVERIFY2(writeTextFile(firstPath, QStringLiteral("survey a\nendsurvey a\n")),
             "First temporary source file could not be written.");
    QVERIFY2(writeTextFile(secondPath, QStringLiteral("survey b\nendsurvey b\n")),
             "Second temporary source file could not be written.");

    QHash<QString, QString> firstContents;
    firstContents.insert(firstPath, QStringLiteral("survey memory-a\nendsurvey memory-a\n"));
    firstContents.insert(secondPath, QStringLiteral("survey memory-b\nendsurvey memory-b\n"));

    QHash<QString, QString> secondContents;
    secondContents.insert(secondPath, QStringLiteral("survey memory-b\nendsurvey memory-b\n"));
    secondContents.insert(firstPath, QStringLiteral("survey memory-a\nendsurvey memory-a\n"));

    const ProjectSourceRequestKey firstKey = projectSourceRequestKey(tempDir.path(), {}, firstContents);
    const ProjectSourceRequestKey secondKey = projectSourceRequestKey(tempDir.path(), {}, secondContents);

    QCOMPARE(firstKey, secondKey);
    QCOMPARE(firstKey.inMemoryDocuments.size(), 2);
    QVERIFY2(firstKey.inMemoryDocuments.at(0).normalizedPath < firstKey.inMemoryDocuments.at(1).normalizedPath,
             "In-memory document keys should be sorted by normalized path.");
}

void ProjectSourceSnapshotTest::preferredConfigAffectsRequestKey()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    QDir projectDir(tempDir.path());
    const QString planConfigPath = projectDir.filePath(QStringLiteral("plan.thconfig"));
    const QString extendedConfigPath = projectDir.filePath(QStringLiteral("extended.thconfig"));
    QVERIFY2(writeTextFile(planConfigPath, QStringLiteral("source survey.th\n")),
             "Plan config file could not be written.");
    QVERIFY2(writeTextFile(extendedConfigPath, QStringLiteral("source survey.th\n")),
             "Extended config file could not be written.");

    const ProjectSourceRequestKey emptyConfigKey = projectSourceRequestKey(tempDir.path(), {}, {});
    const ProjectSourceRequestKey planConfigKey =
        projectSourceRequestKey(tempDir.path(), planConfigPath, {});
    const ProjectSourceRequestKey equivalentPlanConfigKey =
        projectSourceRequestKey(projectDir.filePath(QStringLiteral(".")),
                                projectDir.filePath(QStringLiteral("./plan.thconfig")),
                                {});
    const ProjectSourceRequestKey extendedConfigKey =
        projectSourceRequestKey(tempDir.path(), extendedConfigPath, {});

    QVERIFY2(emptyConfigKey.stableKey() != planConfigKey.stableKey(),
             "Adding a preferred config should change the request key.");
    QCOMPARE(planConfigKey, equivalentPlanConfigKey);
    QVERIFY2(planConfigKey.stableKey() != extendedConfigKey.stableKey(),
             "Changing the preferred config should change the request key.");
}

void ProjectSourceSnapshotTest::policyKeyAffectsRequestKey()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    const ProjectSourceRequestKey firstKey =
        projectSourceRequestKey(tempDir.path(), {}, {}, QStringLiteral("therion-sources-v1"));
    const ProjectSourceRequestKey secondKey =
        projectSourceRequestKey(tempDir.path(), {}, {}, QStringLiteral("therion-sources-v2"));

    QVERIFY2(firstKey.stableKey() != secondKey.stableKey(),
             "Changing the file policy key should change the request key.");
}

void ProjectSourceSnapshotTest::collectorReadsSortedProjectSourceFiles()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    QDir projectDir(tempDir.path());
    QVERIFY(projectDir.mkdir(QStringLiteral("sub")));
    QVERIFY(projectDir.mkdir(QStringLiteral("build")));
    QVERIFY2(writeTextFile(projectDir.filePath(QStringLiteral("b.th")), QStringLiteral("survey b\nendsurvey b\n")),
             "b.th could not be written.");
    QVERIFY2(writeTextFile(projectDir.filePath(QStringLiteral("a.th2")), QStringLiteral("scrap a\nendscrap\n")),
             "a.th2 could not be written.");
    QVERIFY2(writeTextFile(projectDir.filePath(QStringLiteral("thconfig")), QStringLiteral("source b.th\n")),
             "thconfig could not be written.");
    QVERIFY2(writeTextFile(projectDir.filePath(QStringLiteral("ignore.txt")), QStringLiteral("ignored\n")),
             "ignore.txt could not be written.");
    QVERIFY2(writeTextFile(projectDir.filePath(QStringLiteral("build/generated.th")), QStringLiteral("survey generated\n")),
             "build/generated.th could not be written.");
    QVERIFY2(writeTextFile(projectDir.filePath(QStringLiteral("sub/c.th")), QStringLiteral("survey c\nendsurvey c\n")),
             "sub/c.th could not be written.");

    const ProjectSourceSnapshot snapshot = collectProjectSourceSnapshot(tempDir.path(), {}, {});

    const QDir normalizedProjectDir(normalizeProjectSourcePath(tempDir.path()));
    QStringList relativePaths;
    for (const ProjectSourceDocument &document : snapshot.documents) {
        relativePaths.append(normalizedProjectDir.relativeFilePath(document.normalizedPath));
        QCOMPARE(document.origin, ProjectSourceDocumentOrigin::Filesystem);
        QVERIFY(document.textLoaded);
    }

    QCOMPARE(relativePaths,
             QStringList({QStringLiteral("a.th2"),
                          QStringLiteral("b.th"),
                          QStringLiteral("sub/c.th"),
                          QStringLiteral("thconfig")}));
}

void ProjectSourceSnapshotTest::collectorUsesInMemoryOverrideText()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    QDir projectDir(tempDir.path());
    const QString sourcePath = projectDir.filePath(QStringLiteral("survey.th"));
    QVERIFY2(writeTextFile(sourcePath, QStringLiteral("survey stale\nendsurvey stale\n")),
             "survey.th could not be written.");

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(sourcePath, QStringLiteral("survey live\nendsurvey live\n"));

    const ProjectSourceSnapshot snapshot = collectProjectSourceSnapshot(tempDir.path(), {}, inMemoryContents);

    QCOMPARE(snapshot.documents.size(), 1);
    QCOMPARE(snapshot.documents.constFirst().normalizedPath, normalizeProjectSourcePath(sourcePath));
    QCOMPARE(snapshot.documents.constFirst().text, QStringLiteral("survey live\nendsurvey live\n"));
    QCOMPARE(snapshot.documents.constFirst().origin, ProjectSourceDocumentOrigin::InMemoryOverride);
    QVERIFY(snapshot.documents.constFirst().textLoaded);
}

void ProjectSourceSnapshotTest::collectorIncludesInMemoryOnlySources()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    QDir projectDir(tempDir.path());
    QHash<QString, QString> inMemoryContents;
    const QString sourcePath = projectDir.filePath(QStringLiteral("unsaved.th2"));
    inMemoryContents.insert(sourcePath, QStringLiteral("scrap unsaved\nendscrap\n"));
    inMemoryContents.insert(projectDir.filePath(QStringLiteral("notes.txt")), QStringLiteral("not therion\n"));

    const ProjectSourceSnapshot snapshot = collectProjectSourceSnapshot(tempDir.path(), {}, inMemoryContents);

    QCOMPARE(snapshot.documents.size(), 1);
    QCOMPARE(snapshot.documents.constFirst().normalizedPath, normalizeProjectSourcePath(sourcePath));
    QCOMPARE(snapshot.documents.constFirst().text, QStringLiteral("scrap unsaved\nendscrap\n"));
    QCOMPARE(snapshot.documents.constFirst().origin, ProjectSourceDocumentOrigin::InMemoryOnly);
}

void ProjectSourceSnapshotTest::collectorRetainsOversizedKnownFileWithoutText()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary project directory creation failed.");

    QDir projectDir(tempDir.path());
    const QString sourcePath = projectDir.filePath(QStringLiteral("large.th"));
    QVERIFY2(writeTextFile(sourcePath, QStringLiteral("survey large\nendsurvey large\n")),
             "large.th could not be written.");

    const ProjectSourceSnapshot snapshot = collectProjectSourceSnapshot(tempDir.path(), {}, {}, 8);

    QCOMPARE(snapshot.documents.size(), 1);
    QCOMPARE(snapshot.documents.constFirst().normalizedPath, normalizeProjectSourcePath(sourcePath));
    QCOMPARE(snapshot.documents.constFirst().origin, ProjectSourceDocumentOrigin::Filesystem);
    QVERIFY(!snapshot.documents.constFirst().textLoaded);
    QVERIFY(snapshot.documents.constFirst().text.isEmpty());
    QVERIFY(snapshot.knownFilePaths().contains(normalizeProjectSourcePath(sourcePath)));
}

QTEST_GUILESS_MAIN(ProjectSourceSnapshotTest)

#include "ProjectSourceSnapshotTest.moc"
