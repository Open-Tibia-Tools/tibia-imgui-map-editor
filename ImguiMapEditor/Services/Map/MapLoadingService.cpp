#include "MapLoadingService.h"
#include "IO/HouseXmlReader.h"
#include "IO/Otbm/OtbmReader.h"
#include "IO/SecReader.h"
#include "IO/SpawnXmlReader.h"
#include "Services/TilesetService.h"
#include <climits>
#include <spdlog/spdlog.h>

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
  spdlog::info("Loading map: {}", path.string());

  // First, read OTBM header to get version info
  auto header = IO::OtbmReader::readHeader(path);
  if (!header.success) {
    result.error = header.error;
    spdlog::error("Failed to read map header: {}", header.error);
    return result;
  }

  // The OTBM stores OTB minor version, not client version directly
  uint32_t otb_minor = header.version.client_version_minor;
  spdlog::info("Map OTB minor version: {}", otb_minor);

  // If user already selected a version, use that; otherwise auto-detect
  if (current_version == 0) {
    auto *version = version_registry_.findVersionForOtb(otb_minor);
    if (version) {
      current_version = version->getVersion();
      spdlog::info("Auto-detected client version {} for OTB minor version {}",
                   current_version, otb_minor);
    } else {
      // OTB minor 0 is common for ancient 7.x maps - don't fail, just warn
      if (otb_minor == 0) {
        spdlog::warn("OTB minor version 0 (ancient map). Please select a 7.x "
                     "client version.");
        result.error = "Ancient map format (OTB minor 0). Please select client "
                       "version manually (e.g., 760 or 770).";
      } else {
        result.error = "Cannot find client version for OTB minor version: " +
                       std::to_string(otb_minor);
      }
      spdlog::error("{}", result.error);
      return result;
    }
  } else {
    spdlog::info("Using user-selected client version: {}", current_version);
  }

  // Load client data
  if (!loadClientData(current_version, pending_path)) {
    result.error = "Failed to load client data";
    return result;
  }

  // Create map and load OTBM - map is now owned by result
  IO::OtbmReadResult otbm_result = IO::OtbmReader::read(
      path, client_data_service_.get(),
      [](int percent, const std::string &status) {
        spdlog::debug("Map load: {}% - {}", percent, status);
      });

  if (!otbm_result.success) {
    result.error = otbm_result.error;
    spdlog::error("Failed to load map: {}", otbm_result.error);
    return result;
  }

  // Take ownership of the loaded map
  current_map_ = std::move(otbm_result.map);

  // Copy version info from OTBM result to map for saving
  Domain::ChunkedMap::MapVersion map_version;
  map_version.otbm_version = otbm_result.version.otbm_version;
  map_version.client_version = current_version;
  map_version.items_major_version = otbm_result.version.client_version_major;
  map_version.items_minor_version = otbm_result.version.client_version_minor;
  current_map_->setVersion(map_version);

  current_map_->setFilename(path.string());
  current_map_->setName(path.stem().string());

  // Load Spawns (-spawn.xml)
  std::filesystem::path const spawn_path =
      path.parent_path() / (path.stem().string() + "-spawn.xml");
  IO::SpawnXmlReader::read(spawn_path, *current_map_);

  // Load Houses (-house.xml)
  std::filesystem::path const house_path =
      path.parent_path() / (path.stem().string() + "-house.xml");
  IO::HouseXmlReader::read(house_path, *current_map_);

  // Cache sprites for performance
  if (client_data_service_ && sprite_manager_) {
    size_t cached =
        client_data_service_->optimizeItemSprites(*sprite_manager_, true);
    spdlog::info("Sprite caching: {} item types now use direct lookup", cached);
  }

  spdlog::info("Map loaded: {} tiles, version {}", otbm_result.tile_count,
               otbm_result.version.client_version);

  // Find camera center before transferring ownership
  result.camera_center = findCameraCenter();

  // Transfer ownership of resources to result
  // NOTE: Renderer is NOT transferred - caller uses
  // RenderingManager::createRenderer()
  result.map = std::move(current_map_);
  result.client_data = std::move(client_data_service_);
  result.sprite_manager = std::move(sprite_manager_);

  // Architecture trace: show what MapLoadingService returns (NO renderer!)
  spdlog::info("[MapLoadingService] Returning result:");
  spdlog::info("  - map: {} (tiles: {})", result.map ? "valid" : "null",
               result.map ? result.map->getTileCount() : 0);
  spdlog::info("  - client_data: {}", result.client_data ? "valid" : "null");
  spdlog::info("  - sprite_manager: {}",
               result.sprite_manager ? "valid" : "null");
  spdlog::info(
      "  - renderer: NOT INCLUDED (created by RenderingManager factory)");
  spdlog::info("  - camera_center: ({},{},{})", result.camera_center.x,
               result.camera_center.y, result.camera_center.z);

  result.success = true;
  return result;
}

