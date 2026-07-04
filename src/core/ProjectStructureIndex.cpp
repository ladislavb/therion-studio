#include "ProjectStructureIndex.h"

#include "DocumentFile.h"
#include "TherionCommandLineModel.h"
#include "TherionFileTypes.h"
#include "TherionSourceLogicalDocument.h"
#include "TherionSourceReferenceResolver.h"
#include "TherionSourceSnapshotCache.h"
#include "TherionTokenRules.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <functional>
#include <optional>

namespace TherionStudio
{
namespace
{
struct ProjectBlock
{
    ProjectStructureEntryKind kind = ProjectStructureEntryKind::Unknown;
    QString name;
    QString objectId;
    QString namespacePath;
    bool createsNamespace = true;
};

struct ParsedFileCache
{
    QHash<QString, std::shared_ptr<const TherionSourceLogicalDocument>> prebuiltLogicalDocuments;
    QHash<QString, TherionSourceLogicalDocument> logicalDocuments;
    TherionSourceSnapshotCache sourceSnapshotCache;
    int sourceRevisionCounter = 0;
    ProjectStructureIndexScanStats stats;
};

struct ProjectObjectIdentityGenerator
{
    QHash<QString, int> siblingOrdinalsByScope;

    QString nextObjectId(const QString &category,
                         const QString &name,
                         const QString &sourceFile,
                         const QString &parentObjectId);
};

struct MapReferenceScanResult
{
    QHash<QString, QSet<QString>> scrapReferencesByMapKey;
    QHash<QString, QSet<QString>> childMapReferencesByMapKey;
    QHash<QString, QSet<QString>> previewReferencesByMapKey;
    QVector<ProjectIndexDiagnostic> diagnostics;
};

struct RootConfigResolution
{
    QVector<QString> rootFiles;
    QString configPath;
    QString errorMessage;
};

using NamespaceEntriesByFile = QHash<QString, QVector<ProjectStructureEntry>>;

QString normalizedFilePathKey(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(path);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    const QString resolvedPath = canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
    return QFileInfo(resolvedPath).absoluteFilePath();
}

bool pathIsInsideDirectory(const QString &path, const QString &directoryPath)
{
    if (path.isEmpty() || directoryPath.isEmpty()) {
        return false;
    }

    const QString relativePath = QDir(directoryPath).relativeFilePath(path);
    return relativePath != QStringLiteral("..")
        && !relativePath.startsWith(QStringLiteral("../"))
        && !relativePath.startsWith(QStringLiteral("..\\"))
        && !QDir::isAbsolutePath(relativePath);
}

TherionSourceLogicalDocument logicalDocumentForText(const QString &text,
                                                   TherionSourceSnapshotCache &sourceSnapshotCache,
                                                   TherionSourceDocumentMetadata metadata)
{
    return sourceSnapshotCache.logicalDocument(text, metadata);
}

QString sectionNameFromLine(const TherionParsedLine &parsedLine);
ProjectStructureEntryKind objectKindFromLine(const TherionParsedLine &parsedLine);
QString objectNameFromLine(const TherionParsedLine &parsedLine);
QString normalizedStructureDirective(const QString &directive);
bool projectIndexScanShouldCancel(const std::function<bool()> &shouldCancel);
MapReferenceScanResult scanMapReferences(const QVector<ProjectStructureEntry> &entries,
                                         ParsedFileCache *cache,
                                         const QHash<QString, QString> &inMemoryFileContentsByPath,
                                         const std::function<bool()> &shouldCancel);
QVector<ProjectIndexDiagnostic> scanJoinReferences(const QVector<ProjectStructureEntry> &entries,
                                                   ParsedFileCache *cache,
                                                   const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                   const std::function<bool()> &shouldCancel);
QVector<ProjectIndexDiagnostic> scanStationReferences(const QVector<ProjectStructureEntry> &entries,
                                                      ParsedFileCache *cache,
                                                      const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                      const std::function<bool()> &shouldCancel);
QVector<ProjectIndexDiagnostic> scanDuplicateObjectIds(const QVector<ProjectStructureEntry> &entries,
                                                       ParsedFileCache *cache,
                                                       const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                       const std::function<bool()> &shouldCancel);
void appendProjectIndexDiagnostic(QVector<ProjectIndexDiagnostic> *diagnostics,
                                  ProjectIndexDiagnosticKind kind,
                                  const QString &sourceObjectId,
                                  const QString &sourceFile,
                                  int lineNumber,
                                  int columnNumber,
                                  int columnLength,
                                  const QString &referencedName,
                                  int candidateCount = 0);

QString normalizedIdentityToken(const QString &value)
{
    QString normalized = value.trimmed().toLower();
    normalized.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return normalized;
}

bool projectIndexScanShouldCancel(const std::function<bool()> &shouldCancel)
{
    return shouldCancel && shouldCancel();
}

QString categoryIdentityToken(const QString &category)
{
    QString normalized = normalizedIdentityToken(category);
    normalized.replace(QLatin1Char(' '), QLatin1Char('-'));
    return normalized;
}

QString objectNameIdentityToken(const QString &name)
{
    const QString normalized = normalizedIdentityToken(name);
    return normalized.isEmpty() ? QStringLiteral("anonymous") : normalized;
}

QString mapReferenceLookupKey(QString referenceName)
{
    referenceName = referenceName.trimmed();
    const int namespaceIndex = referenceName.indexOf(QLatin1Char('@'));
    if (namespaceIndex >= 0) {
        referenceName = referenceName.left(namespaceIndex);
    }

    return referenceName.trimmed().toLower();
}

QString normalizedNamespaceToken(QString namespacePath)
{
    namespacePath = namespacePath.trimmed().toLower();
    namespacePath.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QString());
    return namespacePath;
}

QString namespacePathWithChild(const QString &currentNamespacePath, const QString &childName)
{
    const QString child = childName.trimmed();
    if (child.isEmpty()) {
        return currentNamespacePath;
    }
    if (currentNamespacePath.trimmed().isEmpty()) {
        return child;
    }

    return QStringLiteral("%1.%2").arg(child, currentNamespacePath);
}

QStringList namespacePathComponents(const QString &namespacePath)
{
    QStringList components;
    for (const QString &component : namespacePath.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        const QString trimmedComponent = component.trimmed();
        if (!trimmedComponent.isEmpty()) {
            components.append(trimmedComponent);
        }
    }
    return components;
}

QString joinedNamespacePath(const QStringList &components)
{
    return components.join(QLatin1Char('.'));
}

bool namespaceComponentsEqual(const QString &left, const QString &right)
{
    return normalizedNamespaceToken(left) == normalizedNamespaceToken(right);
}

QString namespacePathForReference(const QString &ownerNamespacePath, const QString &referenceNamespacePath)
{
    const QStringList ownerComponents = namespacePathComponents(ownerNamespacePath);
    const QStringList referenceComponents = namespacePathComponents(referenceNamespacePath);
    if (referenceComponents.isEmpty()) {
        return joinedNamespacePath(ownerComponents);
    }
    if (ownerComponents.isEmpty()) {
        return joinedNamespacePath(referenceComponents);
    }

    const int directCount = std::min(ownerComponents.size(), referenceComponents.size());
    bool ownerStartsWithReference = true;
    for (int index = 0; index < directCount; ++index) {
        if (!namespaceComponentsEqual(ownerComponents.at(index), referenceComponents.at(index))) {
            ownerStartsWithReference = false;
            break;
        }
    }
    if (ownerStartsWithReference && directCount == referenceComponents.size()) {
        return joinedNamespacePath(ownerComponents);
    }

    int overlap = std::min(ownerComponents.size(), referenceComponents.size());
    while (overlap > 0) {
        bool matches = true;
        for (int index = 0; index < overlap; ++index) {
            const QString &referenceComponent = referenceComponents.at(referenceComponents.size() - overlap + index);
            const QString &ownerComponent = ownerComponents.at(index);
            if (!namespaceComponentsEqual(referenceComponent, ownerComponent)) {
                matches = false;
                break;
            }
        }
        if (matches) {
            break;
        }
        --overlap;
    }

    QStringList combinedComponents = referenceComponents;
    for (int index = overlap; index < ownerComponents.size(); ++index) {
        combinedComponents.append(ownerComponents.at(index));
    }
    return joinedNamespacePath(combinedComponents);
}

struct MapReferenceParts
{
    QString name;
    QString namespacePath;
    bool hasNamespace = false;
};

MapReferenceParts parseMapReference(const QString &referenceName)
{
    MapReferenceParts parts;
    const QString trimmedReference = referenceName.trimmed();
    const int namespaceIndex = trimmedReference.indexOf(QLatin1Char('@'));
    if (namespaceIndex < 0) {
        parts.name = trimmedReference;
        return parts;
    }

    parts.name = trimmedReference.left(namespaceIndex).trimmed();
    parts.namespacePath = trimmedReference.mid(namespaceIndex + 1).trimmed();
    parts.hasNamespace = true;
    return parts;
}

QString namespacedReferenceLookupKey(const QString &name, const QString &namespacePath)
{
    return QStringLiteral("%1@%2").arg(mapReferenceLookupKey(name), normalizedNamespaceToken(namespacePath));
}

struct ReferenceKeyIndex
{
    QHash<QString, QVector<QString>> keysByNameAndNamespace;
};

struct StationReferenceIndex
{
    QHash<QString, QVector<QString>> keysByExactReference;
    ReferenceKeyIndex keysByNameAndNamespace;
};

enum class ReferenceResolutionState
{
    Missing,
    Unique,
    Ambiguous,
};

enum class MapCompositionContentKind
{
    Unknown,
    Map,
    Scrap,
};

struct ReferenceResolution
{
    ReferenceResolutionState state = ReferenceResolutionState::Missing;
    QString key;
    int candidateCount = 0;
};

struct JoinReferenceParts
{
    QString lookupName;
    QString linePointMark;
    bool requiresLinePointMark = false;
};

ReferenceKeyIndex entryKeysByReferenceName(const QVector<ProjectStructureEntry> &entries,
                                           ProjectStructureEntryKind kind)
{
    ReferenceKeyIndex entryKeys;
    for (const ProjectStructureEntry &entry : entries) {
        if (entry.kind != kind || entry.name.trimmed().isEmpty()) {
            continue;
        }

        const QString nodeKey = ProjectStructureIndex::structureEntryNodeKey(entry);
        entryKeys.keysByNameAndNamespace[namespacedReferenceLookupKey(entry.name, entry.namespacePath)].append(nodeKey);
    }

    return entryKeys;
}

ReferenceKeyIndex mergedReferenceKeysByReferenceName(const QVector<ProjectStructureEntry> &entries,
                                                     std::initializer_list<ProjectStructureEntryKind> kinds)
{
    ReferenceKeyIndex mergedKeys;
    for (const ProjectStructureEntryKind kind : kinds) {
        const ReferenceKeyIndex kindKeys = entryKeysByReferenceName(entries, kind);
        for (auto it = kindKeys.keysByNameAndNamespace.constBegin();
             it != kindKeys.keysByNameAndNamespace.constEnd();
             ++it) {
            mergedKeys.keysByNameAndNamespace[it.key()].append(it.value());
        }
    }
    return mergedKeys;
}

QHash<QString, ProjectStructureEntry> entriesByStructureKey(const QVector<ProjectStructureEntry> &entries)
{
    QHash<QString, ProjectStructureEntry> entriesByKey;
    for (const ProjectStructureEntry &entry : entries) {
        entriesByKey.insert(ProjectStructureIndex::structureEntryNodeKey(entry), entry);
    }
    return entriesByKey;
}

ReferenceResolution resolveCandidates(const QVector<QString> &entryKeys)
{
    QSet<QString> uniqueKeys;
    QVector<QString> orderedUniqueKeys;
    for (const QString &entryKey : entryKeys) {
        if (uniqueKeys.contains(entryKey)) {
            continue;
        }
        uniqueKeys.insert(entryKey);
        orderedUniqueKeys.append(entryKey);
    }

    if (orderedUniqueKeys.size() == 1) {
        return ReferenceResolution{ReferenceResolutionState::Unique, orderedUniqueKeys.first(), 1};
    }
    if (orderedUniqueKeys.size() > 1) {
        return ReferenceResolution{ReferenceResolutionState::Ambiguous, QString(), static_cast<int>(orderedUniqueKeys.size())};
    }

    return {};
}

ReferenceResolution resolveReferenceKey(const ReferenceKeyIndex &entryKeys,
                                        const QString &referenceName,
                                        const QString &ownerNamespacePath)
{
    const MapReferenceParts reference = parseMapReference(referenceName);
    if (reference.name.isEmpty()) {
        return {};
    }

    const QString targetNamespacePath = reference.hasNamespace
        ? namespacePathWithChild(ownerNamespacePath, reference.namespacePath)
        : ownerNamespacePath;

    return resolveCandidates(
        entryKeys.keysByNameAndNamespace.value(namespacedReferenceLookupKey(reference.name, targetNamespacePath)));
}

bool isBuiltInJoinLinePointMark(const QString &markName)
{
    const QString normalizedMark = markName.trimmed().toLower();
    if (normalizedMark == QStringLiteral("end")) {
        return true;
    }

    static const QRegularExpression numericMarker(QStringLiteral(R"(^\d+$)"));
    return numericMarker.match(normalizedMark).hasMatch();
}

JoinReferenceParts parseJoinReference(QString referenceName)
{
    JoinReferenceParts parts;
    referenceName = referenceName.trimmed();

    const int namespaceIndex = referenceName.indexOf(QLatin1Char('@'));
    const int markerIndex = referenceName.indexOf(QLatin1Char(':'));
    if (markerIndex < 0) {
        parts.lookupName = referenceName;
        return parts;
    }

    if (namespaceIndex >= 0 && markerIndex > namespaceIndex) {
        parts.lookupName = referenceName.left(markerIndex).trimmed();
        parts.linePointMark = referenceName.mid(markerIndex + 1).trimmed();
    } else {
        const int markerLength = namespaceIndex < 0
            ? referenceName.size() - markerIndex
            : namespaceIndex - markerIndex;
        parts.lookupName = referenceName;
        parts.linePointMark = referenceName.mid(markerIndex + 1, markerLength - 1).trimmed();
        parts.lookupName.remove(markerIndex, markerLength);
        parts.lookupName = parts.lookupName.trimmed();
    }

    parts.requiresLinePointMark = !parts.linePointMark.isEmpty()
        && !isBuiltInJoinLinePointMark(parts.linePointMark);
    return parts;
}

bool joinReferenceShouldReportMissing(const QString &referenceName)
{
    const JoinReferenceParts referenceParts = parseJoinReference(referenceName);
    const QString lookupName = referenceParts.lookupName;
    const QString nameOnly = mapReferenceLookupKey(lookupName);
    return !referenceParts.linePointMark.isEmpty()
        || lookupName.contains(QLatin1Char('@'))
        || lookupName.contains(QLatin1Char(':'))
        || nameOnly.endsWith(QStringLiteral(".s"))
        || nameOnly.endsWith(QStringLiteral(".m"));
}

QString normalizedStationReferenceToken(QString token)
{
    token = token.trimmed().toLower();
    token.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QString());
    return token;
}

