#pragma once

#include <QTransform>

QT_BEGIN_NAMESPACE
class QRectF;
class QSizeF;
QT_END_NAMESPACE

namespace TherionStudio
{

/**
 * Placement of a raster background layer, in the units the metadata uses:
 * `layerScale` and the rotation pivot come straight from the `##MAPIAH##`
 * `image_insert_v1` record, and `viewScale` maps intrinsic image pixels to
 * preview units.
 */
struct RasterBackgroundTransformInput
{
    qreal viewScaleX = 1.0;
    qreal viewScaleY = 1.0;
    qreal layerScaleX = 1.0;
    qreal layerScaleY = 1.0;
    qreal rotationDeg = 0.0;
    bool pivotSet = false;
    qreal pivotLocalX = 0.0;
    qreal pivotLocalY = 0.0;
    qreal intrinsicWidth = 0.0;
    qreal intrinsicHeight = 0.0;
};

/**
 * Transform mapping intrinsic image coordinates to preview coordinates.
 *
 * The composition matches Mapiah: the image is scaled from its `xx`/`yy`
 * anchor, and only the rotation turns around the pivot of the scaled image.
 * With no rotation the anchor is therefore a fixed point, which is what keeps
 * a background lined up with the survey it was traced from.
 */
QTransform rasterBackgroundLayerTransform(const RasterBackgroundTransformInput &input);

}
