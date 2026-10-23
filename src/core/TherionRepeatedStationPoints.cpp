#include "TherionRepeatedStationPoints.h"

#include "TherionProjectionRules.h"
#include "TherionSourceDocument.h"
#include "TherionSourceLogicalDocument.h"
#include "TherionStationNameRules.h"

#include <QCoreApplication>
#include <QHash>

namespace TherionStudio
{
namespace
{
struct StationPoint
{
    int lineNumber = 0;
    int scrapLineNumber = 0;
    double x = 0.0;
    double y = 0.0;
};

int enclosingScrapLineNumber(const TherionSourceLogicalCommand &command)
{
    for (auto frame = command.blockStackBefore.crbegin(); frame != command.blockStackBefore.crend(); ++frame) {
        if (frame->directive == QStringLiteral("scrap")) {
            return frame->lineNumber;
        }
    }
    return 0;
}

TherionSourceDiagnostic stationDiagnostic(const QString &code,
                                          TherionSourceDiagnosticSeverity severity,
                                          const TherionSourceLogicalArgumentRange &nameRange,
                                          const QString &title,
                                          const QString &message)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = severity;
    diagnostic.lineNumber = nameRange.physicalRange.lineNumber;
    diagnostic.columnNumber = nameRange.physicalRange.columnNumber;
    diagnostic.columnLength = nameRange.physicalRange.columnLength;
    diagnostic.title = title;
    diagnostic.message = message;
    diagnostic.currentText = nameRange.text;
    return diagnostic;
}

// Removes the whole point command, including its line ending.
void setRemovePointFix(TherionSourceDiagnostic *diagnostic,
                       const TherionSourceDocument &sourceDocument,
                       const TherionSourceLogicalCommand &command)
{
    const TherionSourceDocumentLine *firstLine = sourceDocument.lineAtLineNumber(command.startLineNumber);
    const TherionSourceDocumentLine *lastLine = sourceDocument.lineAtLineNumber(command.endLineNumber);
    if (firstLine == nullptr || lastLine == nullptr) {
        return;
    }
    const int startOffset = firstLine->sourceLine.startOffset;
    const int endOffset = lastLine->sourceLine.startOffset + lastLine->sourceLine.textLength
        + lastLine->sourceLine.lineEndingLength;
    diagnostic->currentText = sourceDocument.toText().mid(startOffset, endOffset - startOffset);
    diagnostic->hasFix = true;
    diagnostic->fix.startOffset = startOffset;
    diagnostic->fix.length = endOffset - startOffset;
    diagnostic->fix.description = QCoreApplication::translate("TherionStudio::TherionRepeatedStationPoints", "Remove repeated station point on line %1").arg(command.startLineNumber);
}
}

void appendRepeatedStationPointDiagnostics(QVector<TherionSourceDiagnostic> *diagnostics,
                                           const TherionSourceDocument &sourceDocument,
                                           const TherionSourceLogicalDocument &logicalDocument)
{
    if (diagnostics == nullptr) {
        return;
    }

    const QHash<int, TherionStationNameTransform> transforms =
        therionScrapStationNameTransformsByStartLine(logicalDocument);
    QHash<int, QString> projectionByScrapLine;
    QHash<int, QString> nameByScrapLine;
    // The first point of each station in each scrap, and in each projection.
    QHash<QString, StationPoint> firstInScrap;
    QHash<QString, StationPoint> firstInProjection;
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        if (command.metadata.commandName == QStringLiteral("scrap")) {
            projectionByScrapLine.insert(command.startLineNumber, therionScrapProjection(command));
            nameByScrapLine.insert(command.startLineNumber, command.positionalArgumentRanges.value(0).text);
            continue;
        }
        const std::optional<TherionSourceLogicalArgumentRange> nameRange = therionPointStationNameRange(command);
        const int scrapLineNumber = enclosingScrapLineNumber(command);
        if (!nameRange.has_value() || scrapLineNumber <= 0 || command.positionalArgumentRanges.size() < 2) {
            continue;
        }

        const QString station = therionStationReferenceWithScrapNameTransform(
            nameRange->text,
            therionActiveScrapStationNameTransform(command, transforms).value_or(TherionStationNameTransform{}));
        const StationPoint point{command.startLineNumber,
                                 scrapLineNumber,
                                 command.positionalArgumentRanges.at(0).text.toDouble(),
                                 command.positionalArgumentRanges.at(1).text.toDouble()};

        const QString scrapKey = QString::number(scrapLineNumber) + QLatin1Char('\n') + station;
        const auto inScrapIt = firstInScrap.constFind(scrapKey);
        if (inScrapIt != firstInScrap.constEnd()) {
            const bool samePlace = inScrapIt->x == point.x && inScrapIt->y == point.y;
            TherionSourceDiagnostic diagnostic = stationDiagnostic(
                QStringLiteral("repeated-station-point"),
                TherionSourceDiagnosticSeverity::Warning,
                *nameRange,
                QCoreApplication::translate("TherionStudio::TherionRepeatedStationPoints", "Station drawn twice in a scrap"),
                (samePlace ? QCoreApplication::translate("TherionStudio::TherionRepeatedStationPoints", "Station `%1` is already drawn at the same place in this scrap, on line %2.")
                           : QCoreApplication::translate("TherionStudio::TherionRepeatedStationPoints", "Station `%1` is already drawn at a different place in this scrap, on line %2."))
                    .arg(station)
                    .arg(inScrapIt->lineNumber));
            if (samePlace) {
                setRemovePointFix(&diagnostic, sourceDocument, command);
            }
            diagnostics->append(diagnostic);
            continue;
        }
        firstInScrap.insert(scrapKey, point);

        const QString projection = projectionByScrapLine.value(scrapLineNumber);
        if (therionProjectionIsCrossSection(projection)) {
            continue;
        }
        const QString projectionKey = projection + QLatin1Char('\n') + station;
        const auto inProjectionIt = firstInProjection.constFind(projectionKey);
        if (inProjectionIt == firstInProjection.constEnd()) {
            firstInProjection.insert(projectionKey, point);
            continue;
        }
        if (inProjectionIt->scrapLineNumber != scrapLineNumber) {
            diagnostics->append(stationDiagnostic(
                QStringLiteral("station-in-several-scraps"),
                TherionSourceDiagnosticSeverity::Information,
                *nameRange,
                QCoreApplication::translate("TherionStudio::TherionRepeatedStationPoints", "Station in several scraps"),
                QCoreApplication::translate("TherionStudio::TherionRepeatedStationPoints", "Station `%1` is also drawn in scrap `%2` of the same projection, on line %3.")
                    .arg(station, nameByScrapLine.value(inProjectionIt->scrapLineNumber))
                    .arg(inProjectionIt->lineNumber)));
        }
    }
}
}