QString stationReferenceCandidateKey(const QString &referenceName, const QString &ownerNamespacePath)
{
    const MapReferenceParts reference = parseMapReference(referenceName);
    if (reference.name.isEmpty()) {
        return QString();
    }
    const QString namespacePath = reference.hasNamespace
        ? namespacePathForReference(ownerNamespacePath, reference.namespacePath)
        : ownerNamespacePath;
    return namespacePath.trimmed().isEmpty()
        ? normalizedStationReferenceToken(reference.name)
        : normalizedStationReferenceToken(QStringLiteral("%1@%2").arg(reference.name, namespacePath));
}

QString stationReferenceDefinitionKey(const QString &referenceName, const QString &ownerNamespacePath)
{
    return stationReferenceCandidateKey(referenceName, ownerNamespacePath);
}

QStringList stationReferenceLookupKeys(const QString &referenceName, const QString &ownerNamespacePath)
{
    QStringList keys;
    const QString contextualKey = stationReferenceCandidateKey(referenceName, ownerNamespacePath);
    if (!contextualKey.isEmpty()) {
        keys.append(contextualKey);
    }

    keys.removeDuplicates();
    return keys;
}

void addStationReferenceKey(StationReferenceIndex *index,
                            const QString &referenceName,
                            const QString &ownerNamespacePath,
                            const QString &entryKey)
{
    if (index == nullptr || referenceName.trimmed().isEmpty()) {
        return;
    }

    const QString candidateKey = stationReferenceDefinitionKey(referenceName, ownerNamespacePath);
    if (candidateKey.isEmpty()) {
        return;
    }

    Q_UNUSED(entryKey)
    index->keysByExactReference[candidateKey].append(candidateKey);

    const MapReferenceParts reference = parseMapReference(referenceName);
    if (reference.name.isEmpty()) {
        return;
    }
    const QString targetNamespacePath = reference.hasNamespace
        ? namespacePathForReference(ownerNamespacePath, reference.namespacePath)
        : ownerNamespacePath;
    index->keysByNameAndNamespace.keysByNameAndNamespace[
        namespacedReferenceLookupKey(reference.name, targetNamespacePath)].append(candidateKey);
}

ReferenceResolution resolveStationReference(const StationReferenceIndex &index,
                                            const QString &referenceName,
                                            const QString &ownerNamespacePath)
{
    QVector<QString> candidates;
    for (const QString &lookupKey : stationReferenceLookupKeys(referenceName, ownerNamespacePath)) {
        candidates += index.keysByExactReference.value(lookupKey);
    }
    return resolveCandidates(candidates);
}

bool stationReferenceShouldReportMissing(const QString &referenceName)
{
    return referenceName.contains(QLatin1Char('@'));
}

std::optional<TherionSourceLogicalArgumentRange> pointStationNameReferenceRange(
    const TherionSourceLogicalCommand &command)
{
    if (command.metadata.commandName != QStringLiteral("point")
        || command.positionalArgumentRanges.size() < 3) {
        return std::nullopt;
    }

    const QString pointType = command.positionalArgumentRanges.at(2).text.trimmed().section(QLatin1Char(':'), 0, 0);
    if (pointType.compare(QStringLiteral("station"), Qt::CaseInsensitive) != 0) {
        return std::nullopt;
    }

    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        if (normalizedCommandOptionName(optionEntry.key) != QStringLiteral("name")) {
            continue;
        }
        for (const TherionSourceLogicalArgumentRange &valueRange : optionEntry.valueRanges) {
            if (!valueRange.text.trimmed().isEmpty()) {
                return valueRange;
            }
        }
    }

    return std::nullopt;
}

void appendStationReferenceDiagnosticsForRange(QVector<ProjectIndexDiagnostic> *diagnostics,
                                               const StationReferenceIndex &stationIndex,
                                               const ProjectStructureEntry &ownerEntry,
                                               const QString &sourceFile,
                                               const TherionSourceLogicalArgumentRange &referenceRange,
                                               bool reportPlainMissingReferences)
{
    if (diagnostics == nullptr) {
        return;
    }

    const QString referenceName = referenceRange.text.trimmed();
    if (referenceName.isEmpty()) {
        return;
    }

    const ReferenceResolution resolution =
        resolveStationReference(stationIndex, referenceName, ownerEntry.namespacePath);
    if (resolution.state == ReferenceResolutionState::Unique) {
        return;
    }

    if (resolution.state == ReferenceResolutionState::Ambiguous) {
        appendProjectIndexDiagnostic(diagnostics,
                                     ProjectIndexDiagnosticKind::AmbiguousStationReference,
                                     ownerEntry.objectId,
                                     sourceFile,
                                     referenceRange.physicalRange.lineNumber,
                                     referenceRange.physicalRange.columnNumber,
                                     referenceRange.physicalRange.columnLength,
                                     referenceName,
                                     resolution.candidateCount);
        return;
    }

    if (!reportPlainMissingReferences && !stationReferenceShouldReportMissing(referenceName)) {
        return;
    }

    appendProjectIndexDiagnostic(diagnostics,
                                 ProjectIndexDiagnosticKind::UnknownStationReference,
                                 ownerEntry.objectId,
                                 sourceFile,
                                 referenceRange.physicalRange.lineNumber,
                                 referenceRange.physicalRange.columnNumber,
                                 referenceRange.physicalRange.columnLength,
                                 referenceName);
}

