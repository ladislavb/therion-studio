#pragma once

#include "../core/ISessionStore.h"

#include <QString>

#include <functional>

namespace TherionStudio
{
class MainWindowProjectController final
{
public:
    struct Actions
    {
        std::function<void(const QString &, const QString &)> showWarningDialog;
        std::function<void(const QString &, int)> showStatusBarMessage;
        std::function<void(const QString &)> appendConsoleLine;
        std::function<void(const QString &)> setProjectRootPath;
        std::function<void(const QString &)> applyProjectRootToBrowser;
        std::function<void()> loadStructureNameOverrides;
        std::function<void()> syncOpenDocumentsToProjectRoot;
        std::function<void()> rebuildStructureSidebar;
        std::function<void()> refreshTherionConfigDisplay;
        std::function<void()> updateProjectActionState;
        std::function<void()> ensureWelcomeTab;
        std::function<void()> clearDocumentTabs;
        std::function<void()> resetProjectBrowser;
        std::function<void()> clearProjectValidationResults;
        std::function<void()> persistOpenDocuments;
        std::function<void()> resetProjectTherionRunContext;
    };

    static void openProject(const QString &selectedProjectPath,
                            bool hasNoTabs,
                            bool hasWelcomeTab,
                            ISessionStore &sessionStore,
                            const Actions &actions);

    static void closeProject(const QString &currentProjectPath,
                             const std::function<bool()> &confirmCloseDirtyDocuments,
                             ISessionStore &sessionStore,
                             const Actions &actions);
};
}
