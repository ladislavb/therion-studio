#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace TherionStudio
{
struct TherionParsedLine;
struct TherionSourceLogicalCommand;
class TherionSourceLogicalDocument;

enum class ProjectStructureEntryKind
{
    Unknown,
    Survey,
    Centreline,
    Map,
    Scrap,
    Station,
    Point,
    Line,
    Area,
};

enum class ProjectIndexDiagnosticKind
{
    UnknownMapScrapReference,
    UnknownMapReference,
    AmbiguousMapScrapReference,
    AmbiguousMapReference,
    MixedMapAndScrapReferences,
    UnknownJoinReference,
    UnknownJoinLinePointMark,
    AmbiguousJoinReference,
    UnknownStationReference,
    AmbiguousStationReference,
    DuplicateObjectId,
};

enum class ProjectStationReferenceResolutionState
{
    Missing,
    Unique,
    Ambiguous,
};

struct ProjectStationReferenceResolution
{
    ProjectStationReferenceResolutionState state = ProjectStationReferenceResolutionState::Missing;
    int candidateCount = 0;
};

struct ProjectStructureEntry
{
    ProjectStructureEntryKind kind = ProjectStructureEntryKind::Unknown;
    QString objectId;
    QString parentObjectId;
    QString category;
    QString name;
    QString namespacePath;
    QString sourceFile;
    int lineNumber = 0;
    int depth = 0;
    bool createsNamespace = true;
    QSet<QString> linePointMarks;
};

struct ProjectIndexDiagnostic
{
    ProjectIndexDiagnosticKind kind = ProjectIndexDiagnosticKind::UnknownMapScrapReference;
    QString sourceObjectId;
    QString sourceFile;
    int lineNumber = 0;
    int columnNumber = 1;
    int columnLength = 0;
    QString referencedName;
    int candidateCount = 0;
};

struct ProjectStructureIndexScanStats
{
    int logicalDocumentBuilds = 0;
    int logicalDocumentHits = 0;
    int prebuiltLogicalDocumentHits = 0;
};

struct ProjectIndexSnapshot
{
    QString projectRootPath;
    QString rootConfigPath;
    QVector<QString> rootFilePaths;
    QVector<ProjectStructureEntry> entries;
    QHash<QString, QSet<QString>> mapScrapReferencesByMapKey;
    QHash<QString, QSet<QString>> mapChildReferencesByMapKey;
    QHash<QString, QSet<QString>> mapPreviewReferencesByMapKey;
    QHash<QString, QSet<QString>> stationReferenceCandidateKeysByLookupKey;
    QVector<ProjectIndexDiagnostic> diagnostics;
    ProjectStructureIndexScanStats scanStats;
    bool canceled = false;
};

struct ProjectStructureIndexSource
{
    QString normalizedPath;
    QString text;
    bool textLoaded = true;
    std::shared_ptr<const TherionSourceLogicalDocument> logicalDocument;
};

struct ProjectStructureIndexSourceSet
{
    QString projectRootPath;
    QString preferredConfigPath;
    QVector<ProjectStructureIndexSource> sources;
    std::function<bool()> shouldCancel;
};

class ProjectStructureIndex final
{
public:
    static ProjectIndexSnapshot scanProjectIndex(const QString &projectRootPath,
                                                 QString *errorMessage = nullptr);
    static ProjectIndexSnapshot scanProjectIndex(const QString &projectRootPath,
                                                 const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                 QString *errorMessage = nullptr);
    static ProjectIndexSnapshot scanProjectIndex(const QString &projectRootPath,
                                                 const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                 const QString &preferredConfigPath,
                                                 QString *errorMessage = nullptr);
    static ProjectIndexSnapshot scanProjectIndex(const ProjectStructureIndexSourceSet &sourceSet,
                                                 QString *errorMessage = nullptr);
    static ProjectStationReferenceResolution resolveStationReference(
        const ProjectIndexSnapshot &snapshot,
        const QString &referenceName,
        const QString &ownerNamespacePath = {});
    static QVector<ProjectStructureEntry> scanProject(const QString &projectRootPath, QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanProject(const QString &projectRootPath,
                                                      const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                      QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanTh2Objects(const QString &sourceFile, const QString &text);
    static QVector<ProjectStructureEntry> scanTh2Objects(const QString &sourceFile,
                                                         const QVector<TherionParsedLine> &parsedLines);
    static QVector<ProjectStructureEntry> scanTh2Objects(const QString &sourceFile,
                                                         const QVector<TherionSourceLogicalCommand> &commands);
    static QString structureEntryNodeKey(const ProjectStructureEntry &entry);
};
}