QString mapCompositionReferenceToken(const TherionParsedLine &parsedLine)
{
    // Preview links describe spatial context, not map ownership, and commonly form cycles.
    if (parsedLine.directive == QStringLiteral("break")
        || parsedLine.directive == QStringLiteral("preview")) {
        return QString();
    }

    const QStringList &tokens = parsedLine.tokens;
    if (tokens.isEmpty()) {
        return QString();
    }

    const QString token = tokens.first().trimmed();
    if (token.isEmpty() || token.startsWith(QLatin1Char('-'))) {
        return QString();
    }

    if (tokens.size() == 1) {
        return token;
    }

    const QString previewMode = tokens.last().toLower();
    if (tokens.size() < 3
        || (previewMode != QStringLiteral("above")
            && previewMode != QStringLiteral("below")
            && previewMode != QStringLiteral("none"))) {
        return QString();
    }

    const QString firstOffsetToken = tokens.value(1).trimmed();
    const QString lastOffsetToken = tokens.value(tokens.size() - 2).trimmed();
    if (!firstOffsetToken.startsWith(QLatin1Char('['))
        || !lastOffsetToken.endsWith(QLatin1Char(']'))) {
        return QString();
    }

    return token;
}

QString mapPreviewReferenceToken(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("preview")) {
        return QString();
    }

    const QString previewMode = parsedLine.tokens.value(1).trimmed().toLower();
    if (previewMode != QStringLiteral("above")
        && previewMode != QStringLiteral("below")
        && previewMode != QStringLiteral("none")) {
        return QString();
    }

    return parsedLine.tokens.value(2).trimmed();
}

MapCompositionContentKind mapCompositionContentKind(const QString &referenceName)
{
    const QString lookupName = mapReferenceLookupKey(referenceName);
    if (lookupName.endsWith(QStringLiteral(".m"))) {
        return MapCompositionContentKind::Map;
    }
    if (lookupName.endsWith(QStringLiteral(".s"))) {
        return MapCompositionContentKind::Scrap;
    }

    return MapCompositionContentKind::Unknown;
}

void appendMapReferenceDiagnostic(QVector<ProjectIndexDiagnostic> *diagnostics,
                                  ProjectIndexDiagnosticKind kind,
                                  const ProjectStructureEntry &mapEntry,
                                  const QString &sourceFile,
                                  int lineNumber,
                                  int columnNumber,
                                  int columnLength,
                                  const QString &referencedName,
                                  int candidateCount = 0)
{
    if (diagnostics == nullptr) {
        return;
    }

    ProjectIndexDiagnostic diagnostic;
    diagnostic.kind = kind;
    diagnostic.sourceObjectId = mapEntry.objectId;
    diagnostic.sourceFile = sourceFile;
    diagnostic.lineNumber = lineNumber;
    diagnostic.columnNumber = qMax(1, columnNumber);
    diagnostic.columnLength = qMax(0, columnLength);
    diagnostic.referencedName = referencedName;
    diagnostic.candidateCount = candidateCount;
    diagnostics->append(diagnostic);
}

void appendProjectIndexDiagnostic(QVector<ProjectIndexDiagnostic> *diagnostics,
                                  ProjectIndexDiagnosticKind kind,
                                  const QString &sourceObjectId,
                                  const QString &sourceFile,
                                  int lineNumber,
                                  int columnNumber,
                                  int columnLength,
                                  const QString &referencedName,
                                  int candidateCount)
{
    if (diagnostics == nullptr) {
        return;
    }

    ProjectIndexDiagnostic diagnostic;
    diagnostic.kind = kind;
    diagnostic.sourceObjectId = sourceObjectId;
    diagnostic.sourceFile = sourceFile;
    diagnostic.lineNumber = lineNumber;
    diagnostic.columnNumber = qMax(1, columnNumber);
    diagnostic.columnLength = qMax(0, columnLength);
    diagnostic.referencedName = referencedName;
    diagnostic.candidateCount = candidateCount;
    diagnostics->append(diagnostic);
}

TherionSourcePhysicalRange parsedLineTokenPhysicalRange(const TherionParsedLine &parsedLine, int tokenIndex)
{
    TherionSourcePhysicalRange range;
    range.lineNumber = parsedLine.lineNumber;
    range.columnNumber = 1;
    range.columnLength = parsedLine.rawText.size();
    if (tokenIndex >= 0 && tokenIndex < parsedLine.tokenSpans.size()) {
        const TherionParsedToken &token = parsedLine.tokenSpans.at(tokenIndex);
        range.columnNumber = token.start + 1;
        range.columnLength = token.length;
    }
    return range;
}

NamespaceEntriesByFile namespaceEntriesByFile(const QVector<ProjectStructureEntry> &entries)
{
    NamespaceEntriesByFile entriesByFile;
    for (const ProjectStructureEntry &entry : entries) {
        if (!entry.createsNamespace
            || entry.sourceFile.trimmed().isEmpty()
            || entry.lineNumber <= 0) {
            continue;
        }
        entriesByFile[normalizedFilePathKey(entry.sourceFile)].append(entry);
    }

    for (auto it = entriesByFile.begin(); it != entriesByFile.end(); ++it) {
        std::sort(it->begin(), it->end(), [](const ProjectStructureEntry &left, const ProjectStructureEntry &right) {
            if (left.lineNumber != right.lineNumber) {
                return left.lineNumber < right.lineNumber;
            }
            return left.depth < right.depth;
        });
    }

    return entriesByFile;
}

ProjectStructureEntry nearestNamespaceEntryForLine(const NamespaceEntriesByFile &entriesByFile,
                                                   const QString &sourceFile,
                                                   int lineNumber)
{
    const auto entriesIt = entriesByFile.constFind(normalizedFilePathKey(sourceFile));
    if (entriesIt == entriesByFile.constEnd()) {
        return ProjectStructureEntry();
    }

    const QVector<ProjectStructureEntry> &entries = entriesIt.value();
    const auto upper = std::upper_bound(entries.cbegin(),
                                        entries.cend(),
                                        lineNumber,
                                        [](int value, const ProjectStructureEntry &entry) {
                                            return value < entry.lineNumber;
                                        });
    if (upper == entries.cbegin()) {
        return ProjectStructureEntry();
    }
    return *(upper - 1);
}

QString escapedIdentityToken(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    return value;
}

QString ProjectObjectIdentityGenerator::nextObjectId(const QString &category,
                                                     const QString &name,
                                                     const QString &sourceFile,
                                                     const QString &parentObjectId)
{
    const QString fileToken = escapedIdentityToken(normalizedFilePathKey(sourceFile));
    const QString parentToken = parentObjectId.isEmpty() ? QStringLiteral("root") : escapedIdentityToken(parentObjectId);
    const QString categoryToken = escapedIdentityToken(categoryIdentityToken(category));
    const QString nameToken = escapedIdentityToken(objectNameIdentityToken(name));
    const QString scopeKey = QStringList{fileToken, parentToken, categoryToken, nameToken}.join(QLatin1Char('|'));
    const int ordinal = siblingOrdinalsByScope.value(scopeKey, 0) + 1;
    siblingOrdinalsByScope.insert(scopeKey, ordinal);

    return QStringList{QStringLiteral("project-object"), fileToken, parentToken, categoryToken, nameToken, QString::number(ordinal)}
        .join(QLatin1Char('|'));
}

ProjectStructureEntryKind projectKindFromDirective(const QString &directive)
{
    const QString normalizedDirective = normalizedStructureDirective(directive);
    if (normalizedDirective == QStringLiteral("survey")) {
        return ProjectStructureEntryKind::Survey;
    }
    if (normalizedDirective == QStringLiteral("centreline")) {
        return ProjectStructureEntryKind::Centreline;
    }
    if (normalizedDirective == QStringLiteral("map")) {
        return ProjectStructureEntryKind::Map;
    }
    if (normalizedDirective == QStringLiteral("scrap")) {
        return ProjectStructureEntryKind::Scrap;
    }
    if (normalizedDirective == QStringLiteral("station")) {
        return ProjectStructureEntryKind::Station;
    }
    if (normalizedDirective == QStringLiteral("point")) {
        return ProjectStructureEntryKind::Point;
    }
    if (normalizedDirective == QStringLiteral("line")) {
        return ProjectStructureEntryKind::Line;
    }
    if (normalizedDirective == QStringLiteral("area")) {
        return ProjectStructureEntryKind::Area;
    }
    return ProjectStructureEntryKind::Unknown;
}

QString projectCategoryFromKind(ProjectStructureEntryKind kind)
{
    switch (kind) {
    case ProjectStructureEntryKind::Survey:
        return QStringLiteral("Surveys");
    case ProjectStructureEntryKind::Centreline:
        return QStringLiteral("Centrelines");
    case ProjectStructureEntryKind::Map:
        return QStringLiteral("Maps");
    case ProjectStructureEntryKind::Scrap:
        return QStringLiteral("Scraps");
    case ProjectStructureEntryKind::Station:
        return QStringLiteral("Stations");
    case ProjectStructureEntryKind::Point:
        return QStringLiteral("Points");
    case ProjectStructureEntryKind::Line:
        return QStringLiteral("Lines");
    case ProjectStructureEntryKind::Area:
        return QStringLiteral("Areas");
    case ProjectStructureEntryKind::Unknown:
        break;
    }

    return QString();
}

QString surveyNameFromLine(const TherionParsedLine &parsedLine)
{
    return parsedLine.tokens.value(1, QStringLiteral("survey"));
}

bool surveyCreatesNamespace(const TherionParsedLine &parsedLine)
{
    QString namespaceValue = commandOptionValue(parsedLine.tokens, QStringLiteral("namespace"));
    if (namespaceValue.isEmpty()) {
        namespaceValue = commandOptionValue(parsedLine.tokens, QStringLiteral("-namespace"));
    }
    return namespaceValue.compare(QStringLiteral("off"), Qt::CaseInsensitive) != 0;
}

QString normalizedStructureDirective(const QString &directive)
{
    if (directive == QStringLiteral("centerline")) {
        return QStringLiteral("centreline");
    }
    if (directive == QStringLiteral("endcenterline")) {
        return QStringLiteral("endcentreline");
    }

    return directive;
}

