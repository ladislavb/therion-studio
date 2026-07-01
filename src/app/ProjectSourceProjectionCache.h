#pragma once

#include "ProjectSourceSnapshot.h"

#include "../core/TherionSourceDocument.h"
#include "../core/TherionSourceLogicalDocument.h"
#include "../core/TherionSourceSnapshotCache.h"
#include "../core/TherionSourceValidationCatalog.h"

#include <QHash>
#include <QString>

#include <memory>

namespace TherionStudio
{
struct ProjectSourceProjectionCacheStats
{
    int sourceDocumentBuilds = 0;
    int sourceDocumentHits = 0;
    int logicalDocumentBuilds = 0;
    int logicalDocumentHits = 0;
    int catalogLogicalDocumentBuilds = 0;
    int catalogLogicalDocumentHits = 0;
};

class ProjectSourceProjectionCache final
{
public:
    [[nodiscard]] const TherionSourceDocument &sourceDocument(const ProjectSourceDocument &document);
    [[nodiscard]] const TherionSourceLogicalDocument &logicalDocument(const ProjectSourceDocument &document);
    [[nodiscard]] std::shared_ptr<const TherionSourceLogicalDocument> logicalDocumentHandle(
        const ProjectSourceDocument &document);
    [[nodiscard]] const TherionSourceLogicalDocument &logicalDocument(
        const ProjectSourceDocument &document,
        const TherionSourceValidationCatalog &catalog,
        TherionSourceSnapshotCatalogKey catalogKey);

    [[nodiscard]] ProjectSourceProjectionCacheStats stats() const;
    void resetStats();
    void clear();

private:
    [[nodiscard]] static TherionSourceDocumentMetadata metadataForDocument(const ProjectSourceDocument &document);
    [[nodiscard]] static QString sourceKeyForDocument(const ProjectSourceDocument &document);
    [[nodiscard]] static QString logicalKeyForDocument(const ProjectSourceDocument &document);
    [[nodiscard]] static QString catalogLogicalKeyForDocument(const ProjectSourceDocument &document,
                                                              TherionSourceSnapshotCatalogKey catalogKey);

    QHash<QString, std::shared_ptr<TherionSourceDocument>> sourceDocuments_;
    QHash<QString, std::shared_ptr<TherionSourceLogicalDocument>> logicalDocuments_;
    QHash<QString, std::shared_ptr<TherionSourceLogicalDocument>> catalogLogicalDocuments_;
    std::shared_ptr<TherionSourceLogicalDocument> transientCatalogLogicalDocument_;
    ProjectSourceProjectionCacheStats stats_;
};
}
