#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

class QGraphicsPixmapItem;
class QImage;

namespace TherionStudio
{

struct TherionAreaAdjust;
struct TherionBackgroundReference;

// Item data role storing the target preview-canvas rectangle a raster layer
// must occupy. The displayed pixmap stays at full resolution; the item scale is
// derived from this rectangle and the current pixmap size.
constexpr int kMapEditorRasterPreviewRectRole = 110;
constexpr int kMapEditorBackgroundRotationDegRole = 111;
constexpr int kMapEditorBackgroundRotationCenterDxRole = 112;
constexpr int kMapEditorBackgroundRotationCenterDyRole = 113;
constexpr int kMapEditorBackgroundPivotSetRole = 114;
constexpr int kMapEditorBackgroundMetadataFormatRole = 115;
constexpr int kMapEditorBackgroundXScaleRole = 116;
constexpr int kMapEditorBackgroundYScaleRole = 117;

// Re-derives the raster layer item scale from its stored target rectangle and
// current (full-resolution) pixmap size. Position is left untouched so a moved
// layer keeps its location across pixmap swaps (e.g. gamma changes).
void applyMapEditorRasterLayerTransform(QGraphicsPixmapItem *item);

// Fits sourceSize into previewBounds preserving aspect ratio (minimum 2px per
// edge) and centers the result inside previewBounds. Used to derive the default
// preview-canvas rectangle for a raster layer that has no model placement.
QRectF fitAndCenterRasterSizeInPreview(const QSizeF &sourceSize, const QRectF &previewBounds);

// Places a raster layer to occupy viewRect, storing the full-resolution display
// pixmap and an item transform that scales it to the rectangle.
bool placeMapEditorRasterLayerInPreviewRect(QGraphicsPixmapItem *item,
                                            const QImage &sourceImage,
                                            const QRectF &viewRect);
bool placeMapEditorRasterLayerPlaceholderInPreviewRect(QGraphicsPixmapItem *item,
                                                       const QRectF &viewRect);

QRectF fittedMapEditorModelBoundsInPreview(const QRectF &modelBounds, const QRectF &previewBounds);
QPointF mapEditorModelToPreviewPoint(const QPointF &modelPoint, const QRectF &modelBounds, const QRectF &previewBounds);
QPointF mapEditorPreviewToModelPoint(const QPointF &previewPoint, const QRectF &modelBounds, const QRectF &previewBounds);
QRectF mapEditorRasterModelRectForItem(const QGraphicsPixmapItem *item,
                                       const QRectF &sourceBounds,
                                       const QRectF &previewBounds);
bool placeMapEditorRasterLayerByModelRect(QGraphicsPixmapItem *item,
                                          const QImage &sourceImage,
                                          const QRectF &modelRect,
                                          const QRectF &modelBounds,
                                          const QRectF &previewBounds);
bool placeMapEditorRasterLayerPlaceholderByModelRect(QGraphicsPixmapItem *item,
                                                     const QRectF &modelRect,
                                                     const QRectF &modelBounds,
                                                     const QRectF &previewBounds);
bool placeMapEditorRasterLayerFromMetadata(QGraphicsPixmapItem *item,
                                           const QImage &sourceImage,
                                           const TherionBackgroundReference &reference,
                                           const TherionAreaAdjust &areaAdjust,
                                           const QRectF &modelBounds,
                                           const QRectF &previewBounds);
bool placeMapEditorRasterLayerPlaceholderFromMetadata(QGraphicsPixmapItem *item,
                                                      const TherionBackgroundReference &reference,
                                                      const TherionAreaAdjust &areaAdjust,
                                                      const QRectF &modelBounds,
                                                      const QRectF &previewBounds);
bool placeMapEditorLayerPlaceholderFromMetadata(QGraphicsPixmapItem *item,
                                                const QSizeF &modelSize,
                                                const TherionBackgroundReference &reference,
                                                const TherionAreaAdjust &areaAdjust,
                                                const QRectF &modelBounds,
                                                const QRectF &previewBounds);
bool placeMapEditorLayerFromMetadata(QGraphicsPixmapItem *item,
                                      const QSizeF &modelSize,
                                      const TherionBackgroundReference &reference,
                                      const TherionAreaAdjust &areaAdjust,
                                      const QRectF &modelBounds,
                                      const QRectF &previewBounds);

void storeMapEditorBackgroundTransformMetadata(QGraphicsPixmapItem *item,
                                               const TherionBackgroundReference &reference);

}
