#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDesktopServices>
#include <QDialog>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QFileDialog>
#include <QFont>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QHash>
#include <QGuiApplication>
#include <QScreen>
#include <QSizePolicy>
#include <QSplitter>
#include <QLineEdit>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QKeyEvent>
#include <QKeySequence>
#include <QVariant>
#include <QWidget>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <functional>
#include <utility>
#include <vector>

#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "three_d_viewer/ThreeDViewerTab.h"
#include "MainWindowDocumentHelpers.h"
#include "MainWindowDocumentController.h"
#include "MainWindowDocumentOpenController.h"
#include "MainWindowDocumentTabOpenController.h"
#include "MainWindowHelpDialog.h"
#include "MainWindowSettingsDialog.h"
#include "MainWindowSessionDocumentController.h"
#include "MainWindowSessionController.h"
#include "MainWindowProjectController.h"
#include "MainWindowRecentFilesService.h"
#include "MainWindowRecentProjectsService.h"
#include "MainWindowWelcomeWidget.h"
#include "MainWindowSessionStateService.h"
#include "MainWindowStructureNameOverridesService.h"
#include "ui/ApplicationStyleSheets.h"
#include "LucideIconFactory.h"
#include "../core/PocketTopoImport.h"
#include "../core/SessionStore.h"
#include "../core/TherionFileTypes.h"
#include "../platform/ApplicationLanguageOverride.h"

namespace
{
constexpr const char *kMapEditorUiSignalsConnectedProperty = "_therionStudioMapEditorUiSignalsConnected";
#if defined(Q_OS_MACOS)
constexpr quint32 kMacVirtualKeyAnsi1 = 0x12;
constexpr quint32 kMacVirtualKeyAnsi2 = 0x13;
#endif

QSize minimumMainWindowSize()
{
    return QSize(720, 560);
}

QSize defaultMainWindowSize()
{
    QSize preferredSize(1280, 820);
    const QSize minimumSize = minimumMainWindowSize();

    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QSize availableSize = screen->availableGeometry().size();
        if (availableSize.isValid()) {
            preferredSize.setWidth(qMin(preferredSize.width(),
                                        qMax(minimumSize.width(), availableSize.width() * 4 / 5)));
            preferredSize.setHeight(qMin(preferredSize.height(),
                                         qMax(minimumSize.height(), availableSize.height() * 4 / 5)));
        }
    }

    return preferredSize.expandedTo(minimumSize);
}

QString pocketTopoTextForInsertionScope(const QString &importedText, const QString &scopeToken)
{
    const QString normalizedScope = scopeToken.trimmed().toLower();
    if (normalizedScope != QStringLiteral("centerline") && normalizedScope != QStringLiteral("centreline")) {
        return importedText;
    }

    QStringList bodyLines;
    const QStringList lines = importedText.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString firstToken = line.trimmed().section(QRegularExpression(QStringLiteral("\\s+")), 0, 0).toLower();
        if (firstToken == QStringLiteral("centerline")
            || firstToken == QStringLiteral("centreline")
            || firstToken == QStringLiteral("endcenterline")
            || firstToken == QStringLiteral("endcentreline")) {
            continue;
        }
        bodyLines.append(line);
    }
    return bodyLines.join(QLatin1Char('\n'));
}

void ensureUsableMainWindowSize(QMainWindow *window)
{
    if (window == nullptr) {
        return;
    }

    const QSize minimumSize = minimumMainWindowSize();
    window->setMinimumSize(minimumSize);
    if (window->width() < minimumSize.width() || window->height() < minimumSize.height()) {
        window->resize(window->size().expandedTo(defaultMainWindowSize()));
    }
}

bool isSupportedTextEditorFilePath(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        return false;
    }

    const QString fileName = info.fileName();
    if (TherionStudio::isTherionConfigFileName(fileName)) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("th")
        || suffix == QStringLiteral("txt")
        || suffix == QStringLiteral("log")) {
        return true;
    }

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(filePath, QMimeDatabase::MatchDefault);
    return mimeType.isValid() && mimeType.inherits(QStringLiteral("text/plain"));
}

bool supportsConfigurableDefaultTextEditorMode(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString fileName = info.fileName().trimmed().toLower();
    const QString suffix = info.suffix().trimmed().toLower();
    return TherionStudio::isTherionConfigFileName(fileName)
        || suffix == QStringLiteral("th");
}

bool openFileExternally(QWidget *parent, const QString &filePath)
{
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
        return true;
    }

    QMessageBox::warning(parent,
                         QObject::tr("Open in External App"),
                         QObject::tr("Failed to open %1 with the system default application.")
                             .arg(QDir::toNativeSeparators(filePath)));
    return false;
}

void showUnsupportedFilePrompt(QWidget *parent, const QString &filePath)
{
    QMessageBox messageBox(parent);
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setWindowTitle(QObject::tr("Unsupported File"));
    messageBox.setText(QObject::tr("Unsupported file."));
    messageBox.setInformativeText(
        QObject::tr("Therion Studio cannot open this file type in the internal editor:\n%1")
            .arg(QDir::toNativeSeparators(filePath)));
    auto *openExternalButton = messageBox.addButton(QObject::tr("Open in External App"), QMessageBox::ActionRole);
    messageBox.addButton(QMessageBox::Cancel);
    messageBox.setDefaultButton(QMessageBox::Cancel);
    messageBox.exec();

    if (messageBox.clickedButton() == openExternalButton) {
        openFileExternally(parent, filePath);
    }
}

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

QWidget *createCenteredMessage(const QString &title,
                               const QString &body,
                               const QString &buttonText = QString(),
                               std::function<void()> onButtonClick = {})
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(title);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *bodyLabel = new QLabel(body);
    bodyLabel->setWordWrap(true);

    layout->addStretch(1);

    layout->addWidget(titleLabel);
    layout->addWidget(bodyLabel);
    if (!buttonText.isEmpty()) {
        auto *button = new QPushButton(buttonText);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QObject::connect(button, &QPushButton::clicked, widget, [onButtonClick]() {
            if (onButtonClick) {
                onButtonClick();
            }
        });
        layout->addWidget(button);
    }

    layout->addStretch(1);

    return widget;
}

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setFrameShape(QFrame::NoFrame);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}

QString canonicalDocumentPath(const QString &filePath)
{
    return TherionStudio::MainWindowDocumentOpenController::canonicalDocumentPath(filePath);
}

bool hasOnlyEditorModeShortcutModifier(const QKeyEvent *event)
{
    if (event == nullptr) {
        return false;
    }

    const Qt::KeyboardModifiers relevantModifiers =
        event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
#if defined(Q_OS_MACOS)
    return relevantModifiers == Qt::ControlModifier || relevantModifiers == Qt::MetaModifier;
#else
    return relevantModifiers == Qt::ControlModifier;
#endif
}

int editorModeShortcutIndexForKeyEvent(const QKeyEvent *event)
{
    if (!hasOnlyEditorModeShortcutModifier(event)) {
        return 0;
    }

    if (event->key() == Qt::Key_1) {
        return 1;
    }
    if (event->key() == Qt::Key_2) {
        return 2;
    }

#if defined(Q_OS_MACOS)
    // Use the physical top-row keys above Q/W so localized layouts such as Czech
    // do not require Shift to type the digit character.
    switch (event->nativeVirtualKey()) {
    case kMacVirtualKeyAnsi1:
        return 1;
    case kMacVirtualKeyAnsi2:
        return 2;
    default:
        break;
    }
#endif

    return 0;
}

class DetachedMapWindow final : public QMainWindow
{
public:
    explicit DetachedMapWindow(TherionStudio::MapEditorTab *mapTab, QWidget *parent = nullptr)
        : QMainWindow(parent)
        , mapTab_(mapTab)
    {
        setAttribute(Qt::WA_DeleteOnClose, true);
        setCentralWidget(mapTab);
    }

    TherionStudio::MapEditorTab *mapTab() const
    {
        return mapTab_;
    }

    void setCloseCallback(std::function<void(TherionStudio::MapEditorTab *)> callback)
    {
        closeCallback_ = std::move(callback);
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (closeCallback_) {
            closeCallback_(mapTab_);
        }
        QMainWindow::closeEvent(event);
    }

private:
    QPointer<TherionStudio::MapEditorTab> mapTab_;
    std::function<void(TherionStudio::MapEditorTab *)> closeCallback_;
};

} // namespace

MainWindow::MainWindow(std::unique_ptr<TherionStudio::ISessionStore> sessionStore,
                       TherionStudio::CommandCatalogStore commandCatalogStore,
                       QWidget *parent,
                       SessionRestoreMode restoreMode)
    : MainWindow(*sessionStore, std::move(commandCatalogStore), parent, restoreMode)
{
    ownedSessionStore_ = std::move(sessionStore);
}

