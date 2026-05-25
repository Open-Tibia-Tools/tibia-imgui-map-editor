#include "ClientDataService.h"

#include <algorithm>
#include <filesystem>
#include <format>

#include <spdlog/spdlog.h>

#include "SpriteManager.h"

namespace MapEditor {
namespace Services {

ClientDataResult
ClientDataService::load(const std::filesystem::path &client_path,
                        const std::filesystem::path &item_metadata_path,
                        uint32_t client_version,
                        ::MapEditor::Domain::ItemDataSource data_source,
                        LoadProgressCallback progress) {
  ClientDataResult result;
  // Clear any existing data first
  clear();

  if (progress)
    progress(0, "Loading item database...");

  // 1. Load item definitions (OTB or SRV format)
  std::vector<Domain::ItemType> item_definitions;

  if (data_source == ::MapEditor::Domain::ItemDataSource::DAT) {
    spdlog::info("ClientDataService: Using DAT-only mode (Client IDs as Server IDs)");
  } else if (data_source == ::MapEditor::Domain::ItemDataSource::SRV) {
    IO::SrvResult srv_result = IO::SrvReader::read(item_metadata_path);
    if (!srv_result.success) {
      result.error = "Failed to load SRV: " + srv_result.error;
      return result;
    }
    item_definitions = std::move(srv_result.items);
    result.otb_version.major_version = 0;
    result.otb_version.minor_version = 0;
    result.otb_version.build_number = 0;
    result.otb_version.valid = false;
    spdlog::info("SRV loaded: {} items", item_definitions.size());
  } else {
    IO::OtbResult otb_result = IO::OtbReader::read(item_metadata_path);
    if (!otb_result.success) {
      result.error = "Failed to load OTB: " + otb_result.error;
      return result;
    }
    item_definitions = std::move(otb_result.items);
    result.otb_version = otb_result.version;
    spdlog::info("OTB loaded: {} items", item_definitions.size());
  }

  if (progress)
    progress(20, "Loading DAT...");

  std::filesystem::path dat_path = client_path / "Tibia.dat";
  if (!std::filesystem::exists(dat_path)) {
    dat_path = client_path / "tibia.dat";
  }

  auto dat_reader = IO::DatReaderFactory::create(client_version);
  if (!dat_reader) {
    result.error = "Unsupported client version: " + std::to_string(client_version);
    return result;
  }

  IO::DatResult dat_result = dat_reader->read(dat_path);
  if (!dat_result.success) {
    result.error = "Failed to read DAT file: " + dat_result.error;
    return result;
  }

  result.dat_signature = dat_result.signature;
  result.item_count = dat_result.items.size();
  result.outfit_count = dat_result.outfits.size();
  result.effect_count = dat_result.effects.size();
  result.missile_count = dat_result.missiles.size();

  if (progress)
    progress(50, "Loading SPR...");

  std::filesystem::path spr_path = client_path / "Tibia.spr";
  if (!std::filesystem::exists(spr_path)) {
    spr_path = client_path / "tibia.spr";
  }

  spr_reader_ = std::make_shared<IO::SprReader>();
  if (!spr_reader_->open(spr_path)) {
    result.error = "Failed to open Tibia.spr";
    return result;
  }

  result.spr_signature = spr_reader_->getSignature();
  result.sprite_count = spr_reader_->getSpriteCount();

  if (progress)
    progress(80, "Merging databases...");

  mergeOtbWithDat(item_definitions, dat_result, client_version);

  client_version_ = client_version;
  loaded_ = true;
  result.success = true;

  if (progress)
    progress(100, "Client data loaded.");

  return result;
}

bool ClientDataService::loadCreatureData(
    const std::filesystem::path &creatures_xml_path) {
  if (!std::filesystem::exists(creatures_xml_path)) {
    return false;
  }

  IO::CreatureXmlReader reader;
  auto new_creatures = reader.read(creatures_xml_path);
  if (new_creatures.empty()) {
    return false;
  }

  creatures_.clear();
  creature_map_.clear();
  for (auto &creature : new_creatures) {
    std::string name_lower = creature->name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    creature_map_[name_lower] = creature.get();
    creatures_.push_back(std::move(creature));
  }
  return true;
}

bool ClientDataService::loadItemData(const std::filesystem::path &items_xml_path) {
  if (!std::filesystem::exists(items_xml_path)) {
    return false;
  }
  IO::ItemXmlReader reader;
  auto xml_items = reader.read(items_xml_path);
  if (xml_items.empty()) {
    return false;
  }
  size_t updated_count = 0;
  for (const auto &xml_item : xml_items) {
    auto *it = const_cast<Domain::ItemType *>(getItemTypeByServerId(xml_item.id));
    if (it) {
      it->name = xml_item.name;
      updated_count++;
    }
  }
  return true;
}

const Domain::ItemType *
ClientDataService::getItemTypeByServerId(uint16_t server_id) const {
  auto it = server_id_index_.find(server_id);
  if (it != server_id_index_.end() && it->second < items_.size()) {
    return &items_[it->second];
  }
  return nullptr;
}

const Domain::ItemType *
ClientDataService::getItemTypeByClientId(uint16_t client_id) const {
  auto it = client_id_index_.find(client_id);
  if (it != client_id_index_.end() && it->second < items_.size()) {
    return &items_[it->second];
  }
  return nullptr;
}

const Domain::CreatureType *
ClientDataService::getCreatureType(const std::string &name) const {
  std::string name_lower = name;
  std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
  auto it = creature_map_.find(name_lower);
  if (it != creature_map_.end()) {
    return it->second;
  }
  return nullptr;
}

const IO::ClientItem *ClientDataService::getOutfitData(uint16_t lookType) const {
  auto it = outfit_index_.find(lookType);
  if (it != outfit_index_.end() && it->second < outfits_.size()) {
    return &outfits_[it->second];
  }
  return nullptr;
}

std::vector<uint32_t>
ClientDataService::getOutfitSpriteIds(uint16_t lookType) const {
  const auto *outfit = getOutfitData(lookType);
  if (outfit) {
    return outfit->sprite_ids;
  }
  return {};
}

size_t ClientDataService::optimizeItemSprites(SpriteManager &sprite_manager,
                                              bool preload_sprites) {
  size_t count = 0;
  for (auto &item : items_) {
    if (!item.sprite_ids.empty()) {
      if (preload_sprites) {
        sprite_manager.getSpriteRegion(item.sprite_ids[0]);
      }
      count++;
    }
  }
  return count;
}

void ClientDataService::clear() {
  items_.clear();
  server_id_index_.clear();
  client_id_index_.clear();
  creatures_.clear();
  creature_map_.clear();
  outfits_.clear();
  outfit_index_.clear();
  spr_reader_.reset();
  loaded_ = false;
  client_version_ = 0;
  max_server_id_ = 0;
  max_client_id_ = 0;
}

void ClientDataService::mergeOtbWithDat(
    const std::vector<Domain::ItemType> &metadata_items,
    const IO::DatResult &dat_result, uint32_t client_version) {

  outfits_ = dat_result.outfits;
  for (size_t i = 0; i < outfits_.size(); ++i) {
    outfit_index_[outfits_[i].id] = i;
  }

  size_t total_count = metadata_items.empty() ? dat_result.items.size() : metadata_items.size();
  items_.reserve(total_count);

  if (metadata_items.empty()) {
      for (const auto& dat_item : dat_result.items) {
          Domain::ItemType it;
          it.server_id = dat_item.id;
          it.client_id = dat_item.id;
          it.name = std::format("Item {}", dat_item.id);

          it.width = dat_item.width;
          it.height = dat_item.height;
          it.layers = dat_item.layers;
          it.pattern_x = dat_item.pattern_x;
          it.pattern_y = dat_item.pattern_y;
          it.pattern_z = dat_item.pattern_z;
          it.frames = dat_item.frames;
          it.sprite_ids = dat_item.sprite_ids;

          it.is_ground = dat_item.is_ground;
          it.is_moveable = !dat_item.is_unmoveable;
          it.is_blocking = dat_item.blocks_pathfinder;
          it.blocks_projectile = dat_item.blocks_missiles;
          it.is_pickupable = dat_item.is_pickupable;
          it.is_stackable = dat_item.is_stackable;
          it.is_fluid_container = dat_item.is_fluid_container;
          it.is_hangable = dat_item.is_hangable;
          it.minimap_color = dat_item.minimap_color;
          it.elevation = dat_item.elevation;
          it.light_level = static_cast<uint8_t>(dat_item.light_level);
          it.light_color = static_cast<uint8_t>(dat_item.light_color);
          it.is_translucent = dat_item.is_translucent;
          it.is_on_bottom = dat_item.is_on_bottom;
          it.is_on_top = dat_item.is_on_top;
          it.is_dont_hide = dat_item.dont_hide;

          size_t index = items_.size();
          items_.push_back(std::move(it));
          server_id_index_[items_[index].server_id] = index;
          client_id_index_[items_[index].client_id] = index;

          if (items_[index].server_id > max_server_id_) max_server_id_ = items_[index].server_id;
          if (items_[index].client_id > max_client_id_) max_client_id_ = items_[index].client_id;
      }
  } else {
      for (const auto &metadata_item : metadata_items) {
        Domain::ItemType it = metadata_item;
        auto dat_it = std::find_if(dat_result.items.begin(), dat_result.items.end(),
            [&](const IO::ClientItem &ci) { return ci.id == it.client_id; });

        if (dat_it != dat_result.items.end()) {
          const auto &dat_item = *dat_it;
          if (it.name.empty()) {
            it.name = std::format("Item {}", dat_item.id);
          }
          it.width = dat_item.width;
          it.height = dat_item.height;
          it.layers = dat_item.layers;
          it.pattern_x = dat_item.pattern_x;
          it.pattern_y = dat_item.pattern_y;
          it.pattern_z = dat_item.pattern_z;
          it.frames = dat_item.frames;
          it.sprite_ids = dat_item.sprite_ids;

          if (it.elevation == 0) it.elevation = dat_item.elevation;
          if (it.light_level == 0) it.light_level = static_cast<uint8_t>(dat_item.light_level);
          if (it.light_color == 0) it.light_color = static_cast<uint8_t>(dat_item.light_color);
          if (it.minimap_color == 0) it.minimap_color = dat_item.minimap_color;
        }

        size_t index = items_.size();
        items_.push_back(std::move(it));
        server_id_index_[items_[index].server_id] = index;
        if (items_[index].client_id > 0) {
          client_id_index_[items_[index].client_id] = index;
        }
        if (items_[index].server_id > max_server_id_) max_server_id_ = items_[index].server_id;
        if (items_[index].client_id > max_client_id_) max_client_id_ = items_[index].client_id;
      }
  }
}

} // namespace Services
} // namespace MapEditor