ProjectStructureEntryKind objectKindFromLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return ProjectStructureEntryKind::Unknown;
    }

    const QString directive = parsedLine.directive;
    if (directive == QStringLiteral("line")) {
        return ProjectStructureEntryKind::Line;
    }
    if (directive == QStringLiteral("area")) {
        return ProjectStructureEntryKind::Area;
    }
    if (directive == QStringLiteral("point")) {
        for (int index = 1; index < parsedLine.tokens.size(); ++index) {
            const QString token = parsedLine.tokens.at(index).toLower();
            if (token.startsWith(QLatin1Char('-'))) {
                continue;
            }
            if (token == QStringLiteral("station")) {
                return ProjectStructureEntryKind::Station;
            }
        }

        return ProjectStructureEntryKind::Point;
    }

    return ProjectStructureEntryKind::Unknown;
}

const TherionSourceLogicalDocument &logicalDocumentForFile(const QString &filePath,
                                                          ParsedFileCache *cache,
                                                          const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    const QString normalizedPath = normalizedFilePathKey(filePath);
    auto iterator = cache->logicalDocuments.find(normalizedPath);
    if (iterator != cache->logicalDocuments.end()) {
        ++cache->stats.logicalDocumentHits;
        return iterator.value();
    }
    const auto prebuiltIt = cache->prebuiltLogicalDocuments.constFind(normalizedPath);
    if (prebuiltIt != cache->prebuiltLogicalDocuments.constEnd() && prebuiltIt.value()) {
        ++cache->stats.prebuiltLogicalDocumentHits;
        return *prebuiltIt.value();
    }

    QString fileContents;
    if (inMemoryFileContentsByPath.contains(normalizedPath)) {
        fileContents = inMemoryFileContentsByPath.value(normalizedPath);
    } else if (!DocumentFile::readUtf8TextFile(normalizedPath, &fileContents, nullptr)) {
        TherionSourceDocumentMetadata metadata;
        metadata.sourceType = therionSourceDocumentTypeForFilePath(normalizedPath);
        metadata.revisionId = ++cache->sourceRevisionCounter;
        cache->logicalDocuments.insert(normalizedPath,
                                       logicalDocumentForText(QString(), cache->sourceSnapshotCache, metadata));
        ++cache->stats.logicalDocumentBuilds;
        return cache->logicalDocuments[normalizedPath];
    }

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = therionSourceDocumentTypeForFilePath(normalizedPath);
    metadata.revisionId = ++cache->sourceRevisionCounter;
    cache->logicalDocuments.insert(normalizedPath,
                                   logicalDocumentForText(fileContents, cache->sourceSnapshotCache, metadata));
    ++cache->stats.logicalDocumentBuilds;
    return cache->logicalDocuments[normalizedPath];
}

bool isOpeningDirective(const QString &directive)
{
    const QString normalizedDirective = normalizedStructureDirective(directive);
    return normalizedDirective == QStringLiteral("survey")
        || normalizedDirective == QStringLiteral("centreline")
        || normalizedDirective == QStringLiteral("map")
        || normalizedDirective == QStringLiteral("scrap");
}

bool isClosingDirective(const QString &directive)
{
    const QString normalizedDirective = normalizedStructureDirective(directive);
    return normalizedDirective == QStringLiteral("endsurvey")
        || normalizedDirective == QStringLiteral("endcentreline")
        || normalizedDirective == QStringLiteral("endmap")
        || normalizedDirective == QStringLiteral("endscrap");
}

void closeBlock(QVector<ProjectBlock> *blockStack, ProjectStructureEntryKind kind, const QString &name = QString())
{
    if (blockStack->isEmpty()) {
        return;
    }

    if (name.isEmpty()) {
        while (!blockStack->isEmpty()) {
            const ProjectBlock block = blockStack->takeLast();
            if (block.kind == kind) {
                return;
            }
        }

        return;
    }

    for (int index = blockStack->size() - 1; index >= 0; --index) {
        const ProjectBlock &block = blockStack->at(index);
        if (block.kind == kind && block.name.compare(name, Qt::CaseInsensitive) == 0) {
            blockStack->resize(index);
            return;
        }
    }

    while (!blockStack->isEmpty()) {
        const ProjectBlock block = blockStack->takeLast();
        if (block.kind == kind) {
            return;
        }
    }
}

ProjectStructureEntry appendStructureEntry(QVector<ProjectStructureEntry> *entries,
                                           ProjectStructureEntryKind kind,
                                           const QString &name,
                                           const QString &sourceFile,
                                           int lineNumber,
                                           const QVector<ProjectBlock> &blockStack,
                                           ProjectObjectIdentityGenerator *identityGenerator,
                                           bool createsNamespace = true,
                                           const QSet<QString> &linePointMarks = {})
{
    ProjectStructureEntry entry;
    const QString category = projectCategoryFromKind(kind);
    entry.kind = kind;
    entry.parentObjectId = blockStack.isEmpty() ? QString() : blockStack.last().objectId;
    entry.objectId = identityGenerator->nextObjectId(category, name, sourceFile, entry.parentObjectId);
    entry.category = category;
    entry.name = name;
    const QString currentNamespacePath = blockStack.isEmpty() ? QString() : blockStack.last().namespacePath;
    entry.namespacePath = kind == ProjectStructureEntryKind::Survey && createsNamespace
        ? namespacePathWithChild(currentNamespacePath, name)
        : currentNamespacePath;
    entry.sourceFile = sourceFile;
    entry.lineNumber = lineNumber;
    entry.depth = blockStack.size();
    entry.createsNamespace = createsNamespace;
    entry.linePointMarks = linePointMarks;
    entries->append(entry);
    return entry;
}

QHash<int, QSet<QString>> linePointMarksByLineStart(const QVector<TherionSourceLogicalCommand> &commands)
{
    QHash<int, QSet<QString>> marksByLineStart;
    int currentLineStart = 0;
    for (const TherionSourceLogicalCommand &command : commands) {
        if (command.metadata.commandName == QStringLiteral("line")) {
            currentLineStart = command.startLineNumber;
            continue;
        }
        if (command.metadata.commandName == QStringLiteral("endline")) {
            currentLineStart = 0;
            continue;
        }
        if (currentLineStart <= 0
            || command.metadata.commandName != QStringLiteral("mark")
            || command.parsed.tokens.size() < 2) {
            continue;
        }
        const QString markName = command.parsed.tokens.value(1).trimmed().toLower();
        if (!markName.isEmpty()) {
            marksByLineStart[currentLineStart].insert(markName);
        }
    }

    return marksByLineStart;
}

void appendProjectStructureFromFile(const QString &filePath,
                                    ParsedFileCache *cache,
                                    QVector<ProjectBlock> *blockStack,
                                    QSet<QString> *activeFiles,
                                    QVector<ProjectStructureEntry> *entries,
                                    ProjectObjectIdentityGenerator *identityGenerator,
                                    const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    const QString normalizedPath = normalizedFilePathKey(filePath);
    if (activeFiles->contains(normalizedPath)) {
        return;
    }

    activeFiles->insert(normalizedPath);
    const TherionSourceLogicalDocument &logicalDocument =
        logicalDocumentForFile(normalizedPath, cache, inMemoryFileContentsByPath);
    const QHash<int, QSet<QString>> lineMarksByLineStart =
        linePointMarksByLineStart(logicalDocument.commands());

    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        const TherionParsedLine &parsedLine = command.parsed;
        const QString directive = normalizedStructureDirective(command.metadata.commandName);

        if (directive == QStringLiteral("input") || directive == QStringLiteral("source")) {
            const QString inputTarget = therionSourceReferencePathToken(command);
            const QString resolvedInputPath = resolveTherionSourceReferencePath(normalizedPath, inputTarget);
            if (!resolvedInputPath.isEmpty()) {
                appendProjectStructureFromFile(resolvedInputPath,
                                               cache,
                                               blockStack,
                                               activeFiles,
                                               entries,
                                               identityGenerator,
                                               inMemoryFileContentsByPath);
            }
            continue;
        }

        if (isClosingDirective(directive)) {
            const ProjectStructureEntryKind closingKind = projectKindFromDirective(directive.mid(3));
            const QString closingName = parsedLine.tokens.size() >= 2 ? parsedLine.tokens.value(1) : QString();
            closeBlock(blockStack, closingKind, closingName);
            continue;
        }

        const ProjectStructureEntryKind openingKind = projectKindFromDirective(directive);
        if (isOpeningDirective(directive)) {
            const QString openingName = directive == QStringLiteral("survey")
                ? surveyNameFromLine(parsedLine)
                : sectionNameFromLine(parsedLine);
            const bool createsNamespace = directive == QStringLiteral("survey") ? surveyCreatesNamespace(parsedLine) : true;
            const ProjectStructureEntry entry = appendStructureEntry(entries,
                                                                    openingKind,
                                                                    openingName,
                                                                    normalizedPath,
                                                                    command.startLineNumber,
                                                                    *blockStack,
                                                                    identityGenerator,
                                                                    createsNamespace);
            blockStack->append(ProjectBlock{openingKind, openingName, entry.objectId, entry.namespacePath, createsNamespace});
            continue;
        }

        const ProjectStructureEntryKind objectKind = objectKindFromLine(parsedLine);
        if (objectKind == ProjectStructureEntryKind::Unknown) {
            continue;
        }

        appendStructureEntry(entries,
                             objectKind,
                             objectNameFromLine(parsedLine),
                             normalizedPath,
                             command.startLineNumber,
                             *blockStack,
                             identityGenerator,
                             true,
                             objectKind == ProjectStructureEntryKind::Line
                                 ? lineMarksByLineStart.value(command.startLineNumber)
                                 : QSet<QString>());
    }

    activeFiles->remove(normalizedPath);
}

QVector<QString> rootProjectFiles(const QVector<QString> &filePaths,
                                  ParsedFileCache *cache,
                                  const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    QSet<QString> includedFiles;
    for (const QString &filePath : filePaths) {
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(filePath, cache, inMemoryFileContentsByPath);
        for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
            if (command.metadata.commandName != QStringLiteral("input")
                && command.metadata.commandName != QStringLiteral("source")) {
                continue;
            }

            const QString inputTarget = therionSourceReferencePathToken(command);
            const QString resolvedInputPath = resolveTherionSourceReferencePath(filePath, inputTarget);
            if (!resolvedInputPath.isEmpty()) {
                includedFiles.insert(normalizedFilePathKey(resolvedInputPath));
            }
        }
    }

    QVector<QString> roots;
    for (const QString &filePath : filePaths) {
        if (!includedFiles.contains(normalizedFilePathKey(filePath))) {
            roots.append(filePath);
        }
    }

    if (roots.isEmpty()) {
        return filePaths;
    }

    return roots;
}

