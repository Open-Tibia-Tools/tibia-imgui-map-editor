#include "PixelPerfectSelectionStrategy.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Creature.h"
#include "IO/Readers/DatReaderBase.h"
#include "IO/SprReader.h"
#include "Services/ClientDataService.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace MapEditor::AppLogic {

using namespace Domain::Selection;

PixelPerfectSelectionStrategy::PixelPerfectSelectionStrategy(
    Services::ClientDataService *client_data)
    : client_data_(client_data) {}

// Helper to create EntityId
static EntityId makeEntityId(const Domain::Position &pos, EntityType type,
                             uint64_t local_id) {
  EntityId id;
  id.position = pos;
  id.type = type;
  id.local_id = local_id;
  return id;
}

SelectionEntry
PixelPerfectSelectionStrategy::selectAt(const Domain::ChunkedMap *map,
                                        const Domain::Position &pos,
                                        const glm::vec2 &pixel_offset) const {
  if (!map) {
    return SelectionEntry{makeEntityId(pos, EntityType::Ground, 0), nullptr, 0};
  }

  // Scan a region of tiles to handle items that "bleed" into the clicked tile.
  const int SEARCH_RANGE = 4;

  for (int dy = SEARCH_RANGE; dy >= 0; --dy) {
    for (int dx = SEARCH_RANGE; dx >= 0; --dx) {
      Domain::Position const candidate_pos = {pos.x + dx, pos.y + dy, pos.z};

      const Domain::Tile *tile = map->getTile(candidate_pos);
      if (!tile)
        continue;

      // Calculate pixel offset relative to this candidate tile.
      glm::vec2 const candidate_offset = pixel_offset - glm::vec2(dx * 32, dy * 32);

      // Check this tile for hits
      auto result = findHitOnTile(tile, candidate_pos, candidate_offset);

      // If we hit a creature, item, or spawn, return it.
      if (result.getType() == EntityType::Creature ||
          result.getType() == EntityType::Item ||
          result.getType() == EntityType::Spawn) {
        return result;
      }

      // Only accept Ground if it's the target tile (dx=0, dy=0)
      if (dx == 0 && dy == 0 && result.getType() == EntityType::Ground &&
          result.entity_ptr != nullptr) {
        return result;
      }
    }
  }

  return SelectionEntry{makeEntityId(pos, EntityType::Ground, 0), nullptr, 0};
}

SelectionEntry PixelPerfectSelectionStrategy::findHitOnTile(
    const Domain::Tile *tile, const Domain::Position &pos,
    const glm::vec2 &pixel_offset) const {
  if (!tile) {
    return SelectionEntry{makeEntityId(pos, EntityType::Ground, 0), nullptr, 0};
  }

  // Check creature first (higher priority than items, same as SmartSelectionStrategy)
  if (tile->hasCreature()) {
    const Domain::Creature *creature = tile->getCreature();
    if (creature && hitTestCreature(creature, pixel_offset, pos)) {
      return SelectionEntry{
          makeEntityId(pos, EntityType::Creature,
                       reinterpret_cast<uint64_t>(creature)),
          creature, 0};
    }
  }

  // Check items from top to bottom
  const auto &items = tile->getItems();

  // Store calculated offsets for each item
  struct ItemRenderInfo {
    Domain::Item *item;
    glm::vec2 offset;
  };
  std::vector<ItemRenderInfo> render_list;
  render_list.reserve(items.size());

  float accumulated_elevation = 0.0f;

  // Pass 1: Calculate rendering positions (Bottom-to-Top)
  for (const auto &item_ptr : items) {
    if (!item_ptr)
      continue;

    Domain::Item *item = item_ptr.get();
    const auto *item_type =
        client_data_->getItemTypeByServerId(item->getServerId());

    float draw_x = 0;
    float draw_y = 0;

    draw_x -= accumulated_elevation;
    draw_y -= accumulated_elevation;

    render_list.push_back({item, glm::vec2(draw_x, draw_y)});

    if (item_type && item_type->hasElevation()) {
      accumulated_elevation += static_cast<float>(item_type->elevation);
    }
  }

  // Pass 2: Hit Test (Top-to-Bottom)
  for (auto it = render_list.rbegin(); it != render_list.rend(); ++it) {
    glm::vec2 effective_offset = pixel_offset;
    effective_offset.x -= it->offset.x;
    effective_offset.y -= it->offset.y;

    if (hitTestItem(it->item, effective_offset, pos)) {
      return SelectionEntry{makeEntityId(pos, EntityType::Item,
                                         reinterpret_cast<uint64_t>(it->item)),
                            it->item, it->item->getServerId()};
    }
  }

  // Check ground
  if (tile->hasGround()) {
    auto *ground = tile->getGround();
    if (ground && hitTestItem(ground, pixel_offset, pos)) {
      return SelectionEntry{makeEntityId(pos, EntityType::Ground, 0), ground,
                            ground->getServerId()};
    }
  }

  return SelectionEntry{makeEntityId(pos, EntityType::Ground, 0), nullptr, 0};
}

