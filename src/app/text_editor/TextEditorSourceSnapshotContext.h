#pragma once

#include "../../core/TherionSourceSnapshotCache.h"

#include <QString>

class QPlainTextEdit;

namespace TherionStudio
{
struct TextEditorSourceSnapshotContext
{
    QString sourceText;
    TherionSourceDocumentMetadata metadata;
    TherionSourceSnapshotCatalogKey catalogKey = TherionSourceSnapshotCatalogKey::none();

    [[nodiscard]] static TextEditorSourceSnapshotContext fromEditor(
        const QPlainTextEdit *editor,
        TherionSourceDocumentType sourceType = TherionSourceDocumentType::Unknown,
        const QString &encodingName = {},
        TherionSourceSnapshotCatalogKey catalogKey = TherionSourceSnapshotCatalogKey::none());

    [[nodiscard]] const TherionSourceDocument &sourceDocument(TherionSourceSnapshotCache &cache) const;
    [[nodiscard]] const TherionSourceLogicalDocument &logicalDocument(TherionSourceSnapshotCache &cache) const;

    template <typename Handler>
    auto withLogicalDocument(TherionSourceSnapshotCache *cache, Handler handler) const
    {
        if (cache != nullptr) {
            return handler(logicalDocument(*cache));
        }

        TherionSourceSnapshotCache localCache;
        return handler(logicalDocument(localCache));
    }
};
}
