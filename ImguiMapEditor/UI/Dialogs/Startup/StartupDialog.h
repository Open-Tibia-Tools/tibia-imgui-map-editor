#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Forward declarations
namespace MapEditor::Services {
class ClientVersionRegistry;
class ConfigService;
} // namespace MapEditor::Services
#include "UI/Dialogs/ClientConfiguration/ClientConfigurationDialog.h"
#include "UI/Dialogs/NewMapDialog.h"
#include "UI/DTOs/ClientInfo.h"
#include "UI/DTOs/RecentMapEntry.h"
#include "UI/DTOs/SelectedMapInfo.h"
#include "UI/Panels/NewMapPanel.h"
#include "UI/Dialogs/Startup/AvailableClientsPanel.h"
#include "UI/Dialogs/Startup/ClientInfoPanel.h"
#include "UI/Dialogs/Startup/RecentMapsPanel.h"
#include "UI/Dialogs/Startup/SelectedMapPanel.h"
namespace MapEditor {
namespace UI {

/**
 * Unified Startup Dialog - multi-column dashboard.
 *
 * Replaces legacy WelcomeScreen with modern layout:
 * - Header: Title + Preferences gear
 * - Sidebar: New Map, Browse Map, Browse .sec buttons
 * - 4 columns: Recent Maps, Selected Map Info, Client Info, Recent Clients
 * - Footer: Exit, Version, Ignore signatures, Client Config, Load Map
 *
 * Uses reactive pattern: actions are returned to controller via
 * consumeResult().
 */
class StartupDialog {
public:
  // === Actions returned to controller ===
  enum class Action {
    None,
    SelectRecentMap,
    SelectClient, // User clicked a client in Available Clients panel
    BrowseMap,
    BrowseSecMap,
    NewMap,           // Open new map modal
    NewMapConfirmed,  // New map modal confirmed - create map
    ClientConfiguration,
    Preferences,
    LoadMap,
    Exit
  };

  struct Result {
    Action action = Action::None;
    std::filesystem::path selected_path;
    uint32_t selected_client_index = 0;
    int selected_index = -1;
    // For NewMapConfirmed action
    NewMapPanel::State new_map_config;
  };

  StartupDialog() = default;
  ~StartupDialog() = default;

  // === Initialization ===
  void initialize(Services::ClientVersionRegistry *registry,
                  Services::ConfigService *config);

  // === Main render (called each frame) ===
  void render(const std::vector<RecentMapEntry> &recent_maps,
              uint32_t matched_client_index);

  // === State setters (from controller) ===
  void setSelectedMapInfo(const SelectedMapInfo &info);
  void setClientInfo(const ClientInfo &info);
  void setSignatureMismatch(bool mismatch, const std::string &message);
  void setIgnoreSignatures(bool ignore);
  void setLoadEnabled(bool enabled);
  void setClientNotConfigured(bool not_configured);
  void setSelectedIndex(int index);

  // === Result getters ===
  bool hasResult() const { return pending_result_.action != Action::None; }
  Result consumeResult();

  // === Sub-dialog access ===
  ClientConfigurationDialog &getClientConfigDialog() {
    return client_config_dialog_;
  }

  // === Modal control ===
  void showNewMapModal() { show_new_map_modal_ = true; }

  // === State queries ===
  bool isIgnoreSignatures() const { return ignore_signatures_; }
  int getSelectedIndex() const { return selected_recent_index_; }
  const SelectedMapInfo &getSelectedMapInfo() const {
    return selected_map_info_;
  }

private:
  // === Render methods ===
  void renderHeader();
  void renderSidebar();
  void renderFooter();

  // Panel rendering methods (delegate to extracted components)
  void renderRecentMapsPanel(const std::vector<RecentMapEntry> &entries);
  void renderSelectedMapPanel();
  void renderClientInfoPanel();
  void renderRecentClientsPanel(uint32_t selected_client_index);

  // === Dependencies ===
  Services::ClientVersionRegistry *registry_ = nullptr;
  Services::ConfigService *config_ = nullptr;

  // === UI State ===
  int selected_recent_index_ = -1;
  SelectedMapInfo selected_map_info_;
  ClientInfo client_info_;
  bool signature_mismatch_ = false;
  std::string signature_mismatch_message_;
  bool ignore_signatures_ = false;
  bool load_enabled_ = false;
  bool client_not_configured_ = false;

  // Modal trigger flags (dialogs manage their own state)
  bool show_new_map_modal_ = false;

  Result pending_result_;

  // === Sub-components (using standalone dialogs for DRY) ===
  NewMapDialog new_map_dialog_;
  ClientConfigurationDialog client_config_dialog_;

  // === Extracted Panel Components ===
  RecentMapsPanel recent_maps_panel_;
  SelectedMapPanel selected_map_panel_;
  ClientInfoPanel client_info_panel_;
  AvailableClientsPanel available_clients_panel_;
};

} // namespace UI
} // namespace MapEditor
