#include "MainWindow.h"

#include <QDir>
#include <QDebug>
#include <QFileDialog>
#include <QPointer>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

#include "MainWindowDocumentHelpers.h"
#include "MainWindowDocumentTabOpenController.h"
#include "../platform/DiagnosticLogging.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"

namespace
{
constexpr auto kWelcomeTabPropertyName = "therionStudioWelcomeTab";

int findWelcomeTabIndex(const QTabWidget *tabs)
{
    if (tabs == nullptr) {
        return -1;
    }

    for (int index = 0; index < tabs->count(); ++index) {
        QWidget *widget = tabs->widget(index);
        if (widget != nullptr && widget->property(kWelcomeTabPropertyName).toBool()) {
            return index;
        }
    }

    return -1;
}

QString defaultSaveDirectory(const QString &projectRootPath)
{
    return projectRootPath.isEmpty() ? QDir::homePath() : projectRootPath;
}

QString saveFilterForDocumentName(const QString &displayName)
{
    const QString lowerName = displayName.trimmed().toLower();
    if (lowerName == QStringLiteral("thconfig")
        || lowerName.startsWith(QStringLiteral("thconfig."))
        || lowerName.endsWith(QStringLiteral(".thconfig"))) {
        return QObject::tr("Therion Config (thconfig *.thconfig);;All Files (*)");
    }
    if (lowerName.endsWith(QStringLiteral(".th2"))) {
        return QObject::tr("Therion Map (*.th2);;All Files (*)");
    }
    return QObject::tr("Therion Source (*.th);;All Files (*)");
}

void removeWelcomeTabIfPresent(QTabWidget *tabs)
{
    const int welcomeTabIndex = findWelcomeTabIndex(tabs);
    if (welcomeTabIndex < 0) {
        return;
    }

    QWidget *welcomeWidget = tabs->widget(welcomeTabIndex);
    tabs->removeTab(welcomeTabIndex);
    if (welcomeWidget != nullptr) {
        welcomeWidget->deleteLater();
    }
}
}

bool MainWindow::isDocumentPathInsideOpenProject(const QString &filePath) const
{
    if (projectRootPath_.trimmed().isEmpty() || filePath.trimmed().isEmpty()) {
        return false;
    }

    const QString relativePath = QDir(projectRootPath_).relativeFilePath(filePath);
    return !relativePath.startsWith(QStringLiteral(".."))
        && !QDir::isAbsolutePath(relativePath);
}

void MainWindow::handleDocumentTextChanged(QWidget *documentWidget)
{
    if (qobject_cast<TherionStudio::MapEditorTab *>(documentWidget) != nullptr) {
        QPointer<QWidget> guardedDocument(documentWidget);
        QTimer::singleShot(0, this, [this, guardedDocument]() {
            if (guardedDocument == nullptr) {
                return;
            }
            if (!projectRootPath_.isEmpty()) {
                requestStructureSidebarRebuild();
            }
            if (currentDocumentWidget() == guardedDocument.data()) {
                rebuildMapObjectsTree();
                refreshWorkspaceModeSwitcher();
            }
        });
        return;
    }

    if (!projectRootPath_.isEmpty()) {
        requestStructureSidebarRebuild();
    }
    const bool canRequestLiveProjectValidation =
        qobject_cast<TherionStudio::MapEditorTab *>(documentWidget) == nullptr;
    const QString documentPath = documentPathForWidget(documentWidget);
    const bool insideOpenProject = isDocumentPathInsideOpenProject(documentPath);
    const bool dirtyDocument = documentIsDirtyForWidget(documentWidget);
    if (canRequestLiveProjectValidation && insideOpenProject) {
        if (TherionStudio::diagnosticLoggingEnabled()) {
            qInfo().noquote()
                << QStringLiteral("project-validation-request-origin trigger=document-changed action=%1 path=\"%2\" dirty=%3 current=%4 tabs=%5")
                       .arg(dirtyDocument ? QStringLiteral("requested") : QStringLiteral("skipped-clean"))
                       .arg(QDir::toNativeSeparators(documentPath))
                       .arg(dirtyDocument ? QStringLiteral("true") : QStringLiteral("false"))
                       .arg(currentDocumentWidget() == documentWidget ? QStringLiteral("true") : QStringLiteral("false"))
                       .arg(editorTabs_ != nullptr ? editorTabs_->count() : 0);
        }
        if (dirtyDocument) {
            requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::DocumentChanged,
                                     false);
        }
    }
    if (currentDocumentWidget() == documentWidget) {
        rebuildMapObjectsTree();
        refreshWorkspaceModeSwitcher();
    }
}