MainWindow::MainWindow(TherionStudio::ISessionStore &sessionStore,
                       TherionStudio::CommandCatalogStore commandCatalogStore,
                       QWidget *parent,
                       SessionRestoreMode restoreMode)
    : QMainWindow(parent)
    , editorTabs_(new QTabWidget(this))
    , projectModel_(new QFileSystemModel(this))
    , structureModel_(new QStandardItemModel(this))
    , searchResultsModel_(new QStandardItemModel(this))
    , validationResultsModel_(new QStandardItemModel(this))
    , mapObjectsModel_(new QStandardItemModel(this))
    , documentFileWatcher_(new QFileSystemWatcher(this))
    , projectFileWatcher_(new QFileSystemWatcher(this))
    , sessionStore_(&sessionStore)
    , commandCatalogStore_(std::move(commandCatalogStore))
    , projectSearchScanner_(new TherionStudio::ProjectSearchScanner(this))
    , projectValidationController_(new TherionStudio::ProjectValidationController(this))
    , structureSidebarScanner_(new TherionStudio::ProjectStructureScanner(this))
{
    setWindowTitle(tr("Therion Studio"));
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AnimatedDocks);

    connect(projectSearchScanner_, &TherionStudio::ProjectSearchScanner::searchFinished,
            this, &MainWindow::handleProjectSearchFinished);
    connect(projectValidationController_, &TherionStudio::ProjectValidationController::validationStarted,
            this, &MainWindow::handleProjectValidationStarted);
    connect(projectValidationController_, &TherionStudio::ProjectValidationController::validationFinished,
            this, &MainWindow::handleProjectValidationFinished);
    connect(structureSidebarScanner_, &TherionStudio::ProjectStructureScanner::scanFinished,
            this, &MainWindow::handleStructureSidebarScanFinished);
    connect(documentFileWatcher_, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::handleWatchedDocumentFileChanged);
    connect(documentFileWatcher_, &QFileSystemWatcher::directoryChanged,
            this, &MainWindow::handleWatchedDocumentDirectoryChanged);
    connect(projectFileWatcher_, &QFileSystemWatcher::directoryChanged,
            this, &MainWindow::handleProjectDirectoryChanged);
    connect(projectFileWatcher_, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::handleProjectFileChanged);

    buildUi();
    qApp->installEventFilter(this);
    setMinimumSize(minimumMainWindowSize());
    resize(defaultMainWindowSize());
    if (restoreMode == SessionRestoreMode::RestoreSession) {
        restoreSessionState();
        requestRestoredProjectValidation();
    }

    if (editorTabs_->count() == 0) {
        addWelcomeTab();
    }

    rebuildStructureSidebar();
    rebuildMapObjectsTree();
    updateMapEditorActionState();
    restoreSidebarWidth();
}

void MainWindow::buildUi()
{
    editorTabs_->setObjectName(QStringLiteral("mainEditorTabs"));
    editorTabs_->setTabsClosable(true);
    editorTabs_->setMovable(true);
    editorTabs_->setDocumentMode(true);
    connect(editorTabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::handleTabCloseRequested);
    connect(editorTabs_, &QTabWidget::currentChanged, this, [this](int) {
        rebuildMapObjectsTree();
        updateMapEditorActionState();
        refreshFileImportActions();
        refreshTherionConfigDisplay();
        refreshMapBackgroundPanel();
        refreshDocumentStatusWidgets();
        refreshWorkspaceModeSwitcher();
        refreshWorkspaceModeSwitcherGeometry();
        refreshViewMenuActions();
    });

    auto *mainContentHost = new QWidget(this);
    mainContentLayout_ = new QHBoxLayout(mainContentHost);
    mainContentLayout_->setContentsMargins(0, 0, 0, 0);
    mainContentLayout_->setSpacing(0);
    mainContentSplitter_ = new QSplitter(Qt::Horizontal, mainContentHost);
    applyThinSplitterStyle(mainContentSplitter_, QStringLiteral("mainContentSplitter"));
    mainContentSplitter_->setChildrenCollapsible(true);
    setCentralWidget(mainContentHost);

    editorAreaHost_ = new QWidget(mainContentSplitter_);
    editorAreaHost_->setObjectName(QStringLiteral("mainEditorArea"));
    editorAreaHost_->setAttribute(Qt::WA_StyledBackground, true);
    editorAreaHost_->setStyleSheet(TherionStudio::mainEditorSurfaceStyleSheet());
    auto *editorAreaHostLayout = new QHBoxLayout(editorAreaHost_);
    editorAreaHostLayout->setContentsMargins(0, 0, 0, 0);
    editorAreaHostLayout->setSpacing(0);

    auto *editorAreaDivider = new QFrame(editorAreaHost_);
    editorAreaDivider->setObjectName(QStringLiteral("mainEditorAreaLeftDivider"));
    editorAreaDivider->setFixedWidth(1);
    editorAreaDivider->setFrameShape(QFrame::NoFrame);

    editorAreaColumn_ = new QWidget(editorAreaHost_);
    editorAreaColumn_->setObjectName(QStringLiteral("mainEditorAreaColumn"));
    editorAreaColumn_->setAttribute(Qt::WA_StyledBackground, true);
    editorAreaHostLayout->addWidget(editorAreaDivider);
    editorAreaHostLayout->addWidget(editorAreaColumn_, 1);

    editorAreaLayout_ = new QVBoxLayout(editorAreaColumn_);
    editorAreaLayout_->setContentsMargins(0, 0, 0, 0);
    editorAreaLayout_->setSpacing(0);
    editorAreaLayout_->addWidget(editorTabs_, 1);

    buildStructureSidebar();
    mainContentSplitter_->addWidget(editorAreaHost_);
    mainContentLayout_->addWidget(mainContentSplitter_, 1);
    mainContentSplitter_->setCollapsible(0, true);
    mainContentSplitter_->setCollapsible(1, false);
    mainContentSplitter_->setStretchFactor(0, 0);
    mainContentSplitter_->setStretchFactor(1, 1);
    buildConsole();
    buildMenus();
    initializeWorkspaceModeSwitcher();

    initializeDocumentStatusWidgets();
    refreshWorkspaceModeSwitcher();
    refreshWorkspaceModeSwitcherGeometry();
    QTimer::singleShot(0, this, [this]() {
        refreshWorkspaceModeSwitcherGeometry();
    });
}