QString pointTypeToken(const TherionParsedLine &parsedLine)
{
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (TherionTokenRules::tokenStartsOption(token) || TherionTokenRules::isNumericToken(token)) {
            continue;
        }

        return token;
    }

    return QString();
}

QString objectNameFromLine(const TherionParsedLine &parsedLine)
{
    const QString directive = parsedLine.directive;

    if (directive == QStringLiteral("line") || directive == QStringLiteral("area")) {
        const QString identifier = commandOptionValue(parsedLine.tokens, QStringLiteral("-id"));
        if (!identifier.isEmpty()) {
            return identifier;
        }

        return parsedLine.tokens.value(1, directive);
    }

    if (directive == QStringLiteral("point")) {
        const QString stationName = commandOptionValue(parsedLine.tokens, QStringLiteral("-name"));
        if (!stationName.isEmpty()) {
            return stationName;
        }

        const QString identifier = commandOptionValue(parsedLine.tokens, QStringLiteral("-id"));
        if (!identifier.isEmpty()) {
            return identifier;
        }

        const QString pointType = pointTypeToken(parsedLine);
        if (!pointType.isEmpty()) {
            return pointType;
        }
    }

    return parsedLine.tokens.value(1, directive);
}

QString objectDisplayText(const ProjectStructureEntry &entry, const TherionParsedLine &parsedLine)
{
    if (entry.kind == ProjectStructureEntryKind::Line || entry.kind == ProjectStructureEntryKind::Area) {
        const QString subtype = parsedLine.tokens.value(1);
        if (!subtype.isEmpty() && subtype != entry.name) {
            return QStringLiteral("%1 (%2)").arg(subtype, entry.name);
        }
    }

    return entry.name;
}

bool structureEntryKindParticipatesInDuplicateIdentity(ProjectStructureEntryKind kind)
{
    return kind == ProjectStructureEntryKind::Survey
        || kind == ProjectStructureEntryKind::Map
        || kind == ProjectStructureEntryKind::Scrap
        || kind == ProjectStructureEntryKind::Point
        || kind == ProjectStructureEntryKind::Line
        || kind == ProjectStructureEntryKind::Area;
}

bool structureEntryUsesScrapObjectIdNamespace(ProjectStructureEntryKind kind)
{
    return kind == ProjectStructureEntryKind::Point
        || kind == ProjectStructureEntryKind::Line
        || kind == ProjectStructureEntryKind::Area;
}

bool structureEntryUsesParentNamespaceName(ProjectStructureEntryKind kind)
{
    return kind == ProjectStructureEntryKind::Survey
        || kind == ProjectStructureEntryKind::Map
        || kind == ProjectStructureEntryKind::Scrap;
}

QString parentNamespacePathForDuplicateIdentity(const ProjectStructureEntry &entry)
{
    if (entry.kind != ProjectStructureEntryKind::Survey || !entry.createsNamespace) {
        return entry.namespacePath;
    }

    QStringList parts = namespacePathComponents(entry.namespacePath);
    if (!parts.isEmpty()) {
        parts.removeFirst();
    }
    return parts.join(QLatin1Char('.'));
}

QString duplicateIdentityLookupKey(const ProjectStructureEntry &entry,
                                   const QString &identifier)
{
    if (structureEntryUsesScrapObjectIdNamespace(entry.kind)) {
        return QStringLiteral("object@%1:%2")
            .arg(entry.parentObjectId,
                 normalizedIdentityToken(identifier));
    }

    if (structureEntryUsesParentNamespaceName(entry.kind)) {
        return QStringLiteral("namespace@%1:%2")
            .arg(normalizedNamespaceToken(parentNamespacePathForDuplicateIdentity(entry)),
                 normalizedIdentityToken(identifier));
    }

    return QString();
}

std::optional<TherionSourceLogicalArgumentRange> commandDuplicateIdentityRange(
    const TherionSourceLogicalCommand &command,
    ProjectStructureEntryKind kind)
{
    if (structureEntryUsesParentNamespaceName(kind)) {
        if (command.positionalArgumentRanges.isEmpty()) {
            return std::nullopt;
        }
        return command.positionalArgumentRanges.constFirst();
    }

    if (!structureEntryUsesScrapObjectIdNamespace(kind)) {
        return std::nullopt;
    }

    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        if (normalizedCommandOptionName(optionEntry.key) != QStringLiteral("id")
            || optionEntry.valueRanges.isEmpty()) {
            continue;
        }
        return optionEntry.valueRanges.constFirst();
    }
    return std::nullopt;
}

QString sectionNameFromLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() >= 2) {
        return parsedLine.tokens.value(1);
    }

    return parsedLine.directive;
}

bool isInterestingProjectFile(const QFileInfo &fileInfo)
{
    const QString suffix = fileInfo.suffix().toLower();
    const QString fileName = fileInfo.fileName();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2")
        || isTherionConfigFileName(fileName);
}

bool isTherionConfigFile(const QFileInfo &fileInfo)
{
    return isTherionConfigFileName(fileInfo.fileName());
}

QString resolvePreferredProjectConfigPath(const QString &preferredConfigPath, const QString &projectRootPath)
{
    const QString trimmedPath = preferredConfigPath.trimmed();
    if (trimmedPath.isEmpty()) {
        return QString();
    }

    QFileInfo configInfo(trimmedPath);
    if (!configInfo.isAbsolute()) {
        configInfo = QFileInfo(QDir(projectRootPath).absoluteFilePath(trimmedPath));
    }
    if (!configInfo.exists() || !isTherionConfigFile(configInfo)) {
        return QString();
    }

    const QString normalizedProjectRoot = normalizedFilePathKey(projectRootPath);
    const QString normalizedConfigPath = normalizedFilePathKey(configInfo.absoluteFilePath());
    if (!pathIsInsideDirectory(normalizedConfigPath, normalizedProjectRoot)) {
        return QString();
    }

    return normalizedConfigPath;
}

RootConfigResolution rootConfigFiles(const QVector<QString> &filePaths,
                                     const QString &projectRootPath,
                                     const QString &preferredConfigPath)
{
    const QString normalizedProjectRoot = normalizedFilePathKey(projectRootPath);
    const QString projectName = QFileInfo(projectRootPath).fileName();
    const QString normalizedPreferredConfigPath = resolvePreferredProjectConfigPath(preferredConfigPath, projectRootPath);
    if (!normalizedPreferredConfigPath.isEmpty()) {
        return RootConfigResolution{{normalizedPreferredConfigPath}, normalizedPreferredConfigPath, QString()};
    }

    QVector<QString> preferredConfigFiles;
    QVector<QString> namedConfigFiles;

    for (const QString &filePath : filePaths) {
        const QFileInfo fileInfo(filePath);
        if (!isTherionConfigFile(fileInfo)) {
            continue;
        }

        if (normalizedFilePathKey(fileInfo.dir().absolutePath()) != normalizedProjectRoot) {
            continue;
        }

        if (isPreferredRootTherionConfigFileName(fileInfo.fileName(), projectName)) {
            preferredConfigFiles.append(filePath);
        } else {
            namedConfigFiles.append(filePath);
        }
    }

    if (!preferredConfigFiles.isEmpty()) {
        std::sort(preferredConfigFiles.begin(),
                  preferredConfigFiles.end(),
                  [projectName](const QString &left, const QString &right) {
                      return preferredRootTherionConfigFilePriority(QFileInfo(left).fileName(), projectName)
                          < preferredRootTherionConfigFilePriority(QFileInfo(right).fileName(), projectName);
                  });
        const QString selectedConfigPath = normalizedFilePathKey(preferredConfigFiles.first());
        return RootConfigResolution{{selectedConfigPath}, selectedConfigPath, QString()};
    }
    if (namedConfigFiles.size() == 1) {
        return RootConfigResolution{namedConfigFiles, normalizedFilePathKey(namedConfigFiles.first()), QString()};
    }
    if (namedConfigFiles.isEmpty()) {
        return RootConfigResolution{{}, QString(), QString()};
    }

    return RootConfigResolution{
        {},
        QString(),
        QCoreApplication::translate("TherionStudio::ProjectStructureIndex",
                                    "Multiple Therion config files were found in the project root. Select a project target config in the Compiler pane to build the structure graph.")
    };
}

ProjectIndexSnapshot scanProjectIndexFromFilePaths(const QString &projectRootPath,
                                                   const QVector<QString> &filePaths,
                                                   const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                   const QHash<QString, std::shared_ptr<const TherionSourceLogicalDocument>> &prebuiltLogicalDocumentsByPath,
                                                   const QString &preferredConfigPath,
                                                   QString *errorMessage,
                                                   const std::function<bool()> &shouldCancel = {})
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    ProjectIndexSnapshot snapshot;
    if (projectRootPath.isEmpty()) {
        return snapshot;
    }
    snapshot.projectRootPath = normalizedFilePathKey(projectRootPath);

    QDir projectRoot(projectRootPath);
    if (!projectRoot.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::ProjectStructureIndex",
                                                        "The selected project folder does not exist.");
        }
        return snapshot;
    }

    QVector<QString> sortedFilePaths = filePaths;
    std::sort(sortedFilePaths.begin(), sortedFilePaths.end(), [](const QString &left, const QString &right) {
        return left.toLower() < right.toLower();
    });

    ParsedFileCache cache;
    auto cancelSnapshot = [&]() {
        snapshot.canceled = true;
        snapshot.scanStats = cache.stats;
        return snapshot;
    };
    if (projectIndexScanShouldCancel(shouldCancel)) {
        return cancelSnapshot();
    }
    cache.prebuiltLogicalDocuments = prebuiltLogicalDocumentsByPath;
    const RootConfigResolution configResolution = rootConfigFiles(sortedFilePaths, projectRootPath, preferredConfigPath);
    snapshot.rootConfigPath = configResolution.configPath;
    if (!configResolution.errorMessage.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = configResolution.errorMessage;
        }
        return snapshot;
    }

    QVector<QString> rootFiles = configResolution.rootFiles;
    if (rootFiles.isEmpty()) {
        rootFiles = rootProjectFiles(sortedFilePaths, &cache, inMemoryFileContentsByPath);
    }
    for (const QString &rootFile : std::as_const(rootFiles)) {
        snapshot.rootFilePaths.append(normalizedFilePathKey(rootFile));
    }

    QVector<ProjectBlock> blockStack;
    QSet<QString> activeFiles;
    ProjectObjectIdentityGenerator identityGenerator;
    for (const QString &filePath : rootFiles) {
        if (projectIndexScanShouldCancel(shouldCancel)) {
            return cancelSnapshot();
        }
        appendProjectStructureFromFile(filePath,
                                       &cache,
                                       &blockStack,
                                       &activeFiles,
                                       &snapshot.entries,
                                       &identityGenerator,
                                       inMemoryFileContentsByPath);
    }
    if (projectIndexScanShouldCancel(shouldCancel)) {
        return cancelSnapshot();
    }

    const MapReferenceScanResult mapReferenceScan = scanMapReferences(snapshot.entries,
                                                                      &cache,
                                                                      inMemoryFileContentsByPath,
                                                                      shouldCancel);
    if (projectIndexScanShouldCancel(shouldCancel)) {
        return cancelSnapshot();
    }
    snapshot.mapScrapReferencesByMapKey = mapReferenceScan.scrapReferencesByMapKey;
    snapshot.mapChildReferencesByMapKey = mapReferenceScan.childMapReferencesByMapKey;
    snapshot.mapPreviewReferencesByMapKey = mapReferenceScan.previewReferencesByMapKey;
    snapshot.diagnostics = mapReferenceScan.diagnostics;
    snapshot.diagnostics += scanJoinReferences(snapshot.entries,
                                               &cache,
                                               inMemoryFileContentsByPath,
                                               shouldCancel);
    if (projectIndexScanShouldCancel(shouldCancel)) {
        return cancelSnapshot();
    }
    snapshot.diagnostics += scanStationReferences(snapshot.entries,
                                                  &cache,
                                                  inMemoryFileContentsByPath,
                                                  shouldCancel);
    if (projectIndexScanShouldCancel(shouldCancel)) {
        return cancelSnapshot();
    }
    snapshot.diagnostics += scanDuplicateObjectIds(snapshot.entries,
                                                   &cache,
                                                   inMemoryFileContentsByPath,
                                                   shouldCancel);
    if (projectIndexScanShouldCancel(shouldCancel)) {
        return cancelSnapshot();
    }
    snapshot.scanStats = cache.stats;
    return snapshot;
}

