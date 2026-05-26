cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(TibiaMapEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TME_UNITY_BUILD "Enable unity build for faster compilation." OFF)
option(TME_ENABLE_IPO "Enable interprocedural optimization in optimized builds." ON)
option(TME_ENABLE_PCH "Enable precompiled headers for faster compilation." ON)
option(TME_ENABLE_ARCH_OPTIMIZATIONS "Enable host-specific CPU optimizations for local optimized builds." OFF)
option(TME_BUILD_IMGUI_DEMO "Compile Dear ImGui demo sources." OFF)

# Find packages (works with both vcpkg and Conan)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Stb REQUIRED)

# nativefiledialog-extended: Try vcpkg first, fallback to local ext/ for Conan
find_package(nfd CONFIG QUIET)
if(NOT nfd_FOUND)
    message(STATUS "nfd not found via package manager, using local ext/nativefiledialog-extended")
    
    # Set platform for nfd
    if(WIN32)
        set(nfd_PLATFORM PLATFORM_WIN32)
        set(nfd_COMPILER COMPILER_MSVC)
    elseif(APPLE)
        set(nfd_PLATFORM PLATFORM_MACOS)
        set(nfd_COMPILER COMPILER_GNU)
    else()
        set(nfd_PLATFORM PLATFORM_LINUX)
        set(nfd_COMPILER COMPILER_GNU)
    endif()
    
    # Add the local nfd subdirectory
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ext/nativefiledialog-extended ext_nfd)
    set(NFD_LOCAL TRUE)
endif()

# Fetch ImGui with docking support
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.8-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

if(TME_BUILD_IMGUI_DEMO)
    list(APPEND IMGUI_SOURCES ${IMGUI_DIR}/imgui_demo.cpp)
endif()

set_source_files_properties(${IMGUI_SOURCES} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
    SKIP_UNITY_BUILD_INCLUSION ON
)

