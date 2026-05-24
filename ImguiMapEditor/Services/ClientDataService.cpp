#include "ClientDataService.h"
#include "SpriteManager.h"
// NOTE: TilesetXmlReader, TilesetRegistry, BrushRegistry, CreatureBrush
// includes removed - tileset logic moved to TilesetService
#include <algorithm>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Services {

ClientDataResult
ClientDataService::load(const std::filesystem::path &client_path,
                        const std::filesystem::path &otb_path,
                        uint32_t client_version,
                        Domain::ItemDataSource data_source,
                        LoadProgressCallback progress) {
  ClientDataResult result;
  // Clear any existing data first
  clear();

  if (progress)
    progress(0, "Loading item database...");

  // 1. Load item definitions (OTB or SRV format)
  // SRV is an ancient text-based format from Tibia 7.0-7.7x
  // OTB is the modern binary format
  std::vector<Domain::ItemType> item_definitions;

  if (data_source == Domain::ItemDataSource::DAT) {
    spdlog::info("ClientDataService: Using DAT-only mode (Client IDs as Server IDs)");
    // item_definitions will be generated later during merge
  } else if (data_source == Domain::ItemDataSource::SRV) {
    // Load SRV format
    std::filesystem::path srv_path = otb_path;

    IO::SrvResult srv_result = IO::SrvReader::read(srv_path);
    if (!srv_result.success) {
      result.error = "Failed to load SRV: " + srv_result.error;
      return result;
    }

    item_definitions = std::move(srv_result.items);

    // SRV format is from 7.x era
    result.otb_version.major_version = 0;
    result.otb_version.minor_version = 0;
    result.otb_version.build_number = 0;
    result.otb_version.valid = false; // No version info in SRV

    spdlog::info("SRV loaded: {} items (ancient 7.x format)",
                 item_definitions.size());
  } else {
    // Load OTB format (default)
    IO::OtbResult otb_result = IO::OtbReader::read(otb_path);
    if (!otb_result.success) {
      result.error = "Failed to load OTB: " + otb_result.error;
      return result;
    }

    item_definitions = std::move(otb_result.items);
    result.otb_version = otb_result.version;

    spdlog::info("OTB loaded: {} items, version {}.{}.{}",
                 item_definitions.size(), otb_result.version.major_version,
                 otb_result.version.minor_version,
                 otb_result.version.build_number);
  }

  if (progress)
    progress(20, "Loading DAT...");

  // 2. Load DAT (client item appearances)
  std::filesystem::path dat_path = client_path / "Tibia.dat";
  auto dat_reader = IO::DatReaderFactory::create(client_version);

  if (!dat_reader) {
    result.error =
        "Unsupported client version: " + std::to_string(client_version);
    return result;
  }

  // Try alternate casing if first fails
  if (!std::filesystem::exists(dat_path)) {
    dat_path = client_path / "tibia.dat";
  }

  // DatReaderBase::read takes the path directly, there is no separate open()
  IO::DatResult dat_result = dat_reader->read(dat_path);
  if (!dat_result.success) {
    result.error = "Failed to read DAT file: " + dat_result.error;
    return result;
  }

  result.dat_signature = dat_result.signature;

  // Fill result stats from DAT (as requested)
  result.item_count = dat_result.items.size();
  result.outfit_count = dat_result.outfits.size();
  result.effect_count = dat_result.effects.size();
  result.missile_count = dat_result.missiles.size();

  spdlog::info("DAT loaded: {} items, {} outfits, {} effects, {} missiles",
               dat_result.items.size(), dat_result.outfits.size(),
               dat_result.effects.size(), dat_result.missiles.size());

  if (progress)
    progress(60, "Merging data...");

  // 3. Merge data
  if (data_source == Domain::ItemDataSource::DAT) {
      // Generate item definitions directly from DAT
      item_definitions.reserve(dat_result.items.size());
      for (const auto& dat_item : dat_result.items) {
          Domain::ItemType it;
          it.server_id = dat_item.id;
          it.client_id = dat_item.id;
          it.name = std::format("Item {}", dat_item.id);
          item_definitions.push_back(std::move(it));
      }
  }

  mergeOtbWithDat(item_definitions, dat_result, client_version);

  // 4. Store outfit data for creature sprite lookup
  outfits_ = dat_result.outfits;
  for (size_t i = 0; i < outfits_.size(); ++i) {
    outfit_index_[outfits_[i].id] = i;
  }
  spdlog::info("Stored {} outfits for creature rendering", outfits_.size());

  if (progress)
    progress(80, "Initializing Sprites...");

  // 4. Initialize Sprite Reader (lazy)
  std::filesystem::path spr_path = client_path / "Tibia.spr";
  if (!std::filesystem::exists(spr_path)) {
    spr_path = client_path / "tibia.spr";
  }

  // Initialize Sprite Reader (if not already present)
  if (!spr_reader_) {
    spr_reader_ = std::make_shared<IO::SprReader>();
  }

  // Need to handle result object, no implicit bool conversion
  // spr_reader_ instance is preserved, only calling open() to reset internal
  // state and load new file
  bool extended = client_version >= 960;
  auto spr_result = spr_reader_->open(spr_path, 0, extended);

  if (spr_result.success) {
    result.spr_signature = spr_reader_->getSignature();
    result.sprite_count = spr_reader_->getSpriteCount();
    spdlog::info("SPR loaded: {} sprites", result.sprite_count);
  } else {
    spdlog::warn("Failed to open SPR file: {}", spr_path.string());
    result.error = "Failed to open SPR file: " + spr_result.error;
    return result;
  }

  // Final success update
  result.success = true;

  loaded_ = true;
  client_version_ = client_version;

  if (progress)
    progress(100, "Done");

  // ...

  // Final success update
  result.success = true;

  loaded_ = true;
  client_version_ = client_version;

  if (progress)
    progress(100, "Done");

  return result;
}

