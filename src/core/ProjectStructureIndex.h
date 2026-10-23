#pragma once

#include <QHash>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

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
    // Join rules between one pair of scraps (see ProjectJoinRules).
    MixedJoinKinds,
    RepeatedScrapJoin,
    SinglePointLineJoin,
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

struct ProjectEquateStation
{
    QString reference;
    QString stationKey;
    QString namespacePath;
    bool resolved = false;
};

enum class ProjectStationLinkKind
{
    Equate,
    // A `data nosurvey` leg: the network is computed from other data and the two stations are then connected.
    NoSurvey,
};

// One `equate` command, or one `data nosurvey` leg, whose stations resolve to more than one survey namespace.
struct ProjectEquateLink
{
    QString sourceFile;
    int lineNumber = 0;
    QVector<ProjectEquateStation> stations;
    ProjectStationLinkKind kind = ProjectStationLinkKind::Equate;
};

enum class ProjectJoinMode
{
    // Two scrap IDs: Therion joins the closest scrap ends (`-count N` asks for N places).
    Scraps,
    // Point or line IDs, optionally with a line point mark (`id:mark`, `:0`, `:end`).
    PointsAndLines,
};

struct ProjectJoinItem
{
    QString reference;
    QString linePointMark;
    ProjectStructureEntryKind kind = ProjectStructureEntryKind::Unknown;
    QString objectId;
    // The scrap itself, or the scrap that contains the joined line or point.
    QString scrapObjectId;
    QString namespacePath;
    bool resolved = false;
};

// One `join` command with its items resolved to scraps, lines, or points.
struct ProjectJoinCommand
{
    QString sourceFile;
    int lineNumber = 0;
    ProjectJoinMode mode = ProjectJoinMode::PointsAndLines;
    // `-count N` for scrap joins; 0 when absent (Therion's automatic join).
    int count = 0;
    QVector<ProjectJoinItem> items;
    // The `join` keyword, and the namespace object that owns the command.
    int columnNumber = 1;
    int columnLength = 0;
    QString ownerObjectId;
};

enum class ProjectJoinRuleViolationKind
{
    // A scrap join and a point/line join between the same scraps.
    MixedJoinKinds,
    // More than one scrap join between the same scraps.
    RepeatedScrapJoin,
    // Only one point/line join between two scraps.
    SinglePointLineJoin,
};

struct ProjectJoinRuleViolation
{
    ProjectJoinRuleViolationKind kind = ProjectJoinRuleViolationKind::MixedJoinKinds;
    QString firstScrapObjectId;
    QString secondScrapObjectId;
    QVector<ProjectJoinCommand> joins;
};

// A `point ... station -name` in a `.th2` scrap that draws an equated station.
struct ProjectStationDrawingPoint
{
    QString stationKey;
    QString sourceFile;
    int lineNumber = 0;
    QString scrapName;
    QString surveyName;
    QString projection;
    QString scrapObjectId;
};

// A `point ... station -name` of a scrap, by line, with the station it resolves to.
struct ProjectScrapStationPoint
{
    int lineNumber = 0;
    QString stationKey;
    // Drawing position, when the point's coordinates are numbers.
    std::optional<QPointF> position;
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
    QVector<ProjectEquateLink> equateLinks;
    QHash<QString, QVector<ProjectStationDrawingPoint>> stationDrawingPointsByStationKey;
    QVector<ProjectJoinCommand> joinCommands;
    QVector<ProjectJoinRuleViolation> joinRuleViolations;
    // Every station point of every scrap that has a projection, for fitting scraps to computed station positions.
    QHash<QString, QVector<ProjectScrapStationPoint>> stationPointsByScrapObjectId;
    // Normalized `-projection` of every scrap, by scrap object ID.
    QHash<QString, QString> scrapProjectionByObjectId;
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
    // Station key of `name` in the survey namespace `namespacePath` (innermost first, as Therion writes
    // `survey.parent`), matching ProjectEquateStation::stationKey.
    static QString stationKey(const QString &name, const QString &namespacePath);
};
}
