#include "MapBackgroundPlacement.h"

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
}