bool ClientDataService::loadCreatureData(
    const std::filesystem::path &creatures_xml_path) {
  auto result = IO::CreatureXmlReader::read(creatures_xml_path);
  if (!result.success) {
    spdlog::error("Failed to load creatures.xml: {}", result.error);
    return false;
  }

  spdlog::info("Loaded {} creatures from XML", result.creatures.size());

  // Merge into storage
  for (auto &creature : result.creatures) {
    std::string lower_name = creature->name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   ::tolower);

    creature_map_[lower_name] = creature.get();
    creatures_.push_back(std::move(creature));
  }

  return true;
}

bool ClientDataService::loadItemData(
    const std::filesystem::path &items_xml_path) {
  if (!loaded_) {
    spdlog::warn("Cannot load items.xml before OTB/DAT data");
    return false;
  }

  auto result =
      IO::ItemXmlReader::load(items_xml_path, items_, server_id_index_);
  if (!result.success) {
    spdlog::warn("Failed to load items.xml: {}", result.error);
    return false;
  }

  spdlog::info("Loaded {} items from XML, merged {} with existing types",
               result.items_loaded, result.items_merged);
  return true;
}

// NOTE: loadTilesetData and generateCreatureTileset have been moved to
// TilesetService This keeps ClientDataService focused on client data files
// (OTB, DAT, SPR, items.xml, creatures.xml)

const Domain::CreatureType *
ClientDataService::getCreatureType(const std::string &name) const {
  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                 ::tolower);

  auto it = creature_map_.find(lower_name);
  if (it != creature_map_.end()) {
    return it->second;
  }
  return nullptr;
}

size_t ClientDataService::optimizeItemSprites(SpriteManager& sprite_manager, bool preload_sprites) {
  size_t cached_count = 0;
  size_t simple_items = 0;

  spdlog::info("Caching sprite regions for {} item types...", items_.size());

  for (auto &item_type : items_) {
    // Only cache simple items (1x1, single layer, no animation)
    // These are ~99% of items and benefit most from caching
    if (item_type.width == 1 && item_type.height == 1 &&
        item_type.layers == 1 && item_type.frames == 1 &&
        !item_type.sprite_ids.empty()) {

      simple_items++;
      uint32_t sprite_id = item_type.sprite_ids[0];

      if (sprite_id == 0)
        continue;

      const Rendering::AtlasRegion *region = nullptr;

      // Preload sprite to atlas if needed
      if (preload_sprites) {
        region = sprite_manager.preloadSprite(sprite_id);
      } else {
        // Just try to get it if it exists
        region = sprite_manager.getSpriteRegion(sprite_id);
      }

      // Cache the region pointer
      if (region) {
        item_type.cached_sprite_region = region;
        cached_count++;
      }
    }
  }

  spdlog::info(
      "Sprite caching complete: {} of {} simple items cached ({:.1f}%)",
      cached_count, simple_items,
      simple_items > 0 ? (100.0 * cached_count / simple_items) : 0.0);

  return cached_count;
}

void ClientDataService::clear() {
  items_.clear();
  server_id_index_.clear();
  client_id_index_.clear();
  max_server_id_ = 0;
  max_client_id_ = 0;

  creatures_.clear();
  creature_map_.clear();

  outfits_.clear();
  outfit_index_.clear();

  // Note: We don't need to reset spr_reader_ here since it's a shared_ptr
  // and will be cleaned up automatically when all references are gone.
  // However, we can reset it if we want to force release.
  // Given the previous fragility, letting it persist or be replaced in load()
  // is safer.

  loaded_ = false;
  client_version_ = 0;
}

