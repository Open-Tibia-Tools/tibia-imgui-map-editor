#include "Application.h"
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include "ImGuiNotify.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <nfd.hpp>
#include <spdlog/spdlog.h>

// Application Components
#include "Application/MapOperationHandler.h"
#include "Application/SessionLifecycleManager.h"
#include "Brushes/BrushController.h"
#include "Controllers/HotkeyController.h"
#include "Controllers/MapInputController.h"
#include "Controllers/SearchController.h"
#include "Controllers/SimulationController.h"
#include "Controllers/StartupController.h"
#include "Controllers/WorkspaceController.h"
#include "Services/SessionWiringService.h"

// Domain & Rendering
#include "Domain/ChunkedMap.h"
#include "Rendering/Map/MapRenderer.h"
// Services
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
// UI Components
#include "Presentation/MainWindow.h"
#include "Presentation/MenuBar.h"
#include "UI/Core/Theme.h"
#include "UI/Dialogs/Startup/StartupDialog.h"
#include "UI/Map/MapPanel.h"
#include "UI/Ribbon/RibbonController.h"

// UI Factory
#include "Application/UIFactory.h"
// Utilities
#include "Presentation/NotificationHelper.h"
namespace MapEditor {

Application::Application() = default;

Application::~Application() { shutdown(); }

bool Application::initialize() {
  spdlog::info("Initializing Tibia Map Editor...");

  // === Platform & Configuration ===
  if (!initializePlatform())
    return false;

  // === Services & Settings ===
  // Services are now initialized within initializePlatform() via
  // SettingsRegistry

  // === Lifecycle Managers ===
  initializeLifecycleManagers();

  // === UI Components & Callbacks ===
  initializeUIComponents();
  wireCallbacks();

  // === Final Wiring ===
  dialogs_.preferences.setHotkeyRegistry(
      &settings_registry_->getHotkeyRegistry());
  dialogs_.preferences.setOtbmSettings(
      &settings_registry_->getOtbmSettings());
  dialogs_.preferences.setThemePtr(
      &settings_registry_->getAppSettings().theme);
  dialogs_.preferences.setApplySettingsCallback([this]() {
    settings_registry_->save();
  });

  // === State Handlers ===
  state_manager_.setStartupUpdater([this]() { onUpdateStartup(); });
  state_manager_.setEditorUpdater([this]() { onUpdateEditor(); });

  spdlog::info("Initialization complete");
  return true;
}

bool Application::initializePlatform() {
  settings_registry_ = std::make_unique<Services::SettingsRegistry>();

  if (!settings_registry_->load())
    return false;

  if (!platform_manager_.initialize(settings_registry_->getConfig()))
    return false;

  // Load app settings AFTER ImGui is initialized (theme needs ImGui context)
  ApplyTheme(settings_registry_->getAppSettings().theme);

  return true;
}

void Application::initializeLifecycleManagers() {
  session_lifecycle_ = std::make_unique<AppLogic::SessionLifecycleManager>();

  session_wiring_ =
      std::make_unique<SessionWiringService>(SessionWiringService::Context{
          &version_manager_, &rendering_manager_, &tab_manager_,
          &settings_registry_->getViewSettings()});
}

void Application::initializeUIComponents() {
  brush_system_ = std::make_unique<Brushes::BrushSystem>();

  // Wire persistence: load custom brushes from AppData
  brush_system_->setConfigService(&settings_registry_->getConfig());

  UIFactoryContext ctx{
      .view_settings = settings_registry_->getViewSettings(),
      .selection_settings = settings_registry_->getSelectionSettings(),
      .hotkey_registry = settings_registry_->getHotkeyRegistry(),
      .app_settings = settings_registry_->getAppSettings(),
      .config = settings_registry_->getConfig(),
      .version_registry = settings_registry_->getVersionRegistry(),
      .recent_locations = settings_registry_->getRecentLocations(),
      .otbm_settings = settings_registry_->getOtbmSettings(),
      .tab_manager = tab_manager_,
      .state_manager = state_manager_,
      .tileset_widget = brush_system_->getTilesetWidget(),
      .brush_controller = brush_system_->getController(),
      .brush_registry = brush_system_->getRegistry(),
      .tileset_service = brush_system_->getTilesetService()};

  ui_ = UIFactory::create(ctx);
}

void Application::wireCallbacks() {
  // Wire preferences callback to open PreferencesDialog
  // Controller is now created in initializeUIComponents
  if (ui_.startup_controller) {
    ui_.startup_controller->setPreferencesCallback(
        [this]() { dialogs_.preferences.show(); });
  }

  // Build context and wire all callbacks via mediator
  CallbackMediator::Context ctx{
      // Platform
      .window = &platform_manager_.getWindow(),
      .callback_router = &platform_manager_.getCallbackRouter(),
      // Core managers
      .state_manager = &state_manager_,
      .version_manager = &version_manager_,
      .tab_manager = &tab_manager_,
      // Services
      .config = &settings_registry_->getConfig(),
      .versions = &settings_registry_->getVersionRegistry(),
      .recent = &settings_registry_->getRecentLocations(),
      .view_settings = &settings_registry_->getViewSettings(),
      // Rendering
      .rendering_manager = &rendering_manager_,
      // UI Components
      .map_panel = ui_.map_panel.get(),
      .minimap = ui_.minimap_window.get(),
      .ingame_box = ui_.ingame_box_window.get(),
      .browse_tile = ui_.browse_tile_window.get(),
      .main_window = ui_.main_window.get(),
      .menu_bar = ui_.menu_bar.get(),
      .file_panel = ui_.file_panel_ptr,
      // Controllers
      .hotkey = ui_.hotkey_controller.get(),
      .input_controller = ui_.input_controller.get(),
      .map_operations = ui_.map_operations.get(),
      // Dialogs (via container)
      .unsaved_modal = &dialogs_.unsaved_changes,
      .import_map = &dialogs_.import_map,
      .import_monsters = &dialogs_.import_monsters,
      .preferences = &dialogs_.preferences,
      .edit_towns = &dialogs_.edit_towns,
      .map_properties = &dialogs_.map_properties,
      // Search components
      .quick_search = ui_.search_controller->getQuickSearchPopup(),
      .advanced_search = ui_.search_controller->getAdvancedSearchDialog(),
      .search_results = ui_.search_controller->getSearchResultsWidget(),
      .search_controller = ui_.search_controller.get(),
      .cleanup_confirm = &dialogs_.cleanup_confirm,
      // Callbacks back to Application
      .quit_callback = [this]() { quit(); },
      .change_version_callback =
          [this]() {
            pending_close_all_ = createVersionCoordinator().initiateSwitch();
          },
      .request_close_tab = [this](int index) { requestCloseTab(index); },
      .trigger_invalid_items_cleanup =
          [this]() {
            dialogs_.cleanup_controller.requestCleanup(
                Presentation::CleanupController::CleanupType::InvalidItems,
                &dialogs_.cleanup_confirm);
          },
      .trigger_house_items_cleanup =
          [this]() {
            dialogs_.cleanup_controller.requestCleanup(
                Presentation::CleanupController::CleanupType::HouseItems,
                &dialogs_.cleanup_confirm);
          },
      // MapOperationHandler callbacks
      .on_map_loaded =
          [this](auto map, auto client_data, auto sprite_manager,
                 const auto &center) {
            onMapLoaded(std::move(map), std::move(client_data),
                        std::move(sprite_manager), center);
          },
      .on_notification =
          [](int type, const std::string &message) {
            Presentation::showNotification(type, message);
          }};

  // Wire session-destroy callback to evict cached search results
  if (session_lifecycle_ && ui_.search_controller) {
    session_lifecycle_->setSessionDestroyCallback(
        [sc = ui_.search_controller.get()](const AppLogic::EditorSession& session) {
          if (auto* map = session.getMap()) {
            sc->forgetSessionMap(map);
          }
        });
  }

  callback_mediator_.wireAll(ctx);
}

void Application::onMapLoaded(
    std::unique_ptr<Domain::ChunkedMap> map,
    std::unique_ptr<Services::ClientDataService> client_data,
    std::unique_ptr<Services::SpriteManager> sprite_manager,
    const Domain::Position &camera_center) {
  // Delegate resource wiring to SessionWiringService
  auto *session = session_wiring_->wireResources(
      std::move(map), std::move(client_data), std::move(sprite_manager),
      ui_.map_operations->getPendingMapPath(), ui_.map_operations.get());

  // Delegate UI synchronization to WorkspaceController
  if (ui_.workspace_controller) {
    // Get registries from TilesetService owned by BrushSystem
    auto &tileset_service = brush_system_->getTilesetService();
    ui_.workspace_controller->bindSession(
        session, version_manager_.getClientData(),
        version_manager_.getSpriteManager(),
        &settings_registry_->getViewSettings(),
        &tileset_service.getTilesetRegistry(),
        &tileset_service.getPaletteRegistry(), &camera_center);
  }

  state_manager_.transition(AppStateManager::State::Editor);
}

void Application::shutdown() {
  // Save palette window state before persisting settings
  if (ui_.palette_window_manager) {
    ui_.palette_window_manager->saveState();
  }

  if (settings_registry_) {
    persistence_manager_.saveApplicationState(
        *settings_registry_, platform_manager_, version_manager_);
  }

  platform_manager_.shutdown();
  spdlog::info("Application shutdown complete");
}

Coordination::VersionSwitchCoordinator Application::createVersionCoordinator() {
  return Coordination::VersionSwitchCoordinator({
      .version_manager = version_manager_,
      .tab_manager = tab_manager_,
      .session_lifecycle = *session_lifecycle_,
      .rendering_manager = rendering_manager_,
      .map_operations = *ui_.map_operations,
      .workspace_controller = *ui_.workspace_controller,
      .state_manager = state_manager_,
      .preferences = dialogs_.preferences,
      .unsaved_changes = dialogs_.unsaved_changes,
  });
}

int Application::run() {
  spdlog::info("Entering main loop");

  while (!should_quit_ && !platform_manager_.shouldClose()) {
    // Process events and check for display errors
    if (!platform_manager_.update()) {
      continue; // Skip frame if display is unavailable
    }

    update();
    render();

    // Process deferred actions (session destruction) AFTER render completes
    processDeferredActions();

    // Process deferred map loads (requested via popup during render phase)
    if (ui_.map_operations && ui_.map_operations->hasPendingMapLoad()) {
      ui_.map_operations->processPendingMapLoad();
    }
  }

  return 0;
}

void Application::processDeferredActions() {
  if (!session_lifecycle_)
    return;

  session_lifecycle_->processDeferredActions(
      tab_manager_, rendering_manager_, ui_.workspace_controller.get(),
      state_manager_, [this]() {
        // Release rendering resources
        rendering_manager_.release();
        // Clear client data ownership
        version_manager_.releaseAll();
        // Clear MapOperationHandler cached pointers
        if (ui_.map_operations) {
          ui_.map_operations->setExistingResources(nullptr, nullptr);
        }
        // Clear secondary client provider in preferences
        dialogs_.preferences.setSecondaryClientProvider(nullptr);
      });
}

void Application::requestCloseTab(int index) {
  if (session_lifecycle_) {
    session_lifecycle_->requestCloseTab(index);
  }
}

void Application::quit() { should_quit_ = true; }

void Application::processEvents() {
  // Deprecated: events are polled in platform_manager_.update() called from
  // run()
}

void Application::update() {
  // Update version manager resources (e.g. async sprite loading)
  version_manager_.update();

  // Drain completed async search results on the main thread
  if (ui_.search_controller) {
    ui_.search_controller->processAsyncSearch();
  }

  state_manager_.update();
}

void Application::render() {
  // Build render context with all dependencies
  RenderOrchestrator::Context ctx{
      // Platform
      .imgui_backend = &platform_manager_.getImGuiBackend(),
      .window = &platform_manager_.getWindow(),
      // State
      .state_manager = &state_manager_,
      .tab_manager = &tab_manager_,
      .version_manager = &version_manager_,
      // Services
      .config = &settings_registry_->getConfig(),
      .view_settings = &settings_registry_->getViewSettings(),
      // Rendering
      .rendering_manager = &rendering_manager_,
      // UI Components
      .map_panel = ui_.map_panel.get(),
      .minimap = ui_.minimap_window.get(),
      .browse_tile = ui_.browse_tile_window.get(),
      .ribbon = ui_.ribbon_controller.get(),
      .file_panel = ui_.file_panel_ptr,
      .main_window = ui_.main_window.get(),
      .quick_search_popup = ui_.search_controller->getQuickSearchPopup(),
      .advanced_search_dialog =
          ui_.search_controller->getAdvancedSearchDialog(),
      .search_results_widget = ui_.search_controller->getSearchResultsWidget(),
      .tileset_widget = &brush_system_->getTilesetWidget(),
      .palette_window_manager = ui_.palette_window_manager.get(),
      .startup_dialog = ui_.startup_dialog.get(),
      .startup_controller = ui_.startup_controller.get(),
      // Dialogs
      .dialogs = &dialogs_,
      // Brush system (for preview rendering)
      .brush_controller = &brush_system_->getController(),
      .brush_size_panel = &brush_system_->getBrushSizePanel(),
      // Map operations (for compatibility popup)
      .map_operations = ui_.map_operations.get(),
      // Callbacks
      .on_perform_version_switch =
          [this]() { createVersionCoordinator().performSwitch(); },
      .on_request_close_tab = [this](int index) { requestCloseTab(index); },
      .get_active_tab_index =
          [this]() { return tab_manager_.getActiveTabIndex(); },
      // Pending state
      .pending_close_all = &pending_close_all_};

  render_orchestrator_.render(ctx);
}

void Application::onUpdateStartup() {
  // Startup controller handles all startup dialog interaction
  if (ui_.startup_controller) {
    ui_.startup_controller->update();

    // Check for exit request from startup dialog
    if (ui_.startup_controller->shouldExit()) {
      quit();
    }
  }
}

void Application::onUpdateEditor() {
  // Delegate creature simulation update to controller
  if (ui_.simulation_controller && ui_.map_panel) {
    ui_.simulation_controller->updateFromPanel(
        ImGui::GetIO().DeltaTime, tab_manager_.getActiveSession(),
        version_manager_.getClientData(), *ui_.map_panel);
  }

  // NOTE: MapCompatibilityPopup is rendered in
  // RenderOrchestrator::renderDialogs() ImGui rendering must happen during
  // render phase (between newFrame and Render)
}

} // namespace MapEditor