bool PixelPerfectSelectionStrategy::hitTestItem(
    const Domain::Item *item, const glm::vec2 &pixel_offset,
    const Domain::Position &tile_pos) const {
  if (!item || !client_data_)
    return false;

  const auto *item_type =
      client_data_->getItemTypeByServerId(item->getServerId());
  if (!item_type) {
    // Fallback: simple 32x32 box check
    return pixel_offset.x >= 0 && pixel_offset.x < 32 && pixel_offset.y >= 0 &&
           pixel_offset.y < 32;
  }

  // Handle item specific draw offsets
  float draw_x = 0;
  float draw_y = 0;

  if (item_type->draw_offset_x != 0 || item_type->draw_offset_y != 0) {
    draw_x = static_cast<float>(item_type->draw_offset_x);
    draw_y = static_cast<float>(item_type->draw_offset_y);
  }

  float const local_x = pixel_offset.x - draw_x;
  float const local_y = pixel_offset.y - draw_y;

  int const cx = -static_cast<int>(std::floor(local_x / 32.0f));
  int const cy = -static_cast<int>(std::floor(local_y / 32.0f));

  int const width = std::max<int>(1, item_type->width);
  int const height = std::max<int>(1, item_type->height);

  if (cx < 0 || cx >= width || cy < 0 || cy >= height) {
    return false;
  }

  int const layers = std::max<int>(1, item_type->layers);
  int const pat_x = std::max<int>(1, item_type->pattern_x);
  int const pat_y = std::max<int>(1, item_type->pattern_y);
  int const pat_z = std::max<int>(1, item_type->pattern_z);
  int const frames = std::max<int>(1, item_type->frames);

  int pattern_x = (tile_pos.x % pat_x);
  int pattern_y = (tile_pos.y % pat_y);
  int pattern_z = (tile_pos.z % pat_z);
  int const frame = 0;

  int fluid_subtype = -1;
  bool const is_fluid = item_type->isFluidContainer() || item_type->isSplash();

  int subtype_index = -1;
  if (item_type->is_stackable) {
    int const count = item->getSubtype();
    if (count <= 1)
      subtype_index = 0;
    else if (count <= 2)
      subtype_index = 1;
    else if (count <= 3)
      subtype_index = 2;
    else if (count <= 4)
      subtype_index = 3;
    else if (count < 10)
      subtype_index = 4;
    else if (count < 25)
      subtype_index = 5;
    else if (count < 50)
      subtype_index = 6;
    else
      subtype_index = 7;
  } else if (is_fluid) {
    fluid_subtype = item->getSubtype();
  }

  if (item_type->is_hangable) {
    if (item_type->hook_south)
      pattern_x = 1;
    else if (item_type->hook_east)
      pattern_x = 2;
    else
      pattern_x = 0;
    pattern_y = 0;
    pattern_z = 0;
  } else if (is_fluid && fluid_subtype >= 0) {
    pattern_x = (fluid_subtype % 4) % pat_x;
    pattern_y = (fluid_subtype / 4) % pat_y;
    pattern_z = 0;
  }

  // FAST PATH: Simple 1x1 stackable items
  bool const can_use_subtype = (subtype_index >= 0 && width == 1 && height == 1);
  if (can_use_subtype &&
      subtype_index < static_cast<int>(item_type->sprite_ids.size())) {
    uint32_t const sprite_id = item_type->sprite_ids[subtype_index];
    if (sprite_id == 0)
      return false;

    auto spr_reader = client_data_->getSpriteReader();
    if (!spr_reader)
      return false;

    auto sprite = spr_reader->loadSprite(sprite_id);
    if (!sprite)
      return false;

    if (!sprite->is_decoded)
      sprite->decode();
    if (sprite->is_empty || sprite->rgba_data.empty())
      return false;

    int local_px = static_cast<int>(std::fmod(local_x, 32.0f));
    int local_py = static_cast<int>(std::fmod(local_y, 32.0f));
    if (local_px < 0)
      local_px += 32;
    if (local_py < 0)
      local_py += 32;

    if (local_px < 0 || local_px >= 32 || local_py < 0 || local_py >= 32)
      return false;

    size_t const pixel_index = (local_py * 32 + local_px) * 4;
    if (pixel_index + 3 >= sprite->rgba_data.size())
      return false;

    uint8_t const alpha = sprite->rgba_data[pixel_index + 3];
    return alpha > 32;
  }

  // SLOW PATH: Multi-tile or pattern-based items
  for (int layer = 0; layer < layers; ++layer) {
    size_t const sprite_index = static_cast<size_t>(
        ((((frame % frames) * pat_z + pattern_z) * pat_y + pattern_y) * pat_x +
         pattern_x) *
            layers +
        layer);

    size_t const final_index = (sprite_index * height + cy) * width + cx;

    if (final_index >= item_type->sprite_ids.size()) {
      continue;
    }

    uint32_t const sprite_id = item_type->sprite_ids[final_index];

    if (sprite_id == 0)
      continue;

    auto spr_reader = client_data_->getSpriteReader();
    if (!spr_reader)
      continue;

    auto sprite = spr_reader->loadSprite(sprite_id);
    if (!sprite) {
      continue;
    }

    if (!sprite->is_decoded)
      sprite->decode();
    if (sprite->is_empty || sprite->rgba_data.empty()) {
      continue;
    }

    int const px = static_cast<int>(local_x) + cx * 32;
    int const py = static_cast<int>(local_y) + cy * 32;

    size_t const pixel_idx = (py * 32 + px) * 4;
    if (pixel_idx + 3 >= sprite->rgba_data.size())
      continue;

    uint8_t const alpha = sprite->rgba_data[pixel_idx + 3];
    if (alpha > 10) {
      return true;
    }
  }

  return false;
}