void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *newWindowAction = fileMenu->addAction(tr("New &Window"));
    newWindowAction->setMenuRole(QAction::NoRole);
    newWindowAction->setShortcut(QKeySequence::New);
    connect(newWindowAction, &QAction::triggered, this, &MainWindow::createNewWindow);

    newProjectMenu_ = fileMenu->addMenu(tr("New Project"));
    projectFromTemplateAction_ = newProjectMenu_->addAction(tr("Project from Template..."));
    connect(projectFromTemplateAction_, &QAction::triggered, this, &MainWindow::createProjectFromDefaultTemplate);
    emptyProjectAction_ = newProjectMenu_->addAction(tr("Empty Project..."));
    connect(emptyProjectAction_, &QAction::triggered, this, &MainWindow::createEmptyProject);

    newFileMenu_ = fileMenu->addMenu(tr("New File"));
    newTherionSourceAction_ = newFileMenu_->addAction(tr("Therion Source (.th)"));
    connect(newTherionSourceAction_, &QAction::triggered, this, &MainWindow::createNewTherionSourceDocument);
    newTherionMapAction_ = newFileMenu_->addAction(tr("Therion Map (.th2)"));
    connect(newTherionMapAction_, &QAction::triggered, this, &MainWindow::createNewTherionMapDocument);
    newTherionConfigAction_ = newFileMenu_->addAction(tr("Therion Config (.thconfig)"));
    connect(newTherionConfigAction_, &QAction::triggered, this, &MainWindow::createNewTherionConfigDocument);

    fileMenu->addSeparator();

    openProjectAction_ = fileMenu->addAction(tr("&Open Project..."));
    openProjectAction_->setShortcut(QKeySequence::Open);
    connect(openProjectAction_, &QAction::triggered, this, &MainWindow::openProject);

    recentProjectsMenu_ = fileMenu->addMenu(tr("Recent Projects"));
    recentFilesMenu_ = fileMenu->addMenu(tr("Recent Files"));

    fileMenu->addSeparator();

    importMenu_ = fileMenu->addMenu(tr("Import"));
    importPocketTopoAction_ = importMenu_->addAction(tr("Import PocketTopo Text..."));
    connect(importPocketTopoAction_, &QAction::triggered, this, &MainWindow::importPocketTopoTextToActiveEditor);

    fileMenu->addSeparator();

    saveAction_ = fileMenu->addAction(tr("&Save"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, &MainWindow::saveActiveDocument);

    saveAllAction_ = fileMenu->addAction(tr("Save &All"));
    saveAllAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAllAction_, &QAction::triggered, this, &MainWindow::saveAllDocuments);

    fileMenu->addSeparator();

    closeProjectAction_ = fileMenu->addAction(tr("&Close Project"));
    connect(closeProjectAction_, &QAction::triggered, this, &MainWindow::closeProject);

    closeTabAction_ = fileMenu->addAction(tr("Close Tab"));
    connect(closeTabAction_, &QAction::triggered, this, &MainWindow::closeActiveTab);

    closeAllTabsAction_ = fileMenu->addAction(tr("Close All Tabs"));
    closeAllTabsAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    connect(closeAllTabsAction_, &QAction::triggered, this, &MainWindow::closeAllTabs);

    fileMenu->addSeparator();
    QAction *settingsAction = fileMenu->addAction(tr("Settings..."));
    settingsAction->setMenuRole(QAction::PreferencesRole);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettingsDialog);

    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    undoAction_ = editMenu->addAction(tr("&Undo"));
    undoAction_->setShortcuts(QKeySequence::Undo);
    undoAction_->setStatusTip(tr("Undo the last change in the active document."));
    connect(undoAction_, &QAction::triggered, this, &MainWindow::triggerUndoForActiveDocument);

    redoAction_ = editMenu->addAction(tr("&Redo"));
    redoAction_->setShortcuts(QKeySequence::Redo);
    redoAction_->setStatusTip(tr("Redo the last undone change in the active document."));
    connect(redoAction_, &QAction::triggered, this, &MainWindow::triggerRedoForActiveDocument);
    editMenu->addSeparator();

    QAction *findAction = editMenu->addAction(tr("&Find"));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() { showFindBar(false); });

    QAction *projectSearchAction = editMenu->addAction(tr("Search in Project"));
    projectSearchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    projectSearchAction->setShortcutContext(Qt::WindowShortcut);
    connect(projectSearchAction, &QAction::triggered, this, [this]() {
        setSidebarPane(SidebarPane::Search);
        if (isSidebarEffectivelyCollapsed()) {
            setSidebarCollapsed(false);
        }
        if (projectSearchEdit_ != nullptr) {
            projectSearchEdit_->setFocus();
            projectSearchEdit_->selectAll();
        }
    });

    QAction *replaceAction = editMenu->addAction(tr("Find and &Replace"));
    replaceAction->setShortcut(QKeySequence::Replace);
    connect(replaceAction, &QAction::triggered, this, [this]() { showFindBar(true); });

    auto *rawModeShortcut = new QAction(this);
    rawModeShortcut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    rawModeShortcut->setShortcutContext(Qt::WindowShortcut);
    addAction(rawModeShortcut);
    connect(rawModeShortcut, &QAction::triggered, this, &MainWindow::triggerRawModeForActiveDocument);

    auto *secondaryModeShortcut = new QAction(this);
    secondaryModeShortcut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    secondaryModeShortcut->setShortcutContext(Qt::WindowShortcut);
    addAction(secondaryModeShortcut);
    connect(secondaryModeShortcut, &QAction::triggered, this, &MainWindow::triggerSecondaryEditorModeForActiveDocument);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    sidebarCollapseAction_ = viewMenu->addAction(QString());
    sidebarCollapseAction_->setStatusTip(tr("Expand or collapse the left sidebar."));
    connect(sidebarCollapseAction_, &QAction::triggered, this, [this]() {
        setSidebarCollapsed(!isSidebarEffectivelyCollapsed());
    });

    rightPanelCollapseAction_ = viewMenu->addAction(QString());
    rightPanelCollapseAction_->setStatusTip(tr("Expand or collapse the active document right-side panel."));
    connect(rightPanelCollapseAction_, &QAction::triggered, this, [this]() {
        setCurrentDocumentRightPanelCollapsed(!currentDocumentRightPanelCollapsed());
    });
    contextHelpCollapseAction_ = viewMenu->addAction(QString());
    contextHelpCollapseAction_->setStatusTip(tr("Expand or collapse the help panel while the map pane is detached."));
    connect(contextHelpCollapseAction_, &QAction::triggered, this, [this]() {
        setCurrentDetachedMapContextHelpCollapsed(!currentDetachedMapContextHelpCollapsed());
    });

    viewMenu->addSeparator();
    mapMagnifierAction_ = viewMenu->addAction(QString());
    mapMagnifierAction_->setData(sessionStore_ == nullptr || sessionStore_->therionMapMagnifierEnabled());
    mapMagnifierAction_->setStatusTip(tr("Show or hide the map magnifier overlay in map editors."));
    connect(mapMagnifierAction_, &QAction::triggered, this, [this]() {
        const bool enabled = !mapMagnifierAction_->data().toBool();
        mapMagnifierAction_->setData(enabled);
        if (sessionStore_ != nullptr) {
            sessionStore_->setTherionMapMagnifierEnabled(enabled);
        }
        setMapMagnifierEnabledForOpenTabs(enabled);
        refreshViewMenuActions();
    });

#if !defined(Q_OS_MACOS)
    viewMenu->addSeparator();
    fullScreenAction_ = viewMenu->addAction(QString());
    fullScreenAction_->setMenuRole(QAction::NoRole);
    fullScreenAction_->setShortcut(QKeySequence::FullScreen);
    fullScreenAction_->setStatusTip(tr("Enter or exit full screen mode."));
    connect(fullScreenAction_, &QAction::triggered, this, [this]() {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
        refreshFullScreenAction();
    });
#endif

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *userManualAction = helpMenu->addAction(tr("User Manual"));
    connect(userManualAction, &QAction::triggered, this, [this]() {
        TherionStudio::showUserManualDialog(this);
    });
    helpMenu->addSeparator();
    QAction *aboutAction = helpMenu->addAction(tr("About Therion Studio"));
#if defined(Q_OS_MACOS)
    aboutAction->setMenuRole(QAction::AboutRole);
#else
    aboutAction->setMenuRole(QAction::NoRole);
#endif
    connect(aboutAction, &QAction::triggered, this, [this]() {
        TherionStudio::showAboutDialog(this);
    });

    updateProjectActionState();
    refreshFileImportActions();
    refreshViewMenuActions();
}

void MainWindow::restoreSessionState()
{
    TherionStudio::MainWindowSessionController::RestoreSessionActions actions;
    actions.restoreGeometry = [this](const QByteArray &geometry) {
        return restoreGeometry(geometry);
    };
    actions.restoreState = [this](const QByteArray &state) {
        restoreState(state);
    };
    actions.resizeToDefault = [this]() {
        resize(defaultMainWindowSize());
    };
    actions.ensureUsableWindowSize = [this]() {
        ensureUsableMainWindowSize(this);
    };
    actions.applyProjectRootToBrowser = [this](const QString &projectPath) {
        projectRootPath_ = projectPath;
        rebuildProjectFileWatcher();
        refreshProjectBrowserView();
    };
    actions.appendConsoleLine = [this](const QString &line) {
        appendConsoleLine(line);
    };
    actions.loadStructureNameOverrides = [this]() { loadStructureNameOverrides(); };
    actions.syncOpenDocumentsToProjectRoot = [this]() { syncOpenDocumentsToProjectRoot(); };
    actions.rebuildStructureSidebar = [this]() { rebuildStructureSidebar(); };
    actions.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    actions.updateProjectActionState = [this]() { updateProjectActionState(); };
    actions.restoreOpenDocuments = [this]() { restoreOpenDocuments(); };
    TherionStudio::MainWindowSessionController::restoreSession(*sessionStore_, actions);
}

void MainWindow::persistSessionState()
{
    TherionStudio::MainWindowSessionStateService::MainWindowStateSnapshot snapshot;
    snapshot.windowGeometry = saveGeometry();
    snapshot.windowState = saveState();
    snapshot.projectRootPath = projectRootPath_;
    snapshot.therionExecutablePath = therionExecutableInput();
    snapshot.therionWorkingDirectory = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text().trimmed() : QString();
    snapshot.therionRunTargetMode = therionRunTargetMode();
    snapshot.therionTargetConfigPath = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text().trimmed() : QString();
    snapshot.structureNameOverridesJson =
        TherionStudio::MainWindowStructureNameOverridesService::serialize(structureNameOverrides_);
    TherionStudio::MainWindowSessionStateService::persistMainWindowState(*sessionStore_, snapshot);
    persistOpenDocuments();
}

void MainWindow::restoreOpenDocuments()
{
    TherionStudio::MainWindowSessionDocumentController::RestoreOpenDocumentsActions actions;
    actions.openMapEditorDocument = [this](const QString &documentPath) {
        openMapEditorTab(documentPath, false);
    };
    actions.openTextEditorDocument = [this](const QString &documentPath) {
        return openTextTab(documentPath, false, false) != nullptr;
    };
    actions.appendSkippedUnsupportedDocumentConsole = [this](const QString &documentPath) {
        appendConsoleLine(tr("Skipped unsupported document during session restore: %1")
                              .arg(QDir::toNativeSeparators(documentPath)));
    };
    actions.activateRestoredDocumentPath = [this](const QString &activeDocumentPath) {
        QWidget *activeWidget = documentWidgetForFilePath(activeDocumentPath);
        if (activeWidget != nullptr) {
            const int tabIndex = editorTabs_->indexOf(activeWidget);
            if (tabIndex >= 0) {
                editorTabs_->setCurrentWidget(activeWidget);
                if (documentPathForWidget(activeWidget).isEmpty()) {
                    editorTabs_->setCurrentIndex(editorTabs_->count() - 1);
                }
            } else {
                focusDetachedMapEditorTab(canonicalDocumentPath(activeDocumentPath));
            }
        }
    };
    actions.persistOpenDocuments = [this]() {
        persistOpenDocuments();
    };
    TherionStudio::MainWindowSessionDocumentController::restoreOpenDocuments(*sessionStore_, actions);
}

