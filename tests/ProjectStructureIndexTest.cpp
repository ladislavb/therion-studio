#include "../src/core/ProjectStructureIndex.h"
#include "../src/core/TherionDocumentParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <iostream>

using namespace TherionStudio;

namespace
{
QString normalizedPathForComparison(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }

    return condition;
}

bool writeTextFile(const QString &filePath, const QString &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    return file.write(contents.toUtf8()) == contents.toUtf8().size();
}

int runProjectStructureHierarchyTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps")), "The temporary maps directory could not be created.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                           QStringLiteral(
                               "survey cave\n"
                               "  input branch\n"
                               "  input maps/map.th2\n"
                               "endsurvey cave\n")),
                "The root Therion file could not be written.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("branch.th")),
                               QStringLiteral(
                                   "survey branch -namespace off\n"
                                   "  centerline\n"
                                   "  endcenterline\n"
                                   "endsurvey branch\n")),
                "The nested survey file could not be written.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                           QStringLiteral(
                               "scrap s1\n"
                               "point 10 20 station -name 1@cave\n"
                               "point 1e2 -2.5E-1 altitude\n"
                               "line wall\n"
                               "endline\n"
                               "endscrap\n")),
                "The TH2 map file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const QVector<ProjectStructureEntry> entries = ProjectStructureIndex::scanProject(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(entries.size() == 7, "The survey hierarchy scan returned an unexpected number of entries.")) {
        return 1;
    }

    struct ExpectedEntry
    {
        ProjectStructureEntryKind kind = ProjectStructureEntryKind::Unknown;
        QString category;
        QString name;
        int depth = 0;
        QString sourceFile;
        int lineNumber = 0;
        bool createsNamespace = true;
    };

    const QVector<ExpectedEntry> expectedEntries = {
        {ProjectStructureEntryKind::Survey, QStringLiteral("Surveys"), QStringLiteral("cave"), 0, projectDir.filePath(QStringLiteral("root.th")), 1, true},
        {ProjectStructureEntryKind::Survey, QStringLiteral("Surveys"), QStringLiteral("branch"), 1, projectDir.filePath(QStringLiteral("branch.th")), 1, false},
        {ProjectStructureEntryKind::Centreline, QStringLiteral("Centrelines"), QStringLiteral("centerline"), 2, projectDir.filePath(QStringLiteral("branch.th")), 2, true},
        {ProjectStructureEntryKind::Scrap, QStringLiteral("Scraps"), QStringLiteral("s1"), 1, projectDir.filePath(QStringLiteral("maps/map.th2")), 1, true},
        {ProjectStructureEntryKind::Station, QStringLiteral("Stations"), QStringLiteral("1@cave"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 2, true},
        {ProjectStructureEntryKind::Point, QStringLiteral("Points"), QStringLiteral("altitude"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 3, true},
        {ProjectStructureEntryKind::Line, QStringLiteral("Lines"), QStringLiteral("wall"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 4, true},
    };

    for (int index = 0; index < expectedEntries.size(); ++index) {
        const ProjectStructureEntry &entry = entries.at(index);
        const ExpectedEntry &expected = expectedEntries.at(index);

        if (!expect(entry.kind == expected.kind, "The survey hierarchy kinds are out of order.")) {
            return 1;
        }
        if (!expect(entry.category == expected.category, "The survey hierarchy categories are out of order.")) {
            return 1;
        }
        if (!expect(entry.name == expected.name, "The survey hierarchy names are out of order.")) {
            return 1;
        }
        if (!expect(entry.depth == expected.depth, "The survey hierarchy nesting depth is incorrect.")) {
            return 1;
        }
        if (!expect(normalizedPathForComparison(entry.sourceFile) == normalizedPathForComparison(expected.sourceFile), "The survey hierarchy source file is incorrect.")) {
            return 1;
        }
        if (!expect(entry.lineNumber == expected.lineNumber, "The survey hierarchy line number is incorrect.")) {
            return 1;
        }
        if (!expect(entry.createsNamespace == expected.createsNamespace, "The survey namespace flag is incorrect.")) {
            return 1;
        }
        if (!expect(!entry.objectId.isEmpty(), "The survey hierarchy entry object ID should not be empty.")) {
            return 1;
        }
        if (!expect((expected.depth == 0) == entry.parentObjectId.isEmpty(),
                    "The survey hierarchy parent object ID does not match the nesting depth.")) {
            return 1;
        }
    }

    return 0;
}

int runProjectStructureContinuedInputTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps")), "The temporary maps directory could not be created.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input \\\n"
                                  "    maps/map.th2\n"
                                  "endsurvey cave\n")),
                "The continued-input root Therion file could not be written.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                              QStringLiteral(
                                  "scrap s1\n"
                                  "point 10 20 station -name a1\n"
                                  "endscrap\n")),
                "The continued-input TH2 map file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const QVector<ProjectStructureEntry> entries = ProjectStructureIndex::scanProject(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    bool foundScrap = false;
    bool foundStation = false;
    for (const ProjectStructureEntry &entry : entries) {
        if (entry.kind == ProjectStructureEntryKind::Scrap
            && entry.name == QStringLiteral("s1")
            && normalizedPathForComparison(entry.sourceFile) == normalizedPathForComparison(projectDir.filePath(QStringLiteral("maps/map.th2")))) {
            foundScrap = true;
        }
        if (entry.kind == ProjectStructureEntryKind::Station
            && entry.name == QStringLiteral("a1")
            && normalizedPathForComparison(entry.sourceFile) == normalizedPathForComparison(projectDir.filePath(QStringLiteral("maps/map.th2")))) {
            foundStation = true;
        }
    }

    if (!expect(foundScrap,
                "The project structure index should follow input targets split across continuation lines.")) {
        return 1;
    }
    if (!expect(foundStation,
                "The project structure index should scan objects from a continued-input target.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexMapScrapReferenceTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps")), "The temporary maps directory could not be created.")) {
        return 1;
    }

    const QString rootFile = projectDir.filePath(QStringLiteral("root.th"));
    const QString mapFile = projectDir.filePath(QStringLiteral("maps/map.th2"));
    if (!expect(writeTextFile(rootFile,
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input maps/map.th2\n"
                                  "  map child-map.m\n"
                                  "    s1\n"
                                  "  endmap\n"
                                  "  map cave-map.m\n"
                                  "    stale-scrap.s\n"
                                  "  endmap\n"
                                  "endsurvey cave\n")),
                "The root Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapFile,
                              QStringLiteral(
                                  "scrap s1\n"
                                  "endscrap\n")),
                "The TH2 map file could not be written.")) {
        return 1;
    }

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(normalizedPathForComparison(rootFile),
                            QStringLiteral(
                                "survey cave\n"
                                "  input maps/map.th2\n"
                                "  map child-map.m\n"
                                "    s1\n"
                                "    preview above cave-map.m\n"
                                "  endmap\n"
                                "  map cave-map.m\n"
                                "    child-map.m [1 2 m] above\n"
                                "    s1\n"
                                "    break\n"
                                "    missing-scrap.s\n"
                                "    missing-map.m\n"
                                "  endmap\n"
                                "endsurvey cave\n"));

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                  inMemoryContents,
                                                                                  &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(snapshot.rootConfigPath.isEmpty(),
                "The project index should leave root config empty when no thconfig graph is used.")) {
        return 1;
    }
    if (!expect(snapshot.rootFilePaths.size() == 1
                    && normalizedPathForComparison(snapshot.rootFilePaths.first()) == normalizedPathForComparison(rootFile),
                "The project index should expose the inferred root source file.")) {
        return 1;
    }

    auto findEntry = [](const QVector<ProjectStructureEntry> &entries,
                        ProjectStructureEntryKind kind,
                        const QString &name,
                        ProjectStructureEntry *foundEntry) {
        for (const ProjectStructureEntry &entry : entries) {
            if (entry.kind == kind && entry.name == name) {
                if (foundEntry != nullptr) {
                    *foundEntry = entry;
                }
                return true;
            }
        }

        return false;
    };

    ProjectStructureEntry mapEntry;
    ProjectStructureEntry childMapEntry;
    ProjectStructureEntry scrapEntry;
    const bool foundMap = findEntry(snapshot.entries,
                                    ProjectStructureEntryKind::Map,
                                    QStringLiteral("cave-map.m"),
                                    &mapEntry);
    const bool foundChildMap = findEntry(snapshot.entries,
                                         ProjectStructureEntryKind::Map,
                                         QStringLiteral("child-map.m"),
                                         &childMapEntry);
    const bool foundScrap = findEntry(snapshot.entries,
                                      ProjectStructureEntryKind::Scrap,
                                      QStringLiteral("s1"),
                                      &scrapEntry);
    if (!expect(foundMap, "The project index did not find the map entry.")) {
        return 1;
    }
    if (!expect(foundChildMap, "The project index did not find the child map entry.")) {
        return 1;
    }
    if (!expect(foundScrap, "The project index did not find the scrap entry.")) {
        return 1;
    }
    if (!expect(!mapEntry.objectId.isEmpty(), "The project index map entry should expose a stable object ID.")) {
        return 1;
    }

    const QString mapKey = ProjectStructureIndex::structureEntryNodeKey(mapEntry);
    if (!expect(mapKey == mapEntry.objectId, "The structure node key should prefer the stable object ID.")) {
        return 1;
    }
    const QString childMapKey = ProjectStructureIndex::structureEntryNodeKey(childMapEntry);
    const QSet<QString> childMapReferences = snapshot.mapChildReferencesByMapKey.value(mapKey);
    if (!expect(childMapReferences.contains(childMapKey),
                "The project index did not resolve the map-to-map composition reference.")) {
        return 1;
    }
    if (!expect(snapshot.mapChildReferencesByMapKey.value(childMapKey).isEmpty(),
                "Preview references should not be treated as map composition hierarchy.")) {
        return 1;
    }
    const QSet<QString> childMapPreviewReferences = snapshot.mapPreviewReferencesByMapKey.value(childMapKey);
    if (!expect(childMapPreviewReferences.contains(mapKey),
                "Preview references should be exposed as non-hierarchical map relationships.")) {
        return 1;
    }

    const QString scrapKey = ProjectStructureIndex::structureEntryNodeKey(scrapEntry);
    const QSet<QString> scrapReferences = snapshot.mapScrapReferencesByMapKey.value(mapKey);
    if (!expect(scrapReferences.contains(scrapKey),
                "The project index did not resolve the namespaced map-to-scrap reference from in-memory source text.")) {
        return 1;
    }
    for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
        if (!expect(diagnostic.referencedName != QStringLiteral("stale-scrap.s"),
                    "The project index used stale on-disk source text for map references.")) {
            return 1;
        }
    }
    if (!expect(snapshot.diagnostics.size() == 3,
                "The project index should report mixed content plus unresolved map scrap and map references.")) {
        return 1;
    }

    bool foundMixedContentDiagnostic = false;
    bool foundMissingScrapDiagnostic = false;
    bool foundMissingMapDiagnostic = false;
    for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
        if (diagnostic.kind == ProjectIndexDiagnosticKind::MixedMapAndScrapReferences
            && diagnostic.sourceObjectId == mapEntry.objectId
            && diagnostic.lineNumber == 9
            && diagnostic.referencedName == QStringLiteral("s1")) {
            foundMixedContentDiagnostic = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapScrapReference
            && diagnostic.sourceObjectId == mapEntry.objectId
            && normalizedPathForComparison(diagnostic.sourceFile) == normalizedPathForComparison(rootFile)
            && diagnostic.lineNumber == 11
            && diagnostic.referencedName == QStringLiteral("missing-scrap.s")) {
            foundMissingScrapDiagnostic = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapReference
            && diagnostic.sourceObjectId == mapEntry.objectId
            && diagnostic.lineNumber == 12
            && diagnostic.referencedName == QStringLiteral("missing-map.m")) {
            foundMissingMapDiagnostic = true;
        }
    }
    if (!expect(foundMixedContentDiagnostic,
                "The project index did not report mixed map/scrap content.")) {
        return 1;
    }
    if (!expect(foundMissingScrapDiagnostic,
                "The unresolved map scrap reference diagnostic payload is incorrect.")) {
        return 1;
    }
    if (!expect(foundMissingMapDiagnostic,
                "The unresolved map reference diagnostic payload is incorrect.")) {
        return 1;
    }

    inMemoryContents.insert(normalizedPathForComparison(rootFile),
                            QStringLiteral(
                                "\n"
                                "survey cave\n"
                                "  input maps/map.th2\n"
                                "  map child-map.m\n"
                                "    s1\n"
                                "    preview above cave-map.m\n"
                                "  endmap\n"
                                "  map cave-map.m\n"
                                "    child-map.m [1 2 m] above\n"
                                "    s1\n"
                                "    break\n"
                                "    missing-scrap.s\n"
                                "    missing-map.m\n"
                                "  endmap\n"
                                "endsurvey cave\n"));
    const ProjectIndexSnapshot shiftedSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                         inMemoryContents,
                                                                                         &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    ProjectStructureEntry shiftedMapEntry;
    const bool foundShiftedMap = findEntry(shiftedSnapshot.entries,
                                           ProjectStructureEntryKind::Map,
                                           QStringLiteral("cave-map.m"),
                                           &shiftedMapEntry);
    if (!expect(foundShiftedMap, "The shifted project index did not find the map entry.")) {
        return 1;
    }
    if (!expect(shiftedMapEntry.objectId == mapEntry.objectId,
                "The project index object ID should stay stable when source line numbers shift.")) {
        return 1;
    }
    if (!expect(shiftedSnapshot.diagnostics.size() == 3
                    && shiftedSnapshot.diagnostics.first().sourceObjectId == shiftedMapEntry.objectId,
                "The shifted project index diagnostic should stay attached to the map object ID.")) {
        return 1;
    }
    ProjectStructureEntry shiftedChildMapEntry;
    if (!expect(findEntry(shiftedSnapshot.entries,
                          ProjectStructureEntryKind::Map,
                          QStringLiteral("child-map.m"),
                          &shiftedChildMapEntry),
                "The shifted project index did not find the child map entry.")) {
        return 1;
    }
    const QSet<QString> shiftedPreviewReferences = shiftedSnapshot.mapPreviewReferencesByMapKey.value(
        ProjectStructureIndex::structureEntryNodeKey(shiftedChildMapEntry));
    if (!expect(shiftedPreviewReferences.contains(ProjectStructureIndex::structureEntryNodeKey(shiftedMapEntry)),
                "The shifted project index preview reference should stay attached to stable map object IDs.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexNamespacedMapReferenceTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps/a"))
                    && projectDir.mkpath(QStringLiteral("maps/b")),
                "The temporary maps directories could not be created.")) {
        return 1;
    }

    const QString rootFile = projectDir.filePath(QStringLiteral("root.th"));
    const QString branchAFile = projectDir.filePath(QStringLiteral("branch-a.th"));
    const QString branchBFile = projectDir.filePath(QStringLiteral("branch-b.th"));
    const QString mapAFile = projectDir.filePath(QStringLiteral("maps/a/map.th2"));
    const QString mapBFile = projectDir.filePath(QStringLiteral("maps/b/map.th2"));

    if (!expect(writeTextFile(rootFile,
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input branch-a.th\n"
                                  "  input branch-b.th\n"
                                  "  map root-map.m\n"
                                  "    target.s@branch_b\n"
                                  "  endmap\n"
                                  "endsurvey cave\n")),
                "The root Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(branchAFile,
                              QStringLiteral(
                                  "survey branch_a\n"
                                  "  input maps/a/map.th2\n"
                                  "endsurvey branch_a\n")),
                "The branch A Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(branchBFile,
                              QStringLiteral(
                                  "survey branch_b\n"
                                  "  input maps/b/map.th2\n"
                                  "endsurvey branch_b\n")),
                "The branch B Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapAFile,
                              QStringLiteral(
                                  "scrap target.s\n"
                                  "endscrap\n")),
                "The branch A TH2 map file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapBFile,
                              QStringLiteral(
                                  "scrap target.s\n"
                                  "endscrap\n")),
                "The branch B TH2 map file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    auto findEntry = [](const QVector<ProjectStructureEntry> &entries,
                        ProjectStructureEntryKind kind,
                        const QString &name,
                        const QString &namespacePath,
                        ProjectStructureEntry *foundEntry) {
        for (const ProjectStructureEntry &entry : entries) {
            if (entry.kind == kind && entry.name == name && entry.namespacePath == namespacePath) {
                if (foundEntry != nullptr) {
                    *foundEntry = entry;
                }
                return true;
            }
        }

        return false;
    };

    ProjectStructureEntry rootMapEntry;
    ProjectStructureEntry branchAScrapEntry;
    ProjectStructureEntry branchBScrapEntry;
    if (!expect(findEntry(snapshot.entries,
                          ProjectStructureEntryKind::Map,
                          QStringLiteral("root-map.m"),
                          QStringLiteral("cave"),
                          &rootMapEntry),
                "The namespaced reference test did not find the root map entry.")) {
        return 1;
    }
    if (!expect(findEntry(snapshot.entries,
                          ProjectStructureEntryKind::Scrap,
                          QStringLiteral("target.s"),
                          QStringLiteral("branch_a.cave"),
                          &branchAScrapEntry),
                "The namespaced reference test did not find the branch A scrap entry.")) {
        return 1;
    }
    if (!expect(findEntry(snapshot.entries,
                          ProjectStructureEntryKind::Scrap,
                          QStringLiteral("target.s"),
                          QStringLiteral("branch_b.cave"),
                          &branchBScrapEntry),
                "The namespaced reference test did not find the branch B scrap entry.")) {
        return 1;
    }

    const QSet<QString> rootMapScraps = snapshot.mapScrapReferencesByMapKey.value(
        ProjectStructureIndex::structureEntryNodeKey(rootMapEntry));
    if (!expect(rootMapScraps.contains(ProjectStructureIndex::structureEntryNodeKey(branchBScrapEntry)),
                "The namespaced map reference did not resolve to the explicitly referenced scrap namespace.")) {
        return 1;
    }
    if (!expect(!rootMapScraps.contains(ProjectStructureIndex::structureEntryNodeKey(branchAScrapEntry)),
                "The namespaced map reference incorrectly resolved to the same-named scrap in another namespace.")) {
        return 1;
    }
    if (!expect(snapshot.diagnostics.isEmpty(),
                "The namespaced map reference should not produce unresolved-reference diagnostics.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexAmbiguousMapReferenceDiagnosticsTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps/a"))
                    && projectDir.mkpath(QStringLiteral("maps/b")),
                "The temporary maps directories could not be created.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input maps/a/map.th2\n"
                                  "  input maps/b/map.th2\n"
                                  "  map branch-map.m\n"
                                  "  endmap\n"
                                  "  map branch-map.m\n"
                                  "  endmap\n"
                                  "  map root-map.m\n"
                                  "    target.s\n"
                                  "    branch-map.m\n"
                                  "  endmap\n"
                                  "endsurvey cave\n")),
                "The root Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/a/map.th2")),
                              QStringLiteral(
                                  "scrap target.s\n"
                                  "endscrap\n")),
                "The branch A TH2 map file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/b/map.th2")),
                              QStringLiteral(
                                  "scrap target.s\n"
                                  "endscrap\n")),
                "The branch B TH2 map file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(snapshot.diagnostics.size() == 5,
                "The project index should report ambiguous map/scrap references, duplicate names, and mixed content.")) {
        return 1;
    }

    bool foundAmbiguousScrap = false;
    bool foundAmbiguousMap = false;
    bool foundMixedContent = false;
    bool foundDuplicateMapName = false;
    bool foundDuplicateScrapName = false;
    for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
        if (diagnostic.kind == ProjectIndexDiagnosticKind::AmbiguousMapScrapReference
            && diagnostic.referencedName == QStringLiteral("target.s")
            && diagnostic.candidateCount == 2
            && diagnostic.lineNumber == 9) {
            foundAmbiguousScrap = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::AmbiguousMapReference
            && diagnostic.referencedName == QStringLiteral("branch-map.m")
            && diagnostic.candidateCount == 2
            && diagnostic.lineNumber == 10) {
            foundAmbiguousMap = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::MixedMapAndScrapReferences
            && diagnostic.referencedName == QStringLiteral("branch-map.m")
            && diagnostic.lineNumber == 10) {
            foundMixedContent = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::DuplicateObjectId
            && diagnostic.referencedName == QStringLiteral("branch-map.m")
            && diagnostic.lineNumber == 6) {
            foundDuplicateMapName = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::DuplicateObjectId
            && diagnostic.referencedName == QStringLiteral("target.s")
            && diagnostic.lineNumber == 1) {
            foundDuplicateScrapName = true;
        }
    }

    if (!expect(foundAmbiguousScrap,
                "The project index did not report the ambiguous scrap reference diagnostic.")) {
        return 1;
    }
    if (!expect(foundAmbiguousMap,
                "The project index did not report the ambiguous map reference diagnostic.")) {
        return 1;
    }
    if (!expect(foundMixedContent,
                "The project index did not report the mixed map/scrap diagnostic.")) {
        return 1;
    }
    if (!expect(foundDuplicateMapName,
                "The project index did not report the duplicate map name diagnostic.")) {
        return 1;
    }
    if (!expect(foundDuplicateScrapName,
                "The project index did not report the duplicate scrap name diagnostic.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexJoinReferenceDiagnosticsTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps/a"))
                    && projectDir.mkpath(QStringLiteral("maps/b")),
                "The temporary join-reference map directories could not be created.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input maps/a/map.th2\n"
                                  "  input maps/b/map.th2\n"
                                  "  join wall1 wall2:end wall2:0 wall2:k1 wall2:k-missing missingLine:k1 missing.s ambiguous.s plainUnknown\n"
                                  "endsurvey cave\n")),
                "The join-reference root Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/a/map.th2")),
                              QStringLiteral(
                                  "scrap a\n"
                                  "line wall -id wall1\n"
                                  "endline\n"
                                  "line wall -id wall2\n"
                                  "  mark k1\n"
                                  "endline\n"
                                  "scrap ambiguous.s\n"
                                  "endscrap\n"
                                  "endscrap\n")),
                "The join-reference first map file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/b/map.th2")),
                              QStringLiteral(
                                  "scrap ambiguous.s\n"
                                  "endscrap\n")),
                "The join-reference second map file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    bool foundMissingJoin = false;
    bool foundMissingJoinWithMark = false;
    bool foundMissingJoinMark = false;
    bool foundAmbiguousJoin = false;
    for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
        if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownJoinReference
            && diagnostic.referencedName == QStringLiteral("missing.s")
            && diagnostic.lineNumber == 4) {
            foundMissingJoin = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownJoinReference
            && diagnostic.referencedName == QStringLiteral("missingLine:k1")
            && diagnostic.lineNumber == 4) {
            foundMissingJoinWithMark = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownJoinLinePointMark
            && diagnostic.referencedName == QStringLiteral("wall2:k-missing")
            && diagnostic.lineNumber == 4) {
            foundMissingJoinMark = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::AmbiguousJoinReference
            && diagnostic.referencedName == QStringLiteral("ambiguous.s")
            && diagnostic.candidateCount == 2
            && diagnostic.lineNumber == 4) {
            foundAmbiguousJoin = true;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("wall1"),
                    "Resolved join line references should not produce diagnostics.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("wall2:end"),
                    "Resolved join endpoint references should not produce diagnostics.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("wall2:0"),
                    "Resolved join start-point references should not produce diagnostics.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("wall2:k1"),
                    "Resolved join marked line-point references should not produce diagnostics.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("plainUnknown"),
                    "Plain unresolved join tokens should not produce conservative diagnostics.")) {
            return 1;
        }
    }

    if (!expect(foundMissingJoin,
                "The project index did not report the missing join reference diagnostic.")) {
        return 1;
    }
    if (!expect(foundMissingJoinWithMark,
                "The project index did not report the missing join reference diagnostic with a line-point mark.")) {
        return 1;
    }
    if (!expect(foundMissingJoinMark,
                "The project index did not report the missing join line-point mark diagnostic.")) {
        return 1;
    }
    if (!expect(foundAmbiguousJoin,
                "The project index did not report the ambiguous join reference diagnostic.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexStationReferenceDiagnosticsTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary station-reference project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps")),
                "The temporary station-reference map directory could not be created.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input maps/map.th2\n"
                                  "  centerline\n"
                                  "    data normal from to compass clino tape\n"
                                  "    a1 a2 0 0 1\n"
                                  "    dup@cave a2 0 0 1\n"
                                  "    dup@cave a1 0 0 1\n"
                                  "    equate a1@cave missing@cave dup@cave plainMissing\n"
                                  "  endcenterline\n"
                                  "  survey 1303\n"
                                  "    survey stara_dvanactka\n"
                                  "      survey 20090809_1\n"
                                  "        centerline\n"
                                  "          data normal from to compass clino tape\n"
                                  "          1.0 - 0 0 1\n"
                                  "          1.0 - 0 0 1\n"
                                  "          1.0 1.1 0 0 1\n"
                                  "        endcenterline\n"
                                  "      endsurvey 20090809_1\n"
                                  "    endsurvey stara_dvanactka\n"
                                  "  endsurvey 1303\n"
                                  "  centerline\n"
                                  "    equate 1.0@20090809_1.stara_dvanactka.1303\n"
                                  "  endcenterline\n"
                                  "  survey totalka\n"
                                  "    centerline\n"
                                  "      fix 7 1152039.23 589091.12 484.10\n"
                                  "    endcenterline\n"
                                  "  endsurvey totalka\n"
                                  "  centerline\n"
                                  "    equate 7@totalka\n"
                                  "  endcenterline\n"
                                  "  survey 1318\n"
                                  "    survey stara_vetrna\n"
                                  "      survey hp\n"
                                  "        centerline\n"
                                  "          data normal from to length compass clino\n"
                                  "          3 4 1 0 0\n"
                                  "          26 27 1 0 0\n"
                                  "          27 28 1 0 0\n"
                                  "        endcenterline\n"
                                  "      endsurvey hp\n"
                                  "      input maps/hp_1318.th2\n"
                                  "    endsurvey stara_vetrna\n"
                                  "    centerline\n"
                                  "      equate 27@hp.stara_vetrna\n"
                                  "    endcenterline\n"
                                  "  endsurvey 1318\n"
                                  "  survey 1319\n"
                                  "    survey hp\n"
                                  "      centerline\n"
                                  "        data normal from to length compass clino\n"
                                  "        3 4 1 0 0\n"
                                  "        4 5 1 0 0\n"
                                  "      endcenterline\n"
                                  "    endsurvey hp\n"
                                  "    input maps/hp_1319.th2\n"
                                  "    centerline\n"
                                  "      equate 4@hp\n"
                                  "    endcenterline\n"
                                  "  endsurvey 1319\n"
                                  "endsurvey cave\n")),
                "The station-reference root Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                              QStringLiteral(
                                  "scrap s1\n"
                                  "point 0 0 station -name dup@cave\n"
                                  "point 1 1 station -name missing-map@cave\n"
                                  "point 2 2 station -name plain-missing\n"
                                  "endscrap\n")),
                "The station-reference map file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/hp_1318.th2")),
                              QStringLiteral(
                                  "scrap s1\n"
                                  "point 0 0 station -name 4@hp\n"
                                  "endscrap\n")),
                "The first namespaced station-reference map file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/hp_1319.th2")),
                              QStringLiteral(
                                  "scrap s1\n"
                                  "point 0 0 station -name 4@hp\n"
                                  "endscrap\n")),
                "The second namespaced station-reference map file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    bool foundMissingStation = false;
    bool foundMissingPointStation = false;
    bool foundPlainMissingPointStation = false;
    for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
        if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownStationReference
            && diagnostic.referencedName == QStringLiteral("missing@cave")
            && diagnostic.lineNumber == 8) {
            foundMissingStation = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownStationReference
            && diagnostic.referencedName == QStringLiteral("missing-map@cave")
            && diagnostic.lineNumber == 3
            && diagnostic.columnLength == QStringLiteral("missing-map@cave").size()) {
            foundMissingPointStation = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::UnknownStationReference
            && diagnostic.referencedName == QStringLiteral("plain-missing")
            && diagnostic.lineNumber == 4
            && diagnostic.columnLength == QStringLiteral("plain-missing").size()) {
            foundPlainMissingPointStation = true;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("a1@cave"),
                    "Resolved equate station references should not produce diagnostics.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("dup@cave"),
                    "Repeated occurrences of the same station reference should not produce ambiguous diagnostics.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("1.0@20090809_1.stara_dvanactka.1303"),
                    "Repeated splay rows for a fully qualified nested station reference should not produce ambiguous diagnostics.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("7@totalka"),
                    "Stations defined by fix commands should resolve as valid station references.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("27@hp.stara_vetrna"),
                    "Nested station references should resolve relative to the current survey context without ambiguous diagnostics.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("4@hp"),
                    "Station references should resolve namespace aliases relative to the current survey context, not globally.")) {
            return 1;
        }
        if (!expect(diagnostic.referencedName != QStringLiteral("plainMissing"),
                    "Plain unresolved equate station tokens should not produce conservative diagnostics.")) {
            return 1;
        }
    }

    if (!expect(foundMissingStation,
                "The project index should report explicit unknown equate station references.")) {
        return 1;
    }
    if (!expect(foundMissingPointStation,
                "The project index should report unknown point station -name references.")) {
        return 1;
    }
    if (!expect(foundPlainMissingPointStation,
                "The project index should report plain unknown point station -name references.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexDuplicateObjectIdDiagnosticsTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary duplicate-object-id project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps")),
                "The temporary duplicate-object-id map directory could not be created.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral(
                                  "survey cave\n"
                                  "  map shared\n"
                                  "  endmap\n"
                                  "  scrap shared\n"
                                  "  endscrap\n"
                                  "  survey shared\n"
                                  "  endsurvey shared\n"
                                  "  input maps/map.th2\n"
                                  "endsurvey cave\n")),
                "The duplicate-object-id root Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                              QStringLiteral(
                                  "scrap s1\n"
                                  "line wall -id line-1\n"
                                  "endline\n"
                                  "line border -id line-1\n"
                                  "endline\n"
                                  "point 0 0 label -id line-1\n"
                                  "endscrap\n")),
                "The duplicate-object-id TH2 file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    bool foundDuplicateLineId = false;
    bool foundDuplicatePointId = false;
    bool foundDuplicateScrapName = false;
    bool foundDuplicateSurveyName = false;
    for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
        if (diagnostic.kind == ProjectIndexDiagnosticKind::DuplicateObjectId
            && diagnostic.referencedName == QStringLiteral("line-1")
            && diagnostic.lineNumber == 4
            && diagnostic.columnNumber == 17) {
            foundDuplicateLineId = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::DuplicateObjectId
            && diagnostic.referencedName == QStringLiteral("line-1")
            && diagnostic.lineNumber == 6
            && diagnostic.columnNumber == 21) {
            foundDuplicatePointId = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::DuplicateObjectId
            && diagnostic.referencedName == QStringLiteral("shared")
            && diagnostic.lineNumber == 4
            && diagnostic.columnNumber == 9) {
            foundDuplicateScrapName = true;
        }
        if (diagnostic.kind == ProjectIndexDiagnosticKind::DuplicateObjectId
            && diagnostic.referencedName == QStringLiteral("shared")
            && diagnostic.lineNumber == 6
            && diagnostic.columnNumber == 10) {
            foundDuplicateSurveyName = true;
        }
    }

    if (!expect(foundDuplicateLineId,
                "The project index should report duplicate explicit line IDs in the same namespace.")) {
        return 1;
    }
    if (!expect(foundDuplicatePointId,
                "The project index should report duplicate explicit IDs across point, line, and area kinds in the same scrap.")) {
        return 1;
    }
    if (!expect(foundDuplicateScrapName && foundDuplicateSurveyName,
                "The project index should report duplicate map, scrap, and survey names in the same namespace.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexThconfigSourceGraphTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("nested")), "The temporary nested project directory could not be created.")) {
        return 1;
    }

    const QString configFile = projectDir.filePath(QStringLiteral("thconfig"));
    const QString sourceFile = projectDir.filePath(QStringLiteral("main.th"));
    const QString nestedConfigFile = projectDir.filePath(QStringLiteral("nested/nested.thconfig"));
    const QString nestedSourceFile = projectDir.filePath(QStringLiteral("nested/nested.th"));
    if (!expect(writeTextFile(configFile,
                              QStringLiteral(
                                  "source main\n")),
                "The temporary thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(sourceFile,
                              QStringLiteral(
                                  "survey from-config\n"
                                  "endsurvey from-config\n")),
                "The temporary thconfig source file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(nestedConfigFile,
                              QStringLiteral(
                                  "source nested.th\n")),
                "The temporary nested thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(nestedSourceFile,
                              QStringLiteral(
                                  "survey nested-config\n"
                                  "endsurvey nested-config\n")),
                "The temporary nested thconfig source file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(snapshot.projectRootPath) == normalizedPathForComparison(projectDir.path()),
                "The thconfig source graph scan should expose the normalized project root path.")) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(snapshot.rootConfigPath) == normalizedPathForComparison(configFile),
                "The thconfig source graph scan should expose the resolved root config path.")) {
        return 1;
    }
    if (!expect(snapshot.rootFilePaths.size() == 1
                    && normalizedPathForComparison(snapshot.rootFilePaths.first()) == normalizedPathForComparison(configFile),
                "The thconfig source graph scan should expose the config as the root traversal file.")) {
        return 1;
    }
    if (!expect(snapshot.entries.size() == 1, "The thconfig source graph scan returned an unexpected entry count.")) {
        return 1;
    }

    const ProjectStructureEntry &entry = snapshot.entries.first();
    if (!expect(entry.kind == ProjectStructureEntryKind::Survey
                    && entry.name == QStringLiteral("from-config")
                    && normalizedPathForComparison(entry.sourceFile) == normalizedPathForComparison(sourceFile),
                "The thconfig source graph scan did not resolve the source target.")) {
        return 1;
    }

    const ProjectIndexSnapshot nestedSnapshot = ProjectStructureIndex::scanProjectIndex(
        projectDir.filePath(QStringLiteral("nested")),
        &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(nestedSnapshot.rootConfigPath) == normalizedPathForComparison(nestedConfigFile),
                "Opening a nested directory as the project root should expose its own root config path.")) {
        return 1;
    }
    if (!expect(nestedSnapshot.entries.size() == 1,
                "Opening a nested directory as the project root should use its root-level config.")) {
        return 1;
    }

    const ProjectStructureEntry &nestedEntry = nestedSnapshot.entries.first();
    if (!expect(nestedEntry.kind == ProjectStructureEntryKind::Survey
                    && nestedEntry.name == QStringLiteral("nested-config")
                    && normalizedPathForComparison(nestedEntry.sourceFile) == normalizedPathForComparison(nestedSourceFile),
                "The nested project root did not resolve its own thconfig source graph.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexRootConfigDisambiguationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("alpha.thconfig")),
                              QStringLiteral("source alpha.th\n")),
                "The first root thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("beta.thconfig")),
                              QStringLiteral("source beta.th\n")),
                "The second root thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("alpha.th")),
                              QStringLiteral(
                                  "survey alpha\n"
                                  "endsurvey alpha\n")),
                "The alpha source file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("beta.th")),
                              QStringLiteral(
                                  "survey beta\n"
                                  "endsurvey beta\n")),
                "The beta source file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot ambiguousSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                           &errorMessage);
    if (!expect(!errorMessage.isEmpty(),
                "Multiple root .thconfig files should require an explicit project target config.")) {
        return 1;
    }
    if (!expect(ambiguousSnapshot.entries.isEmpty(),
                "The project index should not silently merge multiple root .thconfig graphs.")) {
        return 1;
    }
    if (!expect(ambiguousSnapshot.rootConfigPath.isEmpty() && ambiguousSnapshot.rootFilePaths.isEmpty(),
                "An ambiguous project index should not expose a resolved root config or traversal root.")) {
        return 1;
    }

    const ProjectIndexSnapshot betaSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                      QHash<QString, QString>(),
                                                                                      QStringLiteral("beta.thconfig"),
                                                                                      &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(betaSnapshot.rootConfigPath)
                    == normalizedPathForComparison(projectDir.filePath(QStringLiteral("beta.thconfig"))),
                "The preferred project target config should be exposed as the resolved root config.")) {
        return 1;
    }
    if (!expect(betaSnapshot.entries.size() == 1,
                "The preferred project target config should select one root graph.")) {
        return 1;
    }
    if (!expect(betaSnapshot.entries.first().kind == ProjectStructureEntryKind::Survey
                    && betaSnapshot.entries.first().name == QStringLiteral("beta"),
                "The preferred project target config did not select the expected graph.")) {
        return 1;
    }

    const QString projectNamedConfigPath = projectDir.filePath(QFileInfo(projectDir.path()).fileName()
                                                               + QStringLiteral(".thconfig"));
    if (!expect(writeTextFile(projectNamedConfigPath,
                              QStringLiteral("source beta.th\n")),
                "The project-named thconfig file could not be written.")) {
        return 1;
    }

    const ProjectIndexSnapshot projectNamedSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                              &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(projectNamedSnapshot.rootConfigPath)
                    == normalizedPathForComparison(projectNamedConfigPath),
                "The project-named thconfig should disambiguate root config files when no higher-priority config exists.")) {
        return 1;
    }
    if (!expect(projectNamedSnapshot.entries.size() == 1
                    && projectNamedSnapshot.entries.first().name == QStringLiteral("beta"),
                "The project-named thconfig file should select the expected source graph.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("thconfig")),
                              QStringLiteral("source alpha.th\n")),
                "The default root thconfig file could not be written.")) {
        return 1;
    }

    const ProjectIndexSnapshot defaultSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                         &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(defaultSnapshot.rootConfigPath)
                    == normalizedPathForComparison(projectDir.filePath(QStringLiteral("thconfig"))),
                "A root thconfig file should be exposed as the resolved default root config.")) {
        return 1;
    }
    if (!expect(defaultSnapshot.entries.size() == 1
                    && defaultSnapshot.entries.first().name == QStringLiteral("alpha"),
                "A root thconfig file should be the default project graph when no preferred config is set.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("index.thconfig")),
                              QStringLiteral("source beta.th\n")),
                "The index.thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("thconfig.thconfig")),
                              QStringLiteral("source alpha.th\n")),
                "The thconfig.thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("main.thconfig")),
                              QStringLiteral("source beta.th\n")),
                "The main.thconfig file could not be written.")) {
        return 1;
    }

    const ProjectIndexSnapshot preferredNameSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                               &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(preferredNameSnapshot.rootConfigPath)
                    == normalizedPathForComparison(projectDir.filePath(QStringLiteral("thconfig"))),
                "thconfig should win the default project config priority order.")) {
        return 1;
    }
    if (!expect(preferredNameSnapshot.entries.size() == 1
                    && preferredNameSnapshot.entries.first().name == QStringLiteral("alpha"),
                "The prioritized thconfig file should select the expected source graph.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexThconfigWorkRootTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    const QString configFile = projectDir.filePath(QStringLiteral("thconfig.work"));
    const QString sourceFile = projectDir.filePath(QStringLiteral("work.th"));
    if (!expect(writeTextFile(configFile,
                              QStringLiteral("source work.th\n")),
                "The thconfig.work file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(sourceFile,
                              QStringLiteral(
                                  "survey work\n"
                                  "endsurvey work\n")),
                "The thconfig.work source file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(snapshot.rootConfigPath) == normalizedPathForComparison(configFile),
                "A single thconfig.* file should be accepted as the project root config.")) {
        return 1;
    }
    if (!expect(snapshot.entries.size() == 1
                    && snapshot.entries.first().kind == ProjectStructureEntryKind::Survey
                    && snapshot.entries.first().name == QStringLiteral("work"),
                "The thconfig.work root config did not select the expected source graph.")) {
        return 1;
    }

    return 0;
}

