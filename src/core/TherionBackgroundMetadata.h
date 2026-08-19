#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

#include <optional>

namespace TherionStudio
{
enum class TherionBackgroundMetadataFormat
{
    XTherion,
    Mapiah
};

enum class TherionBackgroundLayerFormat
{
    Raster,
    Xvi,
    Svg,
    Unsupported
};

struct TherionBackgroundReference
{
    QString absolutePath;
    TherionBackgroundMetadataFormat metadataFormat = TherionBackgroundMetadataFormat::XTherion;
    TherionBackgroundLayerFormat layerFormat = TherionBackgroundLayerFormat::Raster;
    QPointF basePosition;
    bool hasBasePosition = false;
    bool visible = true;
    bool hasVisibility = false;
    qreal imageScale = 1.0;
    bool hasImageScale = false;
    qreal xScale = 1.0;
    qreal yScale = 1.0;
    qreal rotationCenterDx = 0.0;
    qreal rotationCenterDy = 0.0;
    qreal rotationDeg = 0.0;
    bool pivotSet = false;
    QString rootStationName;
    QSizeF svgIntrinsicSize;
    QRectF svgSourceViewBox;
    bool hasSvgIntrinsicSize = false;
    bool hasSvgSourceViewBox = false;
    bool metadataTopEdgeAnchor = false;
    bool xviReference = false;
    int lineNumber = 0;
};

struct TherionAreaAdjust
{
    QRectF modelRect;
    bool valid = false;
};

QVector<TherionBackgroundReference> parseTherionBackgroundReferences(const QString &documentText,
                                                                     const QString &documentPath);
TherionAreaAdjust parseTherionAreaAdjust(const QString &documentText);
bool isTherionDefaultAreaAdjust(const QRectF &modelRect);
std::optional<QRectF> initialTherionAreaAdjustForXviImport(const TherionAreaAdjust &existingAreaAdjust,
                                                           const QRectF &otherContentBounds,
                                                           const QRectF &placedXviBounds);
QString therionMapiahImageInsertMetadataLine(const QString &absolutePath,
                                             const QString &documentPath,
                                             TherionBackgroundLayerFormat layerFormat,
                                             const QPointF &basePosition,
                                             qreal xScale,
                                             qreal yScale,
                                             qreal rotationCenterDx,
                                             qreal rotationCenterDy,
                                             qreal rotationDeg,
                                             bool pivotSet,
                                             const QString &rootStationName = QString(),
                                             const QSizeF &svgIntrinsicSize = QSizeF(),
                                             const QRectF &svgSourceViewBox = QRectF());
QString therionAreaAdjustMetadataLine(const QRectF &modelRect);
QString therionAreaZoomToMetadataLine();
QString upsertTherionAreaAdjustMetadata(const QString &documentText, const QRectF &modelRect);
}
