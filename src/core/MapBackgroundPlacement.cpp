#include "MapBackgroundPlacement.h"

#include "TherionCommandLineModel.h"
#include "TherionDocumentParser.h"
#include "TherionSourceDocument.h"
#include "TherionTokenRules.h"

#include <QtMath>

namespace TherionStudio
{
namespace
{
QString canonicalStationToken(QString token)
{
    token = token.trimmed();
    while (token.endsWith(QLatin1Char('.'))) {
        token.chop(1);
    }
    return token;
}

QString unqualifiedStationToken(const QString &token)
{
    const QString canonical = canonicalStationToken(token);
    const int namespaceSeparator = canonical.indexOf(QLatin1Char('@'));
    if (namespaceSeparator <= 0) {
        return canonical;
    }
    return canonical.left(namespaceSeparator);
}

bool sizesNearlyEqual(const QSizeF &a, const QSizeF &b)
{
    if (!a.isValid() || !b.isValid()) {
        return false;
    }

    const qreal widthTolerance = qMax<qreal>(1.0, qMax(a.width(), b.width()) * 0.02);
    const qreal heightTolerance = qMax<qreal>(1.0, qMax(a.height(), b.height()) * 0.02);
    return qAbs(a.width() - b.width()) <= widthTolerance
        && qAbs(a.height() - b.height()) <= heightTolerance;
}

bool isOriginLikeBasePosition(const QPointF &position)
{
    return qAbs(position.x()) <= 1e-9 && qAbs(position.y()) <= 1e-9;
}

const XviStationPlacementEntry *resolveXviRootStationEntry(const QVector<XviStationPlacementEntry> &stationEntries,
                                                           const QString &rootStationName)
{
    const QString requested = rootStationName.trimmed();
    if (requested.isEmpty()) {
        return nullptr;
    }

    for (const XviStationPlacementEntry &entry : stationEntries) {
        if (entry.name == requested) {
            return &entry;
        }
    }

    const QString canonicalRequested = canonicalStationToken(requested);
    for (const XviStationPlacementEntry &entry : stationEntries) {
        if (canonicalStationToken(entry.name) == canonicalRequested) {
            return &entry;
        }
    }

    if (requested.contains(QLatin1Char('@'))) {
        return nullptr;
    }

    const XviStationPlacementEntry *unqualifiedMatch = nullptr;
    for (const XviStationPlacementEntry &entry : stationEntries) {
        if (unqualifiedStationToken(entry.name) != canonicalRequested) {
            continue;
        }
        if (unqualifiedMatch != nullptr) {
            return nullptr;
        }
        unqualifiedMatch = &entry;
    }
    return unqualifiedMatch;
}

QString pointTypeTokenFromParsedLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("point")) {
        return QString();
    }

    int numericCoordinateTokens = 0;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (TherionTokenRules::isNumericToken(token)) {
            ++numericCoordinateTokens;
            continue;
        }
        if (numericCoordinateTokens < 2) {
            continue;
        }
        return token.toLower();
    }

    return QString();
}

QString stationNameFromPointLine(const TherionParsedLine &parsedLine)
{
    const QString optionName = commandOptionValue(parsedLine.tokens, QStringLiteral("-name")).trimmed();
    if (!optionName.isEmpty()) {
        return optionName;
    }

    if (pointTypeTokenFromParsedLine(parsedLine) != QStringLiteral("station")) {
        return QString();
    }

    bool sawStationType = false;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (!sawStationType) {
            if (token.toLower() == QStringLiteral("station")) {
                sawStationType = true;
            }
            continue;
        }
        return token;
    }
    return QString();
}

std::optional<QPointF> pointPositionFromParsedLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("point")) {
        return std::nullopt;
    }

    QVector<qreal> coordinates;
    coordinates.reserve(2);
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (!TherionTokenRules::isNumericToken(token)) {
            continue;
        }
        bool ok = false;
        const qreal value = token.toDouble(&ok);
        if (!ok) {
            continue;
        }
        coordinates.append(value);
        if (coordinates.size() == 2) {
            return QPointF(coordinates.at(0), coordinates.at(1));
        }
    }

    return std::nullopt;
}
}

