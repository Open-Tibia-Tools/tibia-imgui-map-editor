#pragma once
#include "Services/ClientVersionRegistry.h"
#include "Services/ConfigService.h"
#include "Services/RecentLocationsService.h"
#include "UI/Dialogs/Startup/StartupDialog.h"
#include "Application/AppStateManager.h"
#include "Application/MapOperationHandler.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace MapEditor {
namespace Presentation { class ClientConfigurationController; }
namespace AppLogic {

/**
 * Controller for StartupDialog - handles business logic using reactive pattern.
 *
 * Matches WelcomeController architecture:
 * - Consumes dialog results via update()
 * - Dispatches to appropriate handler
 * - Calls services and updates dialog state
 */
class StartupController {
public:
  StartupController(UI::StartupDialog &dialog, MapOperationHandler &map_ops,
                    Services::ConfigService &config,
                    Services::ClientVersionRegistry &registry,
                    Services::RecentLocationsService &recent_locations,
                    AppStateManager &state_manager);

  ~StartupController();

  /**
   * Called each frame when in WelcomeScreen state.
   * Consumes dialog result and dispatches to handler.
   */
  void update();

  /**
   * Prepare recent maps list for dialog rendering.
   */
  std::vector<UI::RecentMapEntry> getRecentMaps() const;

  /**
   * Prepare recent clients list for dialog rendering.
   */
  std::vector<uint32_t> getRecentClients() const;

  uint32_t getMatchedClientIndex() const { return matched_client_index_; }

  /**
   * Request application exit.
   */
  void requestExit();
  bool shouldExit() const { return exit_requested_; }

  /**
   * Set callback for opening preferences dialog.
   * Must be set by Application after construction.
   */
  void setPreferencesCallback(std::function<void()> callback) {
    on_open_preferences_ = std::move(callback);
  }

private:
  // === Flow handlers ===
  void handleMapSelection(const std::filesystem::path &path, int index);
  void handleClientAutoMatch(const std::filesystem::path &map_path);
  void handleClientSelection(uint32_t index);
  void handleBrowseMap();
  void handleBrowseSecMap();
  void selectSECMapFolder(const std::filesystem::path &folder);
  void handleNewMapFlow();
  void handleNewMapConfirmed(const UI::NewMapPanel::State& config);
  void handleLoadMap();
  void handleClientConfiguration();
  void handlePreferences();

  // === Dependencies ===
  UI::StartupDialog &dialog_;
  MapOperationHandler &map_ops_;
  Services::ConfigService &config_;
  Services::ClientVersionRegistry &registry_;
  Services::RecentLocationsService &recent_locations_;
  AppStateManager &state_manager_;

  // === State ===
  std::filesystem::path selected_map_path_;
  uint32_t matched_client_index_ = 0;
  bool exit_requested_ = false;

  // === Callbacks ===
  std::function<void()> on_open_preferences_;

  // === Client configuration controller ===
  std::unique_ptr<Presentation::ClientConfigurationController> client_config_ctrl_;
};

} // namespace AppLogic
} // namespace MapEditor
