#pragma once

#include "MapEditorBackgroundAssetCache.h"
#include "MapEditorSvgBackgroundMetadata.h"
#include "../../../core/TherionXviParser.h"

#include <QByteArray>

namespace TherionStudio
{
struct MapEditorSvgBackgroundAsset
{
    QByteArray sourceData;
    MapEditorSvgBackgroundMetadata metadata;
};

class MapEditorBackgroundAssetLoader final
{
public:
    static bool loadXvi(MapEditorBackgroundAssetCache &assetCache,
                        const QString &absolutePath,
                        TherionXviDocument *document);
    static bool loadSvg(MapEditorBackgroundAssetCache &assetCache,
                        const QString &absolutePath,
                        MapEditorSvgBackgroundAsset *asset);
};
}