MapReferenceScanResult scanMapReferences(const QVector<ProjectStructureEntry> &entries,
                                         ParsedFileCache *cache,
                                         const QHash<QString, QString> &inMemoryFileContentsByPath,
                                         const std::function<bool()> &shouldCancel)
{
    MapReferenceScanResult result;

    const ReferenceKeyIndex mapKeysByName = entryKeysByReferenceName(entries, ProjectStructureEntryKind::Map);
    const ReferenceKeyIndex scrapKeysByName = entryKeysByReferenceName(entries, ProjectStructureEntryKind::Scrap);

    QHash<QString, QVector<ProjectStructureEntry>> mapsBySourceFile;
    for (const ProjectStructureEntry &entry : entries) {
        if (entry.kind == ProjectStructureEntryKind::Map && !entry.sourceFile.isEmpty() && entry.lineNumber > 0) {
            mapsBySourceFile[normalizedFilePathKey(entry.sourceFile)].append(entry);
        }
    }
    if (mapsBySourceFile.isEmpty()) {
        return result;
    }

    for (auto fileIt = mapsBySourceFile.constBegin(); fileIt != mapsBySourceFile.constEnd(); ++fileIt) {
        if (projectIndexScanShouldCancel(shouldCancel)) {
            return result;
        }
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(fileIt.key(), cache, inMemoryFileContentsByPath);
        const QVector<TherionSourceLogicalCommand> &commands = logicalDocument.commands();
        if (commands.isEmpty()) {
            continue;
        }

        for (const ProjectStructureEntry &mapEntry : fileIt.value()) {
            if (projectIndexScanShouldCancel(shouldCancel)) {
                return result;
            }
            int mapLineIndex = -1;
            for (int index = 0; index < commands.size(); ++index) {
                if (commands.at(index).startLineNumber == mapEntry.lineNumber
                    && commands.at(index).metadata.commandName == QStringLiteral("map")) {
                    mapLineIndex = index;
                    break;
                }
            }
            if (mapLineIndex < 0) {
                continue;
            }

            QSet<QString> scrapReferences;
            QSet<QString> childMapReferences;
            QSet<QString> previewReferences;
            bool sawMapContent = false;
            bool sawScrapContent = false;
            bool mixedContentDiagnosticAdded = false;
            auto registerContentKind = [&](MapCompositionContentKind contentKind,
                                           const TherionParsedLine &sourceLine,
                                           const QString &referenceName) {
                if (contentKind == MapCompositionContentKind::Unknown) {
                    return;
                }

                const bool mixedWithPreviousContent =
                    (contentKind == MapCompositionContentKind::Map && sawScrapContent)
                    || (contentKind == MapCompositionContentKind::Scrap && sawMapContent);
                if (mixedWithPreviousContent && !mixedContentDiagnosticAdded) {
                    const TherionSourcePhysicalRange referenceRange =
                        parsedLineTokenPhysicalRange(sourceLine, 0);
                    appendMapReferenceDiagnostic(&result.diagnostics,
                                                 ProjectIndexDiagnosticKind::MixedMapAndScrapReferences,
                                                 mapEntry,
                                                 fileIt.key(),
                                                 sourceLine.lineNumber,
                                                 referenceRange.columnNumber,
                                                 referenceRange.columnLength,
                                                 referenceName);
                    mixedContentDiagnosticAdded = true;
                }

                if (contentKind == MapCompositionContentKind::Map) {
                    sawMapContent = true;
                } else if (contentKind == MapCompositionContentKind::Scrap) {
                    sawScrapContent = true;
                }
            };

            int mapDepth = 0;
            for (int index = mapLineIndex; index < commands.size(); ++index) {
                if (projectIndexScanShouldCancel(shouldCancel)) {
                    return result;
                }
                const TherionSourceLogicalCommand &command = commands.at(index);
                const TherionParsedLine &parsedLine = command.parsed;
                const QString directive = command.metadata.commandName;

                if (directive == QStringLiteral("map")) {
                    ++mapDepth;
                    continue;
                }
                if (directive == QStringLiteral("endmap")) {
                    --mapDepth;
                    if (mapDepth <= 0) {
                        break;
                    }
                    continue;
                }
                if (mapDepth != 1) {
                    continue;
                }

                const QString previewReference = mapPreviewReferenceToken(parsedLine);
                if (!previewReference.isEmpty()) {
                    const ReferenceResolution previewResolution = resolveReferenceKey(mapKeysByName,
                                                                                      previewReference,
                                                                                      mapEntry.namespacePath);
                    if (previewResolution.state == ReferenceResolutionState::Unique) {
                        const QString owningMapKey = ProjectStructureIndex::structureEntryNodeKey(mapEntry);
                        if (previewResolution.key != owningMapKey) {
                            previewReferences.insert(previewResolution.key);
                        }
                    }
                    continue;
                }

                const QString candidate = mapCompositionReferenceToken(parsedLine);
                if (candidate.isEmpty()) {
                    continue;
                }
                const TherionSourcePhysicalRange candidateRange = parsedLineTokenPhysicalRange(parsedLine, 0);

                const ReferenceResolution childMapResolution = resolveReferenceKey(mapKeysByName, candidate, mapEntry.namespacePath);
                if (childMapResolution.state == ReferenceResolutionState::Unique) {
                    registerContentKind(MapCompositionContentKind::Map, parsedLine, candidate);
                    const QString owningMapKey = ProjectStructureIndex::structureEntryNodeKey(mapEntry);
                    if (childMapResolution.key != owningMapKey) {
                        childMapReferences.insert(childMapResolution.key);
                    }
                    continue;
                }
                if (childMapResolution.state == ReferenceResolutionState::Ambiguous) {
                    registerContentKind(MapCompositionContentKind::Map, parsedLine, candidate);
                    appendMapReferenceDiagnostic(&result.diagnostics,
                                                 ProjectIndexDiagnosticKind::AmbiguousMapReference,
                                                 mapEntry,
                                                 fileIt.key(),
                                                 parsedLine.lineNumber,
                                                 candidateRange.columnNumber,
                                                 candidateRange.columnLength,
                                                 candidate,
                                                 childMapResolution.candidateCount);
                    continue;
                }

                const ReferenceResolution scrapResolution = resolveReferenceKey(scrapKeysByName, candidate, mapEntry.namespacePath);
                if (scrapResolution.state == ReferenceResolutionState::Unique) {
                    registerContentKind(MapCompositionContentKind::Scrap, parsedLine, candidate);
                    scrapReferences.insert(scrapResolution.key);
                    continue;
                }
                if (scrapResolution.state == ReferenceResolutionState::Ambiguous) {
                    registerContentKind(MapCompositionContentKind::Scrap, parsedLine, candidate);
                    appendMapReferenceDiagnostic(&result.diagnostics,
                                                 ProjectIndexDiagnosticKind::AmbiguousMapScrapReference,
                                                 mapEntry,
                                                 fileIt.key(),
                                                 parsedLine.lineNumber,
                                                 candidateRange.columnNumber,
                                                 candidateRange.columnLength,
                                                 candidate,
                                                 scrapResolution.candidateCount);
                    continue;
                }

                const MapCompositionContentKind unresolvedContentKind = mapCompositionContentKind(candidate);
                if (unresolvedContentKind == MapCompositionContentKind::Unknown) {
                    continue;
                }

                registerContentKind(unresolvedContentKind, parsedLine, candidate);
                appendMapReferenceDiagnostic(&result.diagnostics,
                                             unresolvedContentKind == MapCompositionContentKind::Map
                                                 ? ProjectIndexDiagnosticKind::UnknownMapReference
                                                 : ProjectIndexDiagnosticKind::UnknownMapScrapReference,
                                             mapEntry,
                                             fileIt.key(),
                                             parsedLine.lineNumber,
                                             candidateRange.columnNumber,
                                             candidateRange.columnLength,
                                             candidate);
            }

            if (!scrapReferences.isEmpty()) {
                result.scrapReferencesByMapKey.insert(ProjectStructureIndex::structureEntryNodeKey(mapEntry), scrapReferences);
            }
            if (!childMapReferences.isEmpty()) {
                result.childMapReferencesByMapKey.insert(ProjectStructureIndex::structureEntryNodeKey(mapEntry), childMapReferences);
            }
            if (!previewReferences.isEmpty()) {
                result.previewReferencesByMapKey.insert(ProjectStructureIndex::structureEntryNodeKey(mapEntry), previewReferences);
            }
        }
    }

    return result;
}

