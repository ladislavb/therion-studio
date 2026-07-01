#include "ProjectValidationScanner.h"

#include "ProjectSourceProjectionCache.h"
#include "ProjectSourceSnapshot.h"

#include "../core/ProjectStructureIndex.h"
#include "../core/TherionFileTypes.h"
#include "../core/TherionSourceLogicalDocument.h"
#include "../core/TherionSourceReferenceResolver.h"

#include <QDir>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QDebug>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QtConcurrent>

#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kMaximumProjectValidationFindings = 2000;

bool diagnosticProjectValidationLoggingEnabled()
{
    static const bool enabled = []() {
        const QString value = QString::fromLocal8Bit(qgetenv("THERION_STUDIO_ENABLE_LOG")).trimmed().toLower();
        return value == QStringLiteral("1")
            || value == QStringLiteral("true")
            || value == QStringLiteral("yes")
            || value == QStringLiteral("on");
    }();
    return enabled;
}

void addHashPart(QCryptographicHash *hash, const QString &value)
{
    if (hash == nullptr) {
        return;
    }
    hash->addData(value.toUtf8());
    static const QByteArray separator(1, '\0');
    hash->addData(separator);
}

void addHashPart(QCryptographicHash *hash, int value)
{
    addHashPart(hash, QString::number(value));
}

QStringList sortedStringSet(const QSet<QString> &values)
{
    QStringList sorted(values.begin(), values.end());
    sorted.sort(Qt::CaseSensitive);
    return sorted;
}

QStringList sortedStringList(QStringList values)
{
    values.sort(Qt::CaseSensitive);
    return values;
}

void addStringListHash(QCryptographicHash *hash, QStringList values)
{
    addHashPart(hash, values.size());
    for (const QString &value : sortedStringList(std::move(values))) {
        addHashPart(hash, value);
    }
}

void addStringSetHash(QCryptographicHash *hash, const QSet<QString> &values)
{
    addStringListHash(hash, sortedStringSet(values));
}

template <typename ValuesByKey, typename ValueHandler>
void addSortedHashKeys(QCryptographicHash *hash, const ValuesByKey &valuesByKey, ValueHandler valueHandler)
{
    QStringList keys = valuesByKey.keys();
    keys.sort(Qt::CaseSensitive);
    addHashPart(hash, keys.size());
    for (const QString &key : std::as_const(keys)) {
        addHashPart(hash, key);
        valueHandler(hash, valuesByKey.value(key));
    }
}

QString validationCatalogSignature(const TherionSourceValidationCatalog &catalog)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    addStringSetHash(&hash, catalog.commandNames);
    addSortedHashKeys(&hash, catalog.commandContexts, addStringListHash);
    addSortedHashKeys(&hash, catalog.commandDocumentTypes, addStringSetHash);
    addSortedHashKeys(&hash, catalog.commandOptionNames, addStringSetHash);
    addSortedHashKeys(&hash, catalog.commandRequiredPositionalCount, [](QCryptographicHash *target, int value) {
        addHashPart(target, value);
    });
    addSortedHashKeys(&hash, catalog.commandMaxPositionalCount, [](QCryptographicHash *target, int value) {
        addHashPart(target, value);
    });
    addSortedHashKeys(&hash, catalog.commandArgumentAllowedValuesByKey, addStringListHash);
    addSortedHashKeys(&hash, catalog.commandTypeValues, addStringListHash);
    addSortedHashKeys(&hash, catalog.commandOptionAllowedValuesByKey, addStringListHash);
    addSortedHashKeys(&hash, catalog.commandSubtypeValuesByTypeKey, addStringListHash);
    addSortedHashKeys(&hash, catalog.commandOptionValueArityTokens, [](QCryptographicHash *target, const QString &value) {
        addHashPart(target, value);
    });
    addSortedHashKeys(&hash, catalog.commandOptionFixedArityByKey, [](QCryptographicHash *target, int value) {
        addHashPart(target, value);
    });
    return QString::fromLatin1(hash.result().toHex());
}

QString sourceLineTextAt(const QString &text, int oneBasedLineNumber)
{
    if (oneBasedLineNumber <= 0) {
        return QString();
    }

    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (oneBasedLineNumber > lines.size()) {
        return QString();
    }

    QString line = lines.at(oneBasedLineNumber - 1);
    if (line.endsWith(QLatin1Char('\r'))) {
        line.chop(1);
    }
    return line;
}

