#include "MapEditorRasterBackgroundPlacement.h"

#include <QGraphicsPixmapItem>
#include <QImage>
#include <QPixmap>
#include <QTransform>
#include <QtMath>

#include "MapEditorRasterBackgroundImage.h"
#include "../../../core/MapBackgroundPlacement.h"
#include "../../../core/TherionBackgroundMetadata.h"

namespace TherionStudio
{
namespace
{
QRectF previewRectForRasterModelRect(const QRectF &modelRect,
                                     const QRectF &modelBounds,
                                     const QRectF &previewBounds)
{
    if (!modelRect.isValid() || !modelBounds.isValid() || !previewBounds.isValid()) {
        return QRectF();
    }

    const QPointF modelUpperLeft(modelRect.left(), modelRect.bottom());
    const QPointF modelLowerRight(modelRect.right(), modelRect.top());
    const QPointF viewA = mapEditorModelToPreviewPoint(modelUpperLeft, modelBounds, previewBounds);
    const QPointF viewB = mapEditorModelToPreviewPoint(modelLowerRight, modelBounds, previewBounds);
    return QRectF(QPointF(qMin(viewA.x(), viewB.x()), qMin(viewA.y(), viewB.y())),
                  QPointF(qMax(viewA.x(), viewB.x()), qMax(viewA.y(), viewB.y())));
}

QRectF rasterModelRectFromMetadata(const QString &layerPath,
                                   const TherionBackgroundReference &reference,
                                   const TherionAreaAdjust &areaAdjust)
{
    QSizeF modelSize = mapEditorRasterModelSize(layerPath, 1.0);
    RasterPlacementMetadata placement{};
    placement.basePosition = reference.basePosition;
    placement.hasBasePosition = reference.hasBasePosition;
    placement.topEdgeAnchor = reference.metadataTopEdgeAnchor;

    AreaAdjustMetadata areaMetadata{};
    areaMetadata.modelRect = areaAdjust.modelRect;
    areaMetadata.valid = areaAdjust.valid;

    return resolveRasterModelRect(modelSize, placement, areaMetadata);
}

QRectF layerModelRectFromMetadata(const QSizeF &modelSize,
                                  const TherionBackgroundReference &reference,
                                  const TherionAreaAdjust &areaAdjust)
{
    RasterPlacementMetadata placement{};
    placement.basePosition = reference.basePosition;
    placement.hasBasePosition = reference.hasBasePosition;
    placement.topEdgeAnchor = reference.metadataTopEdgeAnchor;

    AreaAdjustMetadata areaMetadata{};
    areaMetadata.modelRect = areaAdjust.modelRect;
    areaMetadata.valid = areaAdjust.valid;

    return resolveRasterModelRect(modelSize, placement, areaMetadata);
}
}

QRectF fittedMapEditorModelBoundsInPreview(const QRectF &modelBounds, const QRectF &previewBounds)
{
    if (!modelBounds.isValid() || !previewBounds.isValid()) {
        return previewBounds;
    }

    const qreal modelWidth = qMax(1.0, modelBounds.width());
    const qreal modelHeight = qMax(1.0, modelBounds.height());
    const qreal previewWidth = qMax(1.0, previewBounds.width());
    const qreal previewHeight = qMax(1.0, previewBounds.height());
    const qreal zoom = qMin(previewWidth / modelWidth, previewHeight / modelHeight);
    const qreal fittedWidth = modelWidth * zoom;
    const qreal fittedHeight = modelHeight * zoom;
    const qreal left = previewBounds.left() + ((previewWidth - fittedWidth) / 2.0);
    const qreal top = previewBounds.top() + ((previewHeight - fittedHeight) / 2.0);
    return QRectF(left, top, fittedWidth, fittedHeight);
}

QPointF mapEditorModelToPreviewPoint(const QPointF &modelPoint, const QRectF &modelBounds, const QRectF &previewBounds)
{
    if (!modelBounds.isValid() || !previewBounds.isValid()) {
        return modelPoint;
    }

    const QRectF fitted = fittedMapEditorModelBoundsInPreview(modelBounds, previewBounds);
    const qreal zoom = qMin(fitted.width() / qMax(1.0, modelBounds.width()),
                            fitted.height() / qMax(1.0, modelBounds.height()));
    const qreal panX = fitted.left() - (modelBounds.left() * zoom);
    const qreal panY = fitted.top() + (modelBounds.bottom() * zoom);
    return QPointF((modelPoint.x() * zoom) + panX,
                   panY - (modelPoint.y() * zoom));
}

QPointF mapEditorPreviewToModelPoint(const QPointF &previewPoint, const QRectF &modelBounds, const QRectF &previewBounds)
{
    if (!modelBounds.isValid() || !previewBounds.isValid()) {
        return previewPoint;
    }

    const QRectF fitted = fittedMapEditorModelBoundsInPreview(modelBounds, previewBounds);
    const qreal zoom = qMin(fitted.width() / qMax(1.0, modelBounds.width()),
                            fitted.height() / qMax(1.0, modelBounds.height()));
    if (zoom < 1e-9) {
        return previewPoint;
    }

    const qreal panX = fitted.left() - (modelBounds.left() * zoom);
    const qreal panY = fitted.top() + (modelBounds.bottom() * zoom);
    return QPointF((previewPoint.x() - panX) / zoom,
                   (panY - previewPoint.y()) / zoom);
}

QRectF mapEditorRasterModelRectForItem(const QGraphicsPixmapItem *item,
                                       const QRectF &sourceBounds,
                                       const QRectF &previewBounds)
{
    if (item == nullptr) {
        return QRectF();
    }

    const QString layerPath = item->data(0).toString();
    const QSizeF modelSize = mapEditorRasterModelSize(layerPath, 1.0);
    if (!modelSize.isValid() || modelSize.width() <= 0.0 || modelSize.height() <= 0.0) {
        return QRectF();
    }

    if (!sourceBounds.isValid() || !previewBounds.isValid()) {
        return QRectF(QPointF(0.0, -modelSize.height()), modelSize);
    }

    const QPointF modelTopLeft = mapEditorPreviewToModelPoint(item->pos(), sourceBounds, previewBounds);
    return QRectF(modelTopLeft, modelSize);
}

QRectF fitAndCenterRasterSizeInPreview(const QSizeF &sourceSize, const QRectF &previewBounds)
{
    QSize fittedSize = sourceSize.toSize();
    fittedSize.scale(previewBounds.size().toSize(), Qt::KeepAspectRatio);
    fittedSize.setWidth(qMax(fittedSize.width(), 2));
    fittedSize.setHeight(qMax(fittedSize.height(), 2));
    return QRectF(QPointF(previewBounds.center().x() - (fittedSize.width() / 2.0),
                          previewBounds.center().y() - (fittedSize.height() / 2.0)),
                  QSizeF(fittedSize));
}

void applyMapEditorRasterLayerTransform(QGraphicsPixmapItem *item)
{
    if (item == nullptr) {
        return;
    }

    const QRectF viewRect = item->data(kMapEditorRasterPreviewRectRole).toRectF();
    const QSize pixmapSize = item->pixmap().size();
    if (!viewRect.isValid() || pixmapSize.isEmpty()) {
        return;
    }

    const qreal scaleX = viewRect.width() / static_cast<qreal>(pixmapSize.width());
    const qreal scaleY = viewRect.height() / static_cast<qreal>(pixmapSize.height());
    const qreal layerScaleX = item->data(kMapEditorBackgroundXScaleRole).isValid()
        ? qMax(0.01, item->data(kMapEditorBackgroundXScaleRole).toDouble())
        : 1.0;
    const qreal layerScaleY = item->data(kMapEditorBackgroundYScaleRole).isValid()
        ? qMax(0.01, item->data(kMapEditorBackgroundYScaleRole).toDouble())
        : 1.0;
    const qreal rotationDeg = item->data(kMapEditorBackgroundRotationDegRole).toDouble();
    const bool pivotSet = item->data(kMapEditorBackgroundPivotSetRole).toBool();
    const bool hasUserTransform = pivotSet
        || !qFuzzyCompare(layerScaleX, 1.0)
        || !qFuzzyCompare(layerScaleY, 1.0)
        || !qFuzzyIsNull(rotationDeg);
    QTransform transform;
    if (!hasUserTransform) {
        transform.scale(scaleX, scaleY);
    } else {
        const qreal pivotLocalX = pivotSet
            ? item->data(kMapEditorBackgroundRotationCenterDxRole).toDouble()
            : static_cast<qreal>(pixmapSize.width()) / 2.0;
        const qreal pivotLocalY = pivotSet
            ? item->data(kMapEditorBackgroundRotationCenterDyRole).toDouble()
            : static_cast<qreal>(pixmapSize.height()) / 2.0;
        const qreal pivotPreviewX = pivotLocalX * scaleX;
        const qreal pivotPreviewY = pivotLocalY * scaleY;
        transform.translate(pivotPreviewX, pivotPreviewY);
        transform.rotate(rotationDeg);
        transform.scale(layerScaleX, layerScaleY);
        transform.translate(-pivotPreviewX, -pivotPreviewY);
        transform.scale(scaleX, scaleY);
    }

    item->setTransformationMode(Qt::SmoothTransformation);
    item->setTransformOriginPoint(0.0, 0.0);
    item->setScale(1.0);
    item->setRotation(0.0);
    item->setTransform(transform, false);
}

bool placeMapEditorRasterLayerInPreviewRect(QGraphicsPixmapItem *item,
                                            const QImage &sourceImage,
                                            const QRectF &viewRect)
{
    if (item == nullptr || sourceImage.isNull() || !viewRect.isValid() || viewRect.width() < 1.0 || viewRect.height() < 1.0) {
        return false;
    }

    item->setPixmap(QPixmap::fromImage(mapEditorRasterDisplayImage(sourceImage)));
    item->setData(kMapEditorRasterPreviewRectRole, viewRect);
    item->setPos(viewRect.topLeft());
    applyMapEditorRasterLayerTransform(item);
    return true;
}

bool placeMapEditorRasterLayerPlaceholderInPreviewRect(QGraphicsPixmapItem *item, const QRectF &viewRect)
{
    if (item == nullptr || !viewRect.isValid() || viewRect.width() < 1.0 || viewRect.height() < 1.0) {
        return false;
    }

    QImage placeholder(qMax(2, qRound(viewRect.width())),
                       qMax(2, qRound(viewRect.height())),
                       QImage::Format_ARGB32_Premultiplied);
    placeholder.fill(Qt::transparent);
    item->setPixmap(QPixmap::fromImage(placeholder));
    item->setData(kMapEditorRasterPreviewRectRole, viewRect);
    item->setPos(viewRect.topLeft());
    applyMapEditorRasterLayerTransform(item);
    return true;
}

bool placeMapEditorLayerInPreviewRect(QGraphicsPixmapItem *item, const QRectF &viewRect)
{
    if (item == nullptr
        || item->pixmap().isNull()
        || !viewRect.isValid()
        || viewRect.width() < 1.0
        || viewRect.height() < 1.0) {
        return false;
    }

    item->setData(kMapEditorRasterPreviewRectRole, viewRect);
    item->setPos(viewRect.topLeft());
    applyMapEditorRasterLayerTransform(item);
    return true;
}

bool placeMapEditorRasterLayerByModelRect(QGraphicsPixmapItem *item,
                                          const QImage &sourceImage,
                                          const QRectF &modelRect,
                                          const QRectF &modelBounds,
                                          const QRectF &previewBounds)
{
    return placeMapEditorRasterLayerInPreviewRect(item,
                                                  sourceImage,
                                                  previewRectForRasterModelRect(modelRect, modelBounds, previewBounds));
}

bool placeMapEditorRasterLayerPlaceholderByModelRect(QGraphicsPixmapItem *item,
                                                     const QRectF &modelRect,
                                                     const QRectF &modelBounds,
                                                     const QRectF &previewBounds)
{
    return placeMapEditorRasterLayerPlaceholderInPreviewRect(item,
                                                             previewRectForRasterModelRect(modelRect, modelBounds, previewBounds));
}

bool placeMapEditorRasterLayerFromMetadata(QGraphicsPixmapItem *item,
                                           const QImage &sourceImage,
                                           const TherionBackgroundReference &reference,
                                           const TherionAreaAdjust &areaAdjust,
                                           const QRectF &modelBounds,
                                           const QRectF &previewBounds)
{
    if (item == nullptr || !reference.hasBasePosition) {
        return false;
    }

    const QString layerPath = item->data(0).toString();
    const QRectF modelRect = rasterModelRectFromMetadata(layerPath, reference, areaAdjust);
    if (!modelRect.isValid()) {
        return false;
    }

    storeMapEditorBackgroundTransformMetadata(item, reference);
    return placeMapEditorRasterLayerByModelRect(item, sourceImage, modelRect, modelBounds, previewBounds);
}

bool placeMapEditorRasterLayerPlaceholderFromMetadata(QGraphicsPixmapItem *item,
                                                      const TherionBackgroundReference &reference,
                                                      const TherionAreaAdjust &areaAdjust,
                                                      const QRectF &modelBounds,
                                                      const QRectF &previewBounds)
{
    if (item == nullptr || !reference.hasBasePosition) {
        return false;
    }

    const QRectF modelRect = rasterModelRectFromMetadata(item->data(0).toString(), reference, areaAdjust);
    if (!modelRect.isValid()) {
        return false;
    }

    storeMapEditorBackgroundTransformMetadata(item, reference);
    return placeMapEditorRasterLayerPlaceholderByModelRect(item, modelRect, modelBounds, previewBounds);
}

bool placeMapEditorLayerPlaceholderFromMetadata(QGraphicsPixmapItem *item,
                                                const QSizeF &modelSize,
                                                const TherionBackgroundReference &reference,
                                                const TherionAreaAdjust &areaAdjust,
                                                const QRectF &modelBounds,
                                                const QRectF &previewBounds)
{
    if (item == nullptr || !reference.hasBasePosition) {
        return false;
    }

    const QRectF modelRect = layerModelRectFromMetadata(modelSize, reference, areaAdjust);
    if (!modelRect.isValid()) {
        return false;
    }

    storeMapEditorBackgroundTransformMetadata(item, reference);
    return placeMapEditorRasterLayerPlaceholderByModelRect(item, modelRect, modelBounds, previewBounds);
}

bool placeMapEditorLayerFromMetadata(QGraphicsPixmapItem *item,
                                      const QSizeF &modelSize,
                                      const TherionBackgroundReference &reference,
                                      const TherionAreaAdjust &areaAdjust,
                                      const QRectF &modelBounds,
                                      const QRectF &previewBounds)
{
    if (item == nullptr || !reference.hasBasePosition) {
        return false;
    }

    const QRectF modelRect = layerModelRectFromMetadata(modelSize, reference, areaAdjust);
    if (!modelRect.isValid()) {
        return false;
    }

    storeMapEditorBackgroundTransformMetadata(item, reference);
    return placeMapEditorLayerInPreviewRect(item, previewRectForRasterModelRect(modelRect, modelBounds, previewBounds));
}

void storeMapEditorBackgroundTransformMetadata(QGraphicsPixmapItem *item,
                                               const TherionBackgroundReference &reference)
{
    if (item == nullptr) {
        return;
    }

    item->setData(kMapEditorBackgroundRotationDegRole, reference.rotationDeg);
    item->setData(kMapEditorBackgroundRotationCenterDxRole, reference.rotationCenterDx);
    item->setData(kMapEditorBackgroundRotationCenterDyRole, reference.rotationCenterDy);
    item->setData(kMapEditorBackgroundPivotSetRole, reference.pivotSet);
    item->setData(kMapEditorBackgroundMetadataFormatRole,
                  static_cast<int>(reference.metadataFormat));
    item->setData(kMapEditorBackgroundXScaleRole, reference.xScale);
    item->setData(kMapEditorBackgroundYScaleRole, reference.yScale);
}

}