void MainWindow::persistOpenDocuments()
{
    TherionStudio::MainWindowSessionDocumentController::PersistOpenDocumentsSnapshot snapshot;

    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        const QString filePath = documentPathForWidget(tabWidget);
        if (filePath.isEmpty()) {
            continue;
        }

        snapshot.tabDocumentPaths.append(filePath);
    }

    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab == nullptr) {
            continue;
        }
        const QString filePath = documentPathForWidget(detachedTab);
        if (filePath.isEmpty()) {
            continue;
        }
        snapshot.detachedMapDocumentPaths.append(filePath);
    }

    for (auto iterator = detachedMapWindowsByPath_.constBegin(); iterator != detachedMapWindowsByPath_.constEnd(); ++iterator) {
        QMainWindow *window = iterator.value();
        if (window != nullptr && window->isActiveWindow()) {
            snapshot.activeDetachedDocumentPaths.append(iterator.key());
            break;
        }
    }
    QWidget *currentWidget = currentDocumentWidget();
    snapshot.currentDocumentPath = currentWidget != nullptr ? documentPathForWidget(currentWidget) : QString();

    TherionStudio::MainWindowSessionDocumentController::persistOpenDocuments(snapshot, *sessionStore_);
}

void MainWindow::addWelcomeTab()
{
    QWidget *welcomeWidget = nullptr;
    if (projectRootPath_.isEmpty()) {
        welcomeWidget = TherionStudio::createMainWindowProjectWelcomeWidget(
            tr("Therion Studio"),
            tr("Open or create a project to begin working with Therion documents, maps, and structure views."),
            tr("Open Existing Project..."),
            [this]() {
                openProject();
            },
            tr("New Empty Project"),
            [this]() {
                createEmptyProject();
            },
            tr("New Project from Template..."),
            [this]() {
                createProjectFromDefaultTemplate();
            },
            sessionStore_ != nullptr ? sessionStore_->recentProjectPaths() : QStringList(),
            [this](const QString &projectPath) {
                openProjectPath(projectPath);
            });
    } else {
        welcomeWidget = TherionStudio::createMainWindowActiveProjectWelcomeWidget(
            tr("Therion Studio"),
            projectRootPath_,
            tr("Open file from sidebar to begin editing this project."),
            sessionStore_ != nullptr ? sessionStore_->recentFilePathsForProject(projectRootPath_) : QStringList(),
            [this](const QString &filePath) {
                openProjectFilePath(filePath);
            });
    }
    welcomeWidget->setProperty(kWelcomeTabPropertyName, true);

    const int existingWelcomeIndex = findWelcomeTabIndex(editorTabs_);
    if (existingWelcomeIndex >= 0) {
        const bool wasCurrent = editorTabs_->currentIndex() == existingWelcomeIndex;
        QWidget *existingWidget = editorTabs_->widget(existingWelcomeIndex);
        editorTabs_->removeTab(existingWelcomeIndex);
        if (existingWidget != nullptr) {
            existingWidget->deleteLater();
        }
        editorTabs_->insertTab(existingWelcomeIndex, welcomeWidget, tr("Welcome"));
        if (wasCurrent) {
            editorTabs_->setCurrentIndex(existingWelcomeIndex);
        }
    } else {
        editorTabs_->addTab(welcomeWidget, tr("Welcome"));
    }

    refreshDocumentStatusWidgets();
    refreshWorkspaceModeSwitcher();
}

void MainWindow::createNewWindow()
{
    auto sessionStore = std::make_unique<TherionStudio::InMemorySessionStore>();
    if (sessionStore_ != nullptr) {
        sessionStore->setApplicationLanguage(sessionStore_->applicationLanguage());
        sessionStore->setDefaultTextEditorMode(sessionStore_->defaultTextEditorMode());
        sessionStore->setTherionExecutablePath(sessionStore_->therionExecutablePath());
        sessionStore->setTherionRunTargetMode(sessionStore_->therionRunTargetMode());
        sessionStore->setTherionMapMagnifierEnabled(sessionStore_->therionMapMagnifierEnabled());
        sessionStore->setTherionMapObjectsAutoCollapseExpandScrapsEnabled(
            sessionStore_->therionMapObjectsAutoCollapseExpandScrapsEnabled());
        sessionStore->setLastProjectParentDirectory(sessionStore_->lastProjectParentDirectory());
        sessionStore->setRecentProjectPaths(sessionStore_->recentProjectPaths());
    }

    auto *window = new MainWindow(std::move(sessionStore),
                                  commandCatalogStore_,
                                  nullptr,
                                  SessionRestoreMode::StartEmpty);
    window->show();
}

void MainWindow::showSettingsDialog()
{
    if (sessionStore_ == nullptr) {
        return;
    }

    TherionStudio::MainWindowSettingsDialog::Settings initialSettings;
    const QString nativeApplicationLanguage =
        TherionStudio::Platform::applicationLanguageOverride();
    initialSettings.applicationLanguage = nativeApplicationLanguage != QStringLiteral("system")
        ? nativeApplicationLanguage
        : sessionStore_->applicationLanguage();
    initialSettings.therionExecutablePath = sessionStore_->therionExecutablePath();
    initialSettings.defaultTextEditorMode = sessionStore_->defaultTextEditorMode();

    TherionStudio::MainWindowSettingsDialog dialog(initialSettings, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const TherionStudio::MainWindowSettingsDialog::Settings updatedSettings = dialog.settings();
    const QString updatedApplicationLanguage =
        TherionStudio::Platform::normalizeApplicationLanguageSetting(
            updatedSettings.applicationLanguage);
    const bool languageChanged =
        updatedApplicationLanguage
        != TherionStudio::Platform::normalizeApplicationLanguageSetting(
            initialSettings.applicationLanguage);

    sessionStore_->setApplicationLanguage(updatedApplicationLanguage);
    TherionStudio::Platform::setApplicationLanguageOverride(updatedApplicationLanguage);
    sessionStore_->setTherionExecutablePath(updatedSettings.therionExecutablePath);
    sessionStore_->setDefaultTextEditorMode(updatedSettings.defaultTextEditorMode);
    refreshTherionConfigDisplay();

    if (languageChanged) {
        QMessageBox::information(this,
                                 tr("Settings"),
                                 tr("Language changes will take effect after restarting Therion Studio."));
    }
}

void MainWindow::openProject()
{
    const QString selectedProjectPath =
        QFileDialog::getExistingDirectory(this, tr("Open Therion Project"), QDir::homePath());

    openProjectPath(selectedProjectPath);
}

void MainWindow::openProjectPath(const QString &selectedProjectPath)
{
    const QString previousProjectRootPath = projectRootPath_;

    TherionStudio::MainWindowProjectController::Actions actions;
    actions.showWarningDialog = [this](const QString &title, const QString &message) {
        QMessageBox::warning(this, title, message);
    };
    actions.showStatusBarMessage = [this](const QString &message, int timeoutMs) {
        statusBar()->showMessage(message, timeoutMs);
    };
    actions.appendConsoleLine = [this](const QString &line) {
        appendConsoleLine(line);
    };
    actions.setProjectRootPath = [this](const QString &projectRootPath) {
        projectRootPath_ = projectRootPath;
        rebuildProjectFileWatcher();
        if (projectRootPath_.isEmpty()) {
            clearValidationRailIndicator();
        }
    };
    actions.applyProjectRootToBrowser = [this](const QString &projectRootPath) {
        Q_UNUSED(projectRootPath);
        refreshProjectBrowserView();
    };
    actions.loadStructureNameOverrides = [this]() { loadStructureNameOverrides(); };
    actions.syncOpenDocumentsToProjectRoot = [this]() { syncOpenDocumentsToProjectRoot(); };
    actions.rebuildStructureSidebar = [this]() { rebuildStructureSidebar(); };
    actions.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    actions.updateProjectActionState = [this]() { updateProjectActionState(); };
    actions.ensureWelcomeTab = [this]() { addWelcomeTab(); };
    TherionStudio::MainWindowProjectController::openProject(selectedProjectPath,
                                                            editorTabs_->count() == 0,
                                                            findWelcomeTabIndex(editorTabs_) >= 0,
                                                            *sessionStore_,
                                                            actions);
    if (projectRootPath_ != previousProjectRootPath
        && !projectRootPath_.trimmed().isEmpty()
        && QDir(projectRootPath_).exists()) {
        requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::ProjectOpened, false);
    }
}

void MainWindow::closeProject()
{
    TherionStudio::MainWindowProjectController::Actions actions;
    actions.showStatusBarMessage = [this](const QString &message, int timeoutMs) {
        statusBar()->showMessage(message, timeoutMs);
    };
    actions.appendConsoleLine = [this](const QString &line) {
        appendConsoleLine(line);
    };
    actions.setProjectRootPath = [this](const QString &projectRootPath) {
        projectRootPath_ = projectRootPath;
        rebuildProjectFileWatcher();
        if (projectRootPath_.isEmpty()) {
            clearValidationRailIndicator();
        }
    };
    actions.clearDocumentTabs = [this]() { clearDocumentTabs(); };
    actions.resetProjectBrowser = [this]() { resetProjectBrowser(); };
    actions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    actions.resetProjectTherionRunContext = [this]() { resetProjectTherionRunContext(); };
    actions.rebuildStructureSidebar = [this]() { rebuildStructureSidebar(); };
    actions.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    actions.updateProjectActionState = [this]() { updateProjectActionState(); };
    TherionStudio::MainWindowProjectController::closeProject(projectRootPath_,
                                                             [this]() { return confirmCloseDirtyDocuments(); },
                                                             *sessionStore_,
                                                             actions);
}

