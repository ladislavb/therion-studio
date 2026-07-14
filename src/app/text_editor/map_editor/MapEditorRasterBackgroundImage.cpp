#include "MapEditorRasterBackgroundImage.h"

#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QtMath>

#include <cmath>

namespace TherionStudio
{
namespace
{
// Upper bound on the longest edge of a raster layer's display pixmap. Keeps the
// image far crisper than the previous preview-resolution rasterization while
// bounding texture memory for very large scans.
constexpr int kMaxRasterDisplayEdge = 6000;

QImage cappedRasterDisplayImage(const QImage &image)
{
    if (image.isNull()) {
        return image;
    }
    const int longestEdge = qMax(image.width(), image.height());
    if (longestEdge <= kMaxRasterDisplayEdge) {
        return image;
    }
    return image.scaled(QSize(kMaxRasterDisplayEdge, kMaxRasterDisplayEdge),
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
}

QString normalizedRasterPathKey(const QString &path)
{
    return MapEditorBackgroundAssetCache::canonicalSourceIdentity(path);
}

std::optional<MapEditorBackgroundAssetRequest> rasterImageRequest(const QString &layerPath,
                                                                   const QString &decodeOptions)
{
    if (layerPath.isEmpty() || isMapEditorXviBackgroundPath(layerPath)) {
        return std::nullopt;
    }

    const QFileInfo fileInfo(layerPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return std::nullopt;
    }

    return MapEditorBackgroundAssetRequest{
        .key = MapEditorBackgroundAssetKey{
            .canonicalSourcePath = normalizedRasterPathKey(fileInfo.absoluteFilePath()),
            .format = MapEditorBackgroundAssetFormat::Raster,
            .decodeOptions = decodeOptions},
        .revision = MapEditorBackgroundAssetRevision{
            .byteSize = fileInfo.size(),
            .modifiedMilliseconds = fileInfo.lastModified().toMSecsSinceEpoch()}};
}

std::optional<QImage> cachedRasterImage(MapEditorBackgroundAssetCache &cache,
                                        const QString &layerPath,
                                        const QString &decodeOptions)
{
    const std::optional<MapEditorBackgroundAssetRequest> request = rasterImageRequest(layerPath, decodeOptions);
    if (!request.has_value()) {
        return std::nullopt;
    }

    const MapEditorBackgroundAssetCacheResult result = cache.find(request.value());
    if (!result.cacheHit || !result.loadResult.payload) {
        return std::nullopt;
    }

    const auto image = std::static_pointer_cast<const QImage>(result.loadResult.payload);
    return image && !image->isNull() ? std::optional<QImage>(*image) : std::nullopt;
}

void rememberRasterImage(MapEditorBackgroundAssetCache &cache,
                         const QString &layerPath,
                         const QString &decodeOptions,
                         const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    const std::optional<MapEditorBackgroundAssetRequest> request = rasterImageRequest(layerPath, decodeOptions);
    if (!request.has_value()) {
        return;
    }

    cache.store(request.value(), MapEditorBackgroundAssetLoadResult{
                                     .payload = std::make_shared<const QImage>(image),
                                     .byteCost = static_cast<std::size_t>(image.sizeInBytes())});
}
}

bool isMapEditorXviBackgroundPath(const QString &layerPath)
{
    return layerPath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive);
}

std::optional<QImage> cachedMapEditorRasterSourceImage(MapEditorBackgroundAssetCache &cache,
                                                       const QString &layerPath)
{
    return cachedRasterImage(cache, layerPath, QStringLiteral("raster-source-v1"));
}

void rememberMapEditorRasterSourceImage(MapEditorBackgroundAssetCache &cache,
                                        const QString &layerPath,
                                        const QImage &image)
{
    rememberRasterImage(cache, layerPath, QStringLiteral("raster-source-v1"), image);
}

MapEditorRasterSourceImageLoadResult readMapEditorRasterSourceImageUncached(const QString &layerPath)
{
    MapEditorRasterSourceImageLoadResult result;
    result.imagePath = QFileInfo(layerPath).absoluteFilePath();
    if (layerPath.isEmpty() || isMapEditorXviBackgroundPath(layerPath)) {
        return result;
    }

    QImageReader imageReader(layerPath);
    imageReader.setAutoTransform(true);
    result.image = imageReader.read();
    return result;
}

QImage readMapEditorRasterSourceImage(MapEditorBackgroundAssetCache &cache, const QString &layerPath)
{
    if (layerPath.isEmpty() || isMapEditorXviBackgroundPath(layerPath)) {
        return QImage();
    }

    if (const std::optional<QImage> cachedImage = cachedMapEditorRasterSourceImage(cache, layerPath); cachedImage.has_value()) {
        return cachedImage.value();
    }

    MapEditorRasterSourceImageLoadResult result = readMapEditorRasterSourceImageUncached(layerPath);
    rememberMapEditorRasterSourceImage(cache, result.imagePath, result.image);
    return result.image;
}

QSizeF mapEditorRasterModelSize(const QString &layerPath, qreal imageScale)
{
    Q_UNUSED(imageScale);

    if (layerPath.isEmpty()) {
        return QSizeF();
    }

    QImageReader imageReader(layerPath);
    imageReader.setAutoTransform(true);
    const QSize imageSize = imageReader.size();
    if (!imageSize.isValid()) {
        return QSizeF();
    }

    return QSizeF(imageSize.width(), imageSize.height());
}

QImage mapEditorRasterDisplayImage(const QImage &sourceImage)
{
    return cappedRasterDisplayImage(sourceImage);
}

std::optional<QImage> cachedMapEditorRasterDisplayImage(MapEditorBackgroundAssetCache &cache,
                                                         const QString &layerPath,
                                                         qreal gamma)
{
    const qreal boundedGamma = qBound(0.2, gamma, 2.5);
    return cachedRasterImage(cache,
                             layerPath,
                             QStringLiteral("raster-display-gamma-v1:%1")
                                 .arg(QString::number(qRound(boundedGamma * 1000.0))));
}

void rememberMapEditorRasterDisplayImage(MapEditorBackgroundAssetCache &cache,
                                         const QString &layerPath,
                                         qreal gamma,
                                         const QImage &image)
{
    const qreal boundedGamma = qBound(0.2, gamma, 2.5);
    rememberRasterImage(cache,
                        layerPath,
                        QStringLiteral("raster-display-gamma-v1:%1")
                            .arg(QString::number(qRound(boundedGamma * 1000.0))),
                        image);
}

QImage gammaCorrectMapEditorRasterSourceImage(QImage sourceImage,
                                              qreal gamma)
{
    if (sourceImage.isNull()) {
        return QImage();
    }

    const qreal boundedGamma = qBound(0.2, gamma, 2.5);
    if (qFuzzyCompare(boundedGamma, 1.0)) {
        const QImage displayImage = cappedRasterDisplayImage(sourceImage);
        return displayImage;
    }

    QImage displayImage = cappedRasterDisplayImage(sourceImage).convertToFormat(QImage::Format_RGBA8888);
    unsigned char lookupTable[256];
    for (int value = 0; value < 256; ++value) {
        const qreal normalized = static_cast<qreal>(value) / 255.0;
        const qreal corrected = std::pow(normalized, 1.0 / boundedGamma);
        lookupTable[value] = static_cast<unsigned char>(qBound(0, qRound(corrected * 255.0), 255));
    }

    for (int y = 0; y < displayImage.height(); ++y) {
        uchar *scanLine = displayImage.scanLine(y);
        for (int x = 0; x < displayImage.width(); ++x) {
            uchar *pixel = scanLine + (x * 4);
            pixel[0] = lookupTable[pixel[0]];
            pixel[1] = lookupTable[pixel[1]];
            pixel[2] = lookupTable[pixel[2]];
        }
    }

    return displayImage;
}

quint64 nextMapEditorRasterGammaRequestId()
{
    static quint64 nextRequestId = 0;
    return ++nextRequestId;
}

quint64 nextMapEditorRasterLoadRequestId()
{
    static quint64 nextRequestId = 0;
    return ++nextRequestId;
}

}