TherionStudio::TextEditorTab *MainWindow::createUntitledTextTab(const QString &suggestedFileName, const QString &contents)
{
    auto *tab = new TherionStudio::TextEditorTab(fileSystem_, commandCatalogStore_);
    tab->setModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    tab->initializeNewDocument(suggestedFileName, contents);

    connect(tab, &TherionStudio::TextEditorTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connect(tab, &TherionStudio::TextEditorTab::dirtyStateChanged, this, [this, tab](bool) {
        updateTabTitle(tab);
        if (!tab->isDirty() && !tab->filePath().isEmpty()) {
            registerDocumentFileWatcher(tab->filePath());
        }
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::TextEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        if (currentDocumentWidget() == tab) {
            handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
        }
    });
    connect(tab, &TherionStudio::TextEditorTab::documentTextChanged, this, [this, tab]() {
        handleDocumentTextChanged(tab);
    });
    connect(tab, &TherionStudio::TextEditorTab::editorModeChanged, this, [this, tab](TherionStudio::TextEditorTab::EditorMode) {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
            refreshViewMenuActions();
        }
    });

    TherionStudio::MainWindowDocumentTabOpenController::Actions openActions;
    openActions.removeWelcomeTabIfPresent = [this]() { removeWelcomeTabIfPresent(editorTabs_); };
    openActions.addTab = [this, tab](const QString &tabTitle) {
        return editorTabs_->addTab(tab, tabTitle);
    };
    openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
    openActions.handleCurrentLineChanged = [this](const QString &, int lineNumber) {
        handleTextEditorCurrentLineChanged(QString(), lineNumber);
    };
    openActions.updateCurrentTabTitle = [this, tab]() { updateTabTitle(tab); };
    openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
    openActions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    openActions.appendConsoleLine = [this](const QString &line) { appendConsoleLine(line); };

    TherionStudio::MainWindowDocumentTabOpenController::AttachNewTabRequest openRequest;
    openRequest.tabTitle = tab->displayName();
    openRequest.currentLineNumber = tab->currentLineNumber();
    openRequest.consoleOpenedLine = tr("Created %1").arg(suggestedFileName);
    if (!TherionStudio::MainWindowDocumentTabOpenController::attachNewTab(openRequest, openActions)) {
        tab->deleteLater();
        return nullptr;
    }
    return tab;
}