void MainWindow::refreshRecentProjectsUi()
{
    if (recentProjectsMenu_ == nullptr) {
        return;
    }

    recentProjectsMenu_->clear();

    const QStringList recentProjectPaths =
        sessionStore_ != nullptr
        ? TherionStudio::MainWindowRecentProjectsService::normalizedRecentProjectPaths(
              sessionStore_->recentProjectPaths())
        : QStringList();
    const bool hasOpenProject = !projectRootPath_.trimmed().isEmpty() && QDir(projectRootPath_).exists();
    recentProjectsMenu_->setEnabled(!hasOpenProject && !recentProjectPaths.isEmpty());

    if (recentProjectPaths.isEmpty()) {
        QAction *emptyAction = recentProjectsMenu_->addAction(tr("No Recent Projects"));
        emptyAction->setEnabled(false);
        return;
    }

    for (int index = 0; index < recentProjectPaths.size(); ++index) {
        const QString projectPath = recentProjectPaths.at(index);
        QAction *recentAction = recentProjectsMenu_->addAction(
            QStringLiteral("&%1 %2").arg(index + 1).arg(QDir::toNativeSeparators(projectPath)));
        recentAction->setStatusTip(QDir::toNativeSeparators(projectPath));
        connect(recentAction, &QAction::triggered, this, [this, projectPath]() {
            openProjectPath(projectPath);
        });
    }
}

void MainWindow::refreshRecentFilesUi()
{
    if (recentFilesMenu_ == nullptr) {
        return;
    }

    recentFilesMenu_->clear();

    const bool hasOpenProject = !projectRootPath_.trimmed().isEmpty() && QDir(projectRootPath_).exists();
    const QStringList recentFilePaths =
        hasOpenProject && sessionStore_ != nullptr
        ? TherionStudio::MainWindowRecentFilesService::normalizedRecentFilePaths(
              projectRootPath_,
              sessionStore_->recentFilePathsForProject(projectRootPath_))
        : QStringList();
    recentFilesMenu_->setEnabled(hasOpenProject);

    if (!hasOpenProject) {
        QAction *emptyAction = recentFilesMenu_->addAction(tr("No Recent Files"));
        emptyAction->setEnabled(false);
        return;
    }

    if (recentFilePaths.isEmpty()) {
        QAction *emptyAction = recentFilesMenu_->addAction(tr("No Recent Files"));
        emptyAction->setEnabled(false);
        return;
    }

    for (int index = 0; index < recentFilePaths.size(); ++index) {
        const QString filePath = recentFilePaths.at(index);
        QAction *recentAction = recentFilesMenu_->addAction(
            QStringLiteral("&%1 %2")
                .arg(index + 1)
                .arg(TherionStudio::MainWindowRecentFilesService::projectRelativeDisplayPath(projectRootPath_, filePath)));
        recentAction->setStatusTip(QDir::toNativeSeparators(filePath));
        connect(recentAction, &QAction::triggered, this, [this, filePath]() {
            openProjectFilePath(filePath);
        });
    }
}

void MainWindow::recordRecentFilePath(const QString &filePath)
{
    if (sessionStore_ == nullptr || projectRootPath_.trimmed().isEmpty()) {
        return;
    }

    sessionStore_->setRecentFilePathsForProject(
        projectRootPath_,
        TherionStudio::MainWindowRecentFilesService::recordOpenedFile(
            projectRootPath_,
            sessionStore_->recentFilePathsForProject(projectRootPath_),
            filePath));
    refreshRecentFilesUi();
}

void MainWindow::importPocketTopoTextToActiveEditor()
{
    auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(currentDocumentWidget());
    if (textTab == nullptr) {
        return;
    }

    QString documentName = textTab->filePath().isEmpty() ? textTab->displayName() : textTab->filePath();
    while (documentName.startsWith(QLatin1Char('*'))) {
        documentName.remove(0, 1);
    }
    if (QFileInfo(documentName).suffix().compare(QStringLiteral("th"), Qt::CaseInsensitive) != 0) {
        return;
    }

    const QString initialDirectory = textTab->filePath().isEmpty()
        ? (projectRootPath_.isEmpty() ? QDir::homePath() : projectRootPath_)
        : QFileInfo(textTab->filePath()).absolutePath();
    const QString importPath = QFileDialog::getOpenFileName(this,
                                                            tr("Import PocketTopo Text"),
                                                            initialDirectory,
                                                            tr("PocketTopo Therion Export (*.txt *.TXT);;All Files (*)"));
    if (importPath.isEmpty()) {
        return;
    }

    QFile file(importPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             tr("Import PocketTopo Text"),
                             tr("Could not read %1.").arg(QDir::toNativeSeparators(importPath)));
        return;
    }

    const QString importedText =
        TherionStudio::convertPocketTopoTextToTherionCentreline(QString::fromUtf8(file.readAll()));
    if (importedText.trimmed().isEmpty()) {
        QMessageBox::warning(this,
                             tr("Import PocketTopo Text"),
                             tr("No PocketTopo centreline data was found in %1.")
                                 .arg(QDir::toNativeSeparators(importPath)));
        return;
    }

    QString insertionText =
        pocketTopoTextForInsertionScope(importedText, textTab->importInsertionScopeToken());
    if (!insertionText.endsWith(QLatin1Char('\n'))) {
        insertionText.append(QLatin1Char('\n'));
    }

    if (!textTab->insertTextAtImportInsertionPoint(insertionText)) {
        statusBar()->showMessage(tr("PocketTopo import skipped: document changed."), 5000);
        return;
    }
    statusBar()->showMessage(tr("Imported PocketTopo centreline data from %1.")
                                 .arg(QFileInfo(importPath).fileName()),
                             5000);
}

void MainWindow::refreshFileImportActions()
{
    auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(currentDocumentWidget());
    QString documentName;
    if (textTab != nullptr) {
        documentName = textTab->filePath().isEmpty() ? textTab->displayName() : textTab->filePath();
        while (documentName.startsWith(QLatin1Char('*'))) {
            documentName.remove(0, 1);
        }
    }

    const bool showPocketTopoImport =
        QFileInfo(documentName).suffix().compare(QStringLiteral("th"), Qt::CaseInsensitive) == 0;
    if (importPocketTopoAction_ != nullptr) {
        importPocketTopoAction_->setEnabled(showPocketTopoImport);
    }
    if (importMenu_ != nullptr) {
        importMenu_->setEnabled(showPocketTopoImport);
    }
}

void MainWindow::createNewTherionSourceDocument()
{
    const QString fileName = tr("untitled.th");
    createUntitledTextTab(fileName, QString::fromUtf8(TherionStudio::initialTherionProjectFileContents(fileName)));
}

void MainWindow::createNewTherionMapDocument()
{
    const QString fileName = tr("untitled.th2");
    createUntitledMapEditorTab(fileName, QString::fromUtf8(TherionStudio::initialTherionProjectFileContents(fileName)));
}

void MainWindow::createNewTherionConfigDocument()
{
    const QString fileName = tr("untitled.thconfig");
    createUntitledTextTab(fileName, QString::fromUtf8(TherionStudio::initialTherionProjectFileContents(fileName)));
}

void MainWindow::handleTextEditorCurrentLineChanged(const QString &filePath, int lineNumber)
{
    updateStructureSelectionFromEditorLocation(filePath, lineNumber);
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr
        && TherionStudio::MainWindowDocumentOpenController::canonicalDocumentPath(mapTab->filePath())
            == TherionStudio::MainWindowDocumentOpenController::canonicalDocumentPath(filePath)) {
        updateMapObjectSelectionFromEditorLocation(filePath, lineNumber);
    }
}

void MainWindow::handleTabCloseRequested(int index)
{
    TherionStudio::MainWindowDocumentController::Actions actions;
    actions.confirmCloseTab = [this](int tabIndex) { return confirmCloseTab(tabIndex); };
    actions.closeTab = [this](int tabIndex) {
        QWidget *tabWidget = editorTabs_->widget(tabIndex);
        if (tabWidget == nullptr) {
            return false;
        }

        const QString documentPath = documentPathForWidget(tabWidget);
        editorTabs_->removeTab(tabIndex);
        unregisterDocumentFileWatcherIfUnused(documentPath);
        tabWidget->deleteLater();
        return true;
    };
    actions.hasNoAttachedTabs = [this]() { return editorTabs_->count() == 0; };
    actions.ensureWelcomeTab = [this]() { addWelcomeTab(); };
    actions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    actions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    TherionStudio::MainWindowDocumentController::CloseTabRequest request;
    request.tabIndex = index;
    TherionStudio::MainWindowDocumentController::closeTab(request, actions);
}