bool PixelPerfectSelectionStrategy::hitTestCreature(
    const Domain::Creature *creature, const glm::vec2 &pixel_offset,
    const Domain::Position &tile_pos) const {
  if (!creature || !client_data_)
    return false;

  // Get creature type to access outfit
  const Domain::CreatureType *creature_type =
      client_data_->getCreatureType(creature->name);
  if (!creature_type || creature_type->outfit.lookType == 0) {
    // Fallback: simple 32x32 box check if no outfit data
    return pixel_offset.x >= 0 && pixel_offset.x < 32 && pixel_offset.y >= 0 &&
           pixel_offset.y < 32;
  }

  // Get outfit data
  const IO::ClientItem *outfit_data =
      client_data_->getOutfitData(creature_type->outfit.lookType);
  if (!outfit_data || outfit_data->sprite_ids.empty()) {
    // Fallback: simple 32x32 box check
    return pixel_offset.x >= 0 && pixel_offset.x < 32 && pixel_offset.y >= 0 &&
           pixel_offset.y < 32;
  }

  int const width = std::max<int>(1, outfit_data->width);
  int const height = std::max<int>(1, outfit_data->height);
  int const layers = std::max<int>(1, outfit_data->layers);
  int const pat_x = std::max<int>(1, outfit_data->pattern_x);
  int const frames = std::max<int>(1, outfit_data->frames);

  // Default direction = South (2)
  int const direction = static_cast<int>(creature->direction) % pat_x;

  // Account for creature displacement offset (creature is drawn at tile pos - offset)
  // CreatureRenderer.cpp uses: draw_x -= displacement_x, draw_y -= displacement_y
  // So we need to add the offset to pixel_offset to get local sprite coordinates
  float const offset_x = outfit_data->has_offset ? static_cast<float>(outfit_data->offset_x) : 0.0f;
  float const offset_y = outfit_data->has_offset ? static_cast<float>(outfit_data->offset_y) : 0.0f;
  glm::vec2 const adjusted_offset = pixel_offset + glm::vec2(offset_x, offset_y);

  // Simple case: 1x1 creature
  if (width == 1 && height == 1) {
    // Bounds check with adjusted offset
    if (adjusted_offset.x < 0 || adjusted_offset.x >= 32 || 
        adjusted_offset.y < 0 || adjusted_offset.y >= 32) {
      return false;
    }

    // Get sprite index for direction (layer 0, frame 0)
    size_t const sprite_index = static_cast<size_t>(direction * layers);
    if (sprite_index >= outfit_data->sprite_ids.size()) {
      return true; // Fallback to hit
    }

    uint32_t const sprite_id = outfit_data->sprite_ids[sprite_index];
    if (sprite_id == 0) {
      return false;
    }

    auto spr_reader = client_data_->getSpriteReader();
    if (!spr_reader) {
      return true; // Fallback
    }

    auto sprite = spr_reader->loadSprite(sprite_id);
    if (!sprite) {
      return true; // Fallback
    }

    if (!sprite->is_decoded)
      sprite->decode();
    if (sprite->is_empty || sprite->rgba_data.empty()) {
      return false;
    }

    int const local_px = static_cast<int>(adjusted_offset.x);
    int const local_py = static_cast<int>(adjusted_offset.y);

    if (local_px < 0 || local_px >= 32 || local_py < 0 || local_py >= 32) {
      return false;
    }

    size_t const pixel_index = (local_py * 32 + local_px) * 4;
    if (pixel_index + 3 >= sprite->rgba_data.size()) {
      return false;
    }

    uint8_t const alpha = sprite->rgba_data[pixel_index + 3];
    return alpha > 32;
  }

  // Multi-tile creature - check if within bounds and any part has alpha
  // Calculate which sub-tile was clicked
  int const cx = -static_cast<int>(std::floor(adjusted_offset.x / 32.0f));
  int const cy = -static_cast<int>(std::floor(adjusted_offset.y / 32.0f));

  if (cx < 0 || cx >= width || cy < 0 || cy >= height) {
    return false;
  }

  // Get sprite for this sub-tile
  size_t const sprite_index = static_cast<size_t>(
      (direction * layers) * height * width + cy * width + cx);
  if (sprite_index >= outfit_data->sprite_ids.size()) {
    return true; // Fallback
  }

  uint32_t const sprite_id = outfit_data->sprite_ids[sprite_index];
  if (sprite_id == 0) {
    return false;
  }

  auto spr_reader = client_data_->getSpriteReader();
  if (!spr_reader) {
    return true;
  }

  auto sprite = spr_reader->loadSprite(sprite_id);
  if (!sprite) {
    return true;
  }

  if (!sprite->is_decoded)
    sprite->decode();
  if (sprite->is_empty || sprite->rgba_data.empty()) {
    return false;
  }

  int local_px = static_cast<int>(std::fmod(adjusted_offset.x, 32.0f));
  int local_py = static_cast<int>(std::fmod(adjusted_offset.y, 32.0f));
  if (local_px < 0)
    local_px += 32;
  if (local_py < 0)
    local_py += 32;

  if (local_px < 0 || local_px >= 32 || local_py < 0 || local_py >= 32) {
    return false;
  }

  size_t const pixel_index = (local_py * 32 + local_px) * 4;
  if (pixel_index + 3 >= sprite->rgba_data.size()) {
    return false;
  }

  uint8_t const alpha = sprite->rgba_data[pixel_index + 3];
  return alpha > 32;
}

} // namespace MapEditor::AppLogic