#pragma once

#include "MapEditorBackgroundAssetCache.h"

#include <QImage>
#include <QSize>
#include <QSizeF>
#include <QString>

#include <optional>

namespace TherionStudio
{

struct MapEditorRasterSourceImageLoadResult
{
    QString imagePath;
    QImage image;
};

bool isMapEditorXviBackgroundPath(const QString &layerPath);
std::optional<QImage> cachedMapEditorRasterSourceImage(MapEditorBackgroundAssetCache &cache,
                                                       const QString &layerPath);
void rememberMapEditorRasterSourceImage(MapEditorBackgroundAssetCache &cache,
                                        const QString &layerPath,
                                        const QImage &image);
MapEditorRasterSourceImageLoadResult readMapEditorRasterSourceImageUncached(const QString &layerPath);
QImage readMapEditorRasterSourceImage(MapEditorBackgroundAssetCache &cache, const QString &layerPath);
QSizeF mapEditorRasterModelSize(const QString &layerPath, qreal imageScale);
// Returns a full-resolution display copy of the source image, downsampled only
// if its longest edge exceeds the display cap. Geometry is unaffected because
// the on-screen size is derived from the item transform, not the pixel count.
QImage mapEditorRasterDisplayImage(const QImage &sourceImage);
// Gamma-corrects at full display resolution. Scaling to the scene is handled by
// the raster layer item transform, so this no longer rescales the image.
std::optional<QImage> cachedMapEditorRasterDisplayImage(MapEditorBackgroundAssetCache &cache,
                                                         const QString &layerPath,
                                                         qreal gamma);
void rememberMapEditorRasterDisplayImage(MapEditorBackgroundAssetCache &cache,
                                         const QString &layerPath,
                                         qreal gamma,
                                         const QImage &image);
QImage gammaCorrectMapEditorRasterSourceImage(QImage sourceImage,
                                              qreal gamma);
quint64 nextMapEditorRasterGammaRequestId();
quint64 nextMapEditorRasterLoadRequestId();

}
