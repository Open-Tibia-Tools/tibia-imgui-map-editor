#pragma once
#include "Domain/ChunkedMap.h"
#include "Domain/Position.h"
#include "Services/ClientDataService.h"
#include "Services/ClientVersionRegistry.h"
#include "Services/SpriteManager.h"
#include "Services/ViewSettings.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <string_view>

namespace MapEditor {

namespace Brushes {
class BrushRegistry;
}

namespace Services {
class TilesetService;
}

namespace Services {

/**
 * Configuration for creating a new map.
 */
struct NewMapConfig {
  std::string map_name;
  int map_width = 256;
  int map_height = 256;
};

/**
 * Result of a map loading operation with full resource ownership.
 * Transfers ownership immediately - no follow-up takeX() calls needed.
 */
struct MapLoadingResult {
  bool success = false;
  std::string error;
  Domain::Position camera_center{0, 0, 7};

  // Resources - ownership transferred to caller
  // NOTE: MapRenderer is NOT included here - use
  // RenderingManager::createRenderer()
  std::unique_ptr<Domain::ChunkedMap> map;
  std::unique_ptr<Services::ClientDataService> client_data;
  std::unique_ptr<Services::SpriteManager> sprite_manager;
};

/**
 * Handles map loading, creation, and client data management.
 *
 * Extracted from Application to separate business logic from orchestration.
 * Uses callbacks for UI updates to maintain decoupling.
 */
class MapLoadingService {
public:
  using ProgressCallback =
      std::function<void(int percent, const std::string &status)>;
  using CameraCenterCallback = std::function<void(const Domain::Position &pos)>;

  MapLoadingService(Services::ClientVersionRegistry &version_registry,
                    Services::ViewSettings &view_settings,
                    Brushes::BrushRegistry &brush_registry,
                    Services::TilesetService &tileset_service);

  ~MapLoadingService() = default;

  // Non-copyable
  MapLoadingService(const MapLoadingService &) = delete;
  MapLoadingService &operator=(const MapLoadingService &) = delete;

  /**
   * Load an existing map from file.
   * @param path Path to OTBM file
   * @param current_client_index Client index (0 for auto-detect)
   * @param pending_path Optional pending map path for client file discovery
   * @return Load result with success/error and camera center
   */
  MapLoadingResult loadMap(const std::filesystem::path &path,
                           uint32_t &current_client_index,
                           const std::filesystem::path &pending_path = {});

  /**
   * Load an existing map from file using existing client data.
   * Use this when loading a second map after client data is already loaded.
   * @param path Path to OTBM file
   * @param existing_client_data Existing client data to use for item type
   * lookup
   * @param existing_sprite_manager Existing sprite manager to reuse
   * @return Load result with map only (client_data/sprite_manager are null)
   */
  MapLoadingResult loadMapWithExistingClientData(
      const std::filesystem::path &path,
      Services::ClientDataService *existing_client_data,
      Services::SpriteManager *existing_sprite_manager);

  /**
   * Load SEC format map using existing client data.
   * Use this when loading a second SEC map after client data is already loaded.
   */
  MapLoadingResult loadSecMapWithExistingClientData(
      const std::filesystem::path &directory,
      Services::ClientDataService *existing_client_data,
      Services::SpriteManager *existing_sprite_manager);

  /**
   * Create a new empty map.
   * @param config New map configuration
   * @param current_client_index Client index to use
   * @return Load result with success/error
   */
  MapLoadingResult createNewMap(const NewMapConfig &config,
                                uint32_t current_client_index);

  /**
   * Load client data (OTB, DAT, SPR) for the specified client index.
   * @param client_index Client index to load
   * @param pending_path Optional path for client file discovery
   * @return true if successful
   */
  bool loadClientData(uint32_t client_index,
                      const std::filesystem::path &pending_path,
                      std::optional<Domain::ItemDataSource> source_override = std::nullopt);

  /**
   * Load SEC format map from directory.
   * @param directory Folder containing *.sec files
   * @param current_client_index Client index (must match items.srv)
   * @return Load result
   */
  MapLoadingResult loadSecMap(const std::filesystem::path &directory,
                              uint32_t current_client_index);

private:
  Domain::Position findCameraCenter() const;

  MapLoadingResult loadSecMapCore(const std::filesystem::path &directory,
                                  Services::ClientDataService &client_data);

  bool tryLoadCreatures(const std::filesystem::path &map_dir,
                        const std::filesystem::path &client_path);
  bool tryLoadItems(const std::filesystem::path &map_dir,
                    const std::filesystem::path &client_path);

  template <typename LoaderFunc>
  bool tryLoadResource(std::string_view filename,
                       const std::filesystem::path &map_dir,
                       const std::filesystem::path &client_path,
                       LoaderFunc loader) {
    struct SearchLocation {
      std::filesystem::path path;
      std::string_view message_format;
    };

    std::vector<SearchLocation> search_locations;
    if (!map_dir.empty()) {
      search_locations.push_back(
          {map_dir / filename, "Loaded {} from map directory"});
    }
    if (!client_path.empty()) {
      search_locations.push_back(
          {client_path / filename, "Loaded {} from client directory"});
    }
    search_locations.push_back(
        {std::filesystem::path("data") / filename, "Loaded bundled {}"});

    for (const auto &loc : search_locations) {
      if (std::filesystem::exists(loc.path) && loader(loc.path)) {
        // Use runtime formatting to allow variable format string
        spdlog::info(fmt::runtime(loc.message_format), filename);
        return true;
      }
    }

    return false;
  }

  Services::ClientVersionRegistry &version_registry_;
  Services::ViewSettings &view_settings_;
  Brushes::BrushRegistry &brush_registry_;
  Services::TilesetService &tileset_service_;

  // Temporary storage during loading (moved to result after load completes)
  std::unique_ptr<Domain::ChunkedMap> current_map_;
  std::unique_ptr<Services::ClientDataService> client_data_service_;
  std::unique_ptr<Services::SpriteManager> sprite_manager_;
};

} // namespace Services
} // namespace MapEditor
