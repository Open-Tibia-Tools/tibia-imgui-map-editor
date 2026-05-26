#include "Tile.h"
#include "ChunkedMap.h" // Needed for Chunk definition
#include "ItemType.h"
#include "IO/Otbm/OtbmOpaqueData.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Domain {

Tile::Tile(const Position &pos) : position_(pos) {}

Tile::~Tile() = default;

void Tile::setOpaqueData(std::unique_ptr<IO::InvalidZoneState> data) {
    opaqueData_ = std::move(data);
}

void Tile::setGround(std::unique_ptr<Item> item) {
  ground_ = std::move(item);
  markDirty();
}

std::unique_ptr<Item> Tile::removeGround() {
  markDirty();
  return std::move(ground_);
}

const Item *Tile::getItem(size_t index) const {
  if (index < items_.size()) {
    return items_[index].get();
  }
  return nullptr;
}

Item *Tile::getItem(size_t index) {
  if (index < items_.size()) {
    return items_[index].get();
  }
  return nullptr;
}

void Tile::addItem(std::unique_ptr<Item> item) {
  if (!item)
    return;

  // Check if this is a ground item (first item or has ground flag)
  const ItemType *type = item->getType();
  bool is_ground = false;
  uint16_t sid = item->getServerId();

  if (type) {
    // Use OTB's ItemGroup::Ground for classification (like original RME)
    // DAT's is_ground is a visual property, OTB's group is the server
    // classification
    is_ground = (type->group == ItemGroup::Ground);
  }

  if (is_ground) {
    ground_ = std::move(item);
  } else {
    // RME-style sorted insertion:
    // - Items with always_on_bottom are sorted by top_order
    // - Items without always_on_bottom are appended at the end
    // - Items WITHOUT type info are placed at the beginning (bottom)
    //   since they are likely corrupted bottom items like borders
    if (type && type->always_on_bottom) {
      // Find insertion point: iterate through existing always_on_bottom items
      // Insert BEFORE the first item with higher top_order or without
      // always_on_bottom
      auto it = items_.begin();
      while (it != items_.end()) {
        const ItemType *it_type = (*it)->getType();
        if (!it_type) {
          // No type info - stop here (items without type are at start)
          break;
        }
        if (!it_type->always_on_bottom) {
          // Reached non-bottom items - insert here
          break;
        }
        if (type->top_order < it_type->top_order) {
          // Current item has lower priority - insert before
          break;
        }
        ++it;
      }
      items_.insert(it, std::move(item));
    } else if (!type) {
      // NO TYPE INFO: Insert at beginning (bottom of visual stack)
      // Invalid items should be at the bottom, not on top
      items_.insert(items_.begin(), std::move(item));
    } else {
      // Non-bottom items are appended at end (maintain insertion order)
      items_.push_back(std::move(item));
    }
  }
  markDirty();
}

std::unique_ptr<Item> Tile::removeItem(size_t index) {
  if (index >= items_.size()) {
    return nullptr;
  }
  auto item = std::move(items_[index]);
  items_.erase(items_.begin() + static_cast<ptrdiff_t>(index));
  markDirty();
  return item;
}

void Tile::clearItems() {
  items_.clear();
  markDirty();
}

void Tile::swapItems(size_t index1, size_t index2) {
  if (index1 < items_.size() && index2 < items_.size() && index1 != index2) {
    std::swap(items_[index1], items_[index2]);
    markDirty();
  }
}

void Tile::addItemDirect(std::unique_ptr<Item> item) {
  if (!item)
    return;
  // Direct append without sorting - used for undo/redo restoration
  // to preserve exact item order from snapshot
  items_.push_back(std::move(item));
  markDirty();
}

void Tile::removeFlag(TileFlag flag) {
  flags_ = static_cast<TileFlag>(static_cast<uint16_t>(flags_) &
                                 ~static_cast<uint16_t>(flag));
}

void Tile::markDirty() {
  if (parent_chunk_) {
    // Dirty metadata is tracked on the chunk, which optimizes rendering
    // and partial updates.
    parent_chunk_->setDirty(true);
  }
}

std::unique_ptr<Tile> Tile::clone() const {
  auto tile = std::make_unique<Tile>(position_);

  if (ground_) {
    tile->ground_ = ground_->clone();
  }

  for (const auto &item : items_) {
    tile->items_.push_back(item->clone());
  }

  tile->flags_ = flags_;
  tile->house_id_ = house_id_;

  if (spawn_) {
    // Spawn is a simple struct, copy it
    tile->spawn_ = std::make_unique<Spawn>(*spawn_);
  }

  if (creature_) {
    tile->creature_ = std::make_unique<Creature>(*creature_);
  }

  if (opaqueData_) {
    tile->opaqueData_ = std::make_unique<IO::InvalidZoneState>(*opaqueData_);
  }

  return tile;
}

bool Tile::hasHookSouth() const {
  // Check all items (including ground) for hook_south property
  if (ground_) {
    const ItemType *type = ground_->getType();
    if (type && type->hook_south)
      return true;
  }
  for (const auto &item : items_) {
    const ItemType *type = item->getType();
    if (type && type->hook_south)
      return true;
  }
  return false;
}

bool Tile::hasHookEast() const {
  // Check all items (including ground) for hook_east property
  if (ground_) {
    const ItemType *type = ground_->getType();
    if (type && type->hook_east)
      return true;
  }
  for (const auto &item : items_) {
    const ItemType *type = item->getType();
    if (type && type->hook_east)
      return true;
  }
  return false;
}

void Tile::setSpawn(std::unique_ptr<Spawn> spawn) {
  bool had_spawn = (spawn_ != nullptr);
  spawn_ = std::move(spawn);
  bool has_spawn = (spawn_ != nullptr);

  if (parent_chunk_) {
    parent_chunk_->invalidateSpawns();
    // Only update count if existence changed
    if (had_spawn && !has_spawn) {
      parent_chunk_->updateSpawnCount(-1);
    } else if (!had_spawn && has_spawn) {
      parent_chunk_->updateSpawnCount(1);
    }
  }
}

std::unique_ptr<Spawn> Tile::removeSpawn() {
  if (spawn_ && parent_chunk_) {
    parent_chunk_->invalidateSpawns();
    parent_chunk_->updateSpawnCount(-1);
  }
  return std::move(spawn_);
}

void Tile::setCreature(std::unique_ptr<Creature> creature) {
  bool had_creature = (creature_ != nullptr);
  creature_ = std::move(creature);
  bool has_creature = (creature_ != nullptr);

  if (parent_chunk_) {
    // Only update count if existence changed
    if (had_creature && !has_creature) {
      parent_chunk_->updateCreatureCount(-1);
    } else if (!had_creature && has_creature) {
      parent_chunk_->updateCreatureCount(1);
    }
  }
}

} // namespace Domain
} // namespace MapEditor