set_source_files_properties(
    IO/Sec/SecTileParser.cpp
    IO/Sec/SecItemParser.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

# Collect source files
set(DOMAIN_SOURCES
    Domain/Item.cpp
    Domain/Tile.cpp
    Domain/ClientVersion.cpp
    Domain/MapInstance.cpp
    Domain/ChunkedMap.cpp
    Domain/CopyBuffer.cpp
    Domain/Selection/SelectionBucket.cpp
    Domain/History/TileSnapshot.cpp
    Domain/History/TileSnapshotCodec.cpp
    Domain/History/HistoryEntry.cpp
    Domain/History/HistoryBuffer.cpp
    Domain/History/HistoryManager.cpp
)

set(RENDERING_SOURCES
    Rendering/Core/Texture.cpp
    Rendering/Core/Shader.cpp
    Rendering/Core/VertexArray.cpp
    Rendering/Camera/ViewCamera.cpp
    Rendering/Map/MapRenderer.cpp
    Rendering/Core/RenderPipeline.cpp
    Rendering/Core/Framebuffer.cpp
    Rendering/Resources/TextureAtlas.cpp
    Rendering/Resources/AtlasManager.cpp
    Rendering/Resources/SpriteAtlasLUT.cpp
    Rendering/Backend/SpriteBatch.cpp
    Rendering/Backend/RingBuffer.cpp
    Rendering/Core/PixelBufferObject.cpp
    Rendering/Backend/MultiDrawIndirectRenderer.cpp
    Rendering/Resources/ShaderLoader.cpp
    Rendering/Map/TileRenderer.cpp
    Rendering/Tile/ChunkSpriteCache.cpp
    Rendering/Frame/RenderState.cpp
    Rendering/Passes/IngamePreviewRenderer.cpp
    Rendering/Visibility/FloorVisibilityCalculator.cpp
    Rendering/Passes/WallOutlineRenderer.cpp
    Rendering/Passes/ShadeRenderer.cpp
    Rendering/Overlays/OverlayRenderer.cpp
    Rendering/Overlays/SpawnLabelOverlay.cpp
    Rendering/Overlays/OutfitOverlay.cpp
    Rendering/Overlays/WaypointOverlay.cpp
    Rendering/Overlays/TooltipOverlay.cpp
    Rendering/Overlays/TooltipBubbleRenderer.cpp
    Rendering/Overlays/OverlaySpriteCache.cpp
    # ItemPreviewRenderer.cpp removed - now using unified PreviewRenderer
    Rendering/Light/LightGatherer.cpp
    Rendering/Light/LightTexture.cpp
    Rendering/Light/LightOverlay.cpp
    Rendering/Light/LightCache.cpp
    Rendering/Light/LightManager.cpp
    Rendering/Minimap/MinimapTexture.cpp
    Rendering/Minimap/MinimapRenderer.cpp
    Rendering/Minimap/ChunkedMapMinimapSource.cpp
    Rendering/Core/RenderTarget.cpp
    Rendering/Visibility/ChunkVisibilityManager.cpp
    Rendering/Tile/ItemRenderer.cpp
    Rendering/Tile/GroundRenderer.cpp
    Rendering/Tile/CreatureRenderer.cpp
    Rendering/Tile/ChunkRenderingStrategy.cpp
    Rendering/Passes/SpawnTintPass.cpp
    Rendering/Overlays/OverlayCollector.cpp
    Rendering/Overlays/PreviewOverlay.cpp
    Rendering/Overlays/GridOverlay.cpp
    Rendering/Overlays/SelectionOverlay.cpp
    Rendering/Overlays/StatusOverlay.cpp
    Rendering/Overlays/OverlayManager.cpp
    Rendering/Passes/GhostFloorRenderer.cpp
    Rendering/Tile/CreatureSpriteHelper.cpp
    Rendering/Passes/BackgroundRenderer.cpp
    Application/Frame/RenderOrchestrator.cpp
    Rendering/Frame/RenderingManager.cpp
    Rendering/Frame/FrameDataCollector.cpp
    Rendering/Passes/TerrainPass.cpp
    Rendering/Passes/LightingPass.cpp
)

set(IO_SOURCES
    IO/BinaryReader.cpp
    IO/NodeFileReader.cpp
    IO/NodeFileWriter.cpp
    IO/OtbReader.cpp
    IO/ScriptReader.cpp
    IO/SrvReader.cpp
    IO/SprReader.cpp
    IO/Otbm/OtbmReader.cpp
    IO/Otbm/OtbmWriter.cpp
    IO/Otbm/OtbmIdConverter.cpp
    IO/Otbm/OtbmTileParser.cpp
    IO/Otbm/OtbmItemParser.cpp
    IO/CreatureXmlReader.cpp
    IO/ItemXmlReader.cpp
    IO/TilesetXmlReader.cpp
    IO/TilesetXmlWriter.cpp
    IO/MaterialsXmlReader.cpp
    IO/PaletteXmlReader.cpp
    IO/SpawnXmlReader.cpp
    IO/SpawnXmlWriter.cpp
    IO/HouseXmlReader.cpp
    IO/HouseXmlWriter.cpp
    IO/WaypointXmlWriter.cpp
    IO/WaypointXmlReader.cpp
    IO/XmlUtils.cpp
    IO/HotkeyJsonReader.cpp
    IO/Readers/DatReaderBase.cpp
    IO/Readers/DatReaderV710.cpp
    IO/Readers/DatReaderV740.cpp
    IO/Readers/DatReaderV755.cpp
    IO/Readers/DatReaderV780.cpp
    IO/Readers/DatReaderV860.cpp
    IO/Readers/DatReaderV1010.cpp
    IO/SecReader.cpp
    IO/Sec/SecTileParser.cpp
    IO/Sec/SecItemParser.cpp
    # Phase 0: Brush XML Parser
    IO/BrushXmlReader.cpp
)

set(SERVICES_SOURCES
    Utils/SpriteUtils.cpp
    Services/ConfigService.cpp
    Services/BrushSettingsService.cpp
    Services/ClientVersionRegistry.cpp
    Services/ClientVersionPersistence.cpp
    Services/ClientSignatureDetector.cpp
    Services/ClientAssetDetector.cpp
    Services/ClientDataService.cpp
    Services/SpriteManager.cpp
    Services/SpriteAsyncLoader.cpp
    Services/SpriteLoadQueue.cpp
    Services/ItemCompositor.cpp
    Services/CreatureSpriteService.cpp
    Services/RecentLocationsService.cpp
    Services/ViewSettings.cpp
    Services/ClientVersionValidator.cpp
    Services/SecondaryClientData.cpp
    Services/CreatureSimulator.cpp
    Services/AppSettings.cpp
    Services/TilesetService.cpp
    Services/HotkeyRegistry.cpp
    Services/Preview/PreviewService.cpp
    Services/Preview/RawBrushPreviewProvider.cpp
    Services/Preview/PastePreviewProvider.cpp
    Services/Preview/DragPreviewProvider.cpp
    Services/Preview/BrushPreviewFactory.cpp
    Services/Preview/CreaturePreviewProvider.cpp
    Services/Preview/SpawnPreviewProvider.cpp
    Services/Preview/ZoneBrushPreviewProvider.cpp
    Services/Map/MapLoadingService.cpp
    Services/Map/MapSavingService.cpp
    Services/Map/MapCleanupService.cpp
    Services/Map/MapSearchService.cpp
    Services/ClipboardService.cpp
    Services/ItemPickerService.cpp
    Services/MapMergeService.cpp
    Services/SessionWiringService.cpp
    Services/SettingsRegistry.cpp
    Services/Selection/SelectionService.cpp
    Services/Map/MapEditingService.cpp
    # Phase 0: Lookup Table Services
    Services/Brushes/BorderLookupService.cpp
    Services/Brushes/WallLookupService.cpp
    Services/Brushes/TableLookupService.cpp
    Services/Brushes/CarpetLookupService.cpp
)

set(UI_SOURCES
    UI/Dialogs/AdvancedSearchDialog.cpp
    UI/Dialogs/ClientConfiguration/ClientConfigurationDialog.cpp
    UI/Dialogs/ClientConfiguration/ClientPropertyEditor.cpp
    UI/Dialogs/ConfirmationDialog.cpp
    UI/Dialogs/EditTownsDialog.cpp
    UI/Dialogs/Import/ImportMapDialog.cpp
    UI/Dialogs/Import/ImportMonstersDialog.cpp
    UI/Dialogs/Properties/CreaturePropertiesDialog.cpp
    UI/Dialogs/Properties/ItemPropertiesDialog.cpp
    UI/Dialogs/Properties/MapPropertiesDialog.cpp
    UI/Dialogs/Properties/SpawnPropertiesDialog.cpp
    UI/Dialogs/Startup/AvailableClientsPanel.cpp
    UI/Dialogs/Startup/ClientInfoPanel.cpp
    UI/Dialogs/Startup/RecentMapsPanel.cpp
    UI/Dialogs/Startup/SelectedMapPanel.cpp
    UI/Dialogs/Startup/StartupDialog.cpp
    UI/Dialogs/NewMapDialog.cpp
    UI/Dialogs/UnsavedChangesModal.cpp
    UI/Dialogs/MapCompatibilityPopup.cpp
    UI/Map/MapContextMenu.cpp
    UI/Map/MapPanel.cpp
    UI/Map/MapPanelInput.cpp
    UI/Map/MapViewCamera.cpp
    UI/Map/SelectionMenu.cpp
    UI/Panels/NewMapPanel.cpp
    UI/Panels/BrushSizePanel.cpp
    UI/PreferencesDialog.cpp
    UI/Ribbon/Panels/BrushesPanel.cpp
    UI/Ribbon/Panels/EditPanel.cpp
    UI/Ribbon/Panels/FilePanel.cpp
    UI/Ribbon/Panels/PalettesPanel.cpp
    UI/Ribbon/Panels/SelectionPanel.cpp
    UI/Ribbon/Panels/ThemePanel.cpp
    UI/Ribbon/Panels/ViewPanel.cpp
    UI/Ribbon/RibbonController.cpp
    UI/Widgets/Properties/PropertyPanelRenderer.cpp
    UI/Widgets/Properties/PropertyWidgets.cpp
    UI/Widgets/QuickSearchPopup.cpp
    UI/Widgets/SearchResultsWidget.cpp
    UI/Widgets/TilesetWidget.cpp
    UI/Widgets/TilesetGridWidget.cpp
    UI/Windows/BrowseTile/BrowseTileWindow.cpp
    UI/Windows/BrowseTile/ItemsListRenderer.cpp
    UI/Windows/BrowseTile/SpawnCreatureRenderer.cpp
    UI/Windows/IngameBoxWindow.cpp
    UI/Windows/MinimapWindow.cpp
    UI/Windows/PaletteWindow.cpp
    UI/Windows/PaletteWindowManager.cpp
    UI/Utils/PreviewUtils.cpp
)

set(PLATFORM_SOURCES
    Platform/GlfwWindow.cpp
    Platform/ImGuiBackend.cpp
    Platform/PlatformCallbackRouter.cpp
)

set(APPLICATION_SOURCES
    Application/AppStateManager.cpp
    Application/CallbackMediator.cpp
    Application/ClientVersionManager.cpp
    Application/EditorSession.cpp
    Application/MapTabManager.cpp
    Application/MapOperationHandler.cpp
    Application/MapConversionHandler.cpp
    Application/PersistenceManager.cpp
    Application/PlatformManager.cpp
    Application/Selection/SmartSelectionStrategy.cpp
    Application/Selection/PixelPerfectSelectionStrategy.cpp
    Application/Selection/FloorScopeHelper.cpp
    Application/Selection/LassoSelectionProcessor.cpp
    Application/SessionLifecycleManager.cpp
    Application/UIFactory.cpp
    Application/Coordination/VersionSwitchCoordinator.cpp
)

set(CONTROLLER_SOURCES
    Controllers/HotkeyController.cpp
    Controllers/MapInputController.cpp
    Controllers/SearchController.cpp
    Controllers/SimulationController.cpp
    Controllers/StartupController.cpp
    Controllers/WindowController.cpp
    Controllers/WorkspaceController.cpp
)

set(PRESENTATION_SOURCES
    Presentation/MenuBar.cpp
    Presentation/MainWindow.cpp
    Presentation/Dialogs/ImportMapController.cpp
    Presentation/Dialogs/CleanupController.cpp
    Presentation/Dialogs/TownPickController.cpp
    Presentation/Dialogs/ClientConfigurationController.cpp
)

set(BRUSHES_SOURCES
    Brushes/BrushSystem.cpp
    Brushes/BrushRegistry.cpp
    Brushes/BrushController.cpp
    Brushes/Behaviors/ItemPlacement.cpp
    Brushes/Behaviors/WeightedSelection.cpp
    Brushes/Types/RawBrush.cpp
    Brushes/Types/CreatureBrush.cpp
    Brushes/Types/SpawnBrush.cpp
    Brushes/Types/FlagBrush.cpp
    Brushes/Types/EraserBrush.cpp
    Brushes/Types/HouseBrush.cpp
    Brushes/Types/WaypointBrush.cpp
    # Phase 0: Enumerations
    Brushes/Enums/BrushEnums.cpp
    # Phase 0: Data Structures
    Brushes/Data/BorderBlock.cpp
    Brushes/Data/WallNode.cpp
    Brushes/Data/DoodadAlternative.cpp
)

set(APP_SOURCES
    Application.cpp
    main.cpp
)

# Create executable
add_executable(${PROJECT_NAME}
    ${DOMAIN_SOURCES}
    ${RENDERING_SOURCES}
    ${IO_SOURCES}
    ${SERVICES_SOURCES}
    ${UI_SOURCES}
    ${PLATFORM_SOURCES}
    ${APPLICATION_SOURCES}
    ${CONTROLLER_SOURCES}
    ${PRESENTATION_SOURCES}
    ${BRUSHES_SOURCES}
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ${TME_UNITY_BUILD})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/fontawesome6
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/imguinotify
    ${CMAKE_CURRENT_SOURCE_DIR}/ext/stb
    ${Stb_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glad::glad
    glfw
    glm::glm
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    pugixml::pugixml
    lz4::lz4
    Boost::boost
    fmt::fmt
    ZLIB::ZLIB
    # stb is header-only - included via find_package(stb) which sets up include paths
)

# Link nfd (vcpkg uses nfd::nfd, local build uses nfd)
if(NFD_LOCAL)
    target_link_libraries(${PROJECT_NAME} PRIVATE nfd)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE nfd::nfd)
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    # Hide console window in release builds (disabled - using main() not WinMain)
    # if(CMAKE_BUILD_TYPE STREQUAL "Release")
    #     set_target_properties(${PROJECT_NAME} PROPERTIES
    #         WIN32_EXECUTABLE TRUE
    #     )
    # endif()
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FS)
endif()

