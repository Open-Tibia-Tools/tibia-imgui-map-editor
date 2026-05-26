#pragma once
#include "Application/MapTabManager.h"
#include "Domain/ChunkedMap.h"
#include "Services/ClientVersionRegistry.h"
#include "Services/ConfigService.h"
#include "Services/Map/MapLoadingService.h"
#include "Services/Map/MapSavingService.h"
#include "Services/OtbmSettings.h"
#include "Services/RecentLocationsService.h"
#include "Services/ViewSettings.h"
#include "UI/Dialogs/MapCompatibilityPopup.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace MapEditor {

namespace Services {
class ClientDataService;
class SpriteManager;
class TilesetService;
} // namespace Services
namespace Brushes {
class BrushRegistry;
}
namespace UI {
class MapPanel;
}
namespace AppLogic {
class MapInputController;
class MapConversionHandler;
} // namespace AppLogic

namespace AppLogic {

/**
 * Handles map operations: open, save, and direct map creation.
 *
 * Single responsibility: map file lifecycle management.
 * Note: Legacy ProjectConfigDialog flow has been replaced with direct
 * operations.
 */
class MapOperationHandler {
public:
  // Callback types for results
  // NOTE: Renderer not included - caller uses
  // RenderingManager::createRenderer()
  using MapLoadedCallback = std::function<void(
      std::unique_ptr<Domain::ChunkedMap> map,
      std::unique_ptr<Services::ClientDataService> client_data,
      std::unique_ptr<Services::SpriteManager> sprite_manager,
      const Domain::Position &camera_center)>;
  using MapSavedCallback = std::function<void(bool success)>;

  // Notification types
  enum class NotificationType { Success, Error, Info, Warning };
  using NotificationCallback =
      std::function<void(NotificationType type, const std::string &message)>;

  MapOperationHandler(Services::ConfigService &config,
                       Services::ClientVersionRegistry &versions,
                       Services::RecentLocationsService &recent_locations,
                       Services::ViewSettings &view_settings,
                       AppLogic::MapTabManager &tab_manager,
                       Brushes::BrushRegistry &brush_registry,
                       Services::TilesetService &tileset_service,
                       const Services::OtbmSettings &otbm_settings);

  // Destructor must be declared here and defined in .cpp due to
  // forward-declared unique_ptr member
  ~MapOperationHandler();

  // Modern map operations (direct, no dialog state)
  void handleOpenMap();     // Opens file browser
  void handleSaveMap();     // Saves current map
  void handleSaveAsMap();   // Save as dialog
  void handleSaveAllMaps(); // Saves all open maps
  void handleOpenRecentMap(const std::filesystem::path &path, uint32_t index);

  /**
   * Create a new map directly and load it into the editor.
   * Called from editor-state shortcut (Ctrl+N / Menu / Ribbon).
   * Creates an unnamed map with header data copied from the current map.
   */
  void handleNewMapDirect(const Services::NewMapConfig &config);

  /**
   * Create a new map and save it to disk without loading into editor.
   * Called from StartupDialog's New Map dialog (Option B flow).
   * Returns true if save succeeded.
   */
  bool createAndSaveNewMap(const Services::NewMapConfig &config,
                           const std::filesystem::path &save_path);

  /**
   * Open a SEC map directly.
   * Called from StartupController after OpenSecMap modal is confirmed.
   */
  void handleOpenSecMapDirect(const std::filesystem::path &sec_folder,
                               uint32_t index);

  /**
   * Open a SEC map from the File menu (Editor state).
   * Handles folder picking and client auto-matching, then delegates to
   * handleOpenSecMapDirect.
   */
  void handleOpenSecMapFromMenu(const std::filesystem::path &folder);

  // ID conversion operations
  void handleConvertToServerId();
  void handleConvertToClientId();

  // Callbacks
  void setMapLoadedCallback(MapLoadedCallback callback) {
    on_map_loaded_ = std::move(callback);
  }
  void setMapSavedCallback(MapSavedCallback callback) {
    on_map_saved_ = std::move(callback);
  }
  void setNotificationCallback(NotificationCallback callback) {
    on_notification_ = std::move(callback);
  }

  const std::filesystem::path &getPendingMapPath() const {
    return pending_map_path_;
  }
  uint32_t getCurrentVersion() const { return current_client_index_; }

  // State accessor
  bool isLoading() const { return is_loading_; }

  // Dependencies needed by Application after load
  void setExistingResources(Services::ClientDataService *client_data,
                            Services::SpriteManager *sprite_manager);

  // Second map loading with compatibility check
  void handleSecondMapOpen(const std::filesystem::path &path);
  UI::MapCompatibilityResult checkMapCompatibility(uint32_t map_items_major,
                                                   uint32_t map_items_minor);

  // Compatibility popup for incompatible maps
  UI::MapCompatibilityPopup &getCompatibilityPopup() {
    return compatibility_popup_;
  }

  // Deferred map loading (to avoid mid-frame state changes)
  void requestDeferredMapLoad(const std::filesystem::path &path,
                               uint32_t index);
  void processPendingMapLoad();
  bool hasPendingMapLoad() const { return deferred_load_pending_; }

private:
  void loadMapFromPath(const std::filesystem::path &path, uint32_t index);
  void transferNewResources(Services::MapLoadingResult result);
  void performSave(EditorSession &session,
                   const std::filesystem::path &save_path, bool is_save_as);
  void notify(NotificationType type, const std::string &message);

  // Dependencies (not owned)
  Services::ConfigService &config_;
  Services::ClientVersionRegistry &versions_;
  Services::RecentLocationsService &recent_locations_;
  Services::ViewSettings &view_settings_;
  AppLogic::MapTabManager &tab_manager_;
  Brushes::BrushRegistry &brush_registry_;
  Services::TilesetService &tileset_service_;
  const Services::OtbmSettings &otbm_settings_;

  // Loading service (owned)
  std::unique_ptr<Services::MapLoadingService> loading_service_;

  // State
  std::filesystem::path pending_map_path_;
  uint32_t current_client_index_ = 0;
  bool is_loading_ = false;

  // Existing resources (not owned, for reuse check)
  Services::ClientDataService *existing_client_data_ = nullptr;
  Services::SpriteManager *existing_sprite_manager_ = nullptr;

  // Conversion handler (owned)
  std::unique_ptr<MapConversionHandler> conversion_handler_;

  // Compatibility popup for second map loading
  UI::MapCompatibilityPopup compatibility_popup_;

  // Deferred map load state (to avoid mid-frame state changes)
  bool deferred_load_pending_ = false;
  std::filesystem::path deferred_load_path_;
  uint32_t deferred_load_index_ = 0;

  // Callbacks
  MapLoadedCallback on_map_loaded_;
  MapSavedCallback on_map_saved_;
  NotificationCallback on_notification_;
};

} // namespace AppLogic
} // namespace MapEditor
