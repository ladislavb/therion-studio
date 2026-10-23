#include "ProjectMapCompositionCheck.h"

#include "TherionProjectionRules.h"

#include <QSet>
#include <QStringList>

#include <algorithm>
#include <functional>

namespace TherionStudio
{
namespace
{
bool holdsScraps(ProjectMapCompositionItemKind kind)
{
    return kind == ProjectMapCompositionItemKind::Scrap;
}

ProjectIndexDiagnostic itemDiagnostic(ProjectIndexDiagnosticKind kind,
                                      const ProjectMapComposition &composition,
                                      const ProjectMapCompositionItem &item)
{
    ProjectIndexDiagnostic diagnostic;
    diagnostic.kind = kind;
    diagnostic.sourceObjectId = composition.sourceObjectId;
    diagnostic.sourceFile = composition.sourceFile;
    diagnostic.lineNumber = item.lineNumber;
    diagnostic.columnNumber = qMax(1, item.columnNumber);
    diagnostic.columnLength = qMax(0, item.columnLength);
    diagnostic.referencedName = item.name;
    return diagnostic;
}
}

QVector<ProjectIndexDiagnostic> checkMapCompositionProjections(
    const QHash<QString, ProjectMapComposition> &compositionsByMapKey,
    const QHash<QString, QString> &scrapProjectionsByKey)
{
    QVector<ProjectIndexDiagnostic> diagnostics;
    QSet<QString> inProgress;
    // Each map's projection identity once worked out, empty when it cannot be known.
    QHash<QString, QString> projectionByMapKey;

    std::function<QString(const QString &)> mapProjection = [&](const QString &mapKey) -> QString {
        if (projectionByMapKey.contains(mapKey) || inProgress.contains(mapKey)) {
            return projectionByMapKey.value(mapKey);
        }
        const auto compositionIt = compositionsByMapKey.constFind(mapKey);
        if (compositionIt == compositionsByMapKey.constEnd()) {
            return QString();
        }
        const ProjectMapComposition &composition = compositionIt.value();
        const QString explicitProjection = therionProjectionIdentity(composition.projection);
        auto report = [&](ProjectIndexDiagnosticKind kind, const ProjectMapCompositionItem &item) {
            diagnostics.append(itemDiagnostic(kind, composition, item));
        };

        inProgress.insert(mapKey);
        QString contentProjection;
        bool firstHoldsScraps = false;
        bool contentKnown = false;
        bool projectionReported = false;
        for (const ProjectMapCompositionItem &item : composition.items) {
            if (item.kind == ProjectMapCompositionItemKind::Map && inProgress.contains(item.key)) {
                report(ProjectIndexDiagnosticKind::RecursiveMapReference, item);
                continue;
            }

            const bool isFirst = !contentKnown;
            if (isFirst) {
                firstHoldsScraps = holdsScraps(item.kind);
            } else if (holdsScraps(item.kind) != firstHoldsScraps) {
                continue;
            }
            contentKnown = true;

            // A survey is drawn in the projection the map already has, or in its `-projection` when it comes first.
            if (item.kind == ProjectMapCompositionItemKind::Survey) {
                if (isFirst && explicitProjection.isEmpty()) {
                    report(ProjectIndexDiagnosticKind::NoProjectionForSurvey, item);
                    continue;
                }
                if (isFirst) {
                    contentProjection = explicitProjection;
                }
                if (contentProjection.startsWith(QStringLiteral("none:"))) {
                    report(ProjectIndexDiagnosticKind::UnsupportedSurveyProjection, item);
                }
                continue;
            }

            const QString itemProjection = item.kind == ProjectMapCompositionItemKind::Map
                ? mapProjection(item.key)
                : therionProjectionIdentity(scrapProjectionsByKey.value(item.key, QStringLiteral("plan")));
            bool reportedHere = false;
            if (!itemProjection.isEmpty() && !projectionReported) {
                const bool differsFromContent =
                    !contentProjection.isEmpty() && itemProjection != contentProjection;
                const bool differsFromMap = !explicitProjection.isEmpty() && itemProjection != explicitProjection;
                if (differsFromContent || differsFromMap) {
                    report(holdsScraps(item.kind) ? ProjectIndexDiagnosticKind::IncompatibleScrapProjection
                                                  : ProjectIndexDiagnosticKind::IncompatibleMapProjection,
                           item);
                    projectionReported = true;
                    reportedHere = true;
                } else if (contentProjection.isEmpty()) {
                    contentProjection = itemProjection;
                }
            }
            // Therion reads a shift on the first scrap of a map but rejects it on any later one.
            if (item.kind == ProjectMapCompositionItemKind::Scrap && item.shifted && !isFirst && !reportedHere) {
                report(ProjectIndexDiagnosticKind::ShiftedScrap, item);
            }
        }
        inProgress.remove(mapKey);

        const QString projection = explicitProjection.isEmpty() ? contentProjection : explicitProjection;
        projectionByMapKey.insert(mapKey, projection);

        for (const ProjectMapCompositionItem &preview : composition.previews) {
            const QString previewProjection = mapProjection(preview.key);
            if (!previewProjection.isEmpty() && !projection.isEmpty() && previewProjection != projection) {
                report(ProjectIndexDiagnosticKind::IncompatibleMapProjection, preview);
            }
        }
        return projection;
    };

    QStringList mapKeys = compositionsByMapKey.keys();
    std::sort(mapKeys.begin(), mapKeys.end());
    for (const QString &mapKey : std::as_const(mapKeys)) {
        mapProjection(mapKey);
    }
    return diagnostics;
}
}
