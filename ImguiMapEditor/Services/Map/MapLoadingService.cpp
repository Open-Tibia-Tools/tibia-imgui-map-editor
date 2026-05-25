#include "MapLoadingService.h"

#include <climits>
#include <filesystem>
#include <optional>

#include <spdlog/spdlog.h>

#include "IO/HouseXmlReader.h"
#include "IO/Otbm/OtbmReader.h"
#include "IO/SecReader.h"
#include "IO/SpawnXmlReader.h"
#include "Services/TilesetService.h"

namespace MapEditor {
namespace Services {

MapLoadingService::MapLoadingService(ClientVersionRegistry &version_registry,
                                     ViewSettings &view_settings,
                                     Brushes::BrushRegistry &brush_registry,
                                     TilesetService &tileset_service)
    : version_registry_(version_registry), view_settings_(view_settings),
      brush_registry_(brush_registry), tileset_service_(tileset_service) {}

MapLoadingResult
MapLoadingService::loadMap(const std::filesystem::path &path,
                           uint32_t &current_version,
                           const std::filesystem::path &pending_path) {
  MapLoadingResult result;

  // 1. Read OTBM header to get version info
  auto header_result = IO::OtbmReader::readHeader(path);
  if (!header_result.success) {
    result.error = "Failed to read map header: " + header_result.error;
    return result;
  }

  const auto &ver = header_result.version;
  spdlog::info("Loading map version {} (Items {}.{})", ver.otbm_version,
               ver.client_version_major, ver.client_version_minor);

  // 2. Select client version
  if (current_version == 0) {
    // Try to find a matching version in registry
    auto *matching_version =
        version_registry_.findVersionForOtb(ver.client_version_minor);
    if (matching_version) {
      current_version = matching_version->getVersion();
      spdlog::info("Auto-detected client version: {}", current_version);
    } else {
      current_version = version_registry_.getDefaultVersion();
      spdlog::info("No exact match for Items {}.{}, using default: {}",
                   ver.client_version_major, ver.client_version_minor,
                   current_version);
    }
  }

  // 3. Load client data
  if (!loadClientData(current_version, pending_path)) {
    result.error = "Failed to load client data for version " +
                   std::to_string(current_version);
    return result;
  }

  // 4. Read actual map content
  auto read_result =
      IO::OtbmReader::read(path, client_data_service_.get(),
                           [](int percent, const std::string &status) {
                             spdlog::info("Map Load: {}% - {}", percent, status);
                           });

  if (!read_result.success) {
    result.error = "Failed to read map data: " + read_result.error;
    return result;
  }

  current_map_ = std::move(read_result.map);

  // Center camera
  result.camera_center = findCameraCenter();

  // Cache sprites for performance
  if (client_data_service_ && sprite_manager_) {
    size_t cached =
        client_data_service_->optimizeItemSprites(*sprite_manager_, true);
    spdlog::info("Sprite caching: {} item types now use direct lookup", cached);
  }

  // Transfer ownership
  result.map = std::move(current_map_);
  result.client_data = std::move(client_data_service_);
  result.sprite_manager = std::move(sprite_manager_);

  result.success = true;
  return result;
}

MapLoadingResult MapLoadingService::loadMapWithExistingClientData(
    const std::filesystem::path &path,
    Services::ClientDataService *existing_client_data,
    Services::SpriteManager *existing_sprite_manager) {
  MapLoadingResult result;

  if (!existing_client_data) {
    result.error = "No client data provided for map loading";
    return result;
  }

  spdlog::info("[MapLoadingService] Loading map with existing client data");

  auto read_result =
      IO::OtbmReader::read(path, existing_client_data,
                           [](int percent, const std::string &status) {
                             spdlog::info("Map Load: {}% - {}", percent, status);
                           });

  if (!read_result.success) {
    result.error = "Failed to read map data: " + read_result.error;
    return result;
  }

  current_map_ = std::move(read_result.map);
  result.camera_center = findCameraCenter();

  // Cache sprites (reuse manager)
  if (existing_sprite_manager) {
    size_t cached =
        existing_client_data->optimizeItemSprites(*existing_sprite_manager, true);
    spdlog::info("Sprite caching (reused): {} item types now use direct lookup",
                 cached);
  }

  result.map = std::move(current_map_);
  result.success = true;
  return result;
}

MapLoadingResult
MapLoadingService::loadSecMap(const std::filesystem::path &directory,
                              uint32_t current_version) {
  MapLoadingResult result;

  // 1. Force SRV mode for SEC maps
  if (!loadClientData(current_version, directory, ::MapEditor::Domain::ItemDataSource::SRV)) {
    result.error = "Failed to load client data (SRV) for version " +
                   std::to_string(current_version);
    return result;
  }

  // 2. Use SecReader to load the directory
  auto read_result = IO::SecReader::read(
      directory, client_data_service_.get(),
      [](int percent, const std::string &status) {
        spdlog::info("SEC Load: {}% - {}", percent, status);
      });

  if (!read_result.success) {
    result.error = "Failed to read SEC map data: " + read_result.error;
    return result;
  }

  current_map_ = std::move(read_result.map);

  // Set version info for the SEC map
  auto *version_info = version_registry_.getVersion(current_version);
  if (version_info) {
    Domain::ChunkedMap::MapVersion map_version;
    map_version.otbm_version = version_info->getOtbmVersion();
    map_version.client_version = current_version;
    map_version.items_major_version = version_info->getOtbMajor();
    map_version.items_minor_version = version_info->getOtbVersion();
    current_map_->setVersion(map_version);
  }

  result.camera_center = findCameraCenter();

  // Cache sprites
  if (client_data_service_ && sprite_manager_) {
    size_t cached =
        client_data_service_->optimizeItemSprites(*sprite_manager_, true);
    spdlog::info("Sprite caching: {} item types now use direct lookup", cached);
  }

  result.map = std::move(current_map_);
  result.client_data = std::move(client_data_service_);
  result.sprite_manager = std::move(sprite_manager_);

  result.success = true;
  return result;
}

MapLoadingResult
MapLoadingService::createNewMap(const NewMapConfig &config,
                                uint32_t current_version) {
  MapLoadingResult result;

  // Load client data if not already loaded
  if (!loadClientData(current_version, {})) {
    result.error = "Failed to load client data";
    return result;
  }

  // Create empty map
  current_map_ = std::make_unique<Domain::ChunkedMap>();
  current_map_->setName(config.map_name);

  // Set full version info from ClientVersion registry
  // This ensures items_major_version and items_minor_version are properly set
  // for saving
  auto *version_info = version_registry_.getVersion(current_version);
  if (version_info) {
    Domain::ChunkedMap::MapVersion map_version;
    map_version.otbm_version = version_info->getOtbmVersion();
    map_version.client_version = current_version;
    map_version.items_major_version = version_info->getOtbMajor();
    map_version.items_minor_version = version_info->getOtbVersion();
    current_map_->setVersion(map_version);
    spdlog::info("New map version set: OTBM v{}, client {}, items {}.{}",
                 map_version.otbm_version, map_version.client_version,
                 map_version.items_major_version,
                 map_version.items_minor_version);
  }

  // Cache sprites for performance
  if (client_data_service_ && sprite_manager_) {
    size_t cached =
        client_data_service_->optimizeItemSprites(*sprite_manager_, true);
    spdlog::info("Sprite caching: {} item types now use direct lookup", cached);
  }

  // Transfer ownership of resources to result
  // NOTE: Renderer is NOT transferred - caller uses
  // RenderingManager::createRenderer()
  result.map = std::move(current_map_);
  result.client_data = std::move(client_data_service_);
  result.sprite_manager = std::move(sprite_manager_);

  result.success = true;
  return result;
}

bool MapLoadingService::loadClientData(
    uint32_t client_version, const std::filesystem::path &pending_path,
    std::optional<::MapEditor::Domain::ItemDataSource> source_override) {
  // Get client version info
  auto *version_info = version_registry_.getVersion(client_version);
  if (!version_info) {
    spdlog::error("Unknown client version: {}", client_version);
    return false;
  }

  // Log expected signatures
  spdlog::info("Client version {} expected signatures:", client_version);
  spdlog::info("  Expected DAT signature: 0x{:08X}",
               version_info->getDatSignature());
  spdlog::info("  Expected SPR signature: 0x{:08X}",
               version_info->getSprSignature());
  spdlog::info("  Expected OTB version: {}", version_info->getOtbVersion());

  // Check if client path is configured
  auto version_client_path = version_info->getClientPath();
  spdlog::info("Configured client path: '{}'", version_client_path.string());

  // If not configured, try to find client files in common locations
  if (version_client_path.empty() ||
      !std::filesystem::exists(version_client_path)) {
    if (!pending_path.empty()) {
      auto map_dir = pending_path.parent_path();
      spdlog::info("Trying client files in map directory: {}",
                   map_dir.string());

      if (std::filesystem::exists(map_dir / "Tibia.dat") &&
          std::filesystem::exists(map_dir / "Tibia.spr")) {
        version_info->setClientPath(map_dir);
        spdlog::info("Found client files in map directory");
      }
    }
  }

  auto effective_source = source_override.value_or(version_info->getDataSource());
  auto client_path = version_info->getClientPath();
  auto metadata_filename = (effective_source == ::MapEditor::Domain::ItemDataSource::SRV) ? "items.srv" : "items.otb";

  // Debug: check what files exist
  auto dat_path = version_info->getDatPath();
  auto spr_path = version_info->getSprPath();
  auto metadata_path = client_path / metadata_filename;

  spdlog::info("Checking client files (source mode: {}):",
               (effective_source == ::MapEditor::Domain::ItemDataSource::SRV) ? "SRV" :
               ((effective_source == ::MapEditor::Domain::ItemDataSource::DAT) ? "DAT-only" : "OTB"));
  spdlog::info("  DAT: {} -> {}", dat_path.string(),
               std::filesystem::exists(dat_path) ? "EXISTS" : "NOT FOUND");
  spdlog::info("  SPR: {} -> {}", spr_path.string(),
               std::filesystem::exists(spr_path) ? "EXISTS" : "NOT FOUND");

  if (effective_source != ::MapEditor::Domain::ItemDataSource::DAT) {
      spdlog::info("  Metadata ({}): {} -> {}", metadata_filename, metadata_path.string(),
                   std::filesystem::exists(metadata_path) ? "EXISTS" : "NOT FOUND");
  }

  // Validate required files exist
  bool valid = true;
  if (!std::filesystem::exists(dat_path) || !std::filesystem::exists(spr_path)) {
      valid = false;
  } else if (effective_source != ::MapEditor::Domain::ItemDataSource::DAT) {
      if (!std::filesystem::exists(metadata_path) &&
          !std::filesystem::exists(std::filesystem::current_path() / "data" / metadata_filename)) {
          valid = false;
      }
  }

  if (!valid) {
    std::string missing_list;
    if (!std::filesystem::exists(dat_path)) missing_list += " Tibia.dat";
    if (!std::filesystem::exists(spr_path)) missing_list += " Tibia.spr";
    if (effective_source != ::MapEditor::Domain::ItemDataSource::DAT) {
        if (!std::filesystem::exists(metadata_path)) missing_list += " " + std::string(metadata_filename);
    }
    spdlog::error(
        "Client files not found for version {} in path '{}'. Missing:{}",
        client_version, client_path.string(), missing_list);
    return false;
  }

  // Create client data service if needed
  if (!client_data_service_) {
    client_data_service_ = std::make_unique<Services::ClientDataService>();
  }

  // Resolve final metadata path
  std::filesystem::path final_metadata_path;
  if (effective_source != ::MapEditor::Domain::ItemDataSource::DAT) {
      final_metadata_path = metadata_path;
      if (!std::filesystem::exists(final_metadata_path)) {
          final_metadata_path = std::filesystem::path("data") / metadata_filename;
          spdlog::info("Using metadata from editor data directory: {}", final_metadata_path.string());
      }
  }

  // Load client data
  auto result = client_data_service_->load(
      client_path, final_metadata_path, client_version,
      effective_source,
      [](int percent, const std::string &status) {
        spdlog::info("Loading: {}% - {}", percent, status);
      });

  if (!result.success) {
    spdlog::error("Failed to load client data: {}", result.error);
    return false;
  }

  auto map_dir = pending_path.empty() ? std::filesystem::path()
                                      : pending_path.parent_path();

  if (!tryLoadCreatures(map_dir, version_client_path)) {
    spdlog::warn("No creature data loaded. Spawns may look incorrect.");
  }

  if (!tryLoadItems(map_dir, version_client_path)) {
    spdlog::warn("No items.xml loaded. Item names may be missing.");
  }

  // Use injected TilesetService instead of creating locally
  // Always use the application's data folder for tilesets and palettes,
  // NOT the map directory - these are app resources, not per-map resources
  std::filesystem::path app_data_path =
      std::filesystem::current_path() / "data";

  bool tilesets_loaded = tileset_service_.loadTilesets(app_data_path);
  if (!tilesets_loaded) {
    spdlog::warn("No tilesets found. The palette will be empty.");
  }

  // Load palettes (must be after tilesets since palettes reference tilesets)
  bool palettes_loaded = tileset_service_.loadPalettes(app_data_path);
  if (!palettes_loaded) {
    spdlog::warn("No palettes loaded. Ribbon palette buttons will be empty.");
  }

  // Create sprite manager with the loaded sprites
  sprite_manager_ = std::make_unique<Services::SpriteManager>(
      client_data_service_->getSpriteReader());

  // Initialize SpriteManager (async loading and GPU resources)
  // This must be done here (on main thread) to ensure service is ready for
  // renderers
  sprite_manager_->initializeAsync(Config::Performance::SPRITE_LOADER_THREADS);
  (void)sprite_manager_->getAtlasManager().getWhitePixel();
  (void)sprite_manager_->getInvalidItemPlaceholder();
  sprite_manager_->syncLUTWithAtlas();

  spdlog::info("Client data loaded: {} items, {} sprites", result.item_count,
               result.sprite_count);

  return true;
}

Domain::Position MapLoadingService::findCameraCenter() const {
  Domain::Position first_tile_pos(0, 0, 7);
  bool found_tile = false;
  int min_x = INT_MAX, min_y = INT_MAX, max_x = 0, max_y = 0;
  int tiles_checked = 0;

  current_map_->forEachTileMutable([&](Domain::Tile *tile) {
    if (tile) {
      auto pos = tile->getPosition();
      tiles_checked++;

      if (pos.x < min_x)
        min_x = pos.x;
      if (pos.y < min_y)
        min_y = pos.y;
      if (pos.x > max_x)
        max_x = pos.x;
      if (pos.y > max_y)
        max_y = pos.y;

      if (!found_tile && pos.z == 7) {
        first_tile_pos = pos;
        found_tile = true;
      }
    }
  });

  spdlog::info("Map bounds: X=[{},{}], Y=[{},{}], checked {} tiles", min_x,
               max_x, min_y, max_y, tiles_checked);

  if (found_tile) {
    spdlog::info("Centering camera on first tile at ({},{},{})",
                 first_tile_pos.x, first_tile_pos.y, first_tile_pos.z);
    return first_tile_pos;
  } else {
    int center_x = (min_x + max_x) / 2;
    int center_y = (min_y + max_y) / 2;
    spdlog::info("No ground tiles found, centering on bounds center ({},{},7)",
                 center_x, center_y);
    return Domain::Position(center_x, center_y, 7);
  }
}

bool MapLoadingService::tryLoadCreatures(
    const std::filesystem::path &map_dir,
    const std::filesystem::path &client_path) {
  return tryLoadResource("creatures.xml", map_dir, client_path,
                         [this](const std::filesystem::path &path) {
                           return client_data_service_->loadCreatureData(path);
                         });
}

bool MapLoadingService::tryLoadItems(const std::filesystem::path &map_dir,
                                     const std::filesystem::path &client_path) {
  return tryLoadResource("items.xml", map_dir, client_path,
                         [this](const std::filesystem::path &path) {
                           return client_data_service_->loadItemData(path);
                         });
}

} // namespace Services
} // namespace MapEditor