QVector<ProjectIndexDiagnostic> scanJoinReferences(const QVector<ProjectStructureEntry> &entries,
                                                   ParsedFileCache *cache,
                                                   const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                   const std::function<bool()> &shouldCancel)
{
    QVector<ProjectIndexDiagnostic> diagnostics;
    const ReferenceKeyIndex joinObjectKeysByName = mergedReferenceKeysByReferenceName(
        entries,
        {ProjectStructureEntryKind::Scrap,
         ProjectStructureEntryKind::Line,
         ProjectStructureEntryKind::Point,
         ProjectStructureEntryKind::Station});
    const QHash<QString, ProjectStructureEntry> entriesByKey = entriesByStructureKey(entries);
    const NamespaceEntriesByFile namespaceEntries = namespaceEntriesByFile(entries);

    QSet<QString> sourceFiles;
    for (const ProjectStructureEntry &entry : entries) {
        if (!entry.sourceFile.trimmed().isEmpty()) {
            sourceFiles.insert(normalizedFilePathKey(entry.sourceFile));
        }
    }

    for (const QString &sourceFile : std::as_const(sourceFiles)) {
        if (projectIndexScanShouldCancel(shouldCancel)) {
            return diagnostics;
        }
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(sourceFile, cache, inMemoryFileContentsByPath);
        for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
            if (projectIndexScanShouldCancel(shouldCancel)) {
                return diagnostics;
            }
            if (command.metadata.commandName != QStringLiteral("join")) {
                continue;
            }

            const ProjectStructureEntry ownerEntry =
                nearestNamespaceEntryForLine(namespaceEntries, sourceFile, command.startLineNumber);
            for (int tokenIndex = 1; tokenIndex < command.parsed.tokens.size(); ++tokenIndex) {
                const QString referenceName = command.parsed.tokens.value(tokenIndex).trimmed();
                if (referenceName.isEmpty() || referenceName.startsWith(QLatin1Char('-'))) {
                    break;
                }

                const JoinReferenceParts referenceParts = parseJoinReference(referenceName);
                if (referenceParts.lookupName.isEmpty()) {
                    continue;
                }

                const ReferenceResolution resolution =
                    resolveReferenceKey(joinObjectKeysByName, referenceParts.lookupName, ownerEntry.namespacePath);
                const TherionSourcePhysicalRange referenceRange =
                    parsedLineTokenPhysicalRange(command.parsed, tokenIndex);
                if (resolution.state == ReferenceResolutionState::Unique) {
                    if (referenceParts.requiresLinePointMark) {
                        const ProjectStructureEntry resolvedEntry = entriesByKey.value(resolution.key);
                        if (resolvedEntry.kind == ProjectStructureEntryKind::Line
                            && !resolvedEntry.linePointMarks.contains(referenceParts.linePointMark.toLower())) {
                            appendProjectIndexDiagnostic(&diagnostics,
                                                         ProjectIndexDiagnosticKind::UnknownJoinLinePointMark,
                                                         ownerEntry.objectId,
                                                         sourceFile,
                                                         referenceRange.lineNumber,
                                                         referenceRange.columnNumber,
                                                         referenceRange.columnLength,
                                                         referenceName);
                        }
                    }
                    continue;
                }

                if (resolution.state == ReferenceResolutionState::Ambiguous) {
                    appendProjectIndexDiagnostic(&diagnostics,
                                                 ProjectIndexDiagnosticKind::AmbiguousJoinReference,
                                                 ownerEntry.objectId,
                                                 sourceFile,
                                                 referenceRange.lineNumber,
                                                 referenceRange.columnNumber,
                                                 referenceRange.columnLength,
                                                 referenceName,
                                                 resolution.candidateCount);
                    continue;
                }

                if (!joinReferenceShouldReportMissing(referenceName)) {
                    continue;
                }
                appendProjectIndexDiagnostic(&diagnostics,
                                             ProjectIndexDiagnosticKind::UnknownJoinReference,
                                             ownerEntry.objectId,
                                             sourceFile,
                                             referenceRange.lineNumber,
                                             referenceRange.columnNumber,
                                             referenceRange.columnLength,
                                             referenceName);
            }
        }
    }

    return diagnostics;
}

bool commandIsInCenterline(const TherionSourceLogicalCommand &command)
{
    const QString currentBlock = normalizedStructureDirective(command.currentBlockDirective);
    return currentBlock == QStringLiteral("centerline")
        || currentBlock == QStringLiteral("centreline");
}

StationReferenceIndex stationReferencesFromProject(const QVector<ProjectStructureEntry> &entries,
                                                   const QSet<QString> &sourceFiles,
                                                   ParsedFileCache *cache,
                                                   const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                   const std::function<bool()> &shouldCancel)
{
    StationReferenceIndex index;
    const NamespaceEntriesByFile namespaceEntries = namespaceEntriesByFile(entries);
    for (const QString &sourceFile : sourceFiles) {
        if (projectIndexScanShouldCancel(shouldCancel)) {
            return index;
        }
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(sourceFile, cache, inMemoryFileContentsByPath);
        int fromColumn = -1;
        int toColumn = -1;
        QString dataNamespacePath;
        for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
            if (projectIndexScanShouldCancel(shouldCancel)) {
                return index;
            }
            if (!commandIsInCenterline(command)) {
                fromColumn = -1;
                toColumn = -1;
                dataNamespacePath.clear();
                continue;
            }

            if (command.metadata.commandName == QStringLiteral("data")) {
                fromColumn = -1;
                toColumn = -1;
                for (int tokenIndex = 2; tokenIndex < command.parsed.tokens.size(); ++tokenIndex) {
                    const QString columnName = command.parsed.tokens.at(tokenIndex).toLower();
                    if (columnName == QStringLiteral("from")) {
                        fromColumn = tokenIndex - 2;
                    } else if (columnName == QStringLiteral("to")) {
                        toColumn = tokenIndex - 2;
                    }
                }
                const ProjectStructureEntry ownerEntry =
                    nearestNamespaceEntryForLine(namespaceEntries, sourceFile, command.startLineNumber);
                dataNamespacePath = ownerEntry.namespacePath;
                continue;
            }

            if (command.metadata.commandName == QStringLiteral("fix")) {
                if (command.parsed.tokens.size() > 1) {
                    const QString stationName = command.parsed.tokens.at(1);
                    if (!stationName.trimmed().isEmpty() && stationName != QStringLiteral("-")) {
                        const ProjectStructureEntry ownerEntry =
                            nearestNamespaceEntryForLine(namespaceEntries, sourceFile, command.startLineNumber);
                        addStationReferenceKey(&index,
                                               stationName,
                                               ownerEntry.namespacePath,
                                               QStringLiteral("centerline-fix:%1:%2")
                                                   .arg(sourceFile, QString::number(command.startLineNumber)));
                    }
                }
                continue;
            }

            if (fromColumn < 0 && toColumn < 0) {
                continue;
            }
            if (command.metadata.commandName == QStringLiteral("station")
                || command.metadata.commandName == QStringLiteral("equate")) {
                continue;
            }

            const QVector<int> stationColumns = {fromColumn, toColumn};
            for (const int stationColumn : stationColumns) {
                if (stationColumn < 0 || stationColumn >= command.parsed.tokens.size()) {
                    continue;
                }
                const QString stationName = command.parsed.tokens.at(stationColumn);
                if (stationName.trimmed().isEmpty() || stationName == QStringLiteral("-")) {
                    continue;
                }
                addStationReferenceKey(&index,
                                       stationName,
                                       dataNamespacePath,
                                       QStringLiteral("centerline-station:%1:%2:%3")
                                           .arg(sourceFile,
                                                QString::number(command.startLineNumber),
                                                QString::number(stationColumn)));
            }
        }
    }

    return index;
}

QVector<ProjectIndexDiagnostic> scanStationReferences(const QVector<ProjectStructureEntry> &entries,
                                                      ParsedFileCache *cache,
                                                      const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                      const std::function<bool()> &shouldCancel)
{
    QVector<ProjectIndexDiagnostic> diagnostics;
    QSet<QString> sourceFiles;
    for (const ProjectStructureEntry &entry : entries) {
        if (!entry.sourceFile.trimmed().isEmpty()) {
            sourceFiles.insert(normalizedFilePathKey(entry.sourceFile));
        }
    }

    const StationReferenceIndex stationIndex =
        stationReferencesFromProject(entries, sourceFiles, cache, inMemoryFileContentsByPath, shouldCancel);
    if (projectIndexScanShouldCancel(shouldCancel)) {
        return diagnostics;
    }
    const NamespaceEntriesByFile namespaceEntries = namespaceEntriesByFile(entries);

    for (const QString &sourceFile : std::as_const(sourceFiles)) {
        if (projectIndexScanShouldCancel(shouldCancel)) {
            return diagnostics;
        }
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(sourceFile, cache, inMemoryFileContentsByPath);
        for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
            if (projectIndexScanShouldCancel(shouldCancel)) {
                return diagnostics;
            }
            if (const std::optional<TherionSourceLogicalArgumentRange> stationPointReference =
                    pointStationNameReferenceRange(command)) {
                const ProjectStructureEntry ownerEntry =
                    nearestNamespaceEntryForLine(namespaceEntries, sourceFile, command.startLineNumber);
                appendStationReferenceDiagnosticsForRange(&diagnostics,
                                                          stationIndex,
                                                          ownerEntry,
                                                          sourceFile,
                                                          *stationPointReference,
                                                          true);
                continue;
            }

            if (command.metadata.commandName != QStringLiteral("equate")) {
                continue;
            }

            const ProjectStructureEntry ownerEntry =
                nearestNamespaceEntryForLine(namespaceEntries, sourceFile, command.startLineNumber);
            for (int tokenIndex = 1; tokenIndex < command.parsed.tokens.size(); ++tokenIndex) {
                const QString referenceName = command.parsed.tokens.value(tokenIndex).trimmed();
                if (referenceName.isEmpty() || referenceName.startsWith(QLatin1Char('-'))) {
                    break;
                }

                const TherionSourcePhysicalRange referenceRange =
                    parsedLineTokenPhysicalRange(command.parsed, tokenIndex);
                appendStationReferenceDiagnosticsForRange(&diagnostics,
                                                          stationIndex,
                                                          ownerEntry,
                                                          sourceFile,
                                                          TherionSourceLogicalArgumentRange{tokenIndex, referenceName, referenceRange},
                                                          false);
            }
        }
    }

    return diagnostics;
}