TherionStudio::MapEditorTab *MainWindow::createUntitledMapEditorTab(const QString &suggestedFileName, const QString &contents)
{
    auto *tab = new TherionStudio::MapEditorTab(fileSystem_, *sessionStore_, commandCatalogStore_);
    tab->setInlineWorkspaceModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    tab->initializeNewDocument(suggestedFileName, contents);

    connect(tab, &TherionStudio::MapEditorTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::dirtyStateChanged, this, [this, tab](bool) {
        updateTabTitle(tab);
        if (!tab->isDirty() && !tab->filePath().isEmpty()) {
            registerDocumentFileWatcher(tab->filePath());
        }
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::currentLineChanged, this, [this, tab](int lineNumber) {
        if (currentDocumentWidget() == tab) {
            handleTextEditorCurrentLineChanged(tab->filePath(), lineNumber);
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::documentTextChanged, this, [this, tab]() {
        handleDocumentTextChanged(tab);
    });
    connect(tab, &TherionStudio::MapEditorTab::backgroundLayersChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshMapBackgroundPanel();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::backgroundLayerPropertiesChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshMapBackgroundPanel();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::modeStatusChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::zoomStatusChanged, this, [this, tab](int) {
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connectMapEditorTabUiSignals(tab);

    TherionStudio::MainWindowDocumentTabOpenController::Actions openActions;
    openActions.removeWelcomeTabIfPresent = [this]() { removeWelcomeTabIfPresent(editorTabs_); };
    openActions.addTab = [this, tab](const QString &tabTitle) {
        return editorTabs_->addTab(tab, tabTitle);
    };
    openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
    openActions.handleCurrentLineChanged = [this](const QString &, int lineNumber) {
        handleTextEditorCurrentLineChanged(QString(), lineNumber);
    };
    openActions.updateCurrentTabTitle = [this, tab]() { updateTabTitle(tab); };
    openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
    openActions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    openActions.appendConsoleLine = [this](const QString &line) { appendConsoleLine(line); };

    TherionStudio::MainWindowDocumentTabOpenController::AttachNewTabRequest openRequest;
    openRequest.tabTitle = tab->displayName();
    openRequest.currentLineNumber = tab->currentLineNumber();
    openRequest.consoleOpenedLine = tr("Created %1").arg(suggestedFileName);
    if (!TherionStudio::MainWindowDocumentTabOpenController::attachNewTab(openRequest, openActions)) {
        tab->deleteLater();
        return nullptr;
    }
    return tab;
}

QString MainWindow::requestSavePathForDocument(QWidget *documentWidget) const
{
    if (documentWidget == nullptr) {
        return QString();
    }

    QString displayName = documentDisplayNameForWidget(documentWidget);
    while (displayName.startsWith(QLatin1Char('*'))) {
        displayName.remove(0, 1);
    }
    if (displayName.trimmed().isEmpty()) {
        displayName = tr("untitled.th");
    }

    const QString initialPath = QDir(defaultSaveDirectory(projectRootPath_)).absoluteFilePath(displayName);
    return QFileDialog::getSaveFileName(const_cast<MainWindow *>(this),
                                        tr("Save As"),
                                        initialPath,
                                        saveFilterForDocumentName(displayName));
}

bool MainWindow::saveDocumentWidget(QWidget *documentWidget, QString *errorMessage)
{
    if (documentWidget == nullptr || documentWidget->property(kWelcomeTabPropertyName).toBool()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Unable to resolve active document.");
        }
        return false;
    }

    const QString previousPath = documentPathForWidget(documentWidget);
    bool saved = false;
    if (previousPath.isEmpty()) {
        const QString savePath = requestSavePathForDocument(documentWidget);
        if (savePath.isEmpty()) {
            return false;
        }
        if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(documentWidget)) {
            saved = textTab->saveAs(savePath, errorMessage);
        } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(documentWidget)) {
            saved = mapTab->saveAs(savePath, errorMessage);
        }
    } else {
        saved = documentSaveForWidget(documentWidget, errorMessage);
    }

    if (!saved) {
        return false;
    }

    updateTabTitle(documentWidget);
    const QString savedPath = documentPathForWidget(documentWidget);
    if (!savedPath.isEmpty()) {
        registerDocumentFileWatcher(savedPath);
        recordRecentFilePath(savedPath);
        if (isDocumentPathInsideOpenProject(savedPath)) {
            requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::DocumentSaved,
                                     false);
        }
        if (previousPath.isEmpty() || previousPath != savedPath) {
            persistOpenDocuments();
            if (!projectRootPath_.isEmpty()) {
                refreshProjectBrowserView(savedPath);
            }
        }
    }
    return true;
}