int runTh2ObjectIndexGroupingTest()
{
    const QVector<ProjectStructureEntry> entries = ProjectStructureIndex::scanTh2Objects(
        QStringLiteral("/tmp/example.th2"),
        QStringLiteral(
            "scrap s1\n"
            "point 0 0 station -name a1\n"
            "point 1 1 station -name a2\n"
            "endscrap\n"));

    if (!expect(entries.size() == 3, "The TH2 object scan should not duplicate the current scrap group.")) {
        return 1;
    }
    if (!expect(entries.at(0).kind == ProjectStructureEntryKind::Scrap
                    && entries.at(1).kind == ProjectStructureEntryKind::Station
                    && entries.at(2).kind == ProjectStructureEntryKind::Station,
                "The TH2 object scan returned unexpected entry kinds.")) {
        return 1;
    }
    if (!expect(!entries.at(0).objectId.isEmpty(), "The TH2 scrap object ID should not be empty.")) {
        return 1;
    }
    if (!expect(entries.at(1).parentObjectId == entries.at(0).objectId
                    && entries.at(2).parentObjectId == entries.at(0).objectId,
                "The TH2 object scan should attach object entries to the current scrap object ID.")) {
        return 1;
    }

    const QVector<ProjectStructureEntry> shiftedEntries = ProjectStructureIndex::scanTh2Objects(
        QStringLiteral("/tmp/example.th2"),
        QStringLiteral(
            "\n"
            "scrap s1\n"
            "point 0 0 station -name a1\n"
            "point 1 1 station -name a2\n"
            "endscrap\n"));

    if (!expect(shiftedEntries.size() == entries.size(), "The shifted TH2 object scan returned an unexpected entry count.")) {
        return 1;
    }
    if (!expect(shiftedEntries.at(1).objectId == entries.at(1).objectId,
                "The TH2 object ID should stay stable when source line numbers shift.")) {
        return 1;
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(QStringLiteral(
        "scrap s1\n"
        "point 0 0 station -name a1\n"
        "point 1 1 station -name a2\n"
        "endscrap\n"));
    const QVector<ProjectStructureEntry> parsedLineEntries = ProjectStructureIndex::scanTh2Objects(
        QStringLiteral("/tmp/example.th2"),
        parsedLines);
    if (!expect(parsedLineEntries.size() == entries.size(),
                "The parsed-line TH2 object scan should match the text-based scan entry count.")) {
        return 1;
    }
    if (!expect(parsedLineEntries.at(1).objectId == entries.at(1).objectId,
                "The parsed-line TH2 object scan should preserve stable object IDs.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexSourceSetUsesProvidedTextTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary source-set project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    const QString rootFile = projectDir.filePath(QStringLiteral("root.th"));
    if (!expect(writeTextFile(rootFile,
                              QStringLiteral(
                                  "survey stale\n"
                                  "endsurvey stale\n")),
                "The source-set disk Therion file could not be written.")) {
        return 1;
    }

    ProjectStructureIndexSourceSet sourceSet;
    sourceSet.projectRootPath = projectDir.path();
    sourceSet.sources.append({normalizedPathForComparison(rootFile),
                              QStringLiteral(
                                  "survey snapshot\n"
                                  "endsurvey snapshot\n"),
                              true,
                              nullptr});

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(sourceSet, &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    bool foundSnapshotSurvey = false;
    bool foundStaleSurvey = false;
    for (const ProjectStructureEntry &entry : snapshot.entries) {
        if (entry.category != QStringLiteral("Surveys")) {
            continue;
        }
        if (entry.name == QStringLiteral("snapshot")) {
            foundSnapshotSurvey = true;
        }
        if (entry.name == QStringLiteral("stale")) {
            foundStaleSurvey = true;
        }
    }

    if (!expect(foundSnapshotSurvey, "Source-set project index should use provided snapshot text.")) {
        return 1;
    }
    if (!expect(!foundStaleSurvey, "Source-set project index should not reread stale disk text.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexSourceSetCanCancelScanTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary cancellable source-set project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    const QString rootFile = projectDir.filePath(QStringLiteral("root.th"));
    if (!expect(writeTextFile(rootFile,
                              QStringLiteral(
                                  "survey cave\n"
                                  "endsurvey cave\n")),
                "The cancellable source-set Therion file could not be written.")) {
        return 1;
    }

    ProjectStructureIndexSourceSet sourceSet;
    sourceSet.projectRootPath = projectDir.path();
    sourceSet.sources.append({normalizedPathForComparison(rootFile),
                              QStringLiteral(
                                  "survey cave\n"
                                  "endsurvey cave\n"),
                              true,
                              nullptr});
    sourceSet.shouldCancel = []() {
        return true;
    };

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(sourceSet, &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(snapshot.canceled, "Source-set project index should report canceled scans.")) {
        return 1;
    }
    if (!expect(snapshot.entries.isEmpty(), "Canceled source-set project index should not expose partial entries.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runProjectStructureHierarchyTest() != 0) {
        return 1;
    }
    if (runProjectStructureContinuedInputTest() != 0) {
        return 1;
    }
    if (runProjectIndexMapScrapReferenceTest() != 0) {
        return 1;
    }
    if (runProjectIndexNamespacedMapReferenceTest() != 0) {
        return 1;
    }
    if (runProjectIndexAmbiguousMapReferenceDiagnosticsTest() != 0) {
        return 1;
    }
    if (runProjectIndexJoinReferenceDiagnosticsTest() != 0) {
        return 1;
    }
    if (runProjectIndexStationReferenceDiagnosticsTest() != 0) {
        return 1;
    }
    if (runProjectIndexDuplicateObjectIdDiagnosticsTest() != 0) {
        return 1;
    }
    if (runProjectIndexThconfigSourceGraphTest() != 0) {
        return 1;
    }
    if (runProjectIndexRootConfigDisambiguationTest() != 0) {
        return 1;
    }
    if (runProjectIndexThconfigWorkRootTest() != 0) {
        return 1;
    }
    if (runTh2ObjectIndexGroupingTest() != 0) {
        return 1;
    }
    if (runProjectIndexSourceSetUsesProvidedTextTest() != 0) {
        return 1;
    }
    if (runProjectIndexSourceSetCanCancelScanTest() != 0) {
        return 1;
    }

    return 0;
}