QRectF resolveRasterModelRect(const QSizeF &imageModelSize,
                              const RasterPlacementMetadata &metadata,
                              const AreaAdjustMetadata &areaAdjust)
{
    if (!imageModelSize.isValid() || imageModelSize.width() <= 0.0 || imageModelSize.height() <= 0.0 || !metadata.hasBasePosition) {
        return QRectF();
    }

    if (areaAdjust.valid
        && areaAdjust.modelRect.isValid()
        && sizesNearlyEqual(imageModelSize, areaAdjust.modelRect.size())
        && isOriginLikeBasePosition(metadata.basePosition)) {
        return areaAdjust.modelRect;
    }

    const QPointF modelTopLeft(metadata.basePosition.x(),
                               metadata.topEdgeAnchor
                                   ? (metadata.basePosition.y() - imageModelSize.height())
                                   : metadata.basePosition.y());
    const QPointF modelBottomRight(modelTopLeft.x() + imageModelSize.width(),
                                   modelTopLeft.y() + imageModelSize.height());
    return QRectF(modelTopLeft, modelBottomRight);
}

XviPlacementResult resolveXviModelOffset(const QPointF &gridOrigin,
                                         const QHash<QString, QPointF> &stations,
                                         const XviPlacementMetadata &metadata)
{
    QVector<XviStationPlacementEntry> stationEntries;
    stationEntries.reserve(stations.size());
    for (auto it = stations.constBegin(); it != stations.constEnd(); ++it) {
        stationEntries.append(XviStationPlacementEntry{it.key(), it.value()});
    }
    return resolveXviModelOffset(gridOrigin, stationEntries, metadata);
}

XviPlacementResult resolveXviModelOffset(const QPointF &gridOrigin,
                                         const QVector<XviStationPlacementEntry> &stationEntries,
                                         const XviPlacementMetadata &metadata)
{
    XviPlacementResult result;
    const QPointF basePosition = metadata.hasBasePosition ? metadata.basePosition : QPointF(0.0, 0.0);
    result.rootRequested = !metadata.rootStationName.trimmed().isEmpty();

    if (result.rootRequested) {
        const XviStationPlacementEntry *matchedStation = resolveXviRootStationEntry(stationEntries, metadata.rootStationName);
        if (matchedStation != nullptr) {
            result.rootResolved = true;
            result.matchedRootStationName = matchedStation->name;
            result.modelOffset = basePosition - matchedStation->position;
            return result;
        }
    }

    result.modelOffset = basePosition - gridOrigin;
    return result;
}

XviBackgroundInsertionPlacement resolvePocketTopoXviInsertionPlacement(
    const QVector<XviStationPlacementEntry> &stationEntries,
    const QString &documentText)
{
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(documentText);
    for (const TherionSourceDocumentLine &sourceLine : sourceDocument.lines()) {
        const TherionParsedLine &parsedLine = sourceLine.sourceLine.parsed;
        if (pointTypeTokenFromParsedLine(parsedLine) != QStringLiteral("station")) {
            continue;
        }

        const QString stationName = stationNameFromPointLine(parsedLine);
        if (stationName.trimmed().isEmpty()) {
            continue;
        }

        const XviStationPlacementEntry *xviStation = resolveXviRootStationEntry(stationEntries, stationName);
        if (xviStation == nullptr) {
            continue;
        }

        const std::optional<QPointF> position = pointPositionFromParsedLine(parsedLine);
        if (!position.has_value()) {
            continue;
        }

        return XviBackgroundInsertionPlacement{position.value(), xviStation->name};
    }

    if (!stationEntries.isEmpty()) {
        return XviBackgroundInsertionPlacement{QPointF(0.0, 0.0), stationEntries.constFirst().name};
    }

    return XviBackgroundInsertionPlacement{QPointF(0.0, 0.0), QString()};
}
}
