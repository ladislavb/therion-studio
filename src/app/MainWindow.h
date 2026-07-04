#pragma once

#include <QHash>
#include <QByteArray>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QPoint>
#include <QProcess>
#include <QSet>

#include <memory>

#include "MainWindowTherionConsoleController.h"
#include "ProjectOutputsScanner.h"
#include "ProjectSearchScanner.h"
#include "ProjectValidationController.h"
#include "ProjectStructureScanner.h"
#include "../core/CommandCatalogStore.h"
#include "../core/ISessionStore.h"
#include "../core/ProjectStructureIndex.h"
#include "../core/QtFileSystem.h"
#include "../core/TherionSourceValidator.h"

class QLabel;
class QAction;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFileSystemModel;
class QCloseEvent;
class QDoubleSpinBox;
class QDockWidget;
class QFrame;
class QLineEdit;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QUrl;
class QFileSystemWatcher;
class QSlider;
class QSplitter;
class QStackedWidget;
class QStandardItemModel;
class QStandardItem;
class QTabWidget;
class QToolButton;
class QTreeView;
class QHBoxLayout;
class QVBoxLayout;
class QModelIndex;
class QResizeEvent;
namespace TherionStudio
{
class TextEditorTab;
class MapEditorTab;
class ThreeDViewerTab;
class TherionSqlReportTab;
class TherionRunnerService;
class ProjectScanCacheService;
}

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    enum class SessionRestoreMode
    {
        RestoreSession,
        StartEmpty
    };

    explicit MainWindow(std::unique_ptr<TherionStudio::ISessionStore> sessionStore,
                        TherionStudio::CommandCatalogStore commandCatalogStore,
                        QWidget *parent = nullptr,
                        SessionRestoreMode restoreMode = SessionRestoreMode::RestoreSession);
    explicit MainWindow(TherionStudio::ISessionStore &sessionStore,
                        TherionStudio::CommandCatalogStore commandCatalogStore,
                        QWidget *parent = nullptr,
                        SessionRestoreMode restoreMode = SessionRestoreMode::RestoreSession);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void createNewWindow();
    void openProject();
    void closeProject();
    void handleProjectTreeActivated(const QModelIndex &index);
    void handleProjectTreeContextMenuRequested(const QPoint &position);
    void handleTextEditorCurrentLineChanged(const QString &filePath, int lineNumber);
    void handleTabCloseRequested(int index);
    void closeActiveTab();
    void closeAllTabs();
    void createNewTherionSourceDocument();
    void createNewTherionMapDocument();
    void createNewTherionConfigDocument();
    void createProjectFromDefaultTemplate();
    void createEmptyProject();
    void saveActiveDocument();
    void saveAllDocuments();
    void runTherion();
    void runTherionProjectConfig();
    void runTherionCurrentConfig();
    void stopTherion();
    void showSettingsDialog();
    void offerApplicationRestart(const QString &message);
    void browseTherionTargetConfig();
    void browseTherionWorkingDirectoryOverride();
    void handleTherionRunnerStandardOutput(const QString &output);
    void handleTherionRunnerStandardError(const QString &output);
    void handleTherionRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleTherionRunnerError(const QString &errorText);
    void handleTherionRunnerStateChanged(bool running);
    void handleTherionConsoleLinkActivated(const QUrl &url);
    void showComingSoon(const QString &featureName);
    void handleMapEditorDetachRequested(TherionStudio::MapEditorTab *tab);