void MainWindow::closeActiveTab()
{
    const int currentIndex = editorTabs_->currentIndex();
    if (currentIndex < 0) {
        return;
    }

    handleTabCloseRequested(currentIndex);
}

void MainWindow::closeAllTabs()
{
    TherionStudio::MainWindowDocumentController::CloseAllRequest request;
    for (int index = editorTabs_->count() - 1; index >= 0; --index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (tabWidget == nullptr) {
            continue;
        }

        const QString documentPath = documentPathForWidget(tabWidget);
        if (documentPath.isEmpty() && tabWidget->property(kWelcomeTabPropertyName).toBool()) {
            continue;
        }

        TherionStudio::MainWindowDocumentController::CloseTabEntry entry;
        entry.tabIndex = index;
        entry.documentPath = documentPath.isEmpty() ? documentDisplayNameForWidget(tabWidget) : documentPath;
        request.tabEntries.append(entry);
    }

    const QList<TherionStudio::MapEditorTab *> detachedTabs = detachedMapEditorTabs();
    for (TherionStudio::MapEditorTab *detachedTab : detachedTabs) {
        if (detachedTab == nullptr) {
            continue;
        }

        const QString path = detachedMapPathsByTab_.value(detachedTab);
        if (path.isEmpty()) {
            continue;
        }
        request.detachedDocumentPaths.append(path);
    }

    TherionStudio::MainWindowDocumentController::Actions actions;
    actions.confirmCloseTab = [this](int tabIndex) { return confirmCloseTab(tabIndex); };
    actions.confirmCloseDocumentPath = [this](const QString &documentPath) {
        if (auto *detachedTab = detachedMapEditorTabForPath(documentPath); detachedTab != nullptr) {
            return confirmCloseDocumentWidget(detachedTab);
        }
        return false;
    };
    actions.closeTab = [this](int tabIndex) {
        QWidget *tabWidget = editorTabs_->widget(tabIndex);
        if (tabWidget == nullptr) {
            return false;
        }

        const QString documentPath = documentPathForWidget(tabWidget);
        editorTabs_->removeTab(tabIndex);
        unregisterDocumentFileWatcherIfUnused(documentPath);
        tabWidget->deleteLater();
        return true;
    };
    actions.closeDetachedDocumentPath = [this](const QString &documentPath) {
        if (QMainWindow *window = detachedMapWindowsByPath_.value(documentPath)) {
            const bool wasClearingTabs = clearingDocumentTabs_;
            clearingDocumentTabs_ = true;
            window->close();
            clearingDocumentTabs_ = wasClearingTabs;
            return true;
        }

        return false;
    };
    actions.hasNoAttachedTabs = [this]() { return editorTabs_->count() == 0; };
    actions.ensureWelcomeTab = [this]() { addWelcomeTab(); };
    actions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    actions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    actions.showStatusBarMessage = [this](const QString &message, int timeoutMs) {
        statusBar()->showMessage(message, timeoutMs);
    };
    TherionStudio::MainWindowDocumentController::closeAll(request, actions);
}

void MainWindow::resetProjectBrowser()
{
    refreshProjectBrowserView();
}

QString MainWindow::structureOverrideKey(const QString &sourceFile, int lineNumber) const
{
    return QStringLiteral("%1|%2|%3").arg(QDir(projectRootPath_).absolutePath(), sourceFile).arg(lineNumber);
}

void MainWindow::loadStructureNameOverrides()
{
    structureNameOverrides_ = TherionStudio::MainWindowStructureNameOverridesService::parse(
        sessionStore_->structureNameOverrides());
}

void MainWindow::saveActiveDocument()
{
    QWidget *documentWidget = currentDocumentWidget();
    if (documentWidget == nullptr) {
        showComingSoon(tr("Save"));
        return;
    }

    QString errorMessage;
    if (!saveDocumentWidget(documentWidget, &errorMessage)) {
        if (!errorMessage.isEmpty()) {
            QMessageBox::warning(this, tr("Save"), errorMessage);
        }
        return;
    }

    statusBar()->showMessage(tr("Saved %1").arg(documentDisplayNameForWidget(documentWidget)), 2000);
}

void MainWindow::saveAllDocuments()
{
    saveAllOpenDocuments();
}

bool MainWindow::saveAllOpenDocuments()
{
    QList<QWidget *> documents;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (tabWidget == nullptr || tabWidget->property(kWelcomeTabPropertyName).toBool()) {
            continue;
        }
        documents.append(tabWidget);
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab == nullptr) {
            continue;
        }
        documents.append(detachedTab);
    }

    bool savedAny = false;
    for (QWidget *documentWidget : documents) {
        if (!documentIsDirtyForWidget(documentWidget)) {
            continue;
        }

        QString errorMessage;
        if (!saveDocumentWidget(documentWidget, &errorMessage)) {
            if (!errorMessage.isEmpty()) {
                QMessageBox::warning(this, tr("Save All"), errorMessage);
            }
            return false;
        }
        savedAny = true;
    }

    statusBar()->showMessage(savedAny ? tr("Saved all modified documents.") : tr("No modified documents to save."), 3000);
    return true;
}

void MainWindow::showFindBar(bool replaceMode)
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        showComingSoon(replaceMode ? tr("Replace") : tr("Find"));
        return;
    }

    documentShowFindBarForWidget(tabWidget, replaceMode);
}

TherionStudio::TextEditorTab *MainWindow::openTextTab(const QString &filePath,
                                                      bool showUnsupportedPrompt,
                                                      bool recordRecentFile)
{
    QList<TherionStudio::MainWindowDocumentOpenController::IndexedDocumentPath> attachedTextTabs;
    attachedTextTabs.reserve(editorTabs_->count());
    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::TextEditorTab *>(editorTabs_->widget(index));
        if (existingTab == nullptr) {
            continue;
        }

        TherionStudio::MainWindowDocumentOpenController::IndexedDocumentPath entry;
        entry.index = index;
        entry.documentPath = existingTab->filePath();
        attachedTextTabs.append(entry);
    }

    const auto openPlan = TherionStudio::MainWindowDocumentOpenController::planOpenTextTab(
        filePath,
        isSupportedTextEditorFilePath(canonicalDocumentPath(filePath)),
        attachedTextTabs);
    const QString canonicalPath = openPlan.canonicalPath;
    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenTextTabAction::UnsupportedDocument) {
        if (showUnsupportedPrompt) {
            showUnsupportedFilePrompt(this, canonicalPath);
        }
        return nullptr;
    }

    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenTextTabAction::ReuseAttachedTab) {
        auto *existingTab = qobject_cast<TherionStudio::TextEditorTab *>(editorTabs_->widget(openPlan.reuseTabIndex));
        if (existingTab == nullptr) {
            return nullptr;
        }

        existingTab->setProjectRootPath(projectRootPath_);
        existingTab->setModeSelectorVisible(false);
        TherionStudio::MainWindowDocumentTabOpenController::Actions openActions;
        openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
        openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
        openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
        TherionStudio::MainWindowDocumentTabOpenController::ActivateExistingTabRequest openRequest;
        openRequest.tabIndex = openPlan.reuseTabIndex;
        TherionStudio::MainWindowDocumentTabOpenController::activateExistingTab(openRequest, openActions);
        if (recordRecentFile) {
            recordRecentFilePath(existingTab->filePath());
        }
        return existingTab;
    }

    auto *tab = new TherionStudio::TextEditorTab(fileSystem_, commandCatalogStore_);
    if (supportsConfigurableDefaultTextEditorMode(canonicalPath)
        && sessionStore_ != nullptr
        && sessionStore_->defaultTextEditorMode().trimmed().toLower() == QStringLiteral("blocks")) {
        tab->setInitialEditorMode(TherionStudio::TextEditorTab::EditorMode::Blocks);
    }
    tab->setModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    connect(tab, &TherionStudio::TextEditorTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });

    connect(tab, &TherionStudio::TextEditorTab::dirtyStateChanged, this, [this, tab](bool) {
        updateTabTitle(tab);
        if (!tab->isDirty()) {
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
    openActions.removeWelcomeTabIfPresent = [this]() {
        const int welcomeTabIndex = findWelcomeTabIndex(editorTabs_);
        if (welcomeTabIndex < 0) {
            return;
        }

        QWidget *welcomeWidget = editorTabs_->widget(welcomeTabIndex);
        editorTabs_->removeTab(welcomeTabIndex);
        if (welcomeWidget != nullptr) {
            welcomeWidget->deleteLater();
        }
    };
    openActions.addTab = [this, tab](const QString &tabTitle) {
        return editorTabs_->addTab(tab, tabTitle);
    };
    openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
    openActions.handleCurrentLineChanged = [this](const QString &openedPath, int lineNumber) {
        handleTextEditorCurrentLineChanged(openedPath, lineNumber);
    };
    openActions.updateCurrentTabTitle = [this, tab]() { updateTabTitle(tab); };
    openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
    openActions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    openActions.appendConsoleLine = [this](const QString &line) { appendConsoleLine(line); };

    TherionStudio::MainWindowDocumentTabOpenController::AttachNewTabRequest openRequest;
    openRequest.tabTitle = tab->displayName();
    openRequest.openedDocumentPath = tab->filePath();
    openRequest.currentLineNumber = tab->currentLineNumber();
    openRequest.consoleOpenedLine = tr("Opened %1").arg(canonicalPath);
    TherionStudio::MainWindowDocumentTabOpenController::attachNewTab(openRequest, openActions);
    registerDocumentFileWatcher(tab->filePath());
    if (recordRecentFile) {
        recordRecentFilePath(tab->filePath());
    }
    updateOpenEditorProjectValidationDiagnostics();
    return tab;
}

void MainWindow::connectMapEditorTabUiSignals(TherionStudio::MapEditorTab *tab)
{
    if (tab == nullptr) {
        return;
    }

    connect(tab,
            &TherionStudio::MapEditorTab::openDedicatedWindowRequested,
            this,
            &MainWindow::handleMapEditorDetachRequested,
            Qt::UniqueConnection);

    if (tab->property(kMapEditorUiSignalsConnectedProperty).toBool()) {
        return;
    }
    tab->setProperty(kMapEditorUiSignalsConnectedProperty, true);

    connect(tab,
            &TherionStudio::MapEditorTab::workspaceModeChanged,
            this,
            [this, tab](TherionStudio::MapEditorTab::WorkspaceMode) {
                if (currentDocumentWidget() == tab) {
                    refreshWorkspaceModeSwitcher();
                    refreshViewMenuActions();
                }
            });
    connect(tab,
            &TherionStudio::MapEditorTab::mapPaneDetachStateChanged,
            this,
            [this, tab](bool) {
                if (currentDocumentWidget() == tab) {
                    refreshDocumentStatusWidgets();
                    refreshWorkspaceModeSwitcher();
                    refreshViewMenuActions();
                }
            });
    connect(tab, &TherionStudio::MapEditorTab::statusHintChanged, this,
            [this, tab]() { if (currentDocumentWidget() == tab) { refreshDocumentStatusWidgets(); } });
    connect(tab, &TherionStudio::MapEditorTab::commandSurfaceStateChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
            refreshViewMenuActions();
        }
    });
}

