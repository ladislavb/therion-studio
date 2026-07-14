#include "TherionStationNameRules.h"

#include "TherionCommandLineModel.h"
#include "TherionSourceLogicalDocument.h"

namespace TherionStudio
{
namespace
{
QString stationNameTransformPart(const QString &value)
{
    const QString trimmedValue = value.trimmed();
    return trimmedValue == QStringLiteral("[]") || trimmedValue.isEmpty()
        ? QString()
        : trimmedValue;
}
}

QString therionStationReferenceWithScrapNameTransform(const QString &referenceName,
                                                      const TherionStationNameTransform &transform)
{
    const QString trimmedReference = referenceName.trimmed();
    if (trimmedReference.isEmpty()) {
        return QString();
    }

    return transform.prefix + trimmedReference + transform.suffix;
}

QHash<int, TherionStationNameTransform> therionScrapStationNameTransformsByStartLine(
    const TherionSourceLogicalDocument &logicalDocument)
{
    QHash<int, TherionStationNameTransform> transforms;
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        if (command.metadata.commandName != QStringLiteral("scrap")) {
            continue;
        }

        for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
            if (normalizedCommandOptionName(optionEntry.key) != QStringLiteral("station-names")
                || optionEntry.valueRanges.size() < 2) {
                continue;
            }

            transforms.insert(command.startLineNumber,
                             {stationNameTransformPart(optionEntry.valueRanges.at(0).text),
                              stationNameTransformPart(optionEntry.valueRanges.at(1).text)});
            break;
        }
    }
    return transforms;
}

std::optional<TherionStationNameTransform> therionActiveScrapStationNameTransform(
    const TherionSourceLogicalCommand &command,
    const QHash<int, TherionStationNameTransform> &transformsByStartLine)
{
    std::optional<TherionStationNameTransform> transform;
    for (const TherionSourceBlockFrame &frame : command.blockStackBefore) {
        if (frame.directive != QStringLiteral("scrap")) {
            continue;
        }

        transform.reset();
        const auto transformIt = transformsByStartLine.constFind(frame.lineNumber);
        if (transformIt != transformsByStartLine.constEnd()) {
            transform = transformIt.value();
        }
    }
    return transform;
}
}