MapLoadingResult MapLoadingService::loadMapWithExistingClientData(
    const std::filesystem::path &path,
    Services::ClientDataService *existing_client_data,
    Services::SpriteManager *existing_sprite_manager) {
  MapLoadingResult result;

  if (!existing_client_data) {
    result.error = "Existing client data is required";
    return result;
  }

  spdlog::info("Loading map with existing client data: {}", path.string());

  // Read OTBM header to get version info (for logging)
  auto header = IO::OtbmReader::readHeader(path);
  if (!header.success) {
    result.error = header.error;
    spdlog::error("Failed to read map header: {}", header.error);
    return result;
  }

  spdlog::info("OTBM v{}, size {}x{}, client version {}.{}",
               header.version.otbm_version, header.version.width,
               header.version.height, header.version.client_version_major,
               header.version.client_version_minor);

  // Load OTBM using the EXISTING client data - items get correct ItemType
  // pointers
  IO::OtbmReadResult otbm_result = IO::OtbmReader::read(
      path, existing_client_data, [](int percent, const std::string &status) {
        spdlog::debug("Map load: {}% - {}", percent, status);
      });

  if (!otbm_result.success) {
    result.error = otbm_result.error;
    spdlog::error("Failed to load map: {}", otbm_result.error);
    return result;
  }

  // Take ownership of the loaded map
  auto loaded_map = std::move(otbm_result.map);

  // Set map metadata
  loaded_map->setFilename(path.string());
  loaded_map->setName(path.stem().string());

  // Copy version info from OTBM result to map for saving
  Domain::ChunkedMap::MapVersion map_version;
  map_version.otbm_version = otbm_result.version.otbm_version;
  map_version.client_version = otbm_result.version.client_version;
  map_version.items_major_version = otbm_result.version.client_version_major;
  map_version.items_minor_version = otbm_result.version.client_version_minor;
  loaded_map->setVersion(map_version);

  // Load Spawns (-spawn.xml)
  std::filesystem::path const spawn_path =
      path.parent_path() / (path.stem().string() + "-spawn.xml");
  IO::SpawnXmlReader::read(spawn_path, *loaded_map);

  // Load Houses (-house.xml)
  std::filesystem::path const house_path =
      path.parent_path() / (path.stem().string() + "-house.xml");
  IO::HouseXmlReader::read(house_path, *loaded_map);

  spdlog::info("Map loaded: {} tiles, version {}", otbm_result.tile_count,
               otbm_result.version.client_version);

  // Find camera center
  current_map_ = std::move(loaded_map);
  result.camera_center = findCameraCenter();

  // Transfer map to result - client_data and sprite_manager stay null
  // (caller reuses existing ones)
  result.map = std::move(current_map_);
  // result.client_data = nullptr (intentionally)
  // result.sprite_manager = nullptr (intentionally)

  spdlog::info("[MapLoadingService::loadMapWithExistingClientData] Returning:");
  spdlog::info("  - map: valid (tiles: {})", result.map->getTileCount());
  spdlog::info("  - client_data: null (using existing)");
  spdlog::info("  - sprite_manager: null (using existing)");
  spdlog::info("  - camera_center: ({},{},{})", result.camera_center.x,
               result.camera_center.y, result.camera_center.z);

  result.success = true;
  return result;
}

MapLoadingResult
MapLoadingService::loadSecMap(const std::filesystem::path &directory,
                              uint32_t current_version) {
  MapLoadingResult result;
  spdlog::info("Loading SEC map from: {}", directory.string());

  if (current_version == 0) {
    result.error =
        "Client version must be specified for SEC maps (no auto-detect)";
    return result;
  }

  // Load client data first
  if (!loadClientData(current_version, directory)) {
    result.error = "Failed to load client data. SEC maps require items.srv.";
    return result;
  }

  // Verify items.srv was loaded (SEC requires server IDs)
  if (!client_data_service_ || !client_data_service_->hasServerIdSupport()) {
    result.error = "SEC maps require items.srv for server ID lookup";
    spdlog::error("{}", result.error);
    return result;
  }

  // Create map and load SEC
  current_map_ = std::make_unique<Domain::ChunkedMap>();

  IO::SecResult sec_result = IO::SecReader::read(
      directory, *current_map_, client_data_service_.get(),
      [](int percent, const std::string &status) {
        spdlog::debug("SEC load: {}% - {}", percent, status);
      });

  if (!sec_result.success) {
    result.error = sec_result.error;
    spdlog::error("Failed to load SEC map: {}", sec_result.error);
    current_map_.reset();
    return result;
  }

  // Set map version info
  Domain::ChunkedMap::MapVersion map_version;
  map_version.otbm_version = 1; // SEC maps are ancient
  map_version.client_version = current_version;
  map_version.items_major_version = 0;
  map_version.items_minor_version = 0;
  current_map_->setVersion(map_version);

  current_map_->setName(directory.filename().string());

  // Cache sprites for performance
  if (client_data_service_ && sprite_manager_) {
    size_t cached =
        client_data_service_->optimizeItemSprites(*sprite_manager_, true);
    spdlog::info("Sprite caching: {} item types now use direct lookup", cached);
  }

  spdlog::info("SEC map loaded: {} sectors, {} tiles, {} items",
               sec_result.sector_count, sec_result.tile_count,
               sec_result.item_count);

  // Find camera center before transferring ownership
  result.camera_center = findCameraCenter();

  // Transfer ownership of resources to result
  // NOTE: Renderer is NOT transferred - caller uses
  // RenderingManager::createRenderer()
  result.map = std::move(current_map_);
  result.client_data = std::move(client_data_service_);
  result.sprite_manager = std::move(sprite_manager_);

  result.success = true;
  return result;
}

