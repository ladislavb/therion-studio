#pragma once

#include "ProjectStructureIndex.h"

#include <QHash>
#include <QString>
#include <QVector>

#include <functional>

namespace TherionStudio
{
class TherionSourceLogicalDocument;

// Inputs owned by ProjectStructureIndex so equate scanning reuses its namespace and
// station-reference resolution instead of duplicating those rules.
struct ProjectEquateLinkScanInput
{
    QVector<QString> sourceFiles;
    QHash<QString, QVector<ProjectStructureEntry>> entriesByFile;
    QHash<QString, ProjectStructureEntry> entriesByObjectId;
    std::function<const TherionSourceLogicalDocument &(const QString &sourceFile)> logicalDocumentForFile;
    std::function<QString(const QString &sourceFile, int lineNumber)> ownerNamespacePath;
    std::function<ProjectEquateStation(const QString &reference, const QString &ownerNamespacePath)> resolveStation;
    std::function<bool()> shouldCancel;
    // `data nosurvey` legs with both stations already resolved; kept when they link different namespaces.
    QVector<ProjectEquateLink> noSurveyLegs;
};

struct ProjectEquateLinkScanResult
{
    QVector<ProjectEquateLink> equateLinks;
    QHash<QString, QVector<ProjectStationDrawingPoint>> stationDrawingPointsByStationKey;
    QHash<QString, QString> scrapProjectionByObjectId;
    QHash<QString, QVector<ProjectScrapStationPoint>> stationPointsByScrapObjectId;
};

[[nodiscard]] QString normalizedScrapProjection(const QStringList &projectionValueTokens);
[[nodiscard]] ProjectEquateLinkScanResult scanProjectEquateLinks(const ProjectEquateLinkScanInput &input);
}
