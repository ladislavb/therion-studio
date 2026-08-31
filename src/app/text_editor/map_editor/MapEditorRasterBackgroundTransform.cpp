#include "MapEditorRasterBackgroundTransform.h"

#include <QtGlobal>

namespace TherionStudio
{

QTransform rasterBackgroundLayerTransform(const RasterBackgroundTransformInput &input)
{
    QTransform transform;

    const bool hasUserTransform = input.pivotSet
        || !qFuzzyCompare(input.layerScaleX, 1.0)
        || !qFuzzyCompare(input.layerScaleY, 1.0)
        || !qFuzzyIsNull(input.rotationDeg);
    if (!hasUserTransform) {
        transform.scale(input.viewScaleX, input.viewScaleY);
        return transform;
    }

    // Pivot of the rotation, expressed in preview units before the layer scale
    // is applied. It defaults to the centre of the image.
    const qreal pivotLocalX = input.pivotSet ? input.pivotLocalX : (input.intrinsicWidth / 2.0);
    const qreal pivotLocalY = input.pivotSet ? input.pivotLocalY : (input.intrinsicHeight / 2.0);
    const qreal pivotPreviewX = pivotLocalX * input.viewScaleX;
    const qreal pivotPreviewY = pivotLocalY * input.viewScaleY;

    // Mapiah scales from the anchor, so the pivot is carried by that same
    // scaling before the rotation turns around it. Translating by the scaled
    // pivot -- rather than by the pivot itself -- is what keeps the `xx`/`yy`
    // anchor fixed when there is no rotation.
    transform.translate(pivotPreviewX * input.layerScaleX, pivotPreviewY * input.layerScaleY);
    transform.rotate(input.rotationDeg);
    transform.scale(input.layerScaleX, input.layerScaleY);
    transform.translate(-pivotPreviewX, -pivotPreviewY);
    transform.scale(input.viewScaleX, input.viewScaleY);
    return transform;
}

}
