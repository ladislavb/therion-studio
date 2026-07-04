#pragma once

#include <QString>

class QDateTime;

namespace TherionStudio
{

QString defaultExportFileName(const QString &kind,
                              const QString &projectRootPath,
                              const QString &fallbackPath,
                              const QString &extension,
                              const QDateTime &timestamp);

QString defaultArtifactExportFileName(const QString &artifactPath,
                                      const QString &fallbackKind,
                                      const QString &extension,
                                      const QDateTime &timestamp);

} // namespace TherionStudio
