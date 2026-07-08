#pragma once

#include <QPointF>
#include <QHash>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct RasterPlacementMetadata
{
    QPointF basePosition;
    bool hasBasePosition = false;
    bool topEdgeAnchor = true;
};

struct AreaAdjustMetadata
{
    QRectF modelRect;
    bool valid = false;
};

struct XviPlacementMetadata
{
    QPointF basePosition;
    bool hasBasePosition = false;
    QString rootStationName;
};

struct XviPlacementResult
{
    QPointF modelOffset;
    bool rootRequested = false;
    bool rootResolved = false;
    QString matchedRootStationName;
};

struct XviStationPlacementEntry
{
    QString name;
    QPointF position;
};

struct XviBackgroundInsertionPlacement
{
    QPointF basePosition;
    QString rootStationName;
};

QRectF resolveRasterModelRect(const QSizeF &imageModelSize,
                              const RasterPlacementMetadata &metadata,
                              const AreaAdjustMetadata &areaAdjust);

XviPlacementResult resolveXviModelOffset(const QPointF &gridOrigin,
                                         const QHash<QString, QPointF> &stations,
                                         const XviPlacementMetadata &metadata);

XviPlacementResult resolveXviModelOffset(const QPointF &gridOrigin,
                                         const QVector<XviStationPlacementEntry> &stationEntries,
                                         const XviPlacementMetadata &metadata);

XviBackgroundInsertionPlacement resolvePocketTopoXviInsertionPlacement(
    const QVector<XviStationPlacementEntry> &stationEntries,
    const QString &documentText);
}
