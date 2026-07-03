#pragma once

#include "../core/ISessionStore.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace TherionStudio
{
class MainWindowSessionDocumentController final
{
public:
    struct RestoreOpenDocumentsActions
    {
        std::function<void(const QString &)> openMapEditorDocument;
        std::function<bool(const QString &)> openSqlReportDocument;
        std::function<bool(const QString &)> openTextEditorDocument;
        std::function<void(const QString &)> appendSkippedUnsupportedDocumentConsole;
        std::function<void(const QString &)> activateRestoredDocumentPath;
        std::function<void()> persistOpenDocuments;
    };

    struct PersistOpenDocumentsSnapshot
    {
        QStringList tabDocumentPaths;
        QStringList detachedMapDocumentPaths;
        QStringList activeDetachedDocumentPaths;
        QString currentDocumentPath;
    };

    static void restoreOpenDocuments(const ISessionStore &sessionStore,
                                     const RestoreOpenDocumentsActions &actions);

    static void persistOpenDocuments(const PersistOpenDocumentsSnapshot &snapshot,
                                     ISessionStore &sessionStore);
};
}