MapLoadingResult MapLoadingService::createNewMap(const NewMapConfig &config,
                                                 uint32_t current_version) {
  MapLoadingResult result;

  spdlog::info("Creating new map: {} ({}x{})", config.map_name,
               config.map_width, config.map_height);

  // Load client data first
  if (!loadClientData(current_version, {})) {
    result.error = "Failed to load client data";
    return result;
  }

  current_map_ = std::make_unique<Domain::ChunkedMap>();
  current_map_->createNew(config.map_width, config.map_height, current_version);
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
    uint32_t client_version, const std::filesystem::path &pending_path) {
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

  // Debug: check what files exist
  auto dat_path = version_info->getDatPath();
  auto spr_path = version_info->getSprPath();
  auto otb_path = version_info->getOtbPath();

  spdlog::info("Checking client files:");
  spdlog::info("  DAT: {} -> {}", dat_path.string(),
               std::filesystem::exists(dat_path) ? "EXISTS" : "NOT FOUND");
  spdlog::info("  SPR: {} -> {}", spr_path.string(),
               std::filesystem::exists(spr_path) ? "EXISTS" : "NOT FOUND");
  spdlog::info("  OTB: {} -> {}", otb_path.string(),
               std::filesystem::exists(otb_path) ? "EXISTS" : "NOT FOUND");

  // Also check for items.srv (ancient 7.x format)
  auto srv_path = version_info->getClientPath() / "items.srv";
  if (!std::filesystem::exists(otb_path) && std::filesystem::exists(srv_path)) {
    spdlog::info("  SRV: {} -> EXISTS (will use as fallback)",
                 srv_path.string());
  }

  // Validate required files exist
  if (!version_info->validateFiles()) {
    std::string missing_list;
    if (!std::filesystem::exists(dat_path)) missing_list += " Tibia.dat";
    if (!std::filesystem::exists(spr_path)) missing_list += " Tibia.spr";
    if (!std::filesystem::exists(otb_path) && !std::filesystem::exists(srv_path))
      missing_list += " items.otb";
    spdlog::error(
        "Client files not found for version {} in path '{}'. Missing:{}",
        client_version, version_info->getClientPath().string(), missing_list);
    spdlog::error("Required files: Tibia.dat, Tibia.spr, and items.otb (or "
                  "items.srv for 7.x)");
    return false;
  }

  // Create client data service if needed
  if (!client_data_service_) {
    client_data_service_ = std::make_unique<Services::ClientDataService>();
  }

  // Use OTB or SRV path from client version
  // ClientDataService::load() handles format detection automatically
  auto final_item_path = otb_path;
  if (!std::filesystem::exists(final_item_path)) {
    // Check for items.srv fallback (ancient 7.x format)
    if (std::filesystem::exists(srv_path)) {
      final_item_path = srv_path;
      spdlog::info("Using items.srv (ancient format) instead of items.otb");
    } else {
      final_item_path = std::filesystem::path("data") / "items.otb";
    }
  }

  // Load client data
  auto result = client_data_service_->load(
      version_info->getClientPath(), final_item_path, client_version,
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
  std::filesystem::path const app_data_path =
      std::filesystem::current_path() / "data";

  bool const tilesets_loaded = tileset_service_.loadTilesets(app_data_path);
  if (!tilesets_loaded) {
    spdlog::warn("No tilesets found. The palette will be empty.");
  }

  // Load palettes (must be after tilesets since palettes reference tilesets)
  bool const palettes_loaded = tileset_service_.loadPalettes(app_data_path);
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