QVector<ProjectIndexDiagnostic> scanDuplicateObjectIds(const QVector<ProjectStructureEntry> &entries,
                                                       ParsedFileCache *cache,
                                                       const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                       const std::function<bool()> &shouldCancel)
{
    QVector<ProjectIndexDiagnostic> diagnostics;
    QHash<QString, QVector<ProjectStructureEntry>> entriesByFile;
    for (const ProjectStructureEntry &entry : entries) {
        if (!structureEntryKindParticipatesInDuplicateIdentity(entry.kind)
            || entry.name.trimmed().isEmpty()
            || entry.sourceFile.trimmed().isEmpty()
            || entry.lineNumber <= 0) {
            continue;
        }
        entriesByFile[normalizedFilePathKey(entry.sourceFile)].append(entry);
    }

    QHash<QString, ProjectStructureEntry> firstEntryByIdKey;
    for (auto fileIt = entriesByFile.constBegin(); fileIt != entriesByFile.constEnd(); ++fileIt) {
        if (projectIndexScanShouldCancel(shouldCancel)) {
            return diagnostics;
        }
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(fileIt.key(), cache, inMemoryFileContentsByPath);
        QHash<int, TherionSourceLogicalCommand> commandsByStartLine;
        for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
            commandsByStartLine.insert(command.startLineNumber, command);
        }

        for (const ProjectStructureEntry &entry : fileIt.value()) {
            if (projectIndexScanShouldCancel(shouldCancel)) {
                return diagnostics;
            }
            const auto commandIt = commandsByStartLine.constFind(entry.lineNumber);
            if (commandIt == commandsByStartLine.constEnd()) {
                continue;
            }

            const std::optional<TherionSourceLogicalArgumentRange> idRange =
                commandDuplicateIdentityRange(commandIt.value(), entry.kind);
            if (!idRange.has_value() || idRange->text.trimmed().isEmpty()) {
                continue;
            }

            const QString idKey = duplicateIdentityLookupKey(entry, idRange->text);
            if (idKey.isEmpty()) {
                continue;
            }
            const auto firstIt = firstEntryByIdKey.constFind(idKey);
            if (firstIt == firstEntryByIdKey.constEnd()) {
                firstEntryByIdKey.insert(idKey, entry);
                continue;
            }

            appendProjectIndexDiagnostic(&diagnostics,
                                         ProjectIndexDiagnosticKind::DuplicateObjectId,
                                         entry.objectId,
                                         entry.sourceFile,
                                         idRange->physicalRange.lineNumber,
                                         idRange->physicalRange.columnNumber,
                                         idRange->physicalRange.columnLength,
                                         idRange->text,
                                         2);
        }
    }

    return diagnostics;
}

QVector<ProjectStructureEntry> scanTh2ObjectsFromLogicalCommands(
    const QString &sourceFile,
    const QVector<TherionSourceLogicalCommand> &commands)
{
    QVector<ProjectStructureEntry> entries;
    if (commands.isEmpty()) {
        return entries;
    }

    QString currentScrapName;
    int currentScrapLine = 0;
    QString currentScrapObjectId;
    ProjectObjectIdentityGenerator identityGenerator;
    const QHash<int, QSet<QString>> lineMarksByLineStart = linePointMarksByLineStart(commands);

    auto ensureScrap = [&](const QString &scrapName, int lineNumber) {
        if (!currentScrapObjectId.isEmpty()) {
            return currentScrapObjectId;
        }

        ProjectStructureEntry entry;
        entry.kind = ProjectStructureEntryKind::Scrap;
        entry.category = projectCategoryFromKind(entry.kind);
        entry.objectId = identityGenerator.nextObjectId(entry.category, scrapName, sourceFile, QString());
        entry.name = scrapName;
        entry.sourceFile = sourceFile;
        entry.lineNumber = lineNumber;
        entry.depth = 0;
        entries.append(entry);
        currentScrapObjectId = entry.objectId;
        return currentScrapObjectId;
    };

    for (const TherionSourceLogicalCommand &command : commands) {
        const TherionParsedLine &parsedLine = command.parsed;
        if (command.metadata.commandName == QStringLiteral("scrap")) {
            currentScrapName = parsedLine.tokens.value(1, QStringLiteral("Unnamed Scrap"));
            currentScrapLine = command.startLineNumber;
            currentScrapObjectId.clear();
            ensureScrap(currentScrapName, currentScrapLine);
            continue;
        }

        if (command.metadata.commandName == QStringLiteral("endscrap")) {
            currentScrapName.clear();
            currentScrapLine = 0;
            currentScrapObjectId.clear();
            continue;
        }

        const ProjectStructureEntryKind kind = objectKindFromLine(parsedLine);
        if (kind == ProjectStructureEntryKind::Unknown) {
            continue;
        }

        if (currentScrapName.isEmpty()) {
            currentScrapName = QObject::tr("Unassigned Objects");
            currentScrapLine = 0;
            currentScrapObjectId.clear();
            ensureScrap(currentScrapName, currentScrapLine);
        }
        const QString parentObjectId = ensureScrap(currentScrapName, currentScrapLine);

        ProjectStructureEntry entry;
        entry.kind = kind;
        entry.parentObjectId = parentObjectId;
        entry.category = projectCategoryFromKind(kind);
        entry.name = objectNameFromLine(parsedLine);
        entry.sourceFile = sourceFile;
        entry.lineNumber = command.startLineNumber;
        entry.depth = 1;
        if (kind == ProjectStructureEntryKind::Line) {
            entry.linePointMarks = lineMarksByLineStart.value(command.startLineNumber);
        }

        if (entry.name.isEmpty()) {
            entry.name = objectDisplayText(entry, parsedLine);
        }
        entry.objectId = identityGenerator.nextObjectId(entry.category,
                                                        entry.name,
                                                        entry.sourceFile,
                                                        entry.parentObjectId);

        entries.append(entry);
    }

    return entries;
}
}

ProjectIndexSnapshot ProjectStructureIndex::scanProjectIndex(const QString &projectRootPath, QString *errorMessage)
{
    return scanProjectIndex(projectRootPath, QHash<QString, QString>(), errorMessage);
}

ProjectIndexSnapshot ProjectStructureIndex::scanProjectIndex(const QString &projectRootPath,
                                                             const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                             QString *errorMessage)
{
    return scanProjectIndex(projectRootPath, inMemoryFileContentsByPath, QString(), errorMessage);
}

ProjectIndexSnapshot ProjectStructureIndex::scanProjectIndex(const QString &projectRootPath,
                                                             const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                             const QString &preferredConfigPath,
                                                             QString *errorMessage)
{
    QVector<QString> filePaths;
    QDirIterator iterator(projectRootPath,
                          {QStringLiteral("*.th"),
                           QStringLiteral("*.th2"),
                           QStringLiteral("*.thconfig"),
                           QStringLiteral("thconfig"),
                           QStringLiteral("thconfig.*")},
                          QDir::Files,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        QFileInfo fileInfo(filePath);
        if (isInterestingProjectFile(fileInfo)) {
            filePaths.append(filePath);
        }
    }

    return scanProjectIndexFromFilePaths(projectRootPath,
                                         filePaths,
                                         inMemoryFileContentsByPath,
                                         {},
                                         preferredConfigPath,
                                         errorMessage);
}

ProjectIndexSnapshot ProjectStructureIndex::scanProjectIndex(const ProjectStructureIndexSourceSet &sourceSet,
                                                             QString *errorMessage)
{
    QVector<QString> filePaths;
    filePaths.reserve(sourceSet.sources.size());
    QHash<QString, QString> sourceTextByPath;
    QHash<QString, std::shared_ptr<const TherionSourceLogicalDocument>> prebuiltLogicalDocumentsByPath;
    for (const ProjectStructureIndexSource &source : sourceSet.sources) {
        const QString normalizedPath = normalizedFilePathKey(source.normalizedPath);
        if (normalizedPath.isEmpty()) {
            continue;
        }
        filePaths.append(normalizedPath);
        if (source.textLoaded) {
            sourceTextByPath.insert(normalizedPath, source.text);
        }
        if (source.logicalDocument) {
            prebuiltLogicalDocumentsByPath.insert(normalizedPath, source.logicalDocument);
        }
    }
    return scanProjectIndexFromFilePaths(sourceSet.projectRootPath,
                                         filePaths,
                                         sourceTextByPath,
                                         prebuiltLogicalDocumentsByPath,
                                         sourceSet.preferredConfigPath,
                                         errorMessage,
                                         sourceSet.shouldCancel);
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanProject(const QString &projectRootPath, QString *errorMessage)
{
    return scanProject(projectRootPath, QHash<QString, QString>(), errorMessage);
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanProject(const QString &projectRootPath,
                                                                  const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                                  QString *errorMessage)
{
    return scanProjectIndex(projectRootPath, inMemoryFileContentsByPath, errorMessage).entries;
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanTh2Objects(const QString &sourceFile, const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return {};
    }

    TherionSourceSnapshotCache sourceSnapshotCache;
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    metadata.revisionId = 1;
    const TherionSourceLogicalDocument logicalDocument =
        logicalDocumentForText(text, sourceSnapshotCache, metadata);
    return scanTh2ObjectsFromLogicalCommands(sourceFile, logicalDocument.commands());
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanTh2Objects(const QString &sourceFile,
                                                                     const QVector<TherionParsedLine> &parsedLines)
{
    if (parsedLines.isEmpty()) {
        return {};
    }

    QVector<TherionSourceLogicalCommand> commands;
    commands.reserve(parsedLines.size());
    for (const TherionParsedLine &parsedLine : parsedLines) {
        TherionSourceLogicalCommand command;
        command.startLineNumber = parsedLine.lineNumber;
        command.parsed = parsedLine;
        command.metadata.commandName = parsedLine.directive;
        commands.append(command);
    }
    return scanTh2ObjectsFromLogicalCommands(sourceFile, commands);
}

QString ProjectStructureIndex::structureEntryNodeKey(const ProjectStructureEntry &entry)
{
    if (!entry.objectId.isEmpty()) {
        return entry.objectId;
    }

    return QStringLiteral("%1:%2").arg(normalizedFilePathKey(entry.sourceFile)).arg(entry.lineNumber);
}
}
