#include "MainWindowSessionDocumentController.h"

#include "MainWindowSessionDocumentService.h"
#include "MainWindowSessionProjectService.h"
#include "MainWindowSessionStateService.h"

namespace TherionStudio
{
void MainWindowSessionDocumentController::restoreOpenDocuments(const ISessionStore &sessionStore,
                                                               const RestoreOpenDocumentsActions &actions)
{
    const std::vector<MainWindowSessionDocumentService::RestoreEntry> restoreEntries =
        MainWindowSessionDocumentService::buildRestoreEntries(sessionStore.openDocumentPaths());
    if (restoreEntries.empty()) {
        return;
    }

    const QString activeDocumentPath = sessionStore.activeDocumentPath();
    bool skippedUnsupportedDocument = false;

    for (const auto &entry : restoreEntries) {
        const QString &documentPath = entry.filePath;
        if (documentPath.isEmpty()
            || MainWindowSessionProjectService::isProtectedMacUserFolder(documentPath)) {
            continue;
        }

        if (entry.target == MainWindowSessionDocumentService::RestoreTarget::MapEditor) {
            if (actions.openMapEditorDocument) {
                actions.openMapEditorDocument(documentPath);
            }
            continue;
        }

        bool opened = false;
        if (entry.target == MainWindowSessionDocumentService::RestoreTarget::SqlReport) {
            opened = !actions.openSqlReportDocument
                || actions.openSqlReportDocument(documentPath);
        } else {
            opened = !actions.openTextEditorDocument
                || actions.openTextEditorDocument(documentPath);
        }
        if (!opened) {
            skippedUnsupportedDocument = true;
            if (actions.appendSkippedUnsupportedDocumentConsole) {
                actions.appendSkippedUnsupportedDocumentConsole(documentPath);
            }
        }
    }

    if (!activeDocumentPath.isEmpty() && actions.activateRestoredDocumentPath) {
        actions.activateRestoredDocumentPath(activeDocumentPath);
    }

    if (skippedUnsupportedDocument && actions.persistOpenDocuments) {
        actions.persistOpenDocuments();
    }
}

void MainWindowSessionDocumentController::persistOpenDocuments(const PersistOpenDocumentsSnapshot &snapshot,
                                                               ISessionStore &sessionStore)
{
    const MainWindowSessionDocumentService::OpenDocumentsState state =
        MainWindowSessionDocumentService::buildOpenDocumentsState(snapshot.tabDocumentPaths,
                                                                  snapshot.detachedMapDocumentPaths,
                                                                  snapshot.activeDetachedDocumentPaths,
                                                                  snapshot.currentDocumentPath);

    MainWindowSessionStateService::OpenDocumentsSnapshot persistedSnapshot;
    persistedSnapshot.openDocumentPaths = state.openDocumentPaths;
    persistedSnapshot.activeDocumentPath = state.activeDocumentPath;
    MainWindowSessionStateService::persistOpenDocuments(sessionStore, persistedSnapshot);
}
}
