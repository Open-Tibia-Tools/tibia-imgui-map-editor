#include "MapEditingService.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include <spdlog/spdlog.h>

#include "Domain/ChunkedMap.h"
#include "Domain/Creature.h"
#include "Domain/History/HistoryManager.h"
#include "Domain/Item.h"
#include "Domain/Spawn.h"
#include "Domain/Tile.h"
#include "Services/Selection/SelectionService.h"

namespace MapEditor::Services {

using namespace Domain::Selection;

bool MapEditingService::moveItems(
    Domain::ChunkedMap *map,
    Services::Selection::SelectionService &selection_service,
    Domain::History::HistoryManager &history_manager, int32_t dx, int32_t dy) {

  if (!map || selection_service.isEmpty() || (dx == 0 && dy == 0)) {
    return false;
  }

  auto entries = selection_service.getAllEntries();

  // Start history operation (with selection state)
  history_manager.beginOperation("Move items",
                                 Domain::History::ActionType::Other,
                                 &selection_service);

  collectAffectedTiles(entries, dx, dy, map, history_manager);

  MoveContext ctx{};
  extractMovables(entries, dx, dy, map, ctx);
  insertMovables(map, ctx);

  bool const has_moves = !ctx.moved_info.empty() || !ctx.pending_creatures.empty() ||
                   !ctx.pending_spawns.empty();

  if (has_moves) {
    history_manager.endOperation(map, &selection_service);
    updateSelectionAfterMove(selection_service, ctx);
    return true;
  } else {
    history_manager.cancelOperation();
    return false;
  }
}

void MapEditingService::collectAffectedTiles(
    const std::vector<Domain::Selection::SelectionEntry> &entries, int32_t dx,
    int32_t dy, Domain::ChunkedMap *map,
    Domain::History::HistoryManager &history_manager) {
  // Collect all unique tile positions that will be affected
  std::unordered_set<uint64_t> affected_tiles;
  for (const auto &entry : entries) {
    Domain::Position const from_pos = entry.getPosition();
    Domain::Position to_pos = from_pos;
    to_pos.x += dx;
    to_pos.y += dy;
    affected_tiles.insert(from_pos.pack());
    affected_tiles.insert(to_pos.pack());
  }

  // Record BEFORE states for all affected tiles
  for (uint64_t const packed : affected_tiles) {
    Domain::Position const tile_pos = Domain::Position::unpack(packed);
    history_manager.recordTileBefore(tile_pos, map->getTile(tile_pos));
  }
}

void MapEditingService::extractMovables(
    const std::vector<Domain::Selection::SelectionEntry> &entries, int32_t dx,
    int32_t dy, Domain::ChunkedMap *map, MoveContext &ctx) {

  // Group by source tile for efficient extraction
  using ItemToMove = std::pair<const Domain::Item *, Domain::Position>;
  using TileItemsMap = std::unordered_map<uint64_t, std::vector<ItemToMove>>;
  TileItemsMap items_by_tile;
  items_by_tile.reserve(entries.size());

  std::vector<std::pair<Domain::Position, Domain::Position>>
      creature_moves; // from, to
  std::vector<std::pair<Domain::Position, Domain::Position>>
      spawn_moves; // from, to

  for (const auto &entry : entries) {
    Domain::Position const from_pos = entry.getPosition();
    Domain::Position to_pos = from_pos;
    to_pos.x += dx;
    to_pos.y += dy;

    switch (entry.getType()) {
    case EntityType::Ground:
    case EntityType::Item: {
      const Domain::Item *item_ptr =
          static_cast<const Domain::Item *>(entry.entity_ptr);
      if (item_ptr) {
        items_by_tile[from_pos.pack()].push_back({item_ptr, to_pos});
      }
      break;
    }
    case EntityType::Creature: {
      creature_moves.push_back({from_pos, to_pos});
      break;
    }
    case EntityType::Spawn: {
      spawn_moves.push_back({from_pos, to_pos});
      break;
    }
    }
  }

  // Extract all items from source tiles (PHASE 1)
  for (auto &[tile_key, item_list] : items_by_tile) {
    Domain::Position const from_pos = Domain::Position::unpack(tile_key);
    Domain::Tile *from_tile = map->getTile(from_pos);
    if (!from_tile)
      continue;

    // Separate ground items from regular items
    std::vector<std::pair<size_t, Domain::Position>>
        indexed_items; // index, destination

    for (const auto &[item_ptr, to_pos] : item_list) {
      // Check if it's ground
      if (from_tile->getGround() == item_ptr) {
        auto ground = from_tile->removeGround();
        if (ground) {
          ctx.pending_items.push_back(
              {from_pos, to_pos, std::move(ground), true});
        }
        continue;
      }

      // Find index in items vector using std::ranges
      auto &items = from_tile->getItems();
      // Using C++20 iterator based find_if
      auto it = std::find_if(items.begin(), items.end(),
                             [&](const std::unique_ptr<Domain::Item> &item) {
                               return item.get() == item_ptr;
                             });

      if (it != items.end()) {
        size_t const index = std::distance(items.begin(), it);
        indexed_items.push_back({index, to_pos});
      }
    }

    // Sort by index descending to remove from back first (preserves indices)
    std::sort(indexed_items.begin(), indexed_items.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });

    for (const auto &[idx, to_pos] : indexed_items) {
      auto moved = from_tile->removeItem(idx);
      if (moved) {
        ctx.pending_items.push_back(
            {from_pos, to_pos, std::move(moved), false});
      }
    }
  }

