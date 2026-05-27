cmake_minimum_required(VERSION 3.21)

set(THERION_STUDIO_VERSION "2026.5.0" CACHE STRING
    "Therion Studio numeric application version. CI may override this from release tags.")
if(NOT THERION_STUDIO_VERSION MATCHES "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
    message(FATAL_ERROR "THERION_STUDIO_VERSION must be numeric, for example 2026.5.0")
endif()

set(THERION_STUDIO_PACKAGE_LABEL "${THERION_STUDIO_VERSION}" CACHE STRING
    "Therion Studio package/artifact label. CI may include a short commit hash for development builds.")
if(NOT THERION_STUDIO_PACKAGE_LABEL MATCHES "^[A-Za-z0-9._-]+$")
    message(FATAL_ERROR "THERION_STUDIO_PACKAGE_LABEL may contain only letters, numbers, dots, underscores, and hyphens")
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

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Svg Concurrent)
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
    src/core/CommandCatalogService.cpp
    src/core/CommandCatalogService.h
    src/core/TherionCommandSyntax.cpp
    src/core/TherionCommandSyntax.h
    src/core/DocumentFile.cpp
    src/core/DocumentFile.h
    src/core/MapBackgroundPlacement.cpp
    src/core/MapBackgroundPlacement.h
    src/core/TherionBackgroundMetadata.cpp
    src/core/TherionBackgroundMetadata.h
    src/core/TherionXviParser.cpp
    src/core/TherionXviParser.h
    src/core/TherionDocumentEditor.cpp
    src/core/TherionDocumentEditor.h
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

set(THERION_PLATFORM_SOURCES
    src/platform/PlatformPathResolver.h)

if(APPLE)
    list(APPEND THERION_PLATFORM_SOURCES src/platform/mac/MacPlatformPathResolver.cpp)
elseif(WIN32)
    list(APPEND THERION_PLATFORM_SOURCES src/platform/win/WinPlatformPathResolver.cpp)
else()
    list(APPEND THERION_PLATFORM_SOURCES src/platform/linux/LinuxPlatformPathResolver.cpp)
endif()

add_library(therion_platform STATIC
    ${THERION_PLATFORM_SOURCES})

target_link_libraries(therion_platform
    PUBLIC
        Qt6::Core)

therion_enable_strict_warnings(therion_platform)

set(THERION_APP_SOURCES
    src/app/ProjectStructureScanner.cpp
    src/app/ProjectStructureScanner.h
    src/app/TherionExecutableSelectionController.cpp
    src/app/TherionExecutableSelectionController.h
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
    src/app/TherionRunnerStartSuccessPresenter.h)

add_library(therion_app STATIC
    ${THERION_APP_SOURCES})

target_link_libraries(therion_app
    PUBLIC
        therion_core
        Qt6::Core
    PRIVATE
        therion_platform
        Qt6::Concurrent)

therion_enable_strict_warnings(therion_app)

function(therion_add_app_unit_test target_name)
    qt_add_executable(${target_name}
        ${ARGN})

    target_link_libraries(${target_name} PRIVATE therion_app)
endfunction()

