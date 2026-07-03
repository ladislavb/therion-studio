#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
struct TherionSourceLogicalCommand;
struct TherionSourcePhysicalRange;

QString canonicalOrAbsoluteFilePath(const QString &path);
QString therionSourceReferencePathToken(const TherionSourceLogicalCommand &command);
TherionSourcePhysicalRange therionSourceReferencePathRange(const TherionSourceLogicalCommand &command);
QStringList therionSourceReferencePathCandidates(const QString &currentFilePath, const QString &referencePath);
QString resolveTherionSourceReferencePath(const QString &currentFilePath, const QString &referencePath);
QString resolveTherionSourceReferencePath(const QString &currentFilePath,
                                          const QString &referencePath,
                                          const QSet<QString> &knownFilePaths);
}
