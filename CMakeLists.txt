cmake_minimum_required(VERSION 3.21)

set(THERION_STUDIO_VERSION "2026.6.8" CACHE STRING
    "Therion Studio numeric application version. CI may override this from release tags.")
if(NOT THERION_STUDIO_VERSION MATCHES "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
    message(FATAL_ERROR "THERION_STUDIO_VERSION must be numeric, for example 2026.5.0")
endif()

set(THERION_STUDIO_PACKAGE_LABEL "${THERION_STUDIO_VERSION}" CACHE STRING
    "Therion Studio package/artifact label. CI may include a short commit hash for development builds.")
if(NOT THERION_STUDIO_PACKAGE_LABEL MATCHES "^[A-Za-z0-9._-]+$")
    message(FATAL_ERROR "THERION_STUDIO_PACKAGE_LABEL may contain only letters, numbers, dots, underscores, and hyphens")
endif()

set(THERION_STUDIO_DEBIAN_VERSION "${THERION_STUDIO_VERSION}" CACHE STRING
    "Debian package Version field. CI may add snapshot metadata for development .deb builds.")
if(NOT THERION_STUDIO_DEBIAN_VERSION MATCHES "^[0-9][A-Za-z0-9.+~-]*$")
    message(FATAL_ERROR "THERION_STUDIO_DEBIAN_VERSION must be a valid Debian version starting with a digit")
endif()

set(THERION_STUDIO_DEB_PLATFORM_LABEL "linux" CACHE STRING
    ".deb artifact platform label. Official CI may set this to a specific Ubuntu baseline.")
if(NOT THERION_STUDIO_DEB_PLATFORM_LABEL MATCHES "^[A-Za-z0-9._-]+$")
    message(FATAL_ERROR "THERION_STUDIO_DEB_PLATFORM_LABEL may contain only letters, numbers, dots, underscores, and hyphens")
endif()

set(THERION_STUDIO_DEB_ARCHITECTURE_LABEL "amd64" CACHE STRING
    ".deb artifact architecture label.")
if(NOT THERION_STUDIO_DEB_ARCHITECTURE_LABEL MATCHES "^[A-Za-z0-9._-]+$")
    message(FATAL_ERROR "THERION_STUDIO_DEB_ARCHITECTURE_LABEL may contain only letters, numbers, dots, underscores, and hyphens")
endif()

project(TherionStudio
    VERSION "${THERION_STUDIO_VERSION}"
    LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(THERION_ENABLE_STRICT_WARNINGS
    "Enable warnings-as-errors for non-UI library targets"
    OFF)
option(THERION_ENABLE_QT_LINUX_DEPLOY_INSTALL
    "Run Qt's Linux runtime deployment script during install; intended for AppDir/AppImage builds"
    OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Svg Concurrent Test Quick QuickWidgets Qml QuickControls2)
find_package(Qt6LinguistTools 6.4 QUIET)

qt_standard_project_setup()

function(therion_enable_strict_warnings target_name)
    if(NOT THERION_ENABLE_STRICT_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

set(THERION_CORE_SOURCES
    src/core/CommandCatalogStore.cpp
    src/core/CommandCatalogStore.h
    src/core/TherionCommandSyntax.cpp
    src/core/TherionCommandSyntax.h
    src/core/TherionCommandLineModel.cpp
    src/core/TherionCommandLineModel.h
    src/core/DocumentFile.cpp
    src/core/DocumentFile.h
    src/core/IFileSystem.h
    src/core/ISessionStore.h
    src/core/QtFileSystem.cpp
    src/core/QtFileSystem.h
    src/core/MapBackgroundPlacement.cpp
    src/core/MapBackgroundPlacement.h
    src/core/PocketTopoImport.cpp
    src/core/PocketTopoImport.h
    src/core/TherionBackgroundMetadata.cpp
    src/core/TherionBackgroundMetadata.h
    src/core/TherionXviParser.cpp
    src/core/TherionXviParser.h
    src/core/TherionFileTypes.h
    src/core/ThreeDViewerCamera.cpp
    src/core/ThreeDViewerCamera.h
    src/core/ThreeDViewerLoxLoader.cpp
    src/core/ThreeDViewerLoxLoader.h
    src/core/ThreeDViewerSceneModel.cpp
    src/core/ThreeDViewerSceneModel.h
    src/core/ThreeDViewerSceneStatistics.cpp
    src/core/ThreeDViewerSceneStatistics.h
    src/core/TherionSourceText.h
    src/core/TherionStringUtils.h
    src/core/TherionTokenRules.h
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
    src/core/TherionSourceDocument.cpp
    src/core/TherionSourceDocument.h
    src/core/TherionSourceLogicalDocument.cpp
    src/core/TherionSourceLogicalDocument.h
    src/core/TherionSourceReferenceResolver.cpp
    src/core/TherionSourceReferenceResolver.h
    src/core/TherionSourceSnapshotCache.cpp
    src/core/TherionSourceSnapshotCache.h
    src/core/TherionSourceValidationCatalog.h
    src/core/TherionSourceValidator.cpp
    src/core/TherionSourceValidator.h
    src/core/ProjectStructureIndex.cpp
    src/core/ProjectStructureIndex.h
    src/core/TherionDocumentParser.cpp
    src/core/TherionDocumentParser.h
    src/core/SessionStore.cpp
    src/core/SessionStore.h)

add_library(therion_core STATIC
    ${THERION_CORE_SOURCES})

target_link_libraries(therion_core
    PUBLIC
        Qt6::Core)

therion_enable_strict_warnings(therion_core)

function(therion_add_core_unit_test target_name)
    qt_add_executable(${target_name}
        ${ARGN})

    target_link_libraries(${target_name} PRIVATE therion_core)
endfunction()

function(therion_add_core_qtest_runner target_name)
    qt_add_executable(${target_name}
        ${ARGN})

    target_link_libraries(${target_name} PRIVATE therion_core Qt6::Test)
endfunction()

set(THERION_PLATFORM_SOURCES
    src/platform/ApplicationIdentity.cpp
    src/platform/ApplicationIdentity.h
    src/platform/ApplicationLanguageOverride.h
    src/platform/PlatformPathResolver.h)

if(APPLE)
    list(APPEND THERION_PLATFORM_SOURCES
        src/platform/mac/MacApplicationLanguageOverride.cpp
        src/platform/mac/MacPlatformPathResolver.cpp)
elseif(WIN32)
    list(APPEND THERION_PLATFORM_SOURCES
        src/platform/ApplicationLanguageOverride.cpp
        src/platform/win/WinPlatformPathResolver.cpp)
else()
    list(APPEND THERION_PLATFORM_SOURCES
        src/platform/ApplicationLanguageOverride.cpp
        src/platform/linux/LinuxPlatformPathResolver.cpp)
endif()

add_library(therion_platform STATIC
    ${THERION_PLATFORM_SOURCES})

target_link_libraries(therion_platform
    PUBLIC
        Qt6::Core)

if(APPLE)
    find_library(THERION_COREFOUNDATION_FRAMEWORK CoreFoundation)
    if(NOT THERION_COREFOUNDATION_FRAMEWORK)
        message(FATAL_ERROR "CoreFoundation framework is required on macOS")
    endif()
    target_link_libraries(therion_platform
        PUBLIC
            ${THERION_COREFOUNDATION_FRAMEWORK})
endif()

therion_enable_strict_warnings(therion_platform)

set(THERION_APP_SOURCES
    src/app/MainWindowSessionDocumentService.cpp
    src/app/MainWindowSessionDocumentService.h
    src/app/MainWindowSessionDocumentController.cpp
    src/app/MainWindowSessionDocumentController.h
    src/app/MainWindowSessionController.cpp
    src/app/MainWindowSessionController.h
    src/app/MainWindowProjectController.cpp
    src/app/MainWindowProjectController.h
    src/app/MainWindowProjectLifecycleService.cpp
    src/app/MainWindowProjectLifecycleService.h
    src/app/MainWindowProjectOrchestrationService.cpp
    src/app/MainWindowProjectOrchestrationService.h
    src/app/MainWindowProjectUiFlowService.cpp
    src/app/MainWindowProjectUiFlowService.h
    src/app/MainWindowProjectWorkspaceService.cpp
    src/app/MainWindowProjectWorkspaceService.h
    src/app/MainWindowRecentFilesService.cpp
    src/app/MainWindowRecentFilesService.h
    src/app/MainWindowRecentProjectsService.cpp
    src/app/MainWindowRecentProjectsService.h
    src/app/MainWindowSessionProjectService.cpp
    src/app/MainWindowSessionProjectService.h
    src/app/ProjectTemplateService.cpp
    src/app/ProjectTemplateService.h
    src/app/MainWindowSessionRestoreOrchestrationService.cpp
    src/app/MainWindowSessionRestoreOrchestrationService.h
    src/app/MainWindowSessionRestoreUiFlowService.cpp
    src/app/MainWindowSessionRestoreUiFlowService.h
    src/app/MainWindowSessionStateService.cpp
    src/app/MainWindowSessionStateService.h
    src/app/MainWindowSessionWindowRestoreService.cpp
    src/app/MainWindowSessionWindowRestoreService.h
    src/app/MainWindowDocumentController.cpp
    src/app/MainWindowDocumentController.h
    src/app/MainWindowDocumentFileWatcher.cpp
    src/app/MainWindowDocumentOpenController.cpp
    src/app/MainWindowDocumentOpenController.h
    src/app/MainWindowDocumentTabOpenController.cpp
    src/app/MainWindowDocumentTabOpenController.h
    src/app/MainWindowTherionRunnerController.cpp
    src/app/MainWindowTherionRunnerController.h
    src/app/MainWindowStructureNameOverridesService.cpp
    src/app/MainWindowStructureNameOverridesService.h
    src/app/MainWindowValidationFixApplyService.cpp
    src/app/MainWindowValidationFixApplyService.h
    src/app/ProjectSearchScanner.cpp
    src/app/ProjectSearchScanner.h
    src/app/ProjectValidationController.cpp
    src/app/ProjectValidationController.h
    src/app/ProjectValidationScanner.cpp
    src/app/ProjectValidationScanner.h
    src/app/ProjectStructureScanner.cpp
    src/app/ProjectStructureScanner.h
    src/app/TherionExecutableSelectionController.cpp
    src/app/TherionExecutableSelectionController.h
    src/app/TherionExecutableDetector.cpp
    src/app/TherionExecutableDetector.h
    src/app/TherionRunnerConfigDisplayController.cpp
    src/app/TherionRunnerConfigDisplayController.h
    src/app/TherionRunnerConfigResolver.cpp
    src/app/TherionRunnerConfigResolver.h
    src/app/TherionRunnerLifecyclePresenter.cpp
    src/app/TherionRunnerLifecyclePresenter.h
    src/app/TherionRunnerService.cpp
    src/app/TherionRunnerService.h
    src/app/TherionRunnerStartResultPresenter.cpp
    src/app/TherionRunnerStartResultPresenter.h
    src/app/TherionRunnerStartSuccessPresenter.cpp
    src/app/TherionRunnerStartSuccessPresenter.h
    src/app/three_d_viewer/ThreeDViewerTab.cpp
    src/app/three_d_viewer/ThreeDViewerTab.h
    src/app/three_d_viewer/ThreeDViewerInspectorState.cpp
    src/app/three_d_viewer/ThreeDViewerInspectorState.h
    src/app/three_d_viewer/ThreeDViewerMeshColorMode.h
    src/app/three_d_viewer/ThreeDViewerInspectorWidget.cpp
    src/app/three_d_viewer/ThreeDViewerInspectorWidget.h
    src/app/three_d_viewer/ThreeDViewerLayerListModel.cpp
    src/app/three_d_viewer/ThreeDViewerLayerListModel.h
    src/app/three_d_viewer/ThreeDViewerViewportController.cpp
    src/app/three_d_viewer/ThreeDViewerViewportController.h
    src/app/three_d_viewer/ThreeDViewerProjection.cpp
    src/app/three_d_viewer/ThreeDViewerProjection.h
    src/app/three_d_viewer/ThreeDViewerViewportItem.cpp
    src/app/three_d_viewer/ThreeDViewerViewportItem.h
    src/app/three_d_viewer/ThreeDViewerViewportWidget.cpp
    src/app/three_d_viewer/ThreeDViewerViewportWidget.h)

add_library(therion_app STATIC
    ${THERION_APP_SOURCES})

target_link_libraries(therion_app
    PUBLIC
        therion_core
        Qt6::Core
        Qt6::Widgets
        Qt6::Svg
        Qt6::Quick
        Qt6::QuickWidgets
        Qt6::Qml
        Qt6::QuickControls2
    PRIVATE
        therion_platform
        Qt6::Concurrent)

therion_enable_strict_warnings(therion_app)

function(therion_add_app_unit_test target_name)
    qt_add_executable(${target_name}
        ${ARGN})

    target_link_libraries(${target_name} PRIVATE therion_app)
endfunction()

set(THERION_APP_UI_STYLE_SOURCES
    src/app/ui/ApplicationStyleSheets.cpp
    src/app/ui/ApplicationStyleSheets.h
    src/app/ui/ApplicationStyleTokens.cpp
    src/app/ui/ApplicationStyleTokens.h
)

set(THERION_MAIN_WINDOW_SOURCES
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/MainWindowFileOpen.cpp
    src/app/MainWindowUntitledDocumentWorkflow.cpp
    src/app/MainWindowTherionConsoleBootstrapper.cpp
    src/app/MainWindowTherionConsoleBootstrapper.h
    src/app/MainWindowTherionConsoleBuilder.cpp
    src/app/MainWindowTherionConsoleBuilder.h
    src/app/MainWindowTherionConsoleWiring.cpp
    src/app/MainWindowTherionConsoleWiring.h
    src/app/MainWindowDocumentHelpers.cpp
    src/app/MainWindowDocumentHelpers.h
    src/app/MainWindowHelpDocument.cpp
    src/app/MainWindowHelpDocument.h
    src/app/MainWindowHelpDialog.cpp
    src/app/MainWindowHelpDialog.h
    src/app/MainWindowMapBackground.cpp
    src/app/MainWindowProjectFileWatcher.cpp
    src/app/MainWindowProjectBrowserSidebar.cpp
    src/app/MainWindowProjectTemplate.cpp
    src/app/MainWindowProjectLifecycle.cpp
    src/app/MainWindowSettingsDialog.cpp
    src/app/MainWindowSettingsDialog.h
    src/app/MainWindowSearchSidebar.cpp
    src/app/MainWindowSidebar.cpp
    src/app/MainWindowStatusUi.cpp
    src/app/MainWindowStructureBrowser.cpp
    src/app/MainWindowStructureRoles.h
    src/app/MainWindowValidationSidebar.cpp
    src/app/MainWindowViewMenu.cpp
    src/app/MainWindowWorkspaceCommandBar.cpp
    src/app/MainWindowWelcomeWidget.cpp
    src/app/MainWindowWelcomeWidget.h
    src/app/MainWindowTherionConsoleController.h
    src/app/MainWindowTherionRunner.cpp
    src/app/LucideIconFactory.h
    src/app/WorkspaceCommandBarStyle.h
    src/app/ui/ApplicationControlMetrics.h
    src/app/ui/ApplicationSegmentedControlStyle.h
    ${THERION_APP_UI_STYLE_SOURCES}
)

set(THERION_TEXT_EDITOR_SHARED_SOURCES
    src/app/text_editor/ContextHelpController.cpp
    src/app/text_editor/ContextHelpController.h
    src/app/text_editor/ContextHelpInspector.cpp
    src/app/text_editor/ContextHelpInspector.h
    src/app/text_editor/DocumentFileInspector.cpp
    src/app/text_editor/DocumentFileInspector.h
    src/app/text_editor/DocumentInspectorPanel.cpp
    src/app/text_editor/DocumentInspectorPanel.h
    src/app/text_editor/InspectorPanel.cpp
    src/app/text_editor/InspectorPanel.h
    src/app/text_editor/InspectorPanelStyle.cpp
    src/app/text_editor/InspectorPanelStyle.h
    src/app/text_editor/TextEditorOptionValidation.cpp
    src/app/text_editor/TextEditorCommandMetadata.h
    src/app/text_editor/TextEditorOptionValidation.h
    src/app/text_editor/TextEditorValidationCatalog.cpp
    src/app/text_editor/TextEditorValidationCatalog.h
    src/app/text_editor/CommandOptionsDialog.cpp
    src/app/text_editor/CommandOptionsDialog.h
    src/app/text_editor/CommandOptionTableDelegate.cpp
    src/app/text_editor/CommandOptionTableDelegate.h
    src/app/text_editor/TextEditorTabBlockEditorPanel.cpp
    src/app/text_editor/block_editor/BlockEditorOptionArgsController.cpp
    src/app/text_editor/block_editor/BlockEditorOptionArgsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsHelpController.h
    src/app/text_editor/block_editor/BlockEditorLineBuildService.cpp
    src/app/text_editor/block_editor/BlockEditorLineBuildService.h
    src/app/text_editor/block_editor/BlockEditorApplyStateController.cpp
    src/app/text_editor/block_editor/BlockEditorApplyStateController.h
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.h
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorSelectionDetailsController.h
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsPaneController.h
    src/app/text_editor/block_editor/BlockEditorToolboxController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxController.h
    src/app/text_editor/block_editor/BlockEditorToolboxList.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxList.h
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.cpp
    src/app/text_editor/block_editor/BlockEditorTokenTagEditor.h
    src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxDetailsController.h
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasSelectionController.h
    src/app/text_editor/block_editor/BlockEditorCanvasView.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasView.h
    src/app/text_editor/block_editor/BlockEditorDragMime.h
    src/app/text_editor/block_editor/BlockEditorCanvasItem.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.h
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.h
    src/app/text_editor/block_editor/BlockEditorDetailsSupport.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsSupport.h
    src/app/text_editor/block_editor/BlockEditorConfigureController.cpp
    src/app/text_editor/block_editor/BlockEditorConfigureController.h
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.cpp
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.h
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h
    src/app/text_editor/block_editor/BlockEditorSourceController.cpp
    src/app/text_editor/block_editor/BlockEditorSourceController.h
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasBoundaryController.h
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.cpp
    src/app/text_editor/block_editor/BlockEditorDropTargetResolver.h
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionPlanner.h
    src/app/text_editor/block_editor/BlockEditorInsertionController.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionController.h
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorInsertionTemplateBuilder.h
    src/app/text_editor/block_editor/BlockEditorMovePlanner.cpp
    src/app/text_editor/block_editor/BlockEditorMovePlanner.h
    src/app/text_editor/block_editor/BlockEditorMoveController.cpp
    src/app/text_editor/block_editor/BlockEditorMoveController.h
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.cpp
    src/app/text_editor/block_editor/BlockEditorMovePreviewController.h
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.cpp
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h
    src/app/text_editor/TextEditorContextHelpController.cpp
    src/app/text_editor/TextEditorContextHelpController.h
    src/app/text_editor/TextEditorCursorController.cpp
    src/app/text_editor/TextEditorCursorController.h
    src/app/text_editor/TextEditorDocumentController.cpp
    src/app/text_editor/TextEditorDocumentController.h
    src/app/text_editor/TextEditorDocumentIoService.cpp
    src/app/text_editor/TextEditorDocumentIoService.h
    src/app/text_editor/TextEditorDocumentPreconditionsService.cpp
    src/app/text_editor/TextEditorDocumentPreconditionsService.h
    src/app/text_editor/TextEditorDocumentPersistenceStateService.cpp
    src/app/text_editor/TextEditorDocumentPersistenceStateService.h
    src/app/text_editor/TextEditorDocumentWorkflowController.cpp
    src/app/text_editor/TextEditorDocumentWorkflowController.h
    src/app/text_editor/TextEditorTabInteractionController.cpp
    src/app/text_editor/TextEditorTabInteractionController.h
    src/app/text_editor/TextEditorEncodingController.cpp
    src/app/text_editor/TextEditorEncodingController.h
    src/app/text_editor/TextEditorAppearanceController.cpp
    src/app/text_editor/TextEditorAppearanceController.h
    src/app/text_editor/TextEditorSourceRewriteController.cpp
    src/app/text_editor/TextEditorSourceRewriteController.h
    src/app/text_editor/TextEditorSourceTransactionController.cpp
    src/app/text_editor/TextEditorSourceTransactionController.h
    src/app/text_editor/TextEditorStatusController.cpp
    src/app/text_editor/TextEditorStatusController.h
    src/app/text_editor/TextEditorModeController.cpp
    src/app/text_editor/TextEditorModeController.h
    src/app/text_editor/TextEditorSurfaceStyler.cpp
    src/app/text_editor/TextEditorSurfaceStyler.h
    src/app/text_editor/raw_editor/RawEditorPanelBuilder.cpp
    src/app/text_editor/raw_editor/RawEditorFindController.cpp
    src/app/text_editor/raw_editor/RawEditorFindController.h
    src/app/text_editor/raw_editor/RawEditorCompletionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionControllerContext.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionController.h
    src/app/text_editor/raw_editor/RawEditorCommandMetadataLoader.cpp
    src/app/text_editor/raw_editor/RawEditorCommandMetadataLoader.h
    src/app/text_editor/raw_editor/RawEditorCompletionContextAnalyzer.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionContextAnalyzer.h
    src/app/text_editor/raw_editor/RawEditorCompletionSuggestionBuilder.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionSuggestionBuilder.h
    src/app/text_editor/raw_editor/RawEditorCompletionPopupController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionPopupController.h
    src/app/text_editor/raw_editor/RawEditorCompletionTokenContext.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionTokenContext.h
    src/app/text_editor/raw_editor/RawEditorCompletionInsertionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionInsertionController.h
    src/app/text_editor/raw_editor/RawEditorTextEdit.cpp
    src/app/text_editor/raw_editor/RawEditorTextEdit.h
)

set(THERION_MAP_EDITOR_SOURCES
    src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.h
    src/app/text_editor/map_editor/MapEditorReferencedAreaResolver.cpp
    src/app/text_editor/map_editor/MapEditorReferencedAreaResolver.h
    src/app/text_editor/map_editor/MapEditorSmartAreaPlanner.cpp
    src/app/text_editor/map_editor/MapEditorSmartAreaPlanner.h
    src/app/text_editor/map_editor/MapEditorRasterBackgroundImage.cpp
    src/app/text_editor/map_editor/MapEditorRasterBackgroundImage.h
    src/app/text_editor/map_editor/MapEditorRasterBackgroundPlacement.cpp
    src/app/text_editor/map_editor/MapEditorRasterBackgroundPlacement.h
    src/app/text_editor/map_editor/MapEditorDetachedPaneWindow.cpp
    src/app/text_editor/map_editor/MapEditorDetachedPaneWindow.h
    src/app/text_editor/map_editor/MapEditorCanvasEditCommandFactory.cpp
    src/app/text_editor/map_editor/MapEditorCanvasEditCommandFactory.h
    src/app/text_editor/map_editor/MapEditorCanvasEditContext.h
    src/app/text_editor/map_editor/MapEditorCanvasEditController.cpp
    src/app/text_editor/map_editor/MapEditorCanvasEditController.h
    src/app/text_editor/map_editor/MapEditorInspectorBackgroundController.cpp
    src/app/text_editor/map_editor/MapEditorInspectorBackgroundController.h
    src/app/text_editor/map_editor/MapEditorInspectorData.cpp
    src/app/text_editor/map_editor/MapEditorInspectorData.h
    src/app/text_editor/map_editor/MapEditorInspectorObjectContext.h
    src/app/text_editor/map_editor/MapEditorInspectorObjectController.cpp
    src/app/text_editor/map_editor/MapEditorInspectorObjectController.h
    src/app/text_editor/map_editor/MapEditorObjectDetailsEditController.cpp
    src/app/text_editor/map_editor/MapEditorObjectDetailsEditController.h
    src/app/text_editor/map_editor/MapEditorObjectDetailsContext.h
    src/app/text_editor/map_editor/MapEditorObjectDetailsPanelController.cpp
    src/app/text_editor/map_editor/MapEditorObjectDetailsPanelController.h
    src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.cpp
    src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.h
    src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.cpp
    src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.h
    src/app/text_editor/map_editor/MapEditorLineExtensionPlanner.cpp
    src/app/text_editor/map_editor/MapEditorLineExtensionPlanner.h
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.cpp
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.h
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.cpp
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.h
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h
    src/app/text_editor/map_editor/MapEditorRecentSymbolsLayout.cpp
    src/app/text_editor/map_editor/MapEditorRecentSymbolsLayout.h
    src/app/text_editor/map_editor/MapEditorLineDecorationItem.cpp
    src/app/text_editor/map_editor/MapEditorLineDecorationItem.h
    src/app/text_editor/map_editor/MapEditorPointSymbolGeometry.h
    src/app/text_editor/map_editor/MapEditorStylePreviewWidget.cpp
    src/app/text_editor/map_editor/MapEditorStylePreviewWidget.h
    src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.cpp
    src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.h
    src/app/text_editor/map_editor/MapEditorInteractiveDrawContext.h
    src/app/text_editor/map_editor/MapEditorInteractiveDrawController.cpp
    src/app/text_editor/map_editor/MapEditorInteractiveDrawController.h
    src/app/text_editor/map_editor/MapEditorInteractiveDrawLogic.cpp
    src/app/text_editor/map_editor/MapEditorInteractiveDrawLogic.h
    src/app/text_editor/map_editor/MapEditorInputPolicy.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.h
    src/app/text_editor/map_editor/MapEditorMagnifierOverlay.cpp
    src/app/text_editor/map_editor/MapEditorMagnifierOverlay.h
    src/app/text_editor/map_editor/MapEditorViewportInputContext.h
    src/app/text_editor/map_editor/MapEditorViewportInputController.cpp
    src/app/text_editor/map_editor/MapEditorViewportInputController.h
    src/app/text_editor/map_editor/MapEditorSceneLifecycleController.cpp
    src/app/text_editor/map_editor/MapEditorSceneLifecycleController.h
    src/app/text_editor/map_editor/MapEditorSceneRefreshController.cpp
    src/app/text_editor/map_editor/MapEditorSceneRefreshController.h
    src/app/text_editor/map_editor/MapEditorSelectionContext.h
    src/app/text_editor/map_editor/MapEditorSelectionController.cpp
    src/app/text_editor/map_editor/MapEditorSelectionController.h
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h
    src/app/text_editor/map_editor/MapEditorSceneThemePolicy.h
    src/app/text_editor/map_editor/MapEditorTabEventRouting.cpp
    src/app/text_editor/map_editor/MapEditorTabControllers.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorControllerDelegates.cpp
    src/app/text_editor/map_editor/MapEditorTabCanvasEditContext.cpp
    src/app/text_editor/map_editor/MapEditorTabSceneRefreshLifecycle.cpp
    src/app/text_editor/map_editor/MapEditorTabSelectionInspectorWorkflow.cpp
    src/app/text_editor/map_editor/MapEditorTabSourceEditWorkflow.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorBackgroundContext.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorObjectContext.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorObjectEvents.cpp
    src/app/text_editor/map_editor/MapEditorTabInteractiveDrawContext.cpp
    src/app/text_editor/map_editor/MapEditorTabLineExtension.cpp
    src/app/text_editor/map_editor/MapEditorTabObjectDetailsContext.cpp
    src/app/text_editor/map_editor/MapEditorTabSceneContexts.cpp
    src/app/text_editor/map_editor/MapEditorTabSelectionContext.cpp
    src/app/text_editor/map_editor/MapEditorTabViewportInputContext.cpp
    src/app/text_editor/map_editor/MapEditorTabDetachedPaneDelegates.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorBackgroundUi.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorPanelUi.cpp
    src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp
    src/app/text_editor/map_editor/MapEditorTabCommandSurfaceDelegates.cpp
    src/app/text_editor/map_editor/MapEditorTabWorkspaceDelegates.cpp
    src/app/text_editor/map_editor/MapEditorUndoArbitrationService.cpp
    src/app/text_editor/map_editor/MapEditorUndoArbitrationService.h
    src/app/text_editor/map_editor/MapEditorTabDocumentValidation.cpp
    src/app/text_editor/map_editor/MapEditorTabNewDocumentDelegates.cpp
    src/app/text_editor/map_editor/MapEditorTab.h
    src/app/text_editor/map_editor/MapEditorXviBackgroundItem.cpp
    src/app/text_editor/map_editor/MapEditorXviBackgroundItem.h
)

set(THERION_TEXT_EDITOR_TAB_SOURCES
    src/app/text_editor/TextEditorTab.cpp
    src/app/text_editor/TextEditorTabBootstrap.cpp
    src/app/text_editor/TextEditorTabBlockDetailsDelegates.cpp
    src/app/text_editor/TextEditorTabDocumentModeDelegates.cpp
    src/app/text_editor/TextEditorTabRawFindDelegates.cpp
    src/app/text_editor/TextEditorTabSourceRewriteDelegates.cpp
    src/app/text_editor/TextEditorTabStatusCursorDelegates.cpp
    src/app/text_editor/TextEditorTabCompletionFacade.cpp
    src/app/text_editor/TextEditorTabAppearanceController.cpp
    src/app/text_editor/TextEditorTabValidation.cpp
    src/app/text_editor/TextEditorTabBlockEditorDetailsHelpController.cpp
    src/app/text_editor/TextEditorTabBlockEditorLineBuildService.cpp
    src/app/text_editor/TextEditorTabBlockEditorOptionArgsController.cpp
    src/app/text_editor/TextEditorTabBlockEditorResolverContexts.cpp
    src/app/text_editor/TextEditorTabBlockEditorSelectionContexts.cpp
    src/app/text_editor/TextEditorTabBlockEditorToolboxController.cpp
    src/app/text_editor/TextEditorTabBlockEditorApplyExecutor.cpp
    src/app/text_editor/TextEditorTabBlockEditorApplyStateController.cpp
    src/app/text_editor/TextEditorTabBlockEditorCanvasBoundaryController.cpp
    src/app/text_editor/TextEditorTabBlockEditorSourceController.cpp
    src/app/text_editor/TextEditorTabContextHelpController.cpp
    src/app/text_editor/TextEditorTabCursorController.cpp
    src/app/text_editor/TextEditorTabDocumentController.cpp
    src/app/text_editor/TextEditorTabEncodingController.cpp
    src/app/text_editor/TextEditorTabInteractionController.cpp
    src/app/text_editor/TextEditorTabModeController.cpp
    src/app/text_editor/TextEditorTabSourceRewriteController.cpp
    src/app/text_editor/TextEditorTabStatusController.cpp
    src/app/text_editor/TextEditorTab.h
    src/editor/TherionSyntaxHighlighter.cpp
    src/editor/TherionSyntaxHighlighter.h
    src/editor/ValidationSeverityStyle.h
)

set(THERION_APPLICATION_BOOTSTRAP_SOURCES
    src/platform/ApplicationAppearanceBootstrap.cpp
    src/platform/ApplicationAppearanceBootstrap.h
    src/platform/ApplicationBootstrap.cpp
    src/platform/ApplicationBootstrap.h
    src/platform/ApplicationStartupBootstrap.cpp
    src/platform/ApplicationStartupBootstrap.h
    src/main.cpp
)

qt_add_executable(TherionStudio MACOSX_BUNDLE
    ${THERION_MAIN_WINDOW_SOURCES}
    ${THERION_TEXT_EDITOR_SHARED_SOURCES}
    ${THERION_MAP_EDITOR_SOURCES}
    ${THERION_TEXT_EDITOR_TAB_SOURCES}
    ${THERION_APPLICATION_BOOTSTRAP_SOURCES})

if(WIN32)
    set_target_properties(TherionStudio PROPERTIES
        WIN32_EXECUTABLE TRUE)
endif()

set(THERION_STUDIO_APP_ICON_SVG
    resources/app/app-icon.svg)
set(THERION_STUDIO_APP_ICON_PNG
    resources/app/app-icon.png)
set(THERION_STUDIO_MACOS_ICON
    resources/app/TherionStudio.icns)
set(THERION_STUDIO_WINDOWS_ICON
    resources/app/TherionStudio.ico)

file(GLOB THERION_MAP_OBJECT_STYLE_FILES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "resources/map_object_styles/*.json")
list(SORT THERION_MAP_OBJECT_STYLE_FILES)

if(APPLE)
    set_source_files_properties(${THERION_STUDIO_MACOS_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(TherionStudio PRIVATE
        ${THERION_STUDIO_MACOS_ICON})
endif()

if(WIN32)
    set(THERION_STUDIO_WINDOWS_ICON_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/${THERION_STUDIO_WINDOWS_ICON}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows/TherionStudio.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/TherionStudio.rc"
        @ONLY)
    target_sources(TherionStudio PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/TherionStudio.rc")
endif()

set(THERION_STUDIO_RESOURCE_FILES
    ${THERION_STUDIO_APP_ICON_SVG}
    ${THERION_STUDIO_APP_ICON_PNG}
    ${THERION_MAP_OBJECT_STYLE_FILES}
    resources/therion_command_catalog.json
    resources/therion_syntax_palette.json
    resources/project_templates/default/template.json
    resources/project_templates/default/main.thconfig
    resources/project_templates/default/main.th
    resources/project_templates/default/surveys/survey1.th
    resources/project_templates/default/scraps/scrap1.th2
    resources/icons/lucide/LICENSE
    resources/icons/lucide/check.svg
    resources/icons/lucide/code.svg
    resources/icons/lucide/cog.svg
    resources/icons/lucide/compass.svg
    resources/icons/lucide/copy.svg
    resources/icons/lucide/external-link.svg
    resources/icons/lucide/grip-vertical.svg
    resources/icons/lucide/eye.svg
    resources/icons/lucide/eye-off.svg
    resources/icons/lucide/file-plus.svg
    resources/icons/lucide/folder-open.svg
    resources/icons/lucide/folder-tree.svg
    resources/icons/lucide/folder-x.svg
    resources/icons/lucide/hand.svg
    resources/icons/lucide/house.svg
    resources/icons/lucide/image.svg
    resources/icons/lucide/spell-check.svg
    resources/icons/lucide/map-pin.svg
    resources/icons/lucide/locate-fixed.svg
    resources/icons/lucide/map.svg
    resources/icons/lucide/mouse-pointer-2.svg
    resources/icons/lucide/network.svg
    resources/icons/lucide/pen-tool.svg
    resources/icons/lucide/pencil-line.svg
    resources/icons/lucide/pentagon.svg
    resources/icons/lucide/puzzle.svg
    resources/icons/lucide/ruler.svg
    resources/icons/lucide/redo-2.svg
    resources/icons/lucide/save.svg
    resources/icons/lucide/scan.svg
    resources/icons/lucide/search.svg
    resources/icons/lucide/screen-share.svg
    resources/icons/lucide/monitor-x.svg
    resources/icons/lucide/spline.svg
    resources/icons/lucide/square.svg
    resources/icons/lucide/square-dashed-mouse-pointer.svg
    resources/icons/lucide/square-plus.svg
    resources/icons/lucide/toy-brick.svg
    resources/icons/lucide/trash-2.svg
    resources/icons/lucide/undo-2.svg
    resources/icons/lucide/play.svg
    resources/icons/lucide/arrow-big-down.svg
    resources/icons/lucide/arrow-big-right.svg
    resources/icons/lucide/view-top.svg
    resources/icons/lucide/view-side.svg
    resources/icons/lucide/view-roll-left.svg
    resources/icons/lucide/view-roll-right.svg
    resources/icons/lucide/rotate-ccw.svg
    resources/icons/lucide/rotate-cw.svg
    resources/icons/lucide/zoom-in.svg
    resources/icons/lucide/zoom-out.svg
    resources/qml/three_d_viewer/ThreeDViewerViewport.qml
    resources/qml/three_d_viewer/ThreeDViewerInspector.qml)

qt_add_resources(TherionStudio "TherionStudioResources"
    PREFIX "/"
    FILES
        ${THERION_STUDIO_RESOURCE_FILES})

qt_add_executable(MapObjectStyleGallery
    tools/MapObjectStyleGallery.cpp
    src/app/text_editor/map_editor/MapEditorInspectorData.cpp
    src/app/text_editor/map_editor/MapEditorInspectorData.h
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h
    src/app/text_editor/map_editor/MapEditorPointSymbolGeometry.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorSceneSupport.h
    src/app/text_editor/map_editor/MapEditorStylePreviewWidget.cpp
    src/app/text_editor/map_editor/MapEditorStylePreviewWidget.h)

set_target_properties(MapObjectStyleGallery PROPERTIES EXCLUDE_FROM_ALL TRUE)

target_link_libraries(MapObjectStyleGallery PRIVATE therion_core Qt6::Widgets Qt6::Svg)

qt_add_resources(MapObjectStyleGallery "MapObjectStyleGalleryResources"
    PREFIX "/"
    FILES
        resources/therion_command_catalog.json
        ${THERION_MAP_OBJECT_STYLE_FILES})

add_custom_target(map_object_style_gallery
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen
            $<TARGET_FILE:MapObjectStyleGallery>
            --output "${CMAKE_BINARY_DIR}/style_gallery"
    DEPENDS MapObjectStyleGallery
    COMMENT "Generating map object style gallery in ${CMAKE_BINARY_DIR}/style_gallery")

set(THERION_STUDIO_TRANSLATION_FILES
    translations/therion_studio_cs.ts
    translations/therion_studio_sk.ts)

set(THERION_STUDIO_STAGED_TRANSLATION_FILES
    translations/therion_studio_de.ts
    translations/therion_studio_es.ts
    translations/therion_studio_fr.ts
    translations/therion_studio_it.ts
    translations/therion_studio_pt.ts)

if(COMMAND qt_add_translations AND Qt6LinguistTools_FOUND)
    qt_add_translations(TherionStudio
        TS_FILES
            ${THERION_STUDIO_TRANSLATION_FILES}
        RESOURCE_PREFIX "/i18n")
else()
    message(STATUS "Qt LinguistTools not found; translation catalog compilation is disabled")
endif()

if(APPLE)
    set(THERION_MACOS_MINIMUM_SYSTEM_VERSION "12.0" CACHE STRING
        "Minimum macOS version declared in the Therion Studio app bundle")
    set_target_properties(TherionStudio PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Therion Studio"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.lblazek.therionstudio"
        MACOSX_BUNDLE_ICON_FILE "TherionStudio.icns"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

target_link_libraries(TherionStudio PRIVATE therion_app therion_core therion_platform Qt6::Widgets Qt6::Svg Qt6::Concurrent)

target_compile_definitions(TherionStudio PRIVATE
    THERION_STUDIO_VERSION_STRING="${PROJECT_VERSION}"
    THERION_STUDIO_PACKAGE_LABEL_STRING="${THERION_STUDIO_PACKAGE_LABEL}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII)

install(TARGETS TherionStudio
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
        docs/USER_MANUAL.md
        docs/USER_MANUAL.cs.md
        docs/USER_MANUAL.sk.md
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/therion-studio")

if(UNIX AND NOT APPLE)
    set(THERION_STUDIO_LINUX_DESKTOP_ID "therion-studio" CACHE STRING
        "Linux desktop file and AppStream application ID, without the .desktop/.metainfo.xml suffix")
    set(THERION_STUDIO_LINUX_ICON_ID "${THERION_STUDIO_LINUX_DESKTOP_ID}" CACHE STRING
        "Linux icon ID installed under hicolor theme, without the .png suffix")
    foreach(_therion_linux_id IN ITEMS
            THERION_STUDIO_LINUX_DESKTOP_ID
            THERION_STUDIO_LINUX_ICON_ID)
        if(NOT "${${_therion_linux_id}}" MATCHES "^[A-Za-z0-9._-]+$")
            message(FATAL_ERROR
                "${_therion_linux_id} may contain only letters, numbers, dots, underscores, and hyphens")
        endif()
    endforeach()

    set(THERION_STUDIO_LINUX_DESKTOP_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/TherionStudio.desktop")
    set(THERION_STUDIO_LINUX_METAINFO_FILE
        "${CMAKE_CURRENT_BINARY_DIR}/TherionStudio.metainfo.xml")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/TherionStudio.desktop.in"
        "${THERION_STUDIO_LINUX_DESKTOP_FILE}"
        @ONLY)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux/TherionStudio.metainfo.xml.in"
        "${THERION_STUDIO_LINUX_METAINFO_FILE}"
        @ONLY)

    install(FILES "${THERION_STUDIO_LINUX_DESKTOP_FILE}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
        RENAME "${THERION_STUDIO_LINUX_DESKTOP_ID}.desktop")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${THERION_STUDIO_APP_ICON_PNG}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
        RENAME "${THERION_STUDIO_LINUX_ICON_ID}.png")
    install(FILES "${THERION_STUDIO_LINUX_METAINFO_FILE}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
        RENAME "${THERION_STUDIO_LINUX_DESKTOP_ID}.metainfo.xml")
endif()

function(therion_generate_deploy_app_script target output_var)
    if(Qt6Core_VERSION VERSION_LESS 6.5)
        qt_generate_deploy_app_script(
            TARGET "${target}"
            FILENAME_VARIABLE deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR)
    else()
        qt_generate_deploy_app_script(
            TARGET "${target}"
            OUTPUT_SCRIPT deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR)
    endif()
    set("${output_var}" "${deploy_script}" PARENT_SCOPE)
endfunction()

if(UNIX AND NOT APPLE AND THERION_ENABLE_QT_LINUX_DEPLOY_INSTALL)
    if(Qt6Core_VERSION VERSION_LESS 6.5)
        message(FATAL_ERROR
            "THERION_ENABLE_QT_LINUX_DEPLOY_INSTALL requires Qt 6.5 or newer. "
            "The Linux packaging workflow configures AppImage builds in the "
            "Debian 13 container used for Linux CMake deployment.")
    endif()
    therion_generate_deploy_app_script(TherionStudio THERION_STUDIO_QT_DEPLOY_SCRIPT)
    install(SCRIPT ${THERION_STUDIO_QT_DEPLOY_SCRIPT})
endif()

if(WIN32)
    therion_generate_deploy_app_script(TherionStudio THERION_STUDIO_QT_DEPLOY_SCRIPT)
    install(SCRIPT ${THERION_STUDIO_QT_DEPLOY_SCRIPT})
    if(TARGET Qt6::windeployqt)
        get_target_property(THERION_STUDIO_WINDEPLOYQT_EXECUTABLE Qt6::windeployqt IMPORTED_LOCATION)
    endif()
    if(NOT THERION_STUDIO_WINDEPLOYQT_EXECUTABLE)
        find_program(THERION_STUDIO_WINDEPLOYQT_EXECUTABLE NAMES windeployqt)
    endif()
    if(THERION_STUDIO_WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            set(_therion_studio_windows_executable \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/TherionStudio.exe\")
            set(_therion_studio_windows_runtime_dir \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
            if(NOT EXISTS \"\${_therion_studio_windows_executable}\")
                message(FATAL_ERROR \"Windows Qt runtime deployment failed: missing installed executable \${_therion_studio_windows_executable}\")
            endif()
            execute_process(
                COMMAND \"${THERION_STUDIO_WINDEPLOYQT_EXECUTABLE}\"
                    --dir \"\${_therion_studio_windows_runtime_dir}\"
                    \"\${_therion_studio_windows_executable}\"
                RESULT_VARIABLE _therion_studio_windeployqt_result)
            if(NOT _therion_studio_windeployqt_result EQUAL 0)
                message(FATAL_ERROR \"windeployqt failed with exit code \${_therion_studio_windeployqt_result}\")
            endif()
        ")
    else()
        message(WARNING
            "Qt6::windeployqt target is not available; "
            "Windows install layout relies on Qt's deploy script only")
    endif()
    if(TARGET Qt6::QWindowsIntegrationPlugin)
        install(IMPORTED_RUNTIME_ARTIFACTS Qt6::QWindowsIntegrationPlugin
            LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/platforms"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/platforms")
    else()
        message(WARNING
            "Qt6::QWindowsIntegrationPlugin target is not available; "
            "Windows install layout relies on the Qt deploy script to provide qwindows.dll")
    endif()

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME "Therion Studio")
    set(CPACK_PACKAGE_VENDOR "Therion Studio")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "TherionStudio-${THERION_STUDIO_PACKAGE_LABEL}-Windows-x86_64")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Therion Studio desktop editor")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Therion Studio")
    set(CPACK_PACKAGE_EXECUTABLES "TherionStudio" "Therion Studio")
    set(CPACK_CREATE_DESKTOP_LINKS "TherionStudio")
    set(CPACK_NSIS_DISPLAY_NAME "Therion Studio")
    set(CPACK_NSIS_PACKAGE_NAME "Therion Studio")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/${THERION_STUDIO_WINDOWS_ICON}")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/${THERION_STUDIO_WINDOWS_ICON}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "${CMAKE_INSTALL_BINDIR}/TherionStudio.exe")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${CMAKE_INSTALL_BINDIR}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "therion-studio")
    set(CPACK_PACKAGE_VENDOR "Therion Studio")
    set(CPACK_PACKAGE_CONTACT "Therion Studio maintainers")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_VERSION "${THERION_STUDIO_DEBIAN_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "therion-studio-${THERION_STUDIO_PACKAGE_LABEL}-${THERION_STUDIO_DEB_PLATFORM_LABEL}-${THERION_STUDIO_DEB_ARCHITECTURE_LABEL}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Therion Studio desktop editor")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Therion Studio maintainers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

if(WIN32 OR (UNIX AND NOT APPLE))
    include(CPack)
endif()

therion_add_core_unit_test(TherionDocumentEditorTest
    tests/TherionDocumentEditorTest.cpp)

therion_add_core_unit_test(TherionSourceValidatorTest
    tests/TherionSourceValidatorTest.cpp)

therion_add_core_unit_test(TherionDocumentParserTest
    tests/TherionDocumentParserTest.cpp)

therion_add_core_qtest_runner(TherionCoreQTests
    tests/core/CoreQTestMain.cpp
    tests/core/ThreeDViewerCameraTest.cpp
    tests/core/TherionFileTypesTest.cpp
    tests/core/ThreeDViewerLoxLoaderTest.cpp
    tests/core/ThreeDViewerSceneModelTest.cpp
    tests/core/ThreeDViewerSceneStatisticsTest.cpp
    tests/core/TherionSourceDocumentTest.cpp
    tests/core/TherionSourceLogicalDocumentTest.cpp
    tests/core/TherionSourceSnapshotCacheTest.cpp
    tests/core/TherionSourceValidatorProjectionTest.cpp
    tests/core/TherionSourceTextTest.cpp
    tests/core/TherionTokenRulesTest.cpp)

therion_add_core_unit_test(CommandCatalogStoreTest
    tests/CommandCatalogStoreTest.cpp)

therion_add_core_unit_test(CommandOptionParserTest
    tests/CommandOptionParserTest.cpp)

therion_add_core_unit_test(TherionProjectStructureIndexTest
    tests/ProjectStructureIndexTest.cpp)

therion_add_core_unit_test(MapBackgroundPlacementTest
    tests/MapBackgroundPlacementTest.cpp)

therion_add_core_unit_test(TherionBackgroundMetadataTest
    tests/TherionBackgroundMetadataTest.cpp)

therion_add_core_unit_test(PocketTopoImportTest
    tests/PocketTopoImportTest.cpp)

therion_add_core_unit_test(TherionXviParserTest
    tests/TherionXviParserTest.cpp)

qt_add_executable(ApplicationIdentityTest
    tests/ApplicationIdentityTest.cpp)

target_link_libraries(ApplicationIdentityTest PRIVATE therion_platform)

qt_add_executable(MapGeometryFeatureParsingTest
    tests/MapGeometryFeatureParsingTest.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h
    src/app/text_editor/map_editor/MapEditorLineDecorationItem.cpp
    src/app/text_editor/map_editor/MapEditorLineDecorationItem.h
    src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.cpp
    src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.h
    src/app/text_editor/map_editor/MapEditorPointSymbolGeometry.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorReferencedAreaResolver.cpp
    src/app/text_editor/map_editor/MapEditorReferencedAreaResolver.h
    src/app/text_editor/map_editor/MapEditorSmartAreaPlanner.cpp
    src/app/text_editor/map_editor/MapEditorSmartAreaPlanner.h
    src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h)

target_link_libraries(MapGeometryFeatureParsingTest PRIVATE therion_core Qt6::Widgets)

qt_add_executable(MapEditorSourceReferenceResolverTest
    tests/MapEditorSourceReferenceResolverTest.cpp
    src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.cpp
    src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.h
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h
    src/app/text_editor/map_editor/MapEditorLineDecorationItem.cpp
    src/app/text_editor/map_editor/MapEditorLineDecorationItem.h
    src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.cpp
    src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.h
    src/app/text_editor/map_editor/MapEditorPointSymbolGeometry.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorReferencedAreaResolver.cpp
    src/app/text_editor/map_editor/MapEditorReferencedAreaResolver.h
    src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h)

target_link_libraries(MapEditorSourceReferenceResolverTest PRIVATE therion_core Qt6::Widgets)

therion_add_core_unit_test(DocumentFileEncodingTest
    tests/DocumentFileEncodingTest.cpp)

qt_add_executable(MapEditorInputPolicyTest
    tests/MapEditorInputPolicyTest.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.h)

target_link_libraries(MapEditorInputPolicyTest PRIVATE Qt6::Widgets)

qt_add_executable(MapEditorLogicQTests
    tests/MapEditorLogicQTestMain.cpp
    tests/MapEditorAreaReferenceResolverTest.cpp
    tests/MapEditorObjectDeletePlannerTest.cpp
    tests/MapEditorObjectMovePlannerTest.cpp
    tests/MapEditorPointSymbolGeometryTest.cpp
    tests/MapEditorUndoArbitrationServiceTest.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.h
    src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.cpp
    src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.h
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.cpp
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.h
    src/app/text_editor/map_editor/MapEditorPointSymbolGeometry.h
    src/app/text_editor/map_editor/MapEditorUndoArbitrationService.cpp
    src/app/text_editor/map_editor/MapEditorUndoArbitrationService.h)

target_link_libraries(MapEditorLogicQTests PRIVATE therion_core Qt6::Core Qt6::Gui Qt6::Test)

qt_add_executable(ThreeDViewerQTests
    tests/ThreeDViewerQTestMain.cpp
    tests/ThreeDViewerProjectionTest.cpp
    tests/ThreeDViewerLayerListModelTest.cpp
    tests/ThreeDViewerInspectorStateTest.cpp
    tests/ThreeDViewerInspectorWidgetTest.cpp
    tests/ThreeDViewerViewportWidgetTest.cpp
    tests/ThreeDViewerViewportControllerTest.cpp
    src/app/three_d_viewer/ThreeDViewerLayerListModel.cpp
    src/app/three_d_viewer/ThreeDViewerLayerListModel.h
    src/app/three_d_viewer/ThreeDViewerInspectorState.cpp
    src/app/three_d_viewer/ThreeDViewerInspectorState.h
    src/app/three_d_viewer/ThreeDViewerMeshColorMode.h
    src/app/three_d_viewer/ThreeDViewerInspectorWidget.cpp
    src/app/three_d_viewer/ThreeDViewerInspectorWidget.h
    src/app/three_d_viewer/ThreeDViewerViewportItem.cpp
    src/app/three_d_viewer/ThreeDViewerViewportItem.h
    src/app/three_d_viewer/ThreeDViewerViewportController.cpp
    src/app/three_d_viewer/ThreeDViewerViewportController.h
    src/app/three_d_viewer/ThreeDViewerProjection.cpp
    src/app/three_d_viewer/ThreeDViewerProjection.h
    src/app/three_d_viewer/ThreeDViewerViewportWidget.cpp
    src/app/three_d_viewer/ThreeDViewerViewportWidget.h)

target_link_libraries(ThreeDViewerQTests PRIVATE therion_core Qt6::Gui Qt6::Svg Qt6::Widgets Qt6::Quick Qt6::QuickWidgets Qt6::Qml Qt6::QuickControls2 Qt6::Test)

qt_add_resources(ThreeDViewerQTests ThreeDViewerQTestsResources
    PREFIX "/"
    FILES
        resources/qml/three_d_viewer/ThreeDViewerViewport.qml
        resources/qml/three_d_viewer/ThreeDViewerInspector.qml)

qt_add_executable(MapEditorObjectStyleCatalogTest
    tests/MapEditorObjectStyleCatalogTest.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h)

target_link_libraries(MapEditorObjectStyleCatalogTest PRIVATE Qt6::Core Qt6::Gui)

qt_add_resources(MapEditorObjectStyleCatalogTest MapEditorObjectStyleCatalogResources
    PREFIX "/"
    FILES
        ${THERION_MAP_OBJECT_STYLE_FILES})

qt_add_executable(MapEditorInspectorDataTest
    tests/MapEditorInspectorDataTest.cpp
    src/app/text_editor/map_editor/MapEditorInspectorData.cpp
    src/app/text_editor/map_editor/MapEditorInspectorData.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h)

target_link_libraries(MapEditorInspectorDataTest PRIVATE therion_core Qt6::Widgets Qt6::Svg)

qt_add_resources(MapEditorInspectorDataTest MapEditorInspectorDataResources
    PREFIX "/"
    FILES
        resources/therion_command_catalog.json)

qt_add_executable(MapEditorObjectDetailsLogicTest
    tests/MapEditorObjectDetailsLogicTest.cpp
    src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.cpp
    src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.h)

target_link_libraries(MapEditorObjectDetailsLogicTest PRIVATE therion_core Qt6::Core)

qt_add_executable(MapEditorLineSplitPlannerTest
    tests/MapEditorLineSplitPlannerTest.cpp
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.cpp
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.h)

target_link_libraries(MapEditorLineSplitPlannerTest PRIVATE therion_core Qt6::Core)

qt_add_executable(MapEditorLineExtensionPlannerTest
    tests/MapEditorLineExtensionPlannerTest.cpp
    src/app/text_editor/map_editor/MapEditorLineExtensionPlanner.cpp
    src/app/text_editor/map_editor/MapEditorLineExtensionPlanner.h
    src/app/text_editor/map_editor/MapEditorSceneSupport.h
    src/app/text_editor/map_editor/MapEditorInteractiveDrawLogic.h)

target_link_libraries(MapEditorLineExtensionPlannerTest PRIVATE Qt6::Widgets Qt6::Gui)

therion_add_core_unit_test(SessionStoreTest
    tests/SessionStoreTest.cpp)

therion_add_app_unit_test(TextEditorOptionValidationTest
    tests/TextEditorOptionValidationTest.cpp
    src/app/text_editor/TextEditorOptionValidation.cpp)

therion_add_app_unit_test(ProjectStructureScannerTest
    tests/ProjectStructureScannerTest.cpp)

therion_add_app_unit_test(ProjectSearchScannerTest
    tests/ProjectSearchScannerTest.cpp)

therion_add_app_unit_test(ProjectValidationScannerTest
    tests/ProjectValidationScannerTest.cpp
    src/app/text_editor/TextEditorValidationCatalog.cpp
    src/app/text_editor/raw_editor/RawEditorCommandMetadataLoader.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp)

therion_add_app_unit_test(ProjectValidationControllerTest
    tests/ProjectValidationControllerTest.cpp)

qt_add_executable(TherionRunnerSupportQTests
    tests/TherionRunnerSupportQTestMain.cpp
    tests/TherionExecutableSelectionControllerTest.cpp
    tests/TherionRunnerConfigDisplayControllerTest.cpp
    tests/TherionRunnerConfigResolverTest.cpp
    tests/TherionRunnerPresenterTest.cpp)

target_link_libraries(TherionRunnerSupportQTests PRIVATE therion_app Qt6::Test)

therion_add_app_unit_test(TherionRunnerServiceTest
    tests/TherionRunnerServiceTest.cpp)

therion_add_app_unit_test(MainWindowTherionRunnerControllerTest
    tests/MainWindowTherionRunnerControllerTest.cpp)

therion_add_app_unit_test(MainWindowDocumentControllerTest
    tests/MainWindowDocumentControllerTest.cpp)

therion_add_app_unit_test(MainWindowDocumentOpenControllerTest
    tests/MainWindowDocumentOpenControllerTest.cpp)

therion_add_app_unit_test(MainWindowDocumentTabOpenControllerTest
    tests/MainWindowDocumentTabOpenControllerTest.cpp)

therion_add_app_unit_test(MainWindowSessionDocumentControllerTest
    tests/MainWindowSessionDocumentControllerTest.cpp)

therion_add_app_unit_test(MainWindowSessionControllerTest
    tests/MainWindowSessionControllerTest.cpp)

therion_add_app_unit_test(MainWindowProjectControllerTest
    tests/MainWindowProjectControllerTest.cpp)

qt_add_executable(MainWindowServiceQTests
    tests/MainWindowServiceQTestMain.cpp
    tests/MainWindowProjectLifecycleServiceTest.cpp
    tests/MainWindowProjectOrchestrationServiceTest.cpp
    tests/MainWindowProjectUiFlowServiceTest.cpp
    tests/MainWindowProjectWorkspaceServiceTest.cpp
    tests/MainWindowSessionDocumentServiceTest.cpp
    tests/MainWindowSessionProjectServiceTest.cpp
    tests/MainWindowSessionRestoreOrchestrationServiceTest.cpp
    tests/MainWindowSessionRestoreUiFlowServiceTest.cpp
    tests/MainWindowSessionStateServiceTest.cpp
    tests/MainWindowSessionWindowRestoreServiceTest.cpp
    tests/MainWindowStructureNameOverridesServiceTest.cpp)

target_link_libraries(MainWindowServiceQTests PRIVATE therion_app Qt6::Test)

therion_add_app_unit_test(ProjectTemplateServiceTest
    tests/ProjectTemplateServiceTest.cpp)

therion_add_app_unit_test(MainWindowRecentProjectsServiceTest
    tests/MainWindowRecentProjectsServiceTest.cpp)

therion_add_app_unit_test(MainWindowRecentFilesServiceTest
    tests/MainWindowRecentFilesServiceTest.cpp)

therion_add_app_unit_test(MainWindowValidationFixApplyServiceTest
    tests/MainWindowValidationFixApplyServiceTest.cpp)

qt_add_executable(BlockEditorApplyExecutorTest
    tests/BlockEditorApplyExecutorTest.cpp
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorApplyExecutor.h
    src/app/text_editor/block_editor/BlockEditorSourceController.cpp
    src/app/text_editor/block_editor/BlockEditorSourceController.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h)

target_link_libraries(BlockEditorApplyExecutorTest PRIVATE Qt6::Widgets)

qt_add_executable(BlockEditorDocumentOutlineBuilderTest
    tests/BlockEditorDocumentOutlineBuilderTest.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.cpp
    src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h)

target_link_libraries(BlockEditorDocumentOutlineBuilderTest PRIVATE therion_core Qt6::Core)

qt_add_executable(BlockEditorMoveSourceRewriterTest
    tests/BlockEditorMoveSourceRewriterTest.cpp
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.cpp
    src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h)

target_link_libraries(BlockEditorMoveSourceRewriterTest PRIVATE therion_core Qt6::Core)

qt_add_executable(BlockEditorCanvasRebuildControllerTest
    tests/BlockEditorCanvasRebuildControllerTest.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.h
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasRebuildController.h
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorSourceController.cpp
    src/app/text_editor/block_editor/BlockEditorSourceController.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h)

target_link_libraries(BlockEditorCanvasRebuildControllerTest PRIVATE therion_core Qt6::Widgets Qt6::Svg)

qt_add_executable(BlockEditorToolboxControllerTest
    tests/BlockEditorToolboxControllerTest.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.cpp
    src/app/text_editor/block_editor/BlockEditorCanvasItem.h
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.cpp
    src/app/text_editor/block_editor/BlockEditorDirectiveRules.h
    src/app/text_editor/block_editor/BlockEditorSourceText.cpp
    src/app/text_editor/block_editor/BlockEditorSourceText.h
    src/app/text_editor/block_editor/BlockEditorToolboxController.cpp
    src/app/text_editor/block_editor/BlockEditorToolboxController.h)

target_link_libraries(BlockEditorToolboxControllerTest PRIVATE therion_core Qt6::Widgets Qt6::Svg)

set(THERION_EDITOR_UI_TEST_BASE_SOURCES
    ${THERION_APP_UI_STYLE_SOURCES}
    ${THERION_TEXT_EDITOR_SHARED_SOURCES})

set(THERION_TEXT_EDITOR_UI_TEST_SOURCES
    ${THERION_EDITOR_UI_TEST_BASE_SOURCES}
    ${THERION_TEXT_EDITOR_TAB_SOURCES})

set(THERION_TEXT_EDITOR_UI_TEST_RESOURCE_FILES
    resources/therion_command_catalog.json
    resources/therion_syntax_palette.json)

add_library(therion_text_editor_ui_test_support STATIC
    ${THERION_TEXT_EDITOR_UI_TEST_SOURCES})

target_link_libraries(therion_text_editor_ui_test_support
    PUBLIC
        therion_core
        Qt6::Widgets
        Qt6::Svg)

function(therion_add_text_editor_ui_test target_name test_source resource_name)
    qt_add_executable(${target_name}
        ${test_source})

    target_link_libraries(${target_name} PRIVATE therion_text_editor_ui_test_support)

    qt_add_resources(${target_name} ${resource_name}
        PREFIX "/"
        FILES
            ${THERION_TEXT_EDITOR_UI_TEST_RESOURCE_FILES})
endfunction()

therion_add_text_editor_ui_test(TextEditorCaretInteractionTest
    tests/TextEditorCaretInteractionTest.cpp
    "TextEditorCaretInteractionResources")

therion_add_text_editor_ui_test(TextEditorCompletionHighlightTest
    tests/TextEditorCompletionHighlightTest.cpp
    "TextEditorCompletionHighlightResources")

therion_add_text_editor_ui_test(TextEditorSourceTransactionControllerTest
    tests/TextEditorSourceTransactionControllerTest.cpp
    "TextEditorSourceTransactionControllerResources")

therion_add_text_editor_ui_test(TextEditorValidationFixesTransactionTest
    tests/TextEditorValidationFixesTransactionTest.cpp
    "TextEditorValidationFixesTransactionResources")

therion_add_text_editor_ui_test(TextEditorImportInsertionTransactionTest
    tests/TextEditorImportInsertionTransactionTest.cpp
    "TextEditorImportInsertionTransactionResources")

therion_add_text_editor_ui_test(TextEditorSystemNormalizationTransactionTest
    tests/TextEditorSystemNormalizationTransactionTest.cpp
    "TextEditorSystemNormalizationTransactionResources")

qt_add_executable(TextEditorDocumentControllerTest
    tests/TextEditorDocumentControllerTest.cpp
    src/app/text_editor/TextEditorDocumentController.cpp
    src/app/text_editor/TextEditorDocumentController.h
    src/app/text_editor/TextEditorDocumentIoService.cpp
    src/app/text_editor/TextEditorDocumentIoService.h
    src/app/text_editor/TextEditorDocumentPreconditionsService.cpp
    src/app/text_editor/TextEditorDocumentPreconditionsService.h
    src/app/text_editor/TextEditorDocumentPersistenceStateService.cpp
    src/app/text_editor/TextEditorDocumentPersistenceStateService.h
    src/app/text_editor/TextEditorDocumentWorkflowController.cpp
    src/app/text_editor/TextEditorDocumentWorkflowController.h)

target_link_libraries(TextEditorDocumentControllerTest PRIVATE therion_core Qt6::Widgets)

qt_add_executable(TextEditorDocumentServiceQTests
    tests/TextEditorDocumentServiceQTestMain.cpp
    tests/TextEditorDocumentIoServiceTest.cpp
    tests/TextEditorDocumentPersistenceStateServiceTest.cpp
    tests/TextEditorDocumentPreconditionsServiceTest.cpp
    tests/TextEditorDocumentWorkflowControllerTest.cpp
    src/app/text_editor/TextEditorDocumentIoService.cpp
    src/app/text_editor/TextEditorDocumentIoService.h
    src/app/text_editor/TextEditorDocumentPersistenceStateService.cpp
    src/app/text_editor/TextEditorDocumentPersistenceStateService.h
    src/app/text_editor/TextEditorDocumentPreconditionsService.cpp
    src/app/text_editor/TextEditorDocumentPreconditionsService.h
    src/app/text_editor/TextEditorDocumentWorkflowController.cpp
    src/app/text_editor/TextEditorDocumentWorkflowController.h)

target_link_libraries(TextEditorDocumentServiceQTests PRIVATE therion_core Qt6::Core Qt6::Test)

qt_add_executable(TextEditorTabInteractionControllerTest
    tests/TextEditorTabInteractionControllerTest.cpp
    src/app/text_editor/TextEditorTabInteractionController.cpp
    src/app/text_editor/TextEditorTabInteractionController.h)

target_link_libraries(TextEditorTabInteractionControllerTest PRIVATE therion_core Qt6::Core Qt6::Widgets)

qt_add_executable(MainWindowHelpDocumentTest
    tests/MainWindowHelpDocumentTest.cpp
    src/app/MainWindowHelpDocument.cpp
    src/app/MainWindowHelpDocument.h)

target_link_libraries(MainWindowHelpDocumentTest PRIVATE Qt6::Core Qt6::Gui)

set(THERION_MAP_EDITOR_UI_TEST_SOURCES
    ${THERION_MAP_EDITOR_SOURCES})

add_library(therion_map_editor_ui_test_support STATIC
    ${THERION_MAP_EDITOR_UI_TEST_SOURCES})

target_link_libraries(therion_map_editor_ui_test_support
    PUBLIC
        therion_text_editor_ui_test_support
        Qt6::Concurrent
        Qt6::Svg)

function(therion_add_map_editor_ui_test target_name test_source resource_name)
    qt_add_executable(${target_name}
        ${test_source})

    target_link_libraries(${target_name} PRIVATE therion_map_editor_ui_test_support)

    qt_add_resources(${target_name} ${resource_name}
        PREFIX "/"
        FILES
            ${THERION_STUDIO_RESOURCE_FILES})
endfunction()

therion_add_map_editor_ui_test(MapEditorDetachedPaneTest
    tests/MapEditorDetachedPaneTest.cpp
    "MapEditorDetachedPaneResources")

therion_add_map_editor_ui_test(MapEditorDragUndoRedoSmokeTest
    tests/MapEditorDragUndoRedoSmokeTest.cpp
    "MapEditorDragUndoRedoSmokeResources")

therion_add_map_editor_ui_test(MapEditorSplitAreaBorderSmokeTest
    tests/MapEditorSplitAreaBorderSmokeTest.cpp
    "MapEditorSplitAreaBorderSmokeResources")

therion_add_map_editor_ui_test(MapEditorSelectionPanelTypeOptionsTest
    tests/MapEditorSelectionPanelTypeOptionsTest.cpp
    "MapEditorSelectionPanelTypeOptionsResources")

therion_add_map_editor_ui_test(MapEditorBackgroundLayerVisibilityTest
    tests/MapEditorBackgroundLayerVisibilityTest.cpp
    "MapEditorBackgroundLayerVisibilityResources")

therion_add_map_editor_ui_test(MapEditorViewportInputControllerTest
    tests/MapEditorViewportInputControllerTest.cpp
    "MapEditorViewportInputControllerResources")

therion_add_map_editor_ui_test(MapEditorCanvasEditSourceTransactionTest
    tests/MapEditorCanvasEditSourceTransactionTest.cpp
    "MapEditorCanvasEditSourceTransactionResources")

therion_add_map_editor_ui_test(MapEditorObjectDetailsQuickFieldsTransactionTest
    tests/MapEditorObjectDetailsQuickFieldsTransactionTest.cpp
    "MapEditorObjectDetailsQuickFieldsTransactionResources")

therion_add_map_editor_ui_test(MapEditorInteractiveDrawTransactionTest
    tests/MapEditorInteractiveDrawTransactionTest.cpp
    "MapEditorInteractiveDrawTransactionResources")

therion_add_map_editor_ui_test(MapEditorLineExtensionTransactionTest
    tests/MapEditorLineExtensionTransactionTest.cpp
    "MapEditorLineExtensionTransactionResources")
target_compile_definitions(MapEditorLineExtensionTransactionTest PRIVATE
    THERION_STUDIO_TESTING)
target_link_libraries(MapEditorLineExtensionTransactionTest PRIVATE
    Qt6::Test)

set(THERION_UNIT_TEST_TARGETS
    CommandCatalogStoreTest
    CommandOptionParserTest
    TherionDocumentEditorTest
    TherionSourceValidatorTest
    TherionDocumentParserTest
    TherionCoreQTests
    ApplicationIdentityTest
    TherionProjectStructureIndexTest
    MapBackgroundPlacementTest
    TherionBackgroundMetadataTest
    PocketTopoImportTest
    TherionXviParserTest
    MapGeometryFeatureParsingTest
    MapEditorSourceReferenceResolverTest
    DocumentFileEncodingTest
    MapEditorInputPolicyTest
    MapEditorLogicQTests
    ThreeDViewerQTests
    MapEditorInspectorDataTest
    MapEditorObjectStyleCatalogTest
    MapEditorObjectDetailsLogicTest
    MapEditorLineExtensionPlannerTest
    MapEditorLineSplitPlannerTest
    TextEditorDocumentControllerTest
    TextEditorDocumentServiceQTests
    TextEditorOptionValidationTest
    TextEditorTabInteractionControllerTest
    MainWindowHelpDocumentTest
    ProjectStructureScannerTest
    ProjectSearchScannerTest
    ProjectValidationScannerTest
    ProjectValidationControllerTest
    SessionStoreTest
    MainWindowServiceQTests
    MainWindowSessionDocumentControllerTest
    MainWindowSessionControllerTest
    MainWindowProjectControllerTest
    ProjectTemplateServiceTest
    MainWindowRecentFilesServiceTest
    MainWindowRecentProjectsServiceTest
    MainWindowDocumentControllerTest
    MainWindowDocumentOpenControllerTest
    MainWindowDocumentTabOpenControllerTest
    MainWindowTherionRunnerControllerTest
    BlockEditorApplyExecutorTest
    BlockEditorCanvasRebuildControllerTest
    BlockEditorDocumentOutlineBuilderTest
    BlockEditorMoveSourceRewriterTest
    BlockEditorToolboxControllerTest
    TherionRunnerSupportQTests
    TherionRunnerServiceTest
    MainWindowValidationFixApplyServiceTest)

set(THERION_UI_TEST_TARGETS
    TextEditorCaretInteractionTest
    TextEditorCompletionHighlightTest
    TextEditorSourceTransactionControllerTest
    TextEditorValidationFixesTransactionTest
    TextEditorImportInsertionTransactionTest
    TextEditorSystemNormalizationTransactionTest
    MapEditorDetachedPaneTest
    MapEditorDragUndoRedoSmokeTest
    MapEditorSplitAreaBorderSmokeTest
    MapEditorSelectionPanelTypeOptionsTest
    MapEditorBackgroundLayerVisibilityTest
    MapEditorCanvasEditSourceTransactionTest
    MapEditorObjectDetailsQuickFieldsTransactionTest
    MapEditorInteractiveDrawTransactionTest
    MapEditorLineExtensionTransactionTest
    MapEditorViewportInputControllerTest)

set(THERION_TEST_EXECUTABLE_TARGETS
    ${THERION_UNIT_TEST_TARGETS}
    ${THERION_UI_TEST_TARGETS})

set(THERION_TEST_SUPPORT_TARGETS
    therion_text_editor_ui_test_support
    therion_map_editor_ui_test_support)

add_custom_target(therion_unit_tests
    DEPENDS ${THERION_UNIT_TEST_TARGETS})

add_custom_target(therion_ui_tests
    DEPENDS ${THERION_UI_TEST_TARGETS})

foreach(test_target IN LISTS THERION_TEST_EXECUTABLE_TARGETS THERION_TEST_SUPPORT_TARGETS)
    target_compile_definitions(${test_target} PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII)
endforeach()

enable_testing()

foreach(test_target IN LISTS THERION_TEST_EXECUTABLE_TARGETS)
    add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()

add_test(NAME StructureConstraintsTest COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/check_structure_constraints.py)

set_tests_properties(
    ${THERION_TEST_EXECUTABLE_TARGETS}
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

set_tests_properties(
    ${THERION_UNIT_TEST_TARGETS}
    StructureConstraintsTest
    PROPERTIES
        LABELS "unit")

set_tests_properties(
    ${THERION_UI_TEST_TARGETS}
    PROPERTIES
        LABELS "ui")