qt_add_executable(TherionStudio MACOSX_BUNDLE
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/MainWindowTherionConsoleBootstrapper.cpp
    src/app/MainWindowTherionConsoleBootstrapper.h
    src/app/MainWindowTherionConsoleBuilder.cpp
    src/app/MainWindowTherionConsoleBuilder.h
    src/app/MainWindowTherionConsoleWiring.cpp
    src/app/MainWindowTherionConsoleWiring.h
    src/app/MainWindowDocumentHelpers.cpp
    src/app/MainWindowDocumentHelpers.h
    src/app/MainWindowHelpDialog.cpp
    src/app/MainWindowHelpDialog.h
    src/app/MainWindowMapBackground.cpp
    src/app/MainWindowProjectLifecycle.cpp
    src/app/MainWindowSidebar.cpp
    src/app/MainWindowStatusUi.cpp
    src/app/MainWindowStructureBrowser.cpp
    src/app/MainWindowStructureRoles.h
    src/app/MainWindowTherionConsoleController.h
    src/app/MainWindowTherionRunner.cpp
    src/app/text_editor/ContextHelpController.cpp
    src/app/text_editor/ContextHelpController.h
    src/app/text_editor/TextEditorOptionValidation.cpp
    src/app/text_editor/TextEditorCommandMetadata.h
    src/app/text_editor/TextEditorOptionValidation.h
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.cpp
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.h
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
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.h
    src/app/text_editor/block_editor/BlockEditorDetailsSupport.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsSupport.h
    src/app/text_editor/block_editor/BlockEditorConfigureController.cpp
    src/app/text_editor/block_editor/BlockEditorConfigureController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.h
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.cpp
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.h
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.h
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.cpp
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.h
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
    src/app/text_editor/TextEditorEncodingController.cpp
    src/app/text_editor/TextEditorEncodingController.h
    src/app/text_editor/TextEditorAppearanceController.cpp
    src/app/text_editor/TextEditorAppearanceController.h
    src/app/text_editor/TextEditorSourceRewriteController.cpp
    src/app/text_editor/TextEditorSourceRewriteController.h
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
    src/app/text_editor/raw_editor/RawEditorCompletionInsertionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionInsertionController.h
    src/app/text_editor/raw_editor/RawEditorTextEdit.cpp
    src/app/text_editor/raw_editor/RawEditorTextEdit.h
    src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.h
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
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.cpp
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.h
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.cpp
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.h
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h
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
    src/app/text_editor/map_editor/MapEditorTab.cpp
    src/app/text_editor/map_editor/MapEditorTabControllers.cpp
    src/app/text_editor/map_editor/MapEditorTabCanvasEditContext.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorBackgroundContext.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorObjectContext.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorObjectEvents.cpp
    src/app/text_editor/map_editor/MapEditorTabInteractiveDrawContext.cpp
    src/app/text_editor/map_editor/MapEditorTabObjectDetailsContext.cpp
    src/app/text_editor/map_editor/MapEditorTabSceneContexts.cpp
    src/app/text_editor/map_editor/MapEditorTabSelectionContext.cpp
    src/app/text_editor/map_editor/MapEditorTabViewportInputContext.cpp
    src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp
    src/app/text_editor/map_editor/MapEditorTab.h
    src/app/text_editor/map_editor/MapEditorXviBackgroundItem.cpp
    src/app/text_editor/map_editor/MapEditorXviBackgroundItem.h
    src/app/text_editor/TextEditorTab.cpp
    src/app/text_editor/TextEditorTabCompletionFacade.cpp
    src/app/text_editor/TextEditorTabAppearanceController.cpp
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
    src/app/text_editor/TextEditorTabModeController.cpp
    src/app/text_editor/TextEditorTabSourceRewriteController.cpp
    src/app/text_editor/TextEditorTabStatusController.cpp
    src/app/text_editor/TextEditorTab.h
    src/editor/TherionSyntaxHighlighter.cpp
    src/editor/TherionSyntaxHighlighter.h
    src/main.cpp)

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
    resources/icons/lucide/LICENSE
    resources/icons/lucide/check.svg
    resources/icons/lucide/code.svg
    resources/icons/lucide/cog.svg
    resources/icons/lucide/compass.svg
    resources/icons/lucide/external-link.svg
    resources/icons/lucide/grip-vertical.svg
    resources/icons/lucide/eye.svg
    resources/icons/lucide/eye-off.svg
    resources/icons/lucide/folder-open.svg
    resources/icons/lucide/folder-tree.svg
    resources/icons/lucide/folder-x.svg
    resources/icons/lucide/hand.svg
    resources/icons/lucide/image.svg
    resources/icons/lucide/map-pin.svg
    resources/icons/lucide/locate-fixed.svg
    resources/icons/lucide/map.svg
    resources/icons/lucide/mouse-pointer-2.svg
    resources/icons/lucide/network.svg
    resources/icons/lucide/pen-tool.svg
    resources/icons/lucide/pencil-line.svg
    resources/icons/lucide/pentagon.svg
    resources/icons/lucide/puzzle.svg
    resources/icons/lucide/redo-2.svg
    resources/icons/lucide/save.svg
    resources/icons/lucide/scan.svg
    resources/icons/lucide/screen-share.svg
    resources/icons/lucide/monitor-x.svg
    resources/icons/lucide/spline.svg
    resources/icons/lucide/square-plus.svg
    resources/icons/lucide/toy-brick.svg
    resources/icons/lucide/trash-2.svg
    resources/icons/lucide/undo-2.svg
    resources/icons/lucide/wand-sparkles.svg
    resources/icons/lucide/play.svg
    resources/icons/lucide/zoom-in.svg
    resources/icons/lucide/zoom-out.svg)

qt_add_resources(TherionStudio "TherionStudioResources"
    PREFIX "/"
    FILES
        ${THERION_STUDIO_RESOURCE_FILES})

set(THERION_STUDIO_TRANSLATION_FILES
    translations/therion_studio_cs.ts)

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
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII)

install(TARGETS TherionStudio
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WIN32)
    qt_generate_deploy_app_script(
        TARGET TherionStudio
        OUTPUT_SCRIPT THERION_STUDIO_QT_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${THERION_STUDIO_QT_DEPLOY_SCRIPT})

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
    include(CPack)
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "therion-studio")
    set(CPACK_PACKAGE_VENDOR "therion-studio")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ladislav Blazek <ladislav.blazek@gmail.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Therion Studio - Qt desktop editor for Therion cave surveying software")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    include(CPack)
