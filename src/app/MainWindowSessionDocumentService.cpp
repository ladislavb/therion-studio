#include "MainWindowSessionDocumentService.h"

#include <QFileInfo>
#include <QSet>

#include <utility>

namespace TherionStudio
{
std::vector<MainWindowSessionDocumentService::RestoreEntry> MainWindowSessionDocumentService::buildRestoreEntries(const QStringList &openDocumentPaths)
{
    std::vector<RestoreEntry> entries;
    entries.reserve(static_cast<size_t>(openDocumentPaths.size()));

    for (const QString &documentPath : openDocumentPaths) {
        if (documentPath.isEmpty()) {
            continue;
        }

        RestoreEntry entry;
        entry.filePath = documentPath;
        if (isMapDocumentPath(documentPath)) {
            entry.target = RestoreTarget::MapEditor;
        } else if (isSqlReportPath(documentPath)) {
            entry.target = RestoreTarget::SqlReport;
        } else {
            entry.target = RestoreTarget::TextEditor;
        }
        entries.push_back(std::move(entry));
    }

    return entries;
}

QStringList MainWindowSessionDocumentService::mergeOpenDocumentPaths(const QStringList &tabDocumentPaths,
                                                                     const QStringList &detachedMapDocumentPaths)
{
    QStringList mergedPaths;
    mergedPaths.reserve(tabDocumentPaths.size() + detachedMapDocumentPaths.size());
    QSet<QString> seenPaths;
    seenPaths.reserve(tabDocumentPaths.size() + detachedMapDocumentPaths.size());

    for (const QString &documentPath : tabDocumentPaths) {
        if (!documentPath.isEmpty()) {
            if (seenPaths.contains(documentPath)) {
                continue;
            }
            seenPaths.insert(documentPath);
            mergedPaths.append(documentPath);
        }
    }

    for (const QString &documentPath : detachedMapDocumentPaths) {
        if (documentPath.isEmpty() || seenPaths.contains(documentPath)) {
            continue;
        }
        seenPaths.insert(documentPath);
        mergedPaths.append(documentPath);
    }

    return mergedPaths;
}

MainWindowSessionDocumentService::OpenDocumentsState MainWindowSessionDocumentService::buildOpenDocumentsState(const QStringList &tabDocumentPaths,
                                                                                                                const QStringList &detachedMapDocumentPaths,
                                                                                                                const QStringList &activeDetachedDocumentPaths,
                                                                                                                const QString &currentDocumentPath)
{
    OpenDocumentsState state;
    state.openDocumentPaths = mergeOpenDocumentPaths(tabDocumentPaths, detachedMapDocumentPaths);

    for (const QString &detachedPath : activeDetachedDocumentPaths) {
        if (!detachedPath.isEmpty()) {
            state.activeDocumentPath = detachedPath;
            return state;
        }
    }

    state.activeDocumentPath = currentDocumentPath;
    return state;
}

bool MainWindowSessionDocumentService::isMapDocumentPath(const QString &filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("th2"), Qt::CaseInsensitive) == 0;
}

bool MainWindowSessionDocumentService::isSqlReportPath(const QString &filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("sql"), Qt::CaseInsensitive) == 0;
}
}
