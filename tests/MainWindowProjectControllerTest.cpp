#include "../src/app/MainWindowProjectController.h"
#include "FakeSessionStore.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QStringList>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runOpenProjectCancelledTest()
{
    FakeSessionStore sessionStore;
    QStringList calls;

    MainWindowProjectController::Actions actions;
    actions.showStatusBarMessage = [&calls](const QString &message, int timeoutMs) {
        calls.append(QStringLiteral("status:%1:%2").arg(message).arg(timeoutMs));
    };
    actions.showWarningDialog = [&calls](const QString &title, const QString &message) {
        calls.append(QStringLiteral("warning:%1:%2").arg(title, message));
    };

    MainWindowProjectController::openProject(QString(),
                                             false,
                                             false,
                                             sessionStore,
                                             actions);

    const QStringList expected = {
        QStringLiteral("status:Open project cancelled:2000")};
    if (!expect(calls == expected,
                "Cancelled open-project controller flow changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runOpenProjectMissingDirectoryTest()
{
    FakeSessionStore sessionStore;
    QStringList calls;

    MainWindowProjectController::Actions actions;
    actions.showStatusBarMessage = [&calls](const QString &message, int timeoutMs) {
        calls.append(QStringLiteral("status:%1:%2").arg(message).arg(timeoutMs));
    };
    actions.showWarningDialog = [&calls](const QString &title, const QString &message) {
        calls.append(QStringLiteral("warning:%1:%2").arg(title, message));
    };

    MainWindowProjectController::openProject(QStringLiteral("/definitely/missing/project/path"),
                                             false,
                                             false,
                                             sessionStore,
                                             actions);

    const QStringList expected = {
        QStringLiteral("warning:Open Project:The selected folder does not exist.")};
    if (!expect(calls == expected,
                "Missing-directory open-project controller flow changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runOpenProjectSuccessExecutionOrderTest()
{
    FakeSessionStore sessionStore;
    QTemporaryDir projectDir;
    if (!expect(projectDir.isValid(),
                "Temporary directory setup failed for open-project controller test.")) {
        return 1;
    }

    QStringList calls;

    MainWindowProjectController::Actions actions;
    actions.setProjectRootPath = [&calls](const QString &projectPath) {
        calls.append(QStringLiteral("set_root:%1").arg(projectPath));
    };
    actions.applyProjectRootToBrowser = [&calls](const QString &projectPath) {
        calls.append(QStringLiteral("apply_root:%1").arg(projectPath));
    };
    actions.loadStructureNameOverrides = [&calls]() { calls.append(QStringLiteral("load_overrides")); };
    actions.syncOpenDocumentsToProjectRoot = [&calls]() { calls.append(QStringLiteral("sync_docs")); };
    actions.rebuildStructureSidebar = [&calls]() { calls.append(QStringLiteral("rebuild_structure")); };
    actions.refreshTherionConfigDisplay = [&calls]() { calls.append(QStringLiteral("refresh_config")); };
    actions.updateProjectActionState = [&calls]() { calls.append(QStringLiteral("update_actions")); };
    actions.ensureWelcomeTab = [&calls]() { calls.append(QStringLiteral("ensure_welcome")); };
    actions.showStatusBarMessage = [&calls](const QString &message, int timeoutMs) {
        calls.append(QStringLiteral("status:%1:%2").arg(message).arg(timeoutMs));
    };
    actions.appendConsoleLine = [&calls](const QString &line) {
        calls.append(QStringLiteral("console:%1").arg(line));
    };

    MainWindowProjectController::openProject(projectDir.path(),
                                             true,
                                             false,
                                             sessionStore,
                                             actions);

    const QString projectPath = projectDir.path();
    const QStringList expected = {
        QStringLiteral("set_root:%1").arg(projectPath),
        QStringLiteral("apply_root:%1").arg(projectPath),
        QStringLiteral("load_overrides"),
        QStringLiteral("sync_docs"),
        QStringLiteral("rebuild_structure"),
        QStringLiteral("refresh_config"),
        QStringLiteral("update_actions"),
        QStringLiteral("ensure_welcome"),
        QStringLiteral("status:Project root set to %1:3000").arg(projectPath),
        QStringLiteral("console:Project root set to %1").arg(projectPath)};
    if (!expect(calls == expected,
                "Open-project controller execution order changed unexpectedly.")) {
        return 1;
    }
    if (!expect(sessionStore.lastProjectPath() == projectPath,
                "Open-project controller should persist project path into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.recentProjectPaths() == QStringList({projectPath}),
                "Open-project controller should record the opened project in recent project history.")) {
        return 1;
    }

    return 0;
}

int runCloseProjectNoProjectOpenTest()
{
    FakeSessionStore sessionStore;
    QStringList calls;
    int confirmCallCount = 0;

    MainWindowProjectController::Actions actions;
    actions.showStatusBarMessage = [&calls](const QString &message, int timeoutMs) {
        calls.append(QStringLiteral("status:%1:%2").arg(message).arg(timeoutMs));
    };
    actions.clearDocumentTabs = [&calls]() { calls.append(QStringLiteral("clear_tabs")); };

    MainWindowProjectController::closeProject(QString(),
                                              [&confirmCallCount]() {
                                                  ++confirmCallCount;
                                                  return true;
                                              },
                                              sessionStore,
                                              actions);

    const QStringList expected = {
        QStringLiteral("status:No project is open:2000")};
    if (!expect(calls == expected,
                "No-project close controller flow changed unexpectedly.")) {
        return 1;
    }
    if (!expect(confirmCallCount == 0,
                "No-project close controller should not ask for dirty-document confirmation.")) {
        return 1;
    }

    return 0;
}

int runCloseProjectSuccessExecutionOrderTest()
{
    FakeSessionStore sessionStore;
    QStringList calls;
    int confirmCallCount = 0;

    MainWindowProjectController::Actions actions;
    actions.setProjectRootPath = [&calls](const QString &projectPath) {
        calls.append(QStringLiteral("set_root:%1").arg(projectPath));
    };
    actions.clearDocumentTabs = [&calls]() { calls.append(QStringLiteral("clear_tabs")); };
    actions.resetProjectBrowser = [&calls]() { calls.append(QStringLiteral("reset_browser")); };
    actions.clearProjectValidationResults = [&calls]() { calls.append(QStringLiteral("clear_validation")); };
    actions.persistOpenDocuments = [&calls]() { calls.append(QStringLiteral("persist_docs")); };
    actions.resetProjectTherionRunContext = [&calls]() { calls.append(QStringLiteral("reset_compiler_project_context")); };
    actions.rebuildStructureSidebar = [&calls]() { calls.append(QStringLiteral("rebuild_structure")); };
    actions.refreshTherionConfigDisplay = [&calls]() { calls.append(QStringLiteral("refresh_config")); };
    actions.updateProjectActionState = [&calls]() { calls.append(QStringLiteral("update_actions")); };
    actions.showStatusBarMessage = [&calls](const QString &message, int timeoutMs) {
        calls.append(QStringLiteral("status:%1:%2").arg(message).arg(timeoutMs));
    };
    actions.appendConsoleLine = [&calls](const QString &line) {
        calls.append(QStringLiteral("console:%1").arg(line));
    };

    MainWindowProjectController::closeProject(QStringLiteral("/tmp/project"),
                                              [&confirmCallCount]() {
                                                  ++confirmCallCount;
                                                  return true;
                                              },
                                              sessionStore,
                                              actions);

    const QStringList expected = {
        QStringLiteral("set_root:"),
        QStringLiteral("clear_tabs"),
        QStringLiteral("reset_browser"),
        QStringLiteral("clear_validation"),
        QStringLiteral("persist_docs"),
        QStringLiteral("reset_compiler_project_context"),
        QStringLiteral("rebuild_structure"),
        QStringLiteral("refresh_config"),
        QStringLiteral("update_actions"),
        QStringLiteral("status:Project closed:3000"),
        QStringLiteral("console:Closed project /tmp/project")};
    if (!expect(calls == expected,
                "Close-project controller execution order changed unexpectedly.")) {
        return 1;
    }
    if (!expect(sessionStore.lastProjectPath().isEmpty(),
                "Close-project controller should clear persisted project path.")) {
        return 1;
    }
    if (!expect(confirmCallCount == 1,
                "Close-project controller should request dirty-document confirmation exactly once.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runOpenProjectCancelledTest() != 0) {
        return 1;
    }
    if (runOpenProjectMissingDirectoryTest() != 0) {
        return 1;
    }
    if (runOpenProjectSuccessExecutionOrderTest() != 0) {
        return 1;
    }
    if (runCloseProjectNoProjectOpenTest() != 0) {
        return 1;
    }
    if (runCloseProjectSuccessExecutionOrderTest() != 0) {
        return 1;
    }

    return 0;
}
