#include "TextEditorSourceSnapshotContext.h"

#include <QPlainTextEdit>

namespace TherionStudio
{
TextEditorSourceSnapshotContext TextEditorSourceSnapshotContext::fromEditor(
    const QPlainTextEdit *editor,
    TherionSourceDocumentType sourceType,
    const QString &encodingName,
    TherionSourceSnapshotCatalogKey catalogKey)
{
    TextEditorSourceSnapshotContext context;
    context.sourceText = editor != nullptr ? editor->toPlainText() : QString();
    context.metadata.sourceType = sourceType;
    context.metadata.encodingName = encodingName;
    context.metadata.revisionId = editor != nullptr && editor->document() != nullptr
        ? editor->document()->revision()
        : 0;
    context.catalogKey = catalogKey;
    return context;
}

const TherionSourceDocument &TextEditorSourceSnapshotContext::sourceDocument(TherionSourceSnapshotCache &cache) const
{
    return cache.sourceDocument(sourceText, metadata);
}

const TherionSourceLogicalDocument &TextEditorSourceSnapshotContext::logicalDocument(
    TherionSourceSnapshotCache &cache) const
{
    return cache.logicalDocument(sourceText, metadata);
}
}