void ClientDataService::mergeOtbWithDat(
    // ...
    const std::vector<Domain::ItemType> &otb_items,
    const IO::DatResult &dat_result, uint32_t client_version) {

  // Build a map of client_id -> DAT item for quick lookup
  std::unordered_map<uint16_t, const IO::ClientItem *> dat_items;
  size_t dat_ground_count = 0;
  for (const auto &item : dat_result.items) {
    dat_items[item.id] = &item;
    if (item.is_ground)
      dat_ground_count++;
  }

  spdlog::info("ClientDataService: Merging {} OTB items with {} DAT items",
               otb_items.size(), dat_result.items.size());

  items_.reserve(otb_items.size());

  for (const auto &otb_item : otb_items) {
    Domain::ItemType merged = otb_item;

    // Find matching DAT entry by client_id
    auto it = dat_items.find(otb_item.client_id);
    if (it != dat_items.end()) {
      const IO::ClientItem *dat = it->second;

      // Merge DAT appearance data
      merged.width = dat->width;
      merged.height = dat->height;
      merged.layers = dat->layers;
      merged.pattern_x = dat->pattern_x;
      merged.pattern_y = dat->pattern_y;
      merged.pattern_z = dat->pattern_z;
      merged.frames = dat->frames;

      // Copy sprite IDs
      merged.sprite_ids = dat->sprite_ids;

      // Copy ground flag from DAT
      merged.is_ground = dat->is_ground;

      // Merge light info
      if (dat->has_light) {
        merged.light_level = static_cast<uint8_t>(dat->light_level);
        merged.light_color = static_cast<uint8_t>(dat->light_color);
      }

      // Translucency - ONLY for client 10.00+
      // Older clients ignore this flag for visibility calculations
      if (client_version >= 1000) {
        merged.is_translucent = dat->is_translucent;
      } else {
        merged.is_translucent = false;
      }

      // Ground speed from DAT
      if (dat->is_ground && dat->ground_speed > 0) {
        merged.speed = dat->ground_speed;
      }

      // Draw offset (critical for proper sprite positioning)
      merged.draw_offset_x = dat->offset_x;
      merged.draw_offset_y = dat->offset_y;

      // Elevation (items on top shift upward visually)
      merged.elevation = dat->elevation;

      // CRITICAL FIX: Ensure OTB flags reflect DAT elevation
      // If the OTB file didn't specify HasElevation but DAT has elevation,
      // force the flag so rendering logic works.
      if (merged.elevation > 0) {
        merged.flags = merged.flags | Domain::ItemFlag::HasElevation;
      }

      // Hangable/hook properties
      merged.is_hangable = dat->is_hangable;
      merged.hook_east =
          dat->is_horizontal; // DAT uses is_horizontal for east hook
      merged.hook_south =
          dat->is_vertical; // DAT uses is_vertical for south hook

      // Minimap color (for minimap rendering)
      if (dat->has_minimap_color) {
        merged.minimap_color = dat->minimap_color;
      }

      // Floor visibility flags (critical for floor rendering in ingame preview)
      merged.is_on_bottom = dat->is_on_bottom;
      merged.is_on_top = dat->is_on_top;
      merged.is_dont_hide = dat->dont_hide;
      merged.blocks_projectile = dat->blocks_missiles;

      // Fluid container flag from DAT (for proper subtype-based sprite
      // rendering)
      merged.is_fluid_container = dat->is_fluid_container;
    }

    // Store the item
    size_t index = items_.size();
    items_.push_back(std::move(merged));

    // Index by server_id and client_id
    if (otb_item.server_id > 0) {
      server_id_index_[otb_item.server_id] = index;
      max_server_id_ = std::max(max_server_id_, otb_item.server_id);
    }
    if (otb_item.client_id > 0) {
      client_id_index_[otb_item.client_id] = index;
      max_client_id_ = std::max(max_client_id_, otb_item.client_id);
    }
  }

  size_t light_count = 0;
  for (const auto &item : items_) {
    if (item.light_level > 0)
      light_count++;
  }
  spdlog::info("Light System: {} items have light_level > 0", light_count);
}

const Domain::ItemType *
ClientDataService::getItemTypeByServerId(uint16_t server_id) const {
  auto it = server_id_index_.find(server_id);
  if (it != server_id_index_.end()) {
    return &items_[it->second];
  }
  return nullptr;
}

const Domain::ItemType *
ClientDataService::getItemTypeByClientId(uint16_t client_id) const {
  auto it = client_id_index_.find(client_id);
  if (it != client_id_index_.end()) {
    return &items_[it->second];
  }
  return nullptr;
}

const IO::ClientItem *
ClientDataService::getOutfitData(uint16_t lookType) const {
  auto it = outfit_index_.find(lookType);
  if (it != outfit_index_.end()) {
    return &outfits_[it->second];
  }
  return nullptr;
}

std::vector<uint32_t>
ClientDataService::getOutfitSpriteIds(uint16_t lookType) const {
  auto it = outfit_index_.find(lookType);
  if (it != outfit_index_.end()) {
    return outfits_[it->second].sprite_ids;
  }
  return {};
}

} // namespace Services
} // namespace MapEditor