if(TME_ENABLE_PCH)
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
    )
endif()

if(TME_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT TME_IPO_SUPPORTED OUTPUT TME_IPO_ERROR)

    if(TME_IPO_SUPPORTED)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
        )
    else()
        message(STATUS "IPO/LTO disabled for ${PROJECT_NAME}: ${TME_IPO_ERROR}")
    endif()
endif()

if(TME_ENABLE_ARCH_OPTIMIZATIONS)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE
            "$<$<CONFIG:Release>:/arch:AVX2>"
            "$<$<CONFIG:RelWithDebInfo>:/arch:AVX2>"
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE
            "$<$<CONFIG:Release>:-march=native>"
            "$<$<CONFIG:RelWithDebInfo>:-march=native>"
        )
    else()
        message(STATUS "Host-specific architecture optimizations are not configured for compiler ${CMAKE_CXX_COMPILER_ID}.")
    endif()
endif()

# Copy data files
set(COPY_DATA_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data/shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data/fonts"
    # Copy clients_templates.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/clients_templates.json"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data/clients_templates.json"
    # Copy entire sample_data directory tree (includes tilesets/, creatures/, items/, etc.)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/sample_data"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
    # Copy shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data/shaders"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/fontawesome6/fa-solid-900.ttf")
    list(APPEND COPY_DATA_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/ext/fontawesome6/fa-solid-900.ttf"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data/fonts/fa-solid-900.ttf"
    )
else()
    message(WARNING "Missing fontawesome6 TTF: ext/fontawesome6/fa-solid-900.ttf (skipping copy).")
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    ${COPY_DATA_COMMANDS}
    COMMENT "Copying data files..."
)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../RME_Readonly/XML/"
    DESTINATION bin/data
    FILES_MATCHING PATTERN "*.xml"
)