private:
    enum class SidebarPane
    {
        FileBrowser = 0,
        StructureBrowser = 1,
        Outputs = 2,
        Search = 3,
        Validation = 4,
        Console = 5
    };

    enum class StructureViewMode
    {
        Survey,
        Map
    };

    void buildUi();
    void buildMenus();
    void buildProjectBrowser();
    void buildOutputsSidebar();
    void buildSearchSidebar();
    void buildValidationSidebar();
    void buildStructureSidebar();
    void buildMapBackgroundPanel(QWidget *parent, QVBoxLayout *parentLayout);
    void buildConsole();
    void addWelcomeTab();
    void openProjectPath(const QString &selectedProjectPath);
    void openProjectFilePath(const QString &filePath);
    void refreshRecentProjectsUi();
    void refreshRecentFilesUi();
    void recordRecentFilePath(const QString &filePath);
    void importPocketTopoTextToActiveEditor();
    void refreshFileImportActions();
    TherionStudio::TextEditorTab *createUntitledTextTab(const QString &suggestedFileName, const QString &contents);
    TherionStudio::MapEditorTab *createUntitledMapEditorTab(const QString &suggestedFileName, const QString &contents);
    TherionStudio::ThreeDViewerTab *openThreeDViewerTab(const QString &filePath, bool recordRecentFile = true);
    TherionStudio::TherionSqlReportTab *openTherionSqlReportTab(const QString &filePath, bool recordRecentFile = true);
    bool saveDocumentWidget(QWidget *documentWidget, QString *errorMessage = nullptr);
    QString requestSavePathForDocument(QWidget *documentWidget) const;
    void restoreSessionState();
    void persistSessionState();
    void restoreOpenDocuments();
    void persistOpenDocuments();
    void resetProjectBrowser();
    void refreshProjectBrowserView(const QString &focusPath = QString(), bool forceReload = false);
    void rebuildStructureSidebar();
    void requestStructureSidebarRebuild();
    void requestProjectOutputsRefresh();
    void handleProjectOutputsScanFinished(const TherionStudio::ProjectOutputsScanner::Result &result);
    void openProjectOutputArtifact(const QModelIndex &index);
    void handleStructureSidebarScanFinished(const TherionStudio::ProjectStructureScanner::Result &result);
    void applyStructureSidebarIndex(const TherionStudio::ProjectIndexSnapshot &projectIndex);
    void showStructureSidebarMessage(const QString &message);
    void requestProjectSearch();
    void handleProjectSearchFinished(const TherionStudio::ProjectSearchScanner::Result &result);
    void requestProjectValidation();
    void clearProjectValidationResults();
    void requestProjectValidation(TherionStudio::ProjectValidationController::Trigger trigger, bool revealPanel);
    void requestRestoredProjectValidation();
    void updateProjectValidationStatusMessage();
    bool isDocumentPathInsideOpenProject(const QString &filePath) const;
    void handleDocumentTextChanged(QWidget *documentWidget);
    void handleProjectValidationStarted(TherionStudio::ProjectValidationController::Trigger trigger,
                                        quint64 generation,
                                        const QString &projectRootPath);
    void handleProjectValidationFinished(TherionStudio::ProjectValidationController::Trigger trigger,
                                         const TherionStudio::ProjectValidationScanner::Result &result);
    void updateOpenEditorProjectValidationDiagnostics();
    void openProjectSearchResult(const QModelIndex &index);
    void handleValidationSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void openValidationResult(const QModelIndex &index);
    void applySelectedValidationFix();
    void exportValidationResultsMarkdown();
    bool applyValidationFixesToValidatedDocument(const QString &filePath,
                                                 const QVector<TherionStudio::TherionSourceDiagnosticFix> &fixes);
    bool activateStructureSidebarAction(const QString &action);
    void updateStructureSidebarSourceLocations(const TherionStudio::ProjectIndexSnapshot &projectIndex);
    void storeCurrentStructureExpansionState();
    void setStructurePanelPage(int pageIndex);
    void rebuildMapObjectsTree();
    void showSidebarPane(SidebarPane pane);
    void setSidebarPane(SidebarPane pane);
    void syncOpenDocumentsToProjectRoot();
    QWidget *documentWidgetForFilePath(const QString &filePath) const;
    void rebuildProjectFileWatcher();
    void clearProjectFileWatcher();
    void invalidateProjectScanCache();
    void handleProjectFileSystemMutation(const QString &changedPath, const QString &previousPath = QString());
    void handleProjectDirectoryChanged(const QString &directoryPath);
    void handleProjectFileChanged(const QString &filePath);
    void registerDocumentFileWatcher(const QString &filePath);
    void unregisterDocumentFileWatcherIfUnused(const QString &filePath);
    void handleWatchedDocumentFileChanged(const QString &filePath);
    void handleWatchedDocumentDirectoryChanged(const QString &directoryPath);
    void processWatchedDocumentFileChange(const QString &filePath);
    bool reloadDocumentWidgetFromDisk(QWidget *documentWidget, QString *errorMessage = nullptr);
    QByteArray documentFileFingerprint(const QString &filePath) const;
    void openStructureSourceIndex(const QModelIndex &current, QTreeView *sourceTree);
    void handleStructureItemActivated(const QModelIndex &index, QTreeView *sourceTree);
    bool confirmCloseTab(int index);
    bool closeOpenDocumentForFilePath(const QString &filePath);
    bool confirmCloseDirtyDocuments();
    void clearDocumentTabs();
    TherionStudio::TextEditorTab *openTextTab(const QString &filePath,
                                              bool showUnsupportedPrompt = true,
                                              bool recordRecentFile = true);
    TherionStudio::MapEditorTab *openMapEditorTab(const QString &filePath,
                                                  bool recordRecentFile = true);
    void connectMapEditorTabUiSignals(TherionStudio::MapEditorTab *tab);
    QWidget *currentDocumentWidget() const;
    TherionStudio::TextEditorTab *currentTextTab() const;
    void showFindBar(bool replaceMode);
    void updateTabTitle(QWidget *tabWidget);
    void appendConsoleLine(const QString &line);
    void clearTherionConsoleOutput();
    void copyTherionConsoleOutput();
    void updateTherionRunnerState();
    void updateProjectActionState();
    void updateMapEditorActionState();
    QString therionWorkingDirectoryOverride() const;
    QString therionConfigResolutionDirectory() const;
    QString resolvedTherionWorkingDirectory() const;
    bool hasExplicitTherionConfigArgument() const;
    QString therionExecutableInput() const;
    QString therionRunTargetMode() const;
    QString currentDocumentTherionConfigPath() const;
    QString resolvedTherionTargetConfigPath() const;
    QString resolvedTherionConfigPath() const;
    void resetProjectTherionRunContext();
    bool clearMissingTherionTargetConfig();
    void refreshTherionConfigDisplay();
    void refreshTherionRunTargetControls();
    void rememberSidebarWidth();
    void restoreSidebarWidth();
    bool isSidebarEffectivelyCollapsed() const;
    void scheduleSidebarCollapseLayoutSync();
    void setSidebarCollapsed(bool collapsed);
    void updateSidebarCollapseButton();
    void rememberConsoleHeight();
    void restoreConsoleHeight();
    void setConsoleCollapsed(bool collapsed);
    void updateConsoleCollapseButton();
    QModelIndex firstStructureSourceIndex(const QModelIndex &index) const;
    bool isStructureContainerIndex(const QModelIndex &index) const;
    QModelIndex findStructureIndexForSourceLocation(const QString &filePath, int lineNumber) const;
    QModelIndex findMapObjectIndexForSourceLocation(const QString &filePath, int lineNumber) const;
    void updateStructureSelectionFromEditorLocation(const QString &filePath, int lineNumber);
    void updateMapObjectSelectionFromEditorLocation(const QString &filePath, int lineNumber);
    QString structureOverrideKey(const QString &sourceFile, int lineNumber) const;
    void loadStructureNameOverrides();
    void refreshMapBackgroundPanel();
    TherionStudio::MapEditorTab *currentMapEditorTab() const;
    void detachMapEditorTab(TherionStudio::MapEditorTab *tab);
    void reattachDetachedMapEditorTab(TherionStudio::MapEditorTab *tab, bool focusTab);
    void focusDetachedMapEditorTab(const QString &canonicalPath);
    TherionStudio::MapEditorTab *detachedMapEditorTabForPath(const QString &canonicalPath) const;
    QList<TherionStudio::MapEditorTab *> detachedMapEditorTabs() const;
    bool confirmCloseDocumentWidget(QWidget *documentWidget);
    bool saveAllOpenDocuments();
    void initializeDocumentStatusWidgets();
    void refreshDocumentStatusWidgets();
    void updateStatusHintLabel(const QString &text);
    void updateDocumentMenuActionState();
    void setCompilerStatusIdle();
    void setCompilerStatusRunning(const QString &configPath);
    void setCompilerStatusResult(bool success, const QString &details);
    void updateCompilerStatusButton(const QString &text, const QString &toolTip, const QString &accentColor);
    void initializeWorkspaceModeSwitcher();
    void refreshWorkspaceModeSwitcher();
    void refreshWorkspaceIconTheme();
    void updateThreeDViewerAutoRotationButton(bool autoRotationEnabled);
    void refreshWorkspaceModeSwitcherGeometry();
    void refreshViewMenuActions();
    void clearValidationRailIndicator();
    void updateValidationRailIndicator();
    void refreshFullScreenAction();
    void setMapMagnifierEnabledForOpenTabs(bool enabled);
    bool currentDocumentHasRightPanel() const;
    bool currentDocumentRightPanelCollapsed() const;
    QString currentDocumentRightPanelLabel() const;
    void setCurrentDocumentRightPanelCollapsed(bool collapsed);
    TherionStudio::ThreeDViewerTab *currentThreeDViewerTab() const;
    TherionStudio::MapEditorTab *currentDetachedMapTabWithContextHelp() const;
    bool currentDetachedMapContextHelpCollapsed() const;
    void setCurrentDetachedMapContextHelpCollapsed(bool collapsed);
    void triggerUndoForActiveDocument();
    void triggerRedoForActiveDocument();
    void triggerValidateDocumentForActiveDocument();
    void triggerRawModeForActiveDocument();
    void triggerSecondaryEditorModeForActiveDocument();
    void triggerCompileCurrentConfigForActiveDocument();
    void triggerZoomInForActiveDocument();
    void triggerZoomOutForActiveDocument();
    void triggerFitForActiveDocument();
    void triggerFitWithBackgroundForActiveDocument();
    void triggerThreeDViewerFitForActiveDocument();
    void triggerThreeDViewerResetForActiveDocument();
    void triggerThreeDViewerTopViewForActiveDocument();
    void triggerThreeDViewerSideViewForActiveDocument();
    void triggerThreeDViewerRollLeftForActiveDocument();
    void triggerThreeDViewerRollRightForActiveDocument();
    void triggerSelectForActiveDocument();
    void triggerCompleteDraftForActiveDocument();
    void triggerInsertScrapForActiveDocument();
    void triggerPointForActiveDocument();
    void triggerLineForActiveDocument();
    void triggerFreehandLineForActiveDocument();
    void triggerAreaForActiveDocument();
    void triggerSmartAreaForActiveDocument();

    QHBoxLayout *mainContentLayout_ = nullptr;
    QWidget *editorAreaHost_ = nullptr;
    QWidget *editorAreaColumn_ = nullptr;
    QVBoxLayout *editorAreaLayout_ = nullptr;
    QTabWidget *editorTabs_ = nullptr;
    QWidget *projectFilesEmptyState_ = nullptr;
    QPushButton *projectFilesOpenProjectButton_ = nullptr;
    QWidget *projectFilesPage_ = nullptr;
    QTreeView *projectTree_ = nullptr;
    QWidget *outputsPage_ = nullptr;
    QLabel *outputsStatusLabel_ = nullptr;
    QTreeView *outputsTree_ = nullptr;
    QButtonGroup *structureViewModeButtons_ = nullptr;
    QStackedWidget *structureViewStack_ = nullptr;
    QVBoxLayout *structureSurveyLayout_ = nullptr;
    QVBoxLayout *structureMapLayout_ = nullptr;
    QTreeView *structureTree_ = nullptr;
    QTreeView *searchResultsTree_ = nullptr;
    QTreeView *validationResultsTree_ = nullptr;
    QTreeView *mapObjectsTree_ = nullptr;
    QFrame *mapBackgroundPanel_ = nullptr;
    QListWidget *mapBackgroundLayersList_ = nullptr;
    QToolButton *mapBackgroundAddButton_ = nullptr;
    QPushButton *mapBackgroundRemoveButton_ = nullptr;
    QPushButton *mapBackgroundMoveUpButton_ = nullptr;
    QPushButton *mapBackgroundMoveDownButton_ = nullptr;
    QPushButton *mapBackgroundVisibilityButton_ = nullptr;
    QDoubleSpinBox *mapBackgroundPosXSpin_ = nullptr;
    QDoubleSpinBox *mapBackgroundPosYSpin_ = nullptr;
    QPushButton *mapBackgroundNudgeLeftButton_ = nullptr;
    QPushButton *mapBackgroundNudgeRightButton_ = nullptr;
    QPushButton *mapBackgroundNudgeUpButton_ = nullptr;
    QPushButton *mapBackgroundNudgeDownButton_ = nullptr;
    QSlider *mapBackgroundOpacitySlider_ = nullptr;
    QSlider *mapBackgroundGammaSlider_ = nullptr;
    QPushButton *mapBackgroundOpacityResetButton_ = nullptr;
    QPushButton *mapBackgroundGammaResetButton_ = nullptr;
    QSplitter *mainContentSplitter_ = nullptr;
    QWidget *sidebarContainer_ = nullptr;
    QWidget *sidebarContentContainer_ = nullptr;
    QToolButton *sidebarStructureButton_ = nullptr;
    QToolButton *sidebarOutputsButton_ = nullptr;
    QToolButton *sidebarSearchButton_ = nullptr;
    QToolButton *sidebarValidationButton_ = nullptr;
    QToolButton *sidebarConsoleButton_ = nullptr;
    QToolButton *sidebarCompileButton_ = nullptr;
    QStackedWidget *sidebarPages_ = nullptr;
    QWidget *consoleSidebarPage_ = nullptr;
    QVBoxLayout *consoleSidebarPageLayout_ = nullptr;
    QToolButton *sidebarCollapseButton_ = nullptr;
    QDockWidget *consoleDock_ = nullptr;
    QToolButton *consoleCollapseButton_ = nullptr;
    QAction *openProjectAction_ = nullptr;
    QAction *closeProjectAction_ = nullptr;
    QMenu *newProjectMenu_ = nullptr;
    QMenu *newFileMenu_ = nullptr;
    QAction *projectFromTemplateAction_ = nullptr;
    QAction *emptyProjectAction_ = nullptr;
    QAction *newTherionSourceAction_ = nullptr;
    QAction *newTherionMapAction_ = nullptr;
    QAction *newTherionConfigAction_ = nullptr;
    QAction *saveAction_ = nullptr;
    QAction *saveAllAction_ = nullptr;
    QAction *closeTabAction_ = nullptr;
    QAction *closeAllTabsAction_ = nullptr;
    QMenu *recentProjectsMenu_ = nullptr;
    QMenu *recentFilesMenu_ = nullptr;
    QMenu *importMenu_ = nullptr;
    QAction *importPocketTopoAction_ = nullptr;
    QAction *undoAction_ = nullptr;
    QAction *redoAction_ = nullptr;
    QAction *sidebarCollapseAction_ = nullptr;
    QAction *rightPanelCollapseAction_ = nullptr;
    QAction *contextHelpCollapseAction_ = nullptr;
    QAction *mapMagnifierAction_ = nullptr;
    QAction *fullScreenAction_ = nullptr;
    bool restartAfterClose_ = false;
    QTextBrowser *consoleView_ = nullptr;
    QLineEdit *therionWorkingDirectoryEdit_ = nullptr;
    QLineEdit *projectSearchEdit_ = nullptr;
    QPushButton *projectSearchButton_ = nullptr;
    QCheckBox *projectSearchWholeWordCheck_ = nullptr;
    QCheckBox *projectSearchMatchCaseCheck_ = nullptr;
    QLabel *projectSearchStatusLabel_ = nullptr;
    QLabel *validationStatusLabel_ = nullptr;
    QLabel *validationDetailTitleLabel_ = nullptr;
    QLabel *validationDetailMessageLabel_ = nullptr;
    QLabel *validationCurrentSourceLabel_ = nullptr;
    QLabel *validationSuggestedSourceLabel_ = nullptr;
    QPlainTextEdit *validationCurrentSourceEdit_ = nullptr;
    QPlainTextEdit *validationSuggestedSourceEdit_ = nullptr;
    QPushButton *validationScanProjectButton_ = nullptr;
    QPushButton *validationExportMarkdownButton_ = nullptr;
    QPushButton *validationApplyFixButton_ = nullptr;
    QPushButton *therionBrowseWorkingDirectoryButton_ = nullptr;
    QLineEdit *therionArgumentsEdit_ = nullptr;
    QComboBox *therionRunTargetCombo_ = nullptr;
    QLineEdit *therionTargetConfigEdit_ = nullptr;
    QPushButton *therionBrowseTargetConfigButton_ = nullptr;
    QLabel *therionConfigNameValue_ = nullptr;
    QLabel *therionConfigPathValue_ = nullptr;
    QLabel *therionWorkingDirectoryValue_ = nullptr;
    QPushButton *therionRunButton_ = nullptr;
    QPushButton *therionStopButton_ = nullptr;
    QPushButton *therionResetWorkingDirectoryButton_ = nullptr;
    QPushButton *therionClearOutputButton_ = nullptr;
    QPushButton *therionCopyOutputButton_ = nullptr;
    QFileSystemWatcher *documentFileWatcher_ = nullptr;
    QFileSystemWatcher *projectFileWatcher_ = nullptr;
    QHash<QString, QByteArray> watchedDocumentFingerprints_;
    QSet<QString> pendingWatchedDocumentChanges_;
    QSet<QString> pendingWatchedDocumentDirectoryChanges_;
    TherionStudio::TherionRunnerService *therionRunnerService_ = nullptr;
    TherionStudio::MainWindowTherionConsoleController therionConsoleController_;
    QLabel *statusHintLabel_ = nullptr;
    QLabel *statusMapZoomLabel_ = nullptr;
    QLabel *statusMapModeLabel_ = nullptr;
    QToolButton *statusCompilerButton_ = nullptr;
    QLabel *statusDocumentEncodingLabel_ = nullptr;
    QString activeTherionRunConfigPath_;
    QString activeTherionRunWorkingDirectory_;
    QString activeTherionRunStandardError_;
    QWidget *workspaceModeSwitcher_ = nullptr;
    QWidget *workspaceMapModeSwitcher_ = nullptr;
    QWidget *workspaceTextModeSwitcher_ = nullptr;
    QWidget *workspaceZoomGroup_ = nullptr;
    QWidget *workspaceThreeDViewerGroup_ = nullptr;
    QWidget *workspaceMapToolsGroup_ = nullptr;
    QToolButton *workspaceNewDocumentButton_ = nullptr;
    QToolButton *workspaceSaveButton_ = nullptr;
    QToolButton *workspaceUndoButton_ = nullptr;
    QToolButton *workspaceRedoButton_ = nullptr;
    QToolButton *workspaceCompileCurrentConfigButton_ = nullptr;
    QToolButton *workspaceZoomInButton_ = nullptr;
    QToolButton *workspaceZoomOutButton_ = nullptr;
    QToolButton *workspaceFitButton_ = nullptr;
    QToolButton *workspaceFitBackgroundButton_ = nullptr;
    QToolButton *workspaceThreeDViewerFitButton_ = nullptr;
    QToolButton *workspaceThreeDViewerResetButton_ = nullptr;
    QToolButton *workspaceThreeDViewerMeasureButton_ = nullptr;
    QToolButton *workspaceThreeDViewerAutoRotateButton_ = nullptr;
    QToolButton *workspaceThreeDViewerOrthographicButton_ = nullptr;
    QToolButton *workspaceThreeDViewerTopViewButton_ = nullptr;
    QToolButton *workspaceThreeDViewerSideViewButton_ = nullptr;
    QToolButton *workspaceThreeDViewerRollLeftButton_ = nullptr;
    QToolButton *workspaceThreeDViewerRollRightButton_ = nullptr;
    QToolButton *workspaceSelectButton_ = nullptr;
    QToolButton *workspaceCompleteDraftButton_ = nullptr;
    QToolButton *workspaceInsertScrapButton_ = nullptr;
    QToolButton *workspacePointButton_ = nullptr;
    QToolButton *workspaceLineButton_ = nullptr;
    QToolButton *workspaceFreehandLineButton_ = nullptr;
    QToolButton *workspaceAreaButton_ = nullptr;
    QToolButton *workspaceSmartAreaButton_ = nullptr;
    QFrame *workspaceEditSeparator_ = nullptr;
    QFrame *workspaceHistorySeparator_ = nullptr;
    QFrame *workspaceCompileSeparator_ = nullptr;
    QFrame *workspaceZoomSeparator_ = nullptr;
    QToolButton *workspaceVisualModeButton_ = nullptr;
    QToolButton *workspaceRawModeButton_ = nullptr;
    QToolButton *workspaceMapPaneWindowButton_ = nullptr;
    QToolButton *workspaceTextRawModeButton_ = nullptr;
    QToolButton *workspaceBlocksModeButton_ = nullptr;
    bool workspaceModeSwitcherSyncInProgress_ = false;
    QFileSystemModel *projectModel_ = nullptr;
    QStandardItemModel *structureModel_ = nullptr;
    QStandardItemModel *outputsModel_ = nullptr;
    QStandardItemModel *searchResultsModel_ = nullptr;
    QStandardItemModel *validationResultsModel_ = nullptr;
    QStandardItemModel *mapObjectsModel_ = nullptr;
    QVector<TherionStudio::TherionSourceDiagnostic> validationDiagnostics_;
    QVector<QString> validationDiagnosticFilePaths_;
    QVector<TherionStudio::ProjectValidationScanner::Finding> validationExportFindings_;
    QString validationDocumentPath_;
    QString validationExportScopeLabel_;
    QString validationExportProjectRootPath_;
    QString lastAppliedProjectValidationSignature_;
    QHash<quint64, bool> validationRevealByGeneration_;
    bool pendingProjectValidationRevealPanel_ = false;
    bool pendingValidationFixNavigation_ = false;
    bool validationProjectMode_ = false;
    bool validationExportAvailable_ = false;
    bool validationExportLimitReached_ = false;
    int validationExportSearchedFileCount_ = 0;
    int validationProblemCount_ = 0;
    TherionStudio::TherionSourceDiagnosticSeverity validationHighestSeverity_ =
        TherionStudio::TherionSourceDiagnosticSeverity::Warning;
    QString projectRootPath_;
    QHash<QString, QString> projectFileWatcherSignatures_;
    QString projectStructureSummary_;
    QString lastAppliedStructureSidebarSignature_;
    TherionStudio::ProjectIndexSnapshot lastStructureSidebarProjectIndex_;
    QString structureExpansionProjectRootPath_;
    QHash<int, QSet<QString>> structureExpandedNodeKeysByMode_;
    QHash<QString, QString> structureNameOverrides_;
    SidebarPane activeSidebarPane_ = SidebarPane::FileBrowser;
    StructureViewMode structureViewMode_ = StructureViewMode::Survey;
    int sidebarExpandedWidth_ = 320;
    int sidebarRailWidth_ = 56;
    int consoleExpandedHeight_ = 240;
    bool sidebarCollapsed_ = false;
    bool consoleCollapsed_ = false;
    bool updatingSidebarSplitter_ = false;
    bool sidebarCollapseSyncPending_ = false;
    bool updatingMapBackgroundPanel_ = false;
    bool hasAppliedStructureSidebarIndex_ = false;
    QHash<int, bool> hasStructureExpansionStateByMode_;
    bool clearingDocumentTabs_ = false;
    bool shuttingDown_ = false;

    QHash<QString, QPointer<QMainWindow>> detachedMapWindowsByPath_;
    QHash<TherionStudio::MapEditorTab *, QString> detachedMapPathsByTab_;

    TherionStudio::QtFileSystem fileSystem_;
    std::unique_ptr<TherionStudio::ISessionStore> ownedSessionStore_;
    TherionStudio::ISessionStore *sessionStore_ = nullptr;
    TherionStudio::CommandCatalogStore commandCatalogStore_;
    std::shared_ptr<TherionStudio::ProjectScanCacheService> projectScanCacheService_;
    TherionStudio::ProjectSearchScanner *projectSearchScanner_ = nullptr;
    TherionStudio::ProjectOutputsScanner *projectOutputsScanner_ = nullptr;
    TherionStudio::ProjectValidationController *projectValidationController_ = nullptr;
    TherionStudio::ProjectStructureScanner *structureSidebarScanner_ = nullptr;
};
