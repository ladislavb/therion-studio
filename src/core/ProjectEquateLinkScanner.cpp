#include "ProjectEquateLinkScanner.h"

#include "Th2CentrelineAlignment.h"
#include "TherionCommandLineModel.h"
#include "TherionSourceLogicalDocument.h"
#include "TherionStationNameRules.h"

#include <QSet>

namespace TherionStudio
{
namespace
{
bool scanShouldCancel(const ProjectEquateLinkScanInput &input)
{
    return input.shouldCancel && input.shouldCancel();
}

QString surveyNameForEntry(const ProjectEquateLinkScanInput &input, const ProjectStructureEntry &entry)
{
    QString objectId = entry.parentObjectId;
    while (!objectId.isEmpty()) {
        const auto it = input.entriesByObjectId.constFind(objectId);
        if (it == input.entriesByObjectId.constEnd()) {
            break;
        }
        if (it->kind == ProjectStructureEntryKind::Survey) {
            return it->name;
        }
        objectId = it->parentObjectId;
    }
    return QString();
}

ProjectStructureEntry scrapEntryAtLine(const ProjectEquateLinkScanInput &input, const QString &sourceFile, int lineNumber)
{
    for (const ProjectStructureEntry &entry : input.entriesByFile.value(sourceFile)) {
        if (entry.kind == ProjectStructureEntryKind::Scrap && entry.lineNumber == lineNumber) {
            return entry;
        }
    }
    return ProjectStructureEntry();
}

bool linksDifferentNamespaces(const QVector<ProjectEquateStation> &stations)
{
    QSet<QString> namespaces;
    for (const ProjectEquateStation &station : stations) {
        if (station.resolved) {
            namespaces.insert(station.namespacePath.toLower());
        }
    }
    return namespaces.size() > 1;
}

void scanEquateCommand(const ProjectEquateLinkScanInput &input,
                       const QString &sourceFile,
                       const TherionSourceLogicalCommand &command,
                       ProjectEquateLinkScanResult *result)
{
    ProjectEquateLink link;
    link.sourceFile = sourceFile;
    link.lineNumber = command.startLineNumber;
    const QString ownerNamespacePath = input.ownerNamespacePath(sourceFile, command.startLineNumber);
    for (const TherionSourceLogicalArgumentRange &argument : command.positionalArgumentRanges) {
        const QString reference = argument.text.trimmed();
        if (reference.isEmpty() || reference.startsWith(QLatin1Char('-'))) {
            break;
        }
        link.stations.append(input.resolveStation(reference, ownerNamespacePath));
    }
    if (linksDifferentNamespaces(link.stations)) {
        result->equateLinks.append(link);
    }
}
}

QString normalizedScrapProjection(const QStringList &projectionValueTokens)
{
    QStringList parts = projectionValueTokens.join(QLatin1Char(' ')).toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // Degrees are Therion's default angle unit, so `[elevation 90 deg]` names the same view as `[elevation 90]`.
    static const QStringList degreeUnits = {QStringLiteral("deg"), QStringLiteral("degree"), QStringLiteral("degrees")};
    if (parts.size() == 3 && degreeUnits.contains(parts.last())) {
        parts.removeLast();
    }
    return parts.isEmpty() ? QStringLiteral("plan") : parts.join(QLatin1Char(' '));
}

ProjectEquateLinkScanResult scanProjectEquateLinks(const ProjectEquateLinkScanInput &input)
{
    ProjectEquateLinkScanResult result;
    QVector<ProjectStationDrawingPoint> drawingPoints;
    for (const QString &sourceFile : input.sourceFiles) {
        if (scanShouldCancel(input)) {
            return result;
        }
        const TherionSourceLogicalDocument &logicalDocument = input.logicalDocumentForFile(sourceFile);
        const QHash<int, TherionStationNameTransform> stationNameTransforms =
            therionScrapStationNameTransformsByStartLine(logicalDocument);
        ProjectStructureEntry scrapEntry;
        QString scrapProjection;
        for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
            const QString &commandName = command.metadata.commandName;
            if (commandName == QStringLiteral("equate")) {
                scanEquateCommand(input, sourceFile, command, &result);
                continue;
            }
            if (commandName == QStringLiteral("scrap")) {
                scrapEntry = scrapEntryAtLine(input, sourceFile, command.startLineNumber);
                scrapProjection = normalizedScrapProjection(
                    commandOptionUnbracketedValueTokens(command.parsed.tokens, QStringLiteral("projection")));
                if (!scrapEntry.objectId.isEmpty()) {
                    result.scrapProjectionByObjectId.insert(scrapEntry.objectId, scrapProjection);
                }
                continue;
            }
            if (commandName == QStringLiteral("endscrap")) {
                scrapEntry = ProjectStructureEntry();
                continue;
            }
            // Scraps with `-projection none` have no geometric relation to the centreline.
            if (commandName != QStringLiteral("point") || scrapEntry.lineNumber <= 0
                || scrapProjection == QStringLiteral("none")) {
                continue;
            }
            const std::optional<TherionSourceLogicalArgumentRange> stationNameRange =
                therionStationPointNameRange(command);
            if (!stationNameRange.has_value()) {
                continue;
            }
            const QString stationName = stationNameRange->text.trimmed();
            const std::optional<TherionStationNameTransform> transform =
                therionActiveScrapStationNameTransform(command, stationNameTransforms);
            const QString reference = transform.has_value()
                ? therionStationReferenceWithScrapNameTransform(stationName, transform.value())
                : stationName;
            const ProjectEquateStation station = input.resolveStation(reference, scrapEntry.namespacePath);
            if (station.stationKey.isEmpty()) {
                continue;
            }
            result.stationPointsByScrapObjectId[scrapEntry.objectId].append(
                ProjectScrapStationPoint{command.startLineNumber, station.stationKey, th2PointCommandPosition(command)});
            drawingPoints.append(ProjectStationDrawingPoint{station.stationKey,
                                                            sourceFile,
                                                            command.startLineNumber,
                                                            scrapEntry.name,
                                                            surveyNameForEntry(input, scrapEntry),
                                                            scrapProjection,
                                                            scrapEntry.objectId});
        }
    }

    for (ProjectEquateLink leg : input.noSurveyLegs) {
        if (linksDifferentNamespaces(leg.stations)) {
            leg.kind = ProjectStationLinkKind::NoSurvey;
            result.equateLinks.append(leg);
        }
    }

    // Keep only drawing points for equated stations; the index stays small for large projects.
    QSet<QString> equatedStationKeys;
    for (const ProjectEquateLink &link : std::as_const(result.equateLinks)) {
        for (const ProjectEquateStation &station : link.stations) {
            equatedStationKeys.insert(station.stationKey);
        }
    }
    for (const ProjectStationDrawingPoint &point : std::as_const(drawingPoints)) {
        if (equatedStationKeys.contains(point.stationKey)) {
            result.stationDrawingPointsByStationKey[point.stationKey].append(point);
        }
    }
    return result;
}
}
