#pragma once

#include "ThreeDViewerSceneModel.h"

#include <QByteArray>
#include <QString>

namespace TherionStudio
{

class ThreeDViewerLoxLoader
{
public:
    struct Result
    {
        ThreeDViewerSceneModel scene;
        QString error;

        bool ok() const { return error.isEmpty(); }
    };

    Result loadFile(const QString &path) const;
    Result loadStationMetadataFile(const QString &path) const;
    Result loadBytes(const QByteArray &bytes) const;
};

} // namespace TherionStudio