void MainWindow::setMapMagnifierEnabledForOpenTabs(bool enabled)
{
    for (int index = 0; index < editorTabs_->count(); ++index) {
        if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(index))) {
            mapTab->setMagnifierEnabled(enabled);
        }
    }

    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab != nullptr) {
            detachedTab->setMagnifierEnabled(enabled);
        }
    }
}

TherionStudio::MapEditorTab *MainWindow::openMapEditorTab(const QString &filePath,
                                                          bool recordRecentFile)
{
    QStringList detachedMapDocumentPaths;
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        if (detachedTab == nullptr) {
            continue;
        }

        detachedMapDocumentPaths.append(detachedTab->filePath());
    }

    QList<TherionStudio::MainWindowDocumentOpenController::IndexedDocumentPath> attachedMapTabs;
    attachedMapTabs.reserve(editorTabs_->count());
    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *existingTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(index));
        if (existingTab == nullptr) {
            continue;
        }

        TherionStudio::MainWindowDocumentOpenController::IndexedDocumentPath entry;
        entry.index = index;
        entry.documentPath = existingTab->filePath();
        attachedMapTabs.append(entry);
    }

    const auto openPlan = TherionStudio::MainWindowDocumentOpenController::planOpenMapTab(
        filePath,
        detachedMapDocumentPaths,
        attachedMapTabs);
    const QString canonicalPath = openPlan.canonicalPath;

    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenMapTabAction::FocusDetachedTab) {
        auto *detachedTab = detachedMapEditorTabForPath(canonicalPath);
        if (detachedTab == nullptr) {
            return nullptr;
        }

        detachedTab->setProjectRootPath(projectRootPath_);
        detachedTab->setInlineWorkspaceModeSelectorVisible(true);
        focusDetachedMapEditorTab(canonicalPath);
        refreshDocumentStatusWidgets();
        refreshWorkspaceModeSwitcher();
        if (recordRecentFile) {
            recordRecentFilePath(detachedTab->filePath());
        }
        return detachedTab;
    }

    if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenMapTabAction::ReuseAttachedTab) {
        auto *existingTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(openPlan.reuseTabIndex));
        if (existingTab == nullptr) {
            return nullptr;
        }

        existingTab->setProjectRootPath(projectRootPath_);
        existingTab->setInlineWorkspaceModeSelectorVisible(false);
        connectMapEditorTabUiSignals(existingTab);
        TherionStudio::MainWindowDocumentTabOpenController::Actions openActions;
        openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
        openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
        openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
        TherionStudio::MainWindowDocumentTabOpenController::ActivateExistingTabRequest openRequest;
        openRequest.tabIndex = openPlan.reuseTabIndex;
        TherionStudio::MainWindowDocumentTabOpenController::activateExistingTab(openRequest, openActions);
        if (recordRecentFile) {
            recordRecentFilePath(existingTab->filePath());
        }
        return existingTab;
    }

    auto *tab = new TherionStudio::MapEditorTab(fileSystem_, *sessionStore_, commandCatalogStore_);
    tab->setInlineWorkspaceModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    connect(tab, &TherionStudio::MapEditorTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
        }
    });
    connect(tab, &TherionStudio::MapEditorTab::dirtyStateChanged, this, [this, tab](bool) {
        updateTabTitle(tab);
        if (!tab->isDirty()) {
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
    openActions.removeWelcomeTabIfPresent = [this]() {
        const int welcomeTabIndex = findWelcomeTabIndex(editorTabs_);
        if (welcomeTabIndex < 0) {
            return;
        }

        QWidget *welcomeWidget = editorTabs_->widget(welcomeTabIndex);
        editorTabs_->removeTab(welcomeTabIndex);
        if (welcomeWidget != nullptr) {
            welcomeWidget->deleteLater();
        }
    };
    openActions.addTab = [this, tab](const QString &tabTitle) {
        return editorTabs_->addTab(tab, tabTitle);
    };
    openActions.setCurrentTabIndex = [this](int index) { editorTabs_->setCurrentIndex(index); };
    openActions.handleCurrentLineChanged = [this](const QString &openedPath, int lineNumber) {
        handleTextEditorCurrentLineChanged(openedPath, lineNumber);
    };
    openActions.updateCurrentTabTitle = [this, tab]() { updateTabTitle(tab); };
    openActions.refreshDocumentStatusWidgets = [this]() { refreshDocumentStatusWidgets(); };
    openActions.refreshWorkspaceModeSwitcher = [this]() { refreshWorkspaceModeSwitcher(); };
    openActions.persistOpenDocuments = [this]() { persistOpenDocuments(); };
    openActions.appendConsoleLine = [this](const QString &line) { appendConsoleLine(line); };

    TherionStudio::MainWindowDocumentTabOpenController::AttachNewTabRequest openRequest;
    openRequest.tabTitle = tab->displayName();
    openRequest.openedDocumentPath = tab->filePath();
    openRequest.currentLineNumber = tab->currentLineNumber();
    openRequest.consoleOpenedLine = tr("Opened %1").arg(canonicalPath);
    TherionStudio::MainWindowDocumentTabOpenController::attachNewTab(openRequest, openActions);
    registerDocumentFileWatcher(tab->filePath());
    if (recordRecentFile) {
        recordRecentFilePath(tab->filePath());
    }
    updateOpenEditorProjectValidationDiagnostics();
    return tab;
}

QWidget *MainWindow::documentWidgetForFilePath(const QString &filePath) const
{
    if (filePath.trimmed().isEmpty()) {
        return nullptr;
    }
    const QString canonicalPath = canonicalDocumentPath(filePath);

    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (documentPathForWidget(tabWidget) == canonicalPath) {
            return tabWidget;
        }
    }

    if (auto *detachedTab = detachedMapEditorTabForPath(canonicalPath); detachedTab != nullptr) {
        return detachedTab;
    }

    return nullptr;
}

QByteArray MainWindow::documentFileFingerprint(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

void MainWindow::processWatchedDocumentFileChange(const QString &filePath)
{
    const QString canonicalPath = canonicalDocumentPath(filePath);
    QWidget *documentWidget = documentWidgetForFilePath(canonicalPath);
    if (documentWidget == nullptr) {
        unregisterDocumentFileWatcherIfUnused(canonicalPath);
        return;
    }

    if (!QFileInfo::exists(canonicalPath)) {
        watchedDocumentFingerprints_.remove(canonicalPath);
        return;
    }

    if (documentFileWatcher_ != nullptr && !documentFileWatcher_->files().contains(canonicalPath)) {
        documentFileWatcher_->addPath(canonicalPath);
    }

    const QByteArray currentFingerprint = documentFileFingerprint(canonicalPath);
    const QByteArray previousFingerprint = watchedDocumentFingerprints_.value(canonicalPath);
    if (currentFingerprint.isEmpty() || (!previousFingerprint.isEmpty() && currentFingerprint == previousFingerprint)) {
        watchedDocumentFingerprints_.insert(canonicalPath, currentFingerprint);
        return;
    }

    const bool dirty = documentIsDirtyForWidget(documentWidget);
    if (dirty) {
        const QMessageBox::StandardButton decision = QMessageBox::question(
            this,
            tr("File Changed on Disk"),
            tr("The file %1 changed on disk while it has unsaved changes in Therion Studio.\n\nReload from disk and discard the in-memory changes?")
                .arg(QFileInfo(canonicalPath).fileName()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (decision != QMessageBox::Yes) {
            watchedDocumentFingerprints_.insert(canonicalPath, currentFingerprint);
            registerDocumentFileWatcher(canonicalPath);
            statusBar()->showMessage(tr("Kept in-memory version of %1.").arg(QFileInfo(canonicalPath).fileName()), 5000);
            return;
        }
    }

    QString errorMessage;
    if (!reloadDocumentWidgetFromDisk(documentWidget, &errorMessage)) {
        QMessageBox::warning(this,
                             tr("Reload File"),
                             errorMessage.isEmpty()
                                 ? tr("Unable to reload %1 from disk.").arg(canonicalPath)
                                 : errorMessage);
        watchedDocumentFingerprints_.insert(canonicalPath, currentFingerprint);
        registerDocumentFileWatcher(canonicalPath);
        return;
    }

    statusBar()->showMessage(tr("Reloaded %1 from disk.").arg(QFileInfo(canonicalPath).fileName()), 5000);
}

void MainWindow::syncOpenDocumentsToProjectRoot()
{
    for (int index = 0; index < editorTabs_->count(); ++index) {
        documentSetProjectRootPathForWidget(editorTabs_->widget(index), projectRootPath_);
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        documentSetProjectRootPathForWidget(detachedTab, projectRootPath_);
    }
    refreshDocumentStatusWidgets();
}

void MainWindow::handleMapEditorDetachRequested(TherionStudio::MapEditorTab *tab)
{
    detachMapEditorTab(tab);
}

void MainWindow::detachMapEditorTab(TherionStudio::MapEditorTab *tab)
{
    if (tab == nullptr) {
        return;
    }

    const QString canonicalPath = canonicalDocumentPath(tab->filePath());
    if (canonicalPath.isEmpty()) {
        return;
    }

    if (detachedMapPathsByTab_.contains(tab)) {
        focusDetachedMapEditorTab(canonicalPath);
        refreshWorkspaceModeSwitcher();
        return;
    }

    const int tabIndex = editorTabs_->indexOf(tab);
    if (tabIndex >= 0) {
        editorTabs_->removeTab(tabIndex);
        if (editorTabs_->count() == 0) {
            addWelcomeTab();
        }
    }

    // Make the detached map window a true top-level window and explicitly
    // reparent the tab out of the tab stack before attaching it.
    tab->setParent(nullptr);
    tab->setInlineWorkspaceModeSelectorVisible(true);
    auto *window = new DetachedMapWindow(tab);
    window->setWindowTitle(documentDisplayNameForWidget(tab));
    window->setWindowFilePath(canonicalPath);
    window->setCloseCallback([this](TherionStudio::MapEditorTab *mapTab) {
        if (mapTab == nullptr) {
            return;
        }

        if (!clearingDocumentTabs_ && !shuttingDown_) {
            reattachDetachedMapEditorTab(mapTab, true);
            return;
        }

        const QString path = detachedMapPathsByTab_.take(mapTab);
        if (!path.isEmpty()) {
            detachedMapWindowsByPath_.remove(path);
            unregisterDocumentFileWatcherIfUnused(path);
        }
    });

    detachedMapWindowsByPath_.insert(canonicalPath, window);
    detachedMapPathsByTab_.insert(tab, canonicalPath);

    connect(tab, &TherionStudio::MapEditorTab::titleChanged, this, [this, tab]() {
        const QString path = detachedMapPathsByTab_.value(tab);
        if (!path.isEmpty()) {
            if (QMainWindow *window = detachedMapWindowsByPath_.value(path)) {
                window->setWindowTitle(documentDisplayNameForWidget(tab));
            }
        }

        updateTabTitle(tab);
    });
    connect(tab, &QObject::destroyed, this, [this, tab]() {
        const QString path = detachedMapPathsByTab_.take(tab);
        if (!path.isEmpty()) {
            detachedMapWindowsByPath_.remove(path);
            unregisterDocumentFileWatcherIfUnused(path);
        }
    });
    connect(window, &QObject::destroyed, this, [this, canonicalPath]() {
        detachedMapWindowsByPath_.remove(canonicalPath);
    });

    window->resize(tab->size().isValid() ? tab->size() : QSize(1200, 800));
    window->show();
    window->raise();
    window->activateWindow();

    refreshWorkspaceModeSwitcher();
    persistOpenDocuments();
    appendConsoleLine(tr("Opened dedicated map window for %1").arg(canonicalPath));
}

void MainWindow::reattachDetachedMapEditorTab(TherionStudio::MapEditorTab *tab, bool focusTab)
{
    if (tab == nullptr) {
        return;
    }

    const QString path = detachedMapPathsByTab_.take(tab);
    if (!path.isEmpty()) {
        if (QMainWindow *window = detachedMapWindowsByPath_.value(path)) {
            if (window->centralWidget() == tab) {
                window->takeCentralWidget();
            }
        }
        detachedMapWindowsByPath_.remove(path);
    }

    if (editorTabs_->count() == 1 && editorTabs_->widget(0)->property(kWelcomeTabPropertyName).toBool()) {
        QWidget *welcomeWidget = editorTabs_->widget(0);
        editorTabs_->removeTab(0);
        if (welcomeWidget != nullptr) {
            welcomeWidget->deleteLater();
        }
    }

    int existingIndex = editorTabs_->indexOf(tab);
    if (existingIndex < 0) {
        existingIndex = editorTabs_->addTab(tab, tab->displayName());
    }
    if (focusTab && existingIndex >= 0) {
        editorTabs_->setCurrentIndex(existingIndex);
    }

    connectMapEditorTabUiSignals(tab);
    tab->setInlineWorkspaceModeSelectorVisible(false);
    tab->setProjectRootPath(projectRootPath_);
    updateTabTitle(tab);
    refreshWorkspaceModeSwitcher();
    persistOpenDocuments();
}

void MainWindow::focusDetachedMapEditorTab(const QString &canonicalPath)
{
    if (QMainWindow *window = detachedMapWindowsByPath_.value(canonicalPath)) {
        window->showNormal();
        window->raise();
        window->activateWindow();
    }
}

TherionStudio::MapEditorTab *MainWindow::detachedMapEditorTabForPath(const QString &canonicalPath) const
{
    if (QMainWindow *window = detachedMapWindowsByPath_.value(canonicalPath)) {
        if (auto *detachedWindow = dynamic_cast<DetachedMapWindow *>(window)) {
            return detachedWindow->mapTab();
        }
    }
    return nullptr;
}

QList<TherionStudio::MapEditorTab *> MainWindow::detachedMapEditorTabs() const
{
    QList<TherionStudio::MapEditorTab *> tabs;
    for (auto iterator = detachedMapPathsByTab_.constBegin(); iterator != detachedMapPathsByTab_.constEnd(); ++iterator) {
        if (iterator.key() != nullptr) {
            tabs.append(iterator.key());
        }
    }
    return tabs;
}

TherionStudio::TextEditorTab *MainWindow::currentTextTab() const
{
    return qobject_cast<TherionStudio::TextEditorTab *>(currentDocumentWidget());
}

TherionStudio::ThreeDViewerTab *MainWindow::currentThreeDViewerTab() const
{
    return qobject_cast<TherionStudio::ThreeDViewerTab *>(currentDocumentWidget());
}

QWidget *MainWindow::currentDocumentWidget() const
{
    QWidget *widget = editorTabs_->currentWidget();
    if (widget == nullptr || widget->property(kWelcomeTabPropertyName).toBool()) {
        return nullptr;
    }

    return widget;
}

void MainWindow::updateTabTitle(QWidget *tabWidget)
{
    if (tabWidget == nullptr) {
        return;
    }

    const int tabIndex = editorTabs_->indexOf(tabWidget);
    if (tabIndex < 0) {
        return;
    }

    editorTabs_->setTabText(tabIndex, documentDisplayNameForWidget(tabWidget));
    editorTabs_->setTabToolTip(tabIndex, documentPathForWidget(tabWidget));
    if (currentDocumentWidget() == tabWidget) {
        refreshFileImportActions();
        refreshDocumentStatusWidgets();
    }

    persistOpenDocuments();
}

void MainWindow::showComingSoon(const QString &featureName)
{
    const QString message = tr("%1 is not implemented yet.").arg(featureName);
    statusBar()->showMessage(message, 3000);
    appendConsoleLine(message);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmCloseDirtyDocuments()) {
        event->ignore();
        return;
    }

    shuttingDown_ = true;
    persistSessionState();
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    if ((event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)
        && QApplication::activeWindow() == this) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const int editorModeShortcutIndex = editorModeShortcutIndexForKeyEvent(keyEvent);
        if (editorModeShortcutIndex != 0) {
            keyEvent->accept();
            if (event->type() == QEvent::KeyPress) {
                if (editorModeShortcutIndex == 1) {
                    triggerRawModeForActiveDocument();
                } else {
                    triggerSecondaryEditorModeForActiveDocument();
                }
            }
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}
