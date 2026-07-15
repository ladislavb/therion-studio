#include "MapEditorBackgroundAssetLoader.h"

#include <QFile>
#include <QFileInfo>

#include <memory>

namespace TherionStudio
{
bool MapEditorBackgroundAssetLoader::loadXvi(MapEditorBackgroundAssetCache &assetCache,
                                             const QString &absolutePath,
                                             TherionXviDocument *document)
{
    if (document == nullptr || absolutePath.isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(absolutePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const QString sourceIdentity = MapEditorBackgroundAssetCache::canonicalSourceIdentity(fileInfo.absoluteFilePath());
    if (sourceIdentity.isEmpty()) {
        return false;
    }

    const MapEditorBackgroundAssetRequest request{
        .key = MapEditorBackgroundAssetKey{
            .canonicalSourcePath = sourceIdentity,
            .format = MapEditorBackgroundAssetFormat::Xvi,
            .decodeOptions = QStringLiteral("therion-xvi-v1")},
        .revision = MapEditorBackgroundAssetRevision{
            .byteSize = fileInfo.size(),
            .modifiedMilliseconds = fileInfo.lastModified().toMSecsSinceEpoch()}};
    const MapEditorBackgroundAssetCacheResult cacheResult = assetCache.load(
        request,
        [filePath = fileInfo.absoluteFilePath()](const MapEditorBackgroundAssetRequest &) {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                return MapEditorBackgroundAssetLoadResult{
                    .error = QStringLiteral("Unable to read XVI background source.")};
            }

            const QByteArray fileBytes = file.readAll();
            auto parsed = std::make_shared<TherionXviDocument>();
            if (!parseTherionXviDocumentText(QString::fromUtf8(fileBytes), parsed.get())) {
                return MapEditorBackgroundAssetLoadResult{
                    .error = QStringLiteral("Unable to parse XVI background source.")};
            }
            return MapEditorBackgroundAssetLoadResult{
                .payload = std::move(parsed),
                .byteCost = static_cast<std::size_t>(fileBytes.size())};
        });
    if (!cacheResult.loadResult.payload) {
        return false;
    }

    *document = *std::static_pointer_cast<const TherionXviDocument>(cacheResult.loadResult.payload);
    return true;
}

bool MapEditorBackgroundAssetLoader::loadSvg(MapEditorBackgroundAssetCache &assetCache,
                                             const QString &absolutePath,
                                             MapEditorSvgBackgroundAsset *asset)
{
    if (asset == nullptr || absolutePath.isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(absolutePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const MapEditorBackgroundAssetRequest request{
        .key = MapEditorBackgroundAssetKey{
            .canonicalSourcePath = MapEditorBackgroundAssetCache::canonicalSourceIdentity(fileInfo.absoluteFilePath()),
            .format = MapEditorBackgroundAssetFormat::Svg,
            .decodeOptions = QStringLiteral("therion-svg-source-v1")},
        .revision = MapEditorBackgroundAssetRevision{
            .byteSize = fileInfo.size(),
            .modifiedMilliseconds = fileInfo.lastModified().toMSecsSinceEpoch()}};
    const MapEditorBackgroundAssetCacheResult cacheResult = assetCache.load(
        request,
        [filePath = fileInfo.absoluteFilePath()](const MapEditorBackgroundAssetRequest &) {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                return MapEditorBackgroundAssetLoadResult{.error = QStringLiteral("Unable to read SVG background.")};
            }

            const QByteArray sourceData = file.readAll();
            const MapEditorSvgBackgroundMetadata metadata = parseMapEditorSvgBackgroundMetadata(sourceData);
            if (!metadata.valid) {
                return MapEditorBackgroundAssetLoadResult{.error = QStringLiteral("Invalid SVG background.")};
            }

            return MapEditorBackgroundAssetLoadResult{
                .payload = std::make_shared<const MapEditorSvgBackgroundAsset>(MapEditorSvgBackgroundAsset{
                    .sourceData = sourceData,
                    .metadata = metadata}),
                .byteCost = static_cast<std::size_t>(sourceData.size())};
        });
    if (!cacheResult.loadResult.payload) {
        return false;
    }

    *asset = *std::static_pointer_cast<const MapEditorSvgBackgroundAsset>(cacheResult.loadResult.payload);
    return true;
}
}