endif()

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
therion_add_core_unit_test(TherionDocumentEditorTest
    tests/TherionDocumentEditorTest.cpp)

therion_add_core_unit_test(CommandCatalogServiceTest
    tests/CommandCatalogServiceTest.cpp)

therion_add_core_unit_test(TherionProjectStructureIndexTest
    tests/ProjectStructureIndexTest.cpp)

therion_add_core_unit_test(MapBackgroundPlacementTest
    tests/MapBackgroundPlacementTest.cpp)

therion_add_core_unit_test(TherionBackgroundMetadataTest
    tests/TherionBackgroundMetadataTest.cpp)

therion_add_core_unit_test(TherionXviParserTest
    tests/TherionXviParserTest.cpp)

qt_add_executable(MapGeometryFeatureParsingTest
    tests/MapGeometryFeatureParsingTest.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h
    src/app/text_editor/map_editor/MapEditorSceneItems.cpp
    src/app/text_editor/map_editor/MapEditorSceneInternals.h
    src/app/text_editor/map_editor/MapEditorSceneRenderer.cpp
    src/app/text_editor/map_editor/MapEditorSceneSupport.h)

target_link_libraries(MapGeometryFeatureParsingTest PRIVATE therion_core Qt6::Widgets)

therion_add_core_unit_test(DocumentFileEncodingTest
    tests/DocumentFileEncodingTest.cpp)

qt_add_executable(MapEditorInputPolicyTest
    tests/MapEditorInputPolicyTest.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.cpp
    src/app/text_editor/map_editor/MapEditorInputPolicy.h)

target_link_libraries(MapEditorInputPolicyTest PRIVATE Qt6::Widgets)

qt_add_executable(MapEditorAreaReferenceResolverTest
    tests/MapEditorAreaReferenceResolverTest.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.h)

target_link_libraries(MapEditorAreaReferenceResolverTest PRIVATE therion_core Qt6::Core)

qt_add_executable(MapEditorObjectMovePlannerTest
    tests/MapEditorObjectMovePlannerTest.cpp
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.cpp
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.h)

target_link_libraries(MapEditorObjectMovePlannerTest PRIVATE therion_core Qt6::Core)

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

qt_add_executable(MapEditorObjectDeletePlannerTest
    tests/MapEditorObjectDeletePlannerTest.cpp
    src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.cpp
    src/app/text_editor/map_editor/MapEditorObjectDeletePlanner.h)

target_link_libraries(MapEditorObjectDeletePlannerTest PRIVATE therion_core Qt6::Core)

qt_add_executable(MapEditorLineSplitPlannerTest
    tests/MapEditorLineSplitPlannerTest.cpp
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.cpp
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.h)

target_link_libraries(MapEditorLineSplitPlannerTest PRIVATE therion_core Qt6::Core)

therion_add_core_unit_test(SessionStoreTest
    tests/SessionStoreTest.cpp)

therion_add_app_unit_test(ProjectStructureScannerTest
    tests/ProjectStructureScannerTest.cpp)

therion_add_app_unit_test(TherionRunnerConfigResolverTest
    tests/TherionRunnerConfigResolverTest.cpp)

therion_add_app_unit_test(TherionRunnerConfigDisplayControllerTest
    tests/TherionRunnerConfigDisplayControllerTest.cpp)

therion_add_app_unit_test(TherionExecutableSelectionControllerTest
    tests/TherionExecutableSelectionControllerTest.cpp)

therion_add_app_unit_test(TherionRunnerPresenterTest
    tests/TherionRunnerPresenterTest.cpp)

therion_add_app_unit_test(TherionRunnerServiceTest
    tests/TherionRunnerServiceTest.cpp)

set(THERION_EDITOR_UI_TEST_BASE_SOURCES
    src/app/text_editor/ContextHelpController.cpp
    src/app/text_editor/ContextHelpController.h
    src/app/text_editor/TextEditorOptionValidation.cpp
    src/app/text_editor/TextEditorCommandMetadata.h
    src/app/text_editor/TextEditorOptionValidation.h
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.cpp
    src/app/text_editor/block_editor/BlockEditorOptionTableDelegate.h
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
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionParser.h
    src/app/text_editor/block_editor/BlockEditorDetailsSupport.cpp
    src/app/text_editor/block_editor/BlockEditorDetailsSupport.h
    src/app/text_editor/block_editor/BlockEditorConfigureController.cpp
    src/app/text_editor/block_editor/BlockEditorConfigureController.h
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.cpp
    src/app/text_editor/block_editor/BlockEditorCommandOptionsDialog.h
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.cpp
    src/app/text_editor/block_editor/BlockEditorDataBlockDialog.h
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.cpp
    src/app/text_editor/block_editor/BlockEditorDeleteExecutor.h
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.cpp
    src/app/text_editor/block_editor/BlockEditorSingleValueCommandDialog.h
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
    src/app/text_editor/TextEditorEncodingController.cpp
    src/app/text_editor/TextEditorEncodingController.h
    src/app/text_editor/TextEditorAppearanceController.cpp
    src/app/text_editor/TextEditorAppearanceController.h
    src/app/text_editor/TextEditorSourceRewriteController.cpp
    src/app/text_editor/TextEditorSourceRewriteController.h
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
    src/app/text_editor/raw_editor/RawEditorCompletionInsertionController.cpp
    src/app/text_editor/raw_editor/RawEditorCompletionInsertionController.h
    src/app/text_editor/raw_editor/RawEditorTextEdit.cpp
    src/app/text_editor/raw_editor/RawEditorTextEdit.h)

