#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace TherionStudio
{
class MainWindowSessionDocumentService final
{
public:
    enum class RestoreTarget
    {
        TextEditor,
        MapEditor,
        SqlReport
    };

    struct RestoreEntry
    {
        QString filePath;
        RestoreTarget target = RestoreTarget::TextEditor;
    };

    struct OpenDocumentsState
    {
        QStringList openDocumentPaths;
        QString activeDocumentPath;
    };

    static std::vector<RestoreEntry> buildRestoreEntries(const QStringList &openDocumentPaths);
    static QStringList mergeOpenDocumentPaths(const QStringList &tabDocumentPaths,
                                              const QStringList &detachedMapDocumentPaths);
    static OpenDocumentsState buildOpenDocumentsState(const QStringList &tabDocumentPaths,
                                                      const QStringList &detachedMapDocumentPaths,
                                                      const QStringList &activeDetachedDocumentPaths,
                                                      const QString &currentDocumentPath);
    static bool isMapDocumentPath(const QString &filePath);
    static bool isSqlReportPath(const QString &filePath);
};
}