  // Extract all creatures
  for (const auto &[from_pos, to_pos] : creature_moves) {
    Domain::Tile *from_tile = map->getTile(from_pos);
    if (from_tile && from_tile->hasCreature()) {
      auto creature = from_tile->removeCreature();
      if (creature) {
        ctx.pending_creatures.push_back(
            {from_pos, to_pos, std::move(creature)});
      }
    }
  }

  // Extract all spawns
  for (const auto &[from_pos, to_pos] : spawn_moves) {
    Domain::Tile *from_tile = map->getTile(from_pos);
    if (from_tile && from_tile->hasSpawn()) {
      auto spawn = from_tile->removeSpawn();
      if (spawn) {
        map->notifySpawnChange(from_pos, false);
        ctx.pending_spawns.push_back({from_pos, to_pos, std::move(spawn)});
      }
    }
  }
}

void MapEditingService::insertMovables(Domain::ChunkedMap *map,
                                       MoveContext &ctx) {
  // PHASE 2: Insert all items into destination tiles
  // IMPORTANT: Save raw pointers and server IDs BEFORE moving, for selection
  // update

  for (auto &pending : ctx.pending_items) {
    Domain::Tile *to_tile = map->getOrCreateTile(pending.to);
    if (to_tile && pending.item) {
      // Save info BEFORE moving
      ctx.moved_info.push_back(
          {pending.to,
           pending.is_ground ? EntityType::Ground : EntityType::Item,
           pending.item.get(), pending.item->getServerId()});

      if (pending.is_ground) {
        to_tile->setGround(std::move(pending.item));
      } else {
        to_tile->addItem(std::move(pending.item));
      }
      spdlog::debug(
          "[MapEditingService] Moved item from ({},{},{}) to ({},{},{})",
          pending.from.x, pending.from.y, pending.from.z, pending.to.x,
          pending.to.y, pending.to.z);
    }
  }

  // Insert all creatures
  for (auto &pending : ctx.pending_creatures) {
    Domain::Tile *to_tile = map->getOrCreateTile(pending.to);
    if (to_tile && !to_tile->hasCreature()) {
      to_tile->setCreature(std::move(pending.creature));
      spdlog::debug(
          "[MapEditingService] Moved creature from ({},{},{}) to ({},{},{})",
          pending.from.x, pending.from.y, pending.from.z, pending.to.x,
          pending.to.y, pending.to.z);
    }
  }

  // Insert all spawns
  for (auto &pending : ctx.pending_spawns) {
    Domain::Tile *to_tile = map->getOrCreateTile(pending.to);
    if (to_tile && !to_tile->hasSpawn()) {
      pending.spawn->position = pending.to;
      to_tile->setSpawn(std::move(pending.spawn));
      map->notifySpawnChange(pending.to, true);
      spdlog::debug(
          "[MapEditingService] Moved spawn from ({},{},{}) to ({},{},{})",
          pending.from.x, pending.from.y, pending.from.z, pending.to.x,
          pending.to.y, pending.to.z);
    }
  }
}

void MapEditingService::updateSelectionAfterMove(
    Services::Selection::SelectionService &selection_service,
    const MoveContext &ctx) {
  // Update selection to new positions using saved info
  selection_service.clear();
  for (const auto &info : ctx.moved_info) {
    EntityId new_id;
    new_id.position = info.position;
    new_id.type = info.type;
    new_id.local_id = reinterpret_cast<uint64_t>(info.ptr);
    selection_service.addEntity(
        SelectionEntry{new_id, info.ptr, info.server_id});
  }
}

} // namespace MapEditor::Services
