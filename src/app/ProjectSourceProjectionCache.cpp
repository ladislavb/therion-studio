#include "ProjectSourceProjectionCache.h"

#include "../core/TherionFileTypes.h"

#include <QStringList>

namespace TherionStudio
{
namespace
{
QString sourceDocumentTypeKey(TherionSourceDocumentType sourceType)
{
    return QString::number(static_cast<int>(sourceType));
}

QString catalogKeyPart(TherionSourceSnapshotCatalogKey catalogKey)
{
    if (!catalogKey.enabled) {
        return QStringLiteral("catalog=none");
    }
    return QStringLiteral("catalog=%1").arg(catalogKey.revisionId);
}
}

const TherionSourceDocument &ProjectSourceProjectionCache::sourceDocument(const ProjectSourceDocument &document)
{
    const QString key = sourceKeyForDocument(document);
    auto it = sourceDocuments_.constFind(key);
    if (it != sourceDocuments_.constEnd()) {
        ++stats_.sourceDocumentHits;
        return *it.value();
    }

    auto builtDocument = std::make_shared<TherionSourceDocument>(
        TherionSourceDocument::fromText(document.text, metadataForDocument(document)));
    sourceDocuments_.insert(key, builtDocument);
    ++stats_.sourceDocumentBuilds;
    return *builtDocument;
}

const TherionSourceLogicalDocument &ProjectSourceProjectionCache::logicalDocument(const ProjectSourceDocument &document)
{
    const QString key = logicalKeyForDocument(document);
    auto it = logicalDocuments_.constFind(key);
    if (it != logicalDocuments_.constEnd()) {
        ++stats_.logicalDocumentHits;
        return *it.value();
    }

    auto builtDocument = std::make_shared<TherionSourceLogicalDocument>(
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument(document)));
    logicalDocuments_.insert(key, builtDocument);
    ++stats_.logicalDocumentBuilds;
    return *builtDocument;
}

const TherionSourceLogicalDocument &ProjectSourceProjectionCache::logicalDocument(
    const ProjectSourceDocument &document,
    const TherionSourceValidationCatalog &catalog,
    TherionSourceSnapshotCatalogKey catalogKey)
{
    if (!catalogKey.enabled) {
        transientCatalogLogicalDocument_ = std::make_shared<TherionSourceLogicalDocument>(
            TherionSourceLogicalDocument::fromSourceDocument(sourceDocument(document), catalog));
        ++stats_.catalogLogicalDocumentBuilds;
        return *transientCatalogLogicalDocument_;
    }

    const QString key = catalogLogicalKeyForDocument(document, catalogKey);
    auto it = catalogLogicalDocuments_.constFind(key);
    if (it != catalogLogicalDocuments_.constEnd()) {
        ++stats_.catalogLogicalDocumentHits;
        return *it.value();
    }

    auto builtDocument = std::make_shared<TherionSourceLogicalDocument>(
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument(document), catalog));
    catalogLogicalDocuments_.insert(key, builtDocument);
    ++stats_.catalogLogicalDocumentBuilds;
    return *builtDocument;
}

ProjectSourceProjectionCacheStats ProjectSourceProjectionCache::stats() const
{
    return stats_;
}

void ProjectSourceProjectionCache::clear()
{
    sourceDocuments_.clear();
    logicalDocuments_.clear();
    catalogLogicalDocuments_.clear();
    transientCatalogLogicalDocument_.reset();
    stats_ = {};
}

TherionSourceDocumentMetadata ProjectSourceProjectionCache::metadataForDocument(const ProjectSourceDocument &document)
{
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = therionSourceDocumentTypeForFilePath(document.normalizedPath);
    return metadata;
}

QString ProjectSourceProjectionCache::sourceKeyForDocument(const ProjectSourceDocument &document)
{
    const TherionSourceDocumentMetadata metadata = metadataForDocument(document);
    return QStringList({
               QStringLiteral("path=%1").arg(document.normalizedPath),
               QStringLiteral("type=%1").arg(sourceDocumentTypeKey(metadata.sourceType)),
               QStringLiteral("loaded=%1").arg(document.textLoaded ? 1 : 0),
               QStringLiteral("hash=%1").arg(QString::fromLatin1(projectSourceContentHash(document.text).toHex())),
           })
        .join(QLatin1Char('\n'));
}

QString ProjectSourceProjectionCache::logicalKeyForDocument(const ProjectSourceDocument &document)
{
    return sourceKeyForDocument(document) + QStringLiteral("\nlogical=plain-v1");
}

QString ProjectSourceProjectionCache::catalogLogicalKeyForDocument(const ProjectSourceDocument &document,
                                                                   TherionSourceSnapshotCatalogKey catalogKey)
{
    return logicalKeyForDocument(document)
        + QLatin1Char('\n')
        + catalogKeyPart(catalogKey)
        + QStringLiteral("\nlogical=catalog-v1");
}
}