TherionSourceDiagnostic diagnosticForProjectIndexDiagnostic(const ProjectIndexDiagnostic &indexDiagnostic)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.lineNumber = indexDiagnostic.lineNumber;
    diagnostic.columnNumber = qMax(1, indexDiagnostic.columnNumber);
    diagnostic.columnLength = indexDiagnostic.columnLength > 0
        ? indexDiagnostic.columnLength
        : indexDiagnostic.referencedName.size();
    diagnostic.currentText = indexDiagnostic.referencedName;

    switch (indexDiagnostic.kind) {
    case ProjectIndexDiagnosticKind::UnknownMapScrapReference:
        diagnostic.code = QStringLiteral("unknown-map-scrap-reference");
        diagnostic.title = QObject::tr("Unknown map scrap reference");
        diagnostic.message = QObject::tr("Map references scrap `%1`, but no matching scrap was found in the project index.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::UnknownMapReference:
        diagnostic.code = QStringLiteral("unknown-map-reference");
        diagnostic.title = QObject::tr("Unknown map reference");
        diagnostic.message = QObject::tr("Map references child map `%1`, but no matching map was found in the project index.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::AmbiguousMapScrapReference:
        diagnostic.code = QStringLiteral("ambiguous-map-scrap-reference");
        diagnostic.title = QObject::tr("Ambiguous map scrap reference");
        diagnostic.message = QObject::tr("Map scrap reference `%1` matches %2 scraps in the project index.")
                                 .arg(indexDiagnostic.referencedName)
                                 .arg(indexDiagnostic.candidateCount);
        break;
    case ProjectIndexDiagnosticKind::AmbiguousMapReference:
        diagnostic.code = QStringLiteral("ambiguous-map-reference");
        diagnostic.title = QObject::tr("Ambiguous map reference");
        diagnostic.message = QObject::tr("Map reference `%1` matches %2 maps in the project index.")
                                 .arg(indexDiagnostic.referencedName)
                                 .arg(indexDiagnostic.candidateCount);
        break;
    case ProjectIndexDiagnosticKind::MixedMapAndScrapReferences:
        diagnostic.code = QStringLiteral("mixed-map-and-scrap-references");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Warning;
        diagnostic.title = QObject::tr("Mixed map and scrap references");
        diagnostic.message = QObject::tr("Map composition mixes child map and scrap references; `%1` changes the content kind.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::UnknownJoinReference:
        diagnostic.code = QStringLiteral("unknown-join-reference");
        diagnostic.title = QObject::tr("Unknown join reference");
        diagnostic.message = QObject::tr("Join references `%1`, but no matching scrap, line, or point was found in the project index.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::UnknownJoinLinePointMark:
        diagnostic.code = QStringLiteral("unknown-join-line-point-mark");
        diagnostic.title = QObject::tr("Unknown join line-point mark");
        diagnostic.message = QObject::tr("Join references `%1`, but the resolved line does not define that line-point mark.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::AmbiguousJoinReference:
        diagnostic.code = QStringLiteral("ambiguous-join-reference");
        diagnostic.title = QObject::tr("Ambiguous join reference");
        diagnostic.message = QObject::tr("Join reference `%1` matches %2 objects in the project index.")
                                 .arg(indexDiagnostic.referencedName)
                                 .arg(indexDiagnostic.candidateCount);
        break;
    case ProjectIndexDiagnosticKind::UnknownStationReference:
        diagnostic.code = QStringLiteral("unknown-station-reference");
        diagnostic.title = QObject::tr("Unknown station reference");
        diagnostic.message = QObject::tr("Station reference `%1` has no matching station in the project index.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::AmbiguousStationReference:
        diagnostic.code = QStringLiteral("ambiguous-station-reference");
        diagnostic.title = QObject::tr("Ambiguous station reference");
        diagnostic.message = QObject::tr("Station reference `%1` matches %2 stations in the project index.")
                                 .arg(indexDiagnostic.referencedName)
                                 .arg(indexDiagnostic.candidateCount);
        break;
    case ProjectIndexDiagnosticKind::DuplicateObjectId:
        diagnostic.code = QStringLiteral("duplicate-object-id");
        diagnostic.title = QObject::tr("Duplicate object id");
        diagnostic.message = QObject::tr("Object id `%1` is already used by another object in this namespace.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    }

    return diagnostic;
}

bool containsEquivalentFinding(const QVector<ProjectValidationScanner::Finding> &findings,
                               const QString &filePath,
                               const TherionSourceDiagnostic &diagnostic)
{
    for (const ProjectValidationScanner::Finding &finding : findings) {
        if (finding.filePath == filePath
            && finding.diagnostic.code == diagnostic.code
            && finding.diagnostic.lineNumber == diagnostic.lineNumber
            && finding.diagnostic.columnNumber == diagnostic.columnNumber) {
            return true;
        }
    }
    return false;
}

QString normalizedOptionName(QString optionName)
{
    optionName = optionName.trimmed().toLower();
    while (optionName.startsWith(QLatin1Char('-'))) {
        optionName.remove(0, 1);
    }
    return optionName;
}

const TherionSourceLogicalArgumentRange *pointStationNameRange(const TherionSourceLogicalCommand &command)
{
    if (command.metadata.commandName != QStringLiteral("point")
        || command.positionalArgumentRanges.size() < 3) {
        return nullptr;
    }

    const QString pointType = command.positionalArgumentRanges.at(2).text.trimmed().section(QLatin1Char(':'), 0, 0);
    if (pointType.compare(QStringLiteral("station"), Qt::CaseInsensitive) != 0) {
        return nullptr;
    }

    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        if (normalizedOptionName(optionEntry.key) != QStringLiteral("name")) {
            continue;
        }
        for (const TherionSourceLogicalArgumentRange &valueRange : optionEntry.valueRanges) {
            if (!valueRange.text.trimmed().isEmpty()) {
                return &valueRange;
            }
        }
    }

    return nullptr;
}

QSet<QString> indexedProjectSourceFiles(const ProjectIndexSnapshot &snapshot)
{
    QSet<QString> sourceFiles;
    for (const ProjectStructureEntry &entry : snapshot.entries) {
        const QString normalizedPath = canonicalOrAbsoluteFilePath(entry.sourceFile);
        if (!normalizedPath.isEmpty()) {
            sourceFiles.insert(normalizedPath);
        }
    }
    return sourceFiles;
}

ProjectStructureIndexSourceSet projectStructureIndexSourceSetWithLogicalDocuments(
    const ProjectSourceSnapshot &snapshot,
    const QHash<QString, ProjectSourceDocument> &documentByPath,
    ProjectSourceProjectionCache &projectionCache)
{
    ProjectStructureIndexSourceSet sourceSet = projectStructureIndexSourceSet(snapshot);
    for (ProjectStructureIndexSource &source : sourceSet.sources) {
        if (!source.textLoaded) {
            continue;
        }
        const auto documentIt = documentByPath.constFind(source.normalizedPath);
        if (documentIt == documentByPath.constEnd()) {
            continue;
        }
        source.logicalDocument = projectionCache.logicalDocumentHandle(documentIt.value());
    }
    return sourceSet;
}

void appendUnindexedTh2StationNameFindings(ProjectValidationScanner::Result *result,
                                           const ProjectSourceDocument &document,
                                           ProjectSourceProjectionCache &projectionCache)
{
    if (result == nullptr
        || result->limitReached
        || therionSourceDocumentTypeForFilePath(document.normalizedPath) != TherionSourceDocumentType::TherionMap) {
        return;
    }

    const TherionSourceLogicalDocument &logicalDocument =
        projectionCache.logicalDocument(document);
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        const TherionSourceLogicalArgumentRange *nameRange = pointStationNameRange(command);
        if (nameRange == nullptr) {
            continue;
        }
        const QString referenceName = nameRange->text.trimmed();
        if (referenceName.contains(QLatin1Char('@'))) {
            continue;
        }

        TherionSourceDiagnostic diagnostic;
        diagnostic.code = QStringLiteral("unknown-station-reference");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
        diagnostic.lineNumber = nameRange->physicalRange.lineNumber;
        diagnostic.columnNumber = qMax(1, nameRange->physicalRange.columnNumber);
        diagnostic.columnLength = qMax(1, nameRange->physicalRange.columnLength);
        diagnostic.title = QObject::tr("Unknown station reference");
        diagnostic.message = QObject::tr("Station reference `%1` cannot be resolved because this file is not included in the project source graph.")
                                 .arg(referenceName);
        diagnostic.currentText = nameRange->physicalRange.lineText;

        if (!containsEquivalentFinding(result->findings, document.normalizedPath, diagnostic)) {
            result->findings.append({document.normalizedPath, diagnostic});
        }
        if (result->findings.size() >= kMaximumProjectValidationFindings) {
            result->limitReached = true;
            return;
        }
    }
}

void appendProjectIndexFindings(ProjectValidationScanner::Result *result,
                                const QString &projectRootPath,
                                const ProjectIndexSnapshot &snapshot,
                                const QString &indexErrorMessage,
                                const QHash<QString, QString> &searchedTextByPath,
                                const QHash<QString, ProjectSourceDocument> &searchedDocumentByPath,
                                ProjectSourceProjectionCache &projectionCache)
{
    if (result == nullptr || result->limitReached) {
        return;
    }

    if (!indexErrorMessage.isEmpty()) {
        TherionSourceDiagnostic diagnostic;
        diagnostic.code = QStringLiteral("project-index-unavailable");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Warning;
        diagnostic.lineNumber = 1;
        diagnostic.columnNumber = 1;
        diagnostic.columnLength = 1;
        diagnostic.title = QObject::tr("Project index unavailable");
        diagnostic.message = indexErrorMessage;
        diagnostic.currentText = projectRootPath;
        result->findings.append({projectRootPath, diagnostic});
        if (result->findings.size() >= kMaximumProjectValidationFindings) {
            result->limitReached = true;
        }
        return;
    }

    for (const ProjectIndexDiagnostic &indexDiagnostic : snapshot.diagnostics) {
        if (indexDiagnostic.sourceFile.trimmed().isEmpty()) {
            continue;
        }

        TherionSourceDiagnostic diagnostic = diagnosticForProjectIndexDiagnostic(indexDiagnostic);
        const QString normalizedSourceFile = canonicalOrAbsoluteFilePath(indexDiagnostic.sourceFile);
        const QString sourceLine = sourceLineTextAt(searchedTextByPath.value(normalizedSourceFile),
                                                    diagnostic.lineNumber);
        if (!sourceLine.isEmpty()) {
            diagnostic.currentText = sourceLine;
        }
        if (containsEquivalentFinding(result->findings, indexDiagnostic.sourceFile, diagnostic)) {
            continue;
        }
        result->findings.append({indexDiagnostic.sourceFile, diagnostic});
        if (result->findings.size() >= kMaximumProjectValidationFindings) {
            result->limitReached = true;
            return;
        }
    }

    const QSet<QString> indexedSourceFiles = indexedProjectSourceFiles(snapshot);
    for (auto it = searchedTextByPath.constBegin(); it != searchedTextByPath.constEnd(); ++it) {
        if (indexedSourceFiles.contains(it.key())) {
            continue;
        }
        const auto documentIt = searchedDocumentByPath.constFind(it.key());
        if (documentIt == searchedDocumentByPath.constEnd()) {
            continue;
        }
        appendUnindexedTh2StationNameFindings(result,
                                              documentIt.value(),
                                              projectionCache);
        if (result->limitReached) {
            return;
        }
    }
}

void appendFindingsForDocument(ProjectValidationScanner::Result *result,
                               const ProjectSourceDocument &document,
                               const TherionSourceValidationCatalog &validationCatalog,
                               const QSet<QString> &knownProjectFilePaths,
                               ProjectSourceProjectionCache &projectionCache,
                               TherionSourceSnapshotCatalogKey catalogKey)
{
    if (result == nullptr) {
        return;
    }

    const TherionSourceDocument &sourceDocument =
        projectionCache.sourceDocument(document);
    const TherionSourceLogicalDocument &logicalDocument =
        projectionCache.logicalDocument(document, validationCatalog, catalogKey);

    const TherionSourceValidationResult validation =
        TherionSourceValidator::validate(sourceDocument, logicalDocument, validationCatalog);
    const bool suppressUnknownCommandWarnings = isTherionConfigFilePath(document.normalizedPath);
    for (const TherionSourceDiagnostic &diagnostic : validation.diagnostics) {
        if (suppressUnknownCommandWarnings && diagnostic.code == QStringLiteral("unknown-command")) {
            continue;
        }
        result->findings.append({document.normalizedPath, diagnostic});
        if (result->findings.size() >= kMaximumProjectValidationFindings) {
            result->limitReached = true;
            return;
        }
    }

    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        QString commandName = command.metadata.commandName;
        if (commandName.isEmpty()) {
            commandName = command.normalizedDirective;
        }
        if (commandName.isEmpty()) {
            commandName = command.parsed.directive;
        }
        if (commandName != QStringLiteral("input") && commandName != QStringLiteral("source")) {
            continue;
        }

        const QString referencedPath = command.parsed.tokens.value(1).trimmed();
        if (referencedPath.isEmpty()
            || !resolveTherionSourceReferencePath(document.normalizedPath, referencedPath, knownProjectFilePaths).isEmpty()) {
            continue;
        }

        TherionSourceDiagnostic diagnostic;
        diagnostic.code = QStringLiteral("missing-source-reference");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
        diagnostic.lineNumber = command.startLineNumber;
        diagnostic.columnNumber = 1;
        diagnostic.columnLength = command.text.size();
        diagnostic.title = QObject::tr("Missing referenced source file");
        diagnostic.message = QObject::tr("Command `%1` references `%2`, but no matching project file was found.")
                                 .arg(commandName, referencedPath);
        diagnostic.currentText = command.text;
        TherionSourcePhysicalRange tokenRange;
        if (command.physicalRangeForTokenIndex(1, &tokenRange)) {
            diagnostic.lineNumber = tokenRange.lineNumber;
            diagnostic.columnNumber = tokenRange.columnNumber;
            diagnostic.columnLength = tokenRange.columnLength;
        }

        result->findings.append({document.normalizedPath, diagnostic});
        if (result->findings.size() >= kMaximumProjectValidationFindings) {
            result->limitReached = true;
            return;
        }
    }
}

ProjectValidationScanner::Result performProjectValidation(const QString &projectRootPath,
                                                          const TherionSourceValidationCatalog &validationCatalog,
                                                          const QHash<QString, QString> &inMemoryProjectContentsByPath,
                                                          quint64 generation,
                                                          ProjectSourceProjectionCache &projectionCache)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    qint64 collectMs = 0;
    qint64 validateMs = 0;
    qint64 projectIndexMs = 0;
    ProjectSourceSnapshot projectSourceSnapshot;

    ProjectValidationScanner::Result result;
    result.generation = generation;
    result.projectRootPath = projectRootPath;

    auto logAndReturn = [&](ProjectValidationScanner::Result value) {
        value.projectionCacheStats = projectionCache.stats();
        if (diagnosticProjectValidationLoggingEnabled()) {
            qInfo().noquote()
                << QStringLiteral("project-validation-scan generation=%1 files=%2 searched=%3 findings=%4 limit_reached=%5 collect_ms=%6 validate_ms=%7 project_index_ms=%8 total_ms=%9 projection_source_builds=%10 projection_source_hits=%11 projection_logical_builds=%12 projection_logical_hits=%13 projection_catalog_builds=%14 projection_catalog_hits=%15 project_index_logical_builds=%16 project_index_logical_hits=%17 project_index_prebuilt_logical_hits=%18 root=\"%19\" error=\"%20\"")
                       .arg(value.generation)
                       .arg(projectSourceSnapshot.documents.size())
                       .arg(value.searchedFileCount)
                       .arg(value.findings.size())
                       .arg(value.limitReached ? QStringLiteral("true") : QStringLiteral("false"))
                       .arg(collectMs)
                       .arg(validateMs)
                       .arg(projectIndexMs)
                       .arg(totalTimer.elapsed())
                       .arg(value.projectionCacheStats.sourceDocumentBuilds)
                       .arg(value.projectionCacheStats.sourceDocumentHits)
                       .arg(value.projectionCacheStats.logicalDocumentBuilds)
                       .arg(value.projectionCacheStats.logicalDocumentHits)
                       .arg(value.projectionCacheStats.catalogLogicalDocumentBuilds)
                       .arg(value.projectionCacheStats.catalogLogicalDocumentHits)
                       .arg(value.projectIndexScanStats.logicalDocumentBuilds)
                       .arg(value.projectIndexScanStats.logicalDocumentHits)
                       .arg(value.projectIndexScanStats.prebuiltLogicalDocumentHits)
                       .arg(value.projectRootPath)
                       .arg(value.errorMessage);
        }
        return value;
    };

    if (result.projectRootPath.trimmed().isEmpty() || !QDir(result.projectRootPath).exists()) {
        result.errorMessage = QObject::tr("Open a project before validating.");
        return logAndReturn(result);
    }

    QHash<QString, QString> searchedTextByPath;
    {
        QElapsedTimer collectTimer;
        collectTimer.start();
        projectSourceSnapshot = collectProjectSourceSnapshot(result.projectRootPath, {}, inMemoryProjectContentsByPath);
        collectMs = collectTimer.elapsed();
    }
    QSet<QString> knownProjectFilePaths;
    QHash<QString, ProjectSourceDocument> projectSourceDocumentByPath;
    for (const ProjectSourceDocument &document : std::as_const(projectSourceSnapshot.documents)) {
        knownProjectFilePaths.insert(document.normalizedPath);
        projectSourceDocumentByPath.insert(document.normalizedPath, document);
    }

    {
        QElapsedTimer validateTimer;
        validateTimer.start();
        for (const ProjectSourceDocument &document : std::as_const(projectSourceSnapshot.documents)) {
            ++result.searchedFileCount;
            searchedTextByPath.insert(document.normalizedPath, document.text);
            appendFindingsForDocument(&result,
                                      document,
                                      validationCatalog,
                                      knownProjectFilePaths,
                                      projectionCache,
                                      TherionSourceSnapshotCatalogKey::fromRevision(1));
            if (result.limitReached) {
                validateMs = validateTimer.elapsed();
                return logAndReturn(result);
            }
        }
        validateMs = validateTimer.elapsed();
    }

    {
        QElapsedTimer projectIndexTimer;
        projectIndexTimer.start();
        QString indexErrorMessage;
        const ProjectIndexSnapshot projectIndexSnapshot = ProjectStructureIndex::scanProjectIndex(
            projectStructureIndexSourceSetWithLogicalDocuments(projectSourceSnapshot,
                                                               projectSourceDocumentByPath,
                                                               projectionCache),
            &indexErrorMessage);
        result.projectIndexScanStats = projectIndexSnapshot.scanStats;
        appendProjectIndexFindings(&result,
                                   result.projectRootPath,
                                   projectIndexSnapshot,
                                   indexErrorMessage,
                                   searchedTextByPath,
                                   projectSourceDocumentByPath,
                                   projectionCache);
        projectIndexMs = projectIndexTimer.elapsed();
    }

    return logAndReturn(result);
}
}

ProjectValidationScanner::ProjectValidationScanner(QObject *parent)
    : QObject(parent)
    , debounceTimer_(new QTimer(this))
    , scanWatcher_(new QFutureWatcher<Result>(this))
    , projectionCache_(std::make_shared<ProjectSourceProjectionCache>())
{
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(120);
    connect(debounceTimer_, &QTimer::timeout, this, &ProjectValidationScanner::startScan);
    connect(scanWatcher_, &QFutureWatcher<Result>::finished, this, &ProjectValidationScanner::handleScanFinished);
}

void ProjectValidationScanner::requestScan(const QString &projectRootPath,
                                           const TherionSourceValidationCatalog &validationCatalog,
                                           const QHash<QString, QString> &inMemoryProjectContentsByPath)
{
    pendingRequest_.projectRootPath = projectRootPath;
    pendingRequest_.validationCatalog = validationCatalog;
    pendingRequest_.inMemoryProjectContentsByPath = inMemoryProjectContentsByPath;
    hasPendingRequest_ = true;
    if (!debounceTimer_->isActive()) {
        debounceTimer_->start();
    }
}

void ProjectValidationScanner::setDebounceIntervalMs(int intervalMs)
{
    debounceTimer_->setInterval(intervalMs);
}

void ProjectValidationScanner::startScan()
{
    if (!hasPendingRequest_) {
        return;
    }

    if (scanWatcher_->isRunning()) {
        queuedScan_ = true;
        return;
    }

    const Request request = pendingRequest_;
    hasPendingRequest_ = false;
    const quint64 generation = ++generation_;
    const QString normalizedProjectRootPath = canonicalOrAbsoluteFilePath(request.projectRootPath);
    const QString catalogSignature = validationCatalogSignature(request.validationCatalog);
    if (projectionCacheProjectRootPath_ != normalizedProjectRootPath
        || projectionCacheCatalogSignature_ != catalogSignature) {
        projectionCache_->clear();
        projectionCacheProjectRootPath_ = normalizedProjectRootPath;
        projectionCacheCatalogSignature_ = catalogSignature;
    } else {
        projectionCache_->resetStats();
    }
    emit validationStarted(generation, request.projectRootPath);

    const std::shared_ptr<ProjectSourceProjectionCache> projectionCache = projectionCache_;
    auto future = QtConcurrent::run([request, generation, projectionCache]() {
        return performProjectValidation(request.projectRootPath,
                                        request.validationCatalog,
                                        request.inMemoryProjectContentsByPath,
                                        generation,
                                        *projectionCache);
    });
    scanWatcher_->setFuture(future);
}

void ProjectValidationScanner::handleScanFinished()
{
    const Result result = scanWatcher_->result();
    const bool hasSupersedingRequest = queuedScan_ || hasPendingRequest_;
    emit validationFinished(result);

    if (hasSupersedingRequest) {
        queuedScan_ = false;
        debounceTimer_->stop();
        startScan();
    }
}
}