set(THERION_TEXT_EDITOR_UI_TEST_SOURCES
    ${THERION_EDITOR_UI_TEST_BASE_SOURCES}
    src/app/text_editor/TextEditorTab.cpp
    src/app/text_editor/TextEditorTabCompletionFacade.cpp
    src/app/text_editor/TextEditorTabAppearanceController.cpp
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
    src/app/text_editor/TextEditorTabModeController.cpp
    src/app/text_editor/TextEditorTabSourceRewriteController.cpp
    src/app/text_editor/TextEditorTabStatusController.cpp
    src/app/text_editor/TextEditorTab.h
    src/editor/TherionSyntaxHighlighter.cpp
    src/editor/TherionSyntaxHighlighter.h)

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

set(THERION_MAP_EDITOR_UI_TEST_SOURCES
    src/app/text_editor/map_editor/MapEditorBackgroundLayers.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.cpp
    src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.h
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
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.cpp
    src/app/text_editor/map_editor/MapEditorLineSplitPlanner.h
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.cpp
    src/app/text_editor/map_editor/MapEditorObjectMovePlanner.h
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.cpp
    src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h
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
    src/app/text_editor/map_editor/MapEditorTab.cpp
    src/app/text_editor/map_editor/MapEditorTabControllers.cpp
    src/app/text_editor/map_editor/MapEditorTabCanvasEditContext.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorBackgroundContext.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorObjectContext.cpp
    src/app/text_editor/map_editor/MapEditorTabInspectorObjectEvents.cpp
    src/app/text_editor/map_editor/MapEditorTabInteractiveDrawContext.cpp
    src/app/text_editor/map_editor/MapEditorTabObjectDetailsContext.cpp
    src/app/text_editor/map_editor/MapEditorTabSceneContexts.cpp
    src/app/text_editor/map_editor/MapEditorTabSelectionContext.cpp
    src/app/text_editor/map_editor/MapEditorTabViewportInputContext.cpp
    src/app/text_editor/map_editor/MapEditorTabWorkspace.cpp
    src/app/text_editor/map_editor/MapEditorTab.h
    src/app/text_editor/map_editor/MapEditorXviBackgroundItem.cpp
    src/app/text_editor/map_editor/MapEditorXviBackgroundItem.h)

add_library(therion_map_editor_ui_test_support STATIC
    ${THERION_MAP_EDITOR_UI_TEST_SOURCES})

target_link_libraries(therion_map_editor_ui_test_support
    PUBLIC
        therion_text_editor_ui_test_support
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

set(THERION_UNIT_TEST_TARGETS
    CommandCatalogServiceTest
    TherionDocumentEditorTest
    TherionProjectStructureIndexTest
    MapBackgroundPlacementTest
    TherionBackgroundMetadataTest
    TherionXviParserTest
    MapGeometryFeatureParsingTest
    DocumentFileEncodingTest
    MapEditorAreaReferenceResolverTest
    MapEditorInputPolicyTest
    MapEditorInspectorDataTest
    MapEditorObjectStyleCatalogTest
    MapEditorObjectDeletePlannerTest
    MapEditorLineSplitPlannerTest
    MapEditorObjectMovePlannerTest
    ProjectStructureScannerTest
    SessionStoreTest
    TherionExecutableSelectionControllerTest
    TherionRunnerConfigDisplayControllerTest
    TherionRunnerConfigResolverTest
    TherionRunnerPresenterTest
    TherionRunnerServiceTest)

set(THERION_UI_TEST_TARGETS
    TextEditorCaretInteractionTest
    TextEditorCompletionHighlightTest
    MapEditorDetachedPaneTest
    MapEditorDragUndoRedoSmokeTest
    MapEditorSplitAreaBorderSmokeTest
    MapEditorSelectionPanelTypeOptionsTest)

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
endif()
