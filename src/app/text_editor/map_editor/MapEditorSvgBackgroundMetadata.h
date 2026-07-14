#pragma once

#include <QByteArray>
#include <QRectF>
#include <QSizeF>
#include <QString>

namespace TherionStudio
{

struct MapEditorSvgBackgroundMetadata
{
    QSizeF intrinsicSize;
    QRectF sourceViewBox;
    bool valid = false;
};

MapEditorSvgBackgroundMetadata readMapEditorSvgBackgroundMetadata(const QString &absolutePath);
MapEditorSvgBackgroundMetadata parseMapEditorSvgBackgroundMetadata(const QByteArray &svgData);

}
