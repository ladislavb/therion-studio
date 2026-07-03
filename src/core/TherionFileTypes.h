#pragma once

#include "TherionSourceDocument.h"

#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
inline bool isTherionConfigFileName(const QString &fileName)
{
    const QString normalized = fileName.trimmed().toLower();
    return normalized == QStringLiteral("thconfig")
        || normalized.startsWith(QStringLiteral("thconfig."))
        || normalized.endsWith(QStringLiteral(".thconfig"));
}

inline bool isTherionConfigFilePath(const QString &filePath)
{
    return isTherionConfigFileName(QFileInfo(filePath).fileName());
}

inline bool isTherionProjectFileName(const QString &fileName)
{
    if (isTherionConfigFileName(fileName)) {
        return true;
    }

    const QString suffix = QFileInfo(fileName).suffix().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2");
}

inline bool isTherionProjectFilePath(const QString &filePath)
{
    return isTherionProjectFileName(QFileInfo(filePath).fileName());
}

inline bool isThreeDViewerArtifactFileName(const QString &fileName)
{
    return QFileInfo(fileName).suffix().toLower() == QStringLiteral("lox");
}

inline bool isThreeDViewerArtifactFilePath(const QString &filePath)
{
    return isThreeDViewerArtifactFileName(QFileInfo(filePath).fileName());
}

inline bool isTherionSqlExportFileName(const QString &fileName)
{
    return QFileInfo(fileName).suffix().toLower() == QStringLiteral("sql");
}

inline bool isTherionSqlExportFilePath(const QString &filePath)
{
    return isTherionSqlExportFileName(QFileInfo(filePath).fileName());
}

inline TherionSourceDocumentType therionSourceDocumentTypeForFileName(const QString &fileName)
{
    if (isTherionConfigFileName(fileName)) {
        return TherionSourceDocumentType::TherionConfig;
    }

    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == QStringLiteral("th")) {
        return TherionSourceDocumentType::TherionSource;
    }
    if (suffix == QStringLiteral("th2")) {
        return TherionSourceDocumentType::TherionMap;
    }

    return TherionSourceDocumentType::Unknown;
}

inline QString therionSourceDocumentTypeCatalogToken(TherionSourceDocumentType sourceType)
{
    switch (sourceType) {
    case TherionSourceDocumentType::TherionSource:
        return QStringLiteral("th");
    case TherionSourceDocumentType::TherionMap:
        return QStringLiteral("th2");
    case TherionSourceDocumentType::TherionConfig:
        return QStringLiteral("thconfig");
    case TherionSourceDocumentType::Unknown:
        return QString();
    }

    return QString();
}

inline TherionSourceDocumentType therionSourceDocumentTypeForFilePath(const QString &filePath)
{
    return therionSourceDocumentTypeForFileName(QFileInfo(filePath).fileName());
}

inline QByteArray initialTherionProjectFileContents(const QString &fileName)
{
    return isTherionProjectFileName(fileName)
        ? QByteArray("encoding utf-8\n")
        : QByteArray();
}

inline QStringList therionConfigNameFilters()
{
    return {
        QStringLiteral("thconfig"),
        QStringLiteral("thconfig.*"),
        QStringLiteral("*.thconfig"),
    };
}
}
