#include "SelectionService.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Creature.h"
#include "Domain/Item.h"
#include "Domain/Spawn.h"
#include "Domain/Tile.h"
#include <algorithm>

namespace MapEditor::Services::Selection {

using namespace Domain::Selection;

// === Helper: Sync domain object selection state ===

static void syncSelectionState(const SelectionEntry &entry, bool selected) {
  if (entry.getType() == EntityType::Creature) {
    const Domain::Creature *creature =
        static_cast<const Domain::Creature *>(entry.entity_ptr);
    if (creature) {
      if (selected) {
        creature->select();
      } else {
        creature->deselect();
      }
    }
  } else if (entry.getType() == EntityType::Spawn) {
    const Domain::Spawn *spawn =
        static_cast<const Domain::Spawn *>(entry.entity_ptr);
    if (spawn) {
      if (selected) {
        spawn->select();
      } else {
        spawn->deselect();
      }
    }
  }
}

// === Selection Operations ===

void SelectionService::selectAt(Domain::ChunkedMap *map,
                                const Domain::Position &pos,
                                const SelectionFilter &filter,
                                bool clear_first) {
  if (!map) {
    return;
  }

  if (clear_first) {
    clear();
  }

  Domain::Tile  const*tile = map->getTile(pos);
  if (!tile) {
    return;
  }

  addTileEntities(tile, filter);
}

void SelectionService::selectRegion(Domain::ChunkedMap *map, int32_t min_x,
                                    int32_t min_y, int32_t max_x, int32_t max_y,
                                    int16_t z, const SelectionFilter &filter) {
  if (!map) {
    return;
  }

  std::vector<SelectionEntry> added;

  for (int32_t y = min_y; y <= max_y; ++y) {
    for (int32_t x = min_x; x <= max_x; ++x) {
      Domain::Position const pos{x, y, z};
      Domain::Tile *tile = map->getTile(pos);
      if (!tile) {
        continue;
      }

      // Collect entries to add
      if (filter.include_ground) {
        if (const Domain::Item *ground = tile->getGround()) {
          SelectionEntry const entry = createGroundEntry(pos, ground);
          if (!bucket_.contains(entry.id)) {
            bucket_.add(entry);
            added.push_back(entry);
          }
        }
      }

      if (filter.include_items) {
        for (const auto &item : tile->getItems()) {
          SelectionEntry const entry = createItemEntry(pos, item.get());
          if (!bucket_.contains(entry.id)) {
            bucket_.add(entry);
            added.push_back(entry);
          }
        }
      }

      if (filter.include_creatures) {
        if (const Domain::Creature *creature = tile->getCreature()) {
          SelectionEntry const entry = createCreatureEntry(pos, creature);
          if (!bucket_.contains(entry.id)) {
            bucket_.add(entry);
            added.push_back(entry);
          }
        }
      }

      if (filter.include_spawns) {
        if (const Domain::Spawn *spawn = tile->getSpawn()) {
          SelectionEntry const entry = createSpawnEntry(pos, spawn);
          if (!bucket_.contains(entry.id)) {
            bucket_.add(entry);
            added.push_back(entry);
          }
        }
      }
    }
  }

  if (!added.empty()) {
    notifyChanged(added, {});
  }
}

void SelectionService::selectTile(Domain::ChunkedMap *map,
                                  const Domain::Position &pos) {
  selectAt(map, pos, SelectionFilter::all(), false);
}

void SelectionService::deselectAt(const Domain::Position &pos,
                                  const SelectionFilter &filter) {
  auto entries = bucket_.getEntriesAt(pos);
  std::vector<SelectionEntry> removed;

  for (const auto &entry : entries) {
    if (filter.matches(entry.id)) {
      bucket_.remove(entry.id);
      removed.push_back(entry);
    }
  }

  if (!removed.empty()) {
    notifyChanged({}, removed);
  }
}

void SelectionService::toggleAt(Domain::ChunkedMap *map,
                                const Domain::Position &pos,
                                const SelectionFilter &filter) {
  if (!map) {
    return;
  }

  // If filter specifies a single entity, toggle just that entity
  if (filter.specific_entity.has_value()) {
    const EntityId &id = filter.specific_entity.value();
    if (bucket_.contains(id)) {
      // Entity is selected - remove it
      auto entries = bucket_.getEntriesAt(pos);
      for (const auto &e : entries) {
        if (e.id == id) {
          bucket_.remove(id);
          notifyChanged({}, {e});
          return;
        }
      }
    } else {
      // Entity is not selected - need to add it
      // We need to find the entity on the tile
      Domain::Tile *tile = map->getTile(pos);
      if (!tile) {
        return;
      }

      SelectionEntry entry;
      bool found = false;

      switch (id.type) {
      case EntityType::Ground:
        if (const Domain::Item *ground = tile->getGround()) {
          entry = createGroundEntry(pos, ground);
          if (entry.id == id) {
            found = true;
          }
        }
        break;

      case EntityType::Item:
        for (const auto &item : tile->getItems()) {
          entry = createItemEntry(pos, item.get());
          if (entry.id == id) {
            found = true;
            break;
          }
        }
        break;

      case EntityType::Creature:
        if (const Domain::Creature *creature = tile->getCreature()) {
          entry = createCreatureEntry(pos, creature);
          if (entry.id == id) {
            found = true;
          }
        }
        break;

      case EntityType::Spawn:
        if (const Domain::Spawn *spawn = tile->getSpawn()) {
          entry = createSpawnEntry(pos, spawn);
          if (entry.id == id) {
            found = true;
          }
        }
        break;
      }

      if (found) {
        bucket_.add(entry);
        notifyChanged({entry}, {});
      }
    }
    return;
  }

  // Non-specific filter: toggle based on position presence
  if (bucket_.hasEntriesAt(pos)) {
    // Has selection at position - remove matching entries
    deselectAt(pos, filter);
  } else {
    // No selection at position - add matching entries
    selectAt(map, pos, filter, false);
  }
}

void SelectionService::clear() {
  if (bucket_.empty()) {
    return;
  }

  // Deselect all entities before clearing
  for (const auto &entry : bucket_.getAllEntries()) {
    syncSelectionState(entry, false);
  }

  bucket_.clear();
  notifyCleared();
}

// === Direct Entity Operations ===

void SelectionService::addEntity(const SelectionEntry &entry) {
  if (bucket_.contains(entry.id)) {
    return; // Already selected
  }

  bucket_.add(entry);
  syncSelectionState(entry, true); // Sync domain object state
  notifyChanged({entry}, {});
}

void SelectionService::removeEntity(const EntityId &id) {
  if (!bucket_.contains(id)) {
    return; // Not selected
  }

  // Find the entry before removing (for notification)
  auto entries = bucket_.getEntriesAt(id.position);
  SelectionEntry removed_entry;
  for (const auto &e : entries) {
    if (e.id == id) {
      removed_entry = e;
      break;
    }
  }

  bucket_.remove(id);
  syncSelectionState(removed_entry, false); // Sync domain object state
  notifyChanged({}, {removed_entry});
}

void SelectionService::toggleEntity(Domain::ChunkedMap *map,
                                    const SelectionEntry &entry) {
  if (bucket_.contains(entry.id)) {
    removeEntity(entry.id);
  } else {
    addEntity(entry);
  }
}

// === Query ===

bool SelectionService::isSelected(const EntityId &id) const {
  return bucket_.contains(id);
}

bool SelectionService::hasSelectionAt(const Domain::Position &pos) const {
  return bucket_.hasEntriesAt(pos);
}

std::vector<SelectionEntry>
SelectionService::getEntriesAt(const Domain::Position &pos) const {
  return bucket_.getEntriesAt(pos);
}

std::vector<SelectionEntry> SelectionService::getAllEntries() const {
  return bucket_.getAllEntries();
}

std::vector<Domain::Position> SelectionService::getPositions() const {
  return bucket_.getPositions();
}

std::vector<SelectionEntry>
SelectionService::getEntriesOnFloor(int16_t floor) const {
  return bucket_.getEntriesOnFloor(floor);
}

// === Snapshot ===

SelectionSnapshot SelectionService::createSnapshot() const {
  return SelectionSnapshot::capture(bucket_);
}

void SelectionService::restoreSnapshot(const SelectionSnapshot &snapshot) {
  bucket_ = snapshot.restore();
  notifyCleared(); // Full refresh - simpler than computing delta
}

// === Observer Pattern ===

void SelectionService::addObserver(ISelectionObserver *observer) {
  if (observer == nullptr) {
    return;
  }

  auto it = std::find(observers_.begin(), observers_.end(), observer);
  if (it == observers_.end()) {
    observers_.push_back(observer);
  }
}

void SelectionService::removeObserver(ISelectionObserver *observer) {
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  if (it != observers_.end()) {
    observers_.erase(it);
  }
}

// === Bulk Operations ===

void SelectionService::addTileEntities(const Domain::Tile *tile,
                                       const SelectionFilter &filter) {
  if (!tile) {
    return;
  }

  const Domain::Position &pos = tile->getPosition();
  std::vector<SelectionEntry> added;

  // Handle specific entity filter
  if (filter.specific_entity.has_value()) {
    const EntityId &target = filter.specific_entity.value();

    // Ground
    if (target.type == EntityType::Ground) {
      if (const Domain::Item *ground = tile->getGround()) {
        SelectionEntry const entry = createGroundEntry(pos, ground);
        if (entry.id == target && !bucket_.contains(entry.id)) {
          bucket_.add(entry);
          added.push_back(entry);
        }
      }
    }

    // Items
    if (target.type == EntityType::Item) {
      for (const auto &item : tile->getItems()) {
        SelectionEntry const entry = createItemEntry(pos, item.get());
        if (entry.id == target && !bucket_.contains(entry.id)) {
          bucket_.add(entry);
          added.push_back(entry);
          break;
        }
      }
    }

    // Creature
    if (target.type == EntityType::Creature) {
      if (const Domain::Creature *creature = tile->getCreature()) {
        SelectionEntry const entry = createCreatureEntry(pos, creature);
        if (entry.id == target && !bucket_.contains(entry.id)) {
          bucket_.add(entry);
          added.push_back(entry);
        }
      }
    }

    // Spawn
    if (target.type == EntityType::Spawn) {
      if (const Domain::Spawn *spawn = tile->getSpawn()) {
        SelectionEntry const entry = createSpawnEntry(pos, spawn);
        if (entry.id == target && !bucket_.contains(entry.id)) {
          bucket_.add(entry);
          added.push_back(entry);
        }
      }
    }

    if (!added.empty()) {
      notifyChanged(added, {});
    }
    return;
  }

  // Non-specific filter: add based on type flags
  if (filter.include_ground) {
    if (const Domain::Item *ground = tile->getGround()) {
      SelectionEntry const entry = createGroundEntry(pos, ground);
      if (!bucket_.contains(entry.id)) {
        bucket_.add(entry);
        added.push_back(entry);
      }
    }
  }

  if (filter.include_items) {
    for (const auto &item : tile->getItems()) {
      SelectionEntry const entry = createItemEntry(pos, item.get());
      if (!bucket_.contains(entry.id)) {
        bucket_.add(entry);
        added.push_back(entry);
      }
    }
  }

  if (filter.include_creatures) {
    if (const Domain::Creature *creature = tile->getCreature()) {
      SelectionEntry const entry = createCreatureEntry(pos, creature);
      if (!bucket_.contains(entry.id)) {
        bucket_.add(entry);
        added.push_back(entry);
      }
    }
  }

  if (filter.include_spawns) {
    if (const Domain::Spawn *spawn = tile->getSpawn()) {
      SelectionEntry const entry = createSpawnEntry(pos, spawn);
      if (!bucket_.contains(entry.id)) {
        bucket_.add(entry);
        added.push_back(entry);
      }
    }
  }

  if (!added.empty()) {
    notifyChanged(added, {});
  }
}

void SelectionService::removeAllAt(const Domain::Position &pos) {
  auto entries = bucket_.getEntriesAt(pos);
  if (entries.empty()) {
    return;
  }

  bucket_.removeAllAt(pos);
  notifyChanged({}, entries);
}

// === Private Helpers ===

SelectionEntry SelectionService::createGroundEntry(const Domain::Position &pos,
                                                   const Domain::Item *ground) {
  EntityId id;
  id.position = pos;
  id.type = EntityType::Ground;
  id.local_id = 0; // Only one ground per tile

  SelectionEntry entry;
  entry.id = id;
  entry.entity_ptr = ground;
  entry.item_id = ground ? ground->getServerId() : 0;

  return entry;
}

SelectionEntry SelectionService::createItemEntry(const Domain::Position &pos,
                                                 const Domain::Item *item) {
  EntityId id;
  id.position = pos;
  id.type = EntityType::Item;
  id.local_id = reinterpret_cast<uint64_t>(item); // Use pointer as unique ID

  SelectionEntry entry;
  entry.id = id;
  entry.entity_ptr = item;
  entry.item_id = item ? item->getServerId() : 0;

  return entry;
}

SelectionEntry
SelectionService::createCreatureEntry(const Domain::Position &pos,
                                      const Domain::Creature *creature) {
  EntityId id;
  id.position = pos;
  id.type = EntityType::Creature;
  id.local_id = reinterpret_cast<uint64_t>(creature);

  SelectionEntry entry;
  entry.id = id;
  entry.entity_ptr = creature;
  entry.item_id = 0; // Creatures don't have item IDs

  return entry;
}

SelectionEntry SelectionService::createSpawnEntry(const Domain::Position &pos,
                                                  const Domain::Spawn *spawn) {
  EntityId id;
  id.position = pos;
  id.type = EntityType::Spawn;
  id.local_id = reinterpret_cast<uint64_t>(spawn);

  SelectionEntry entry;
  entry.id = id;
  entry.entity_ptr = spawn;
  entry.item_id = 0; // Spawns don't have item IDs

  return entry;
}

void SelectionService::notifyChanged(
    const std::vector<SelectionEntry> &added,
    const std::vector<SelectionEntry> &removed) {
  for (auto *observer : observers_) {
    if (observer) {
      observer->onSelectionChanged(added, removed);
    }
  }
}

void SelectionService::notifyCleared() {
  for (auto *observer : observers_) {
    if (observer) {
      observer->onSelectionCleared();
    }
  }
}

} // namespace MapEditor::Services::Selection
