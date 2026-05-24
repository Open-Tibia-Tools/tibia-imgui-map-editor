#include "ClipboardService.h"
#include "Application/EditorSession.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Creature.h"
#include "Domain/Item.h"
#include "Domain/Selection/SelectionEntry.h"
#include "Domain/Spawn.h"
#include "Services/Selection/SelectionService.h"
#include <algorithm>
#include <ranges>

namespace MapEditor::AppLogic {

ClipboardService::ClipboardService(Domain::CopyBuffer &buffer)
    : buffer_(buffer) {}

size_t ClipboardService::copy(const EditorSession &session) {
  auto &selectionService = session.getSelectionService();
  if (selectionService.isEmpty()) {
    return 0;
  }

  const auto *map = session.getMap();
  if (!map) {
    return 0;
  }

  // Find the origin (min position) for relative coordinates
  Domain::Position const origin = selectionService.getMinBound();

  std::vector<Domain::CopyBuffer::CopiedTile> copied_tiles;

  // Get all selection entries to determine WHAT to copy
  auto entries = selectionService.getAllEntries();

  // Group entries by position
  std::unordered_map<uint64_t, std::vector<Domain::Selection::SelectionEntry>>
      entries_by_pos;
  for (const auto &entry : entries) {
    const auto &pos = entry.getPosition();
    uint64_t const key = static_cast<uint64_t>(pos.x) |
                   (static_cast<uint64_t>(pos.y) << 21) |
                   (static_cast<uint64_t>(pos.z + 8) << 42);
    entries_by_pos[key].push_back(entry);
  }

  // If there are entity-level entries (Item, Ground, Creature, Spawn),
  // copy only those specific entities. Otherwise fall back to copying whole tiles.
  bool has_entity_entries = false;
  for (const auto &entry : entries) {
    auto type = entry.getType();
    if (type == Domain::Selection::EntityType::Item ||
        type == Domain::Selection::EntityType::Ground ||
        type == Domain::Selection::EntityType::Creature ||
        type == Domain::Selection::EntityType::Spawn) {
      has_entity_entries = true;
      break;
    }
  }

  if (has_entity_entries) {
    // Entity-level copy: only copy selected entities
    for (const auto &[key, pos_entries] : entries_by_pos) {
      // Get position from first entry
      const auto &pos = pos_entries[0].getPosition();

      Domain::Position const relative_pos{pos.x - origin.x, pos.y - origin.y,
                                    static_cast<int16_t>(pos.z - origin.z)};

      // Create a new tile containing only selected entities
      auto partial_tile = std::make_unique<Domain::Tile>(pos);

      for (const auto &entry : pos_entries) {
        auto type = entry.getType();
        
        if (type == Domain::Selection::EntityType::Item ||
            type == Domain::Selection::EntityType::Ground) {
          auto *item = static_cast<const Domain::Item *>(entry.entity_ptr);
          if (item) {
            // Clone the specific item (addItem auto-detects ground)
            auto item_copy = item->clone();
            partial_tile->addItem(std::move(item_copy));
          }
        } else if (type == Domain::Selection::EntityType::Creature) {
          auto *creature = static_cast<const Domain::Creature *>(entry.entity_ptr);
          if (creature) {
            auto creature_copy = std::make_unique<Domain::Creature>(*creature);
            creature_copy->deselect(); // Don't copy selection state
            partial_tile->setCreature(std::move(creature_copy));
          }
        } else if (type == Domain::Selection::EntityType::Spawn) {
          auto *spawn = static_cast<const Domain::Spawn *>(entry.entity_ptr);
          if (spawn) {
            auto spawn_copy = std::make_unique<Domain::Spawn>(*spawn);
            spawn_copy->deselect(); // Don't copy selection state
            partial_tile->setSpawn(std::move(spawn_copy));
          }
        }
      }

      if (!partial_tile->getItems().empty() || partial_tile->hasGround() ||
          partial_tile->hasCreature() || partial_tile->hasSpawn()) {
        copied_tiles.emplace_back(relative_pos, std::move(partial_tile));
      }
    }
  } else {
    // Tile-level copy: copy entire tiles
    // Get all unique positions that are selected
    auto positions = selectionService.getPositions();
    for (const auto &pos : positions) {
      const auto *tile = map->getTile(pos);
      if (tile) {
        Domain::Position const relative_pos{pos.x - origin.x, pos.y - origin.y,
                                      static_cast<int16_t>(pos.z - origin.z)};

        auto tile_copy = tile->clone();
        copied_tiles.emplace_back(relative_pos, std::move(tile_copy));
      }
    }
  }

  buffer_.setTiles(std::move(copied_tiles));
  return buffer_.size();
}

size_t ClipboardService::cut(EditorSession &session) {
  // First copy
  size_t const count = copy(session);
  if (count == 0) {
    return 0;
  }

  auto &selectionService = session.getSelectionService();
  auto *map = session.getMap();

  // Get all selection entries to determine WHAT to delete
  auto entries = selectionService.getAllEntries();

  // Check if we have item-level or ground-level entries
  bool has_entity_entries = false;
  for (const auto &entry : entries) {
    if (entry.getType() == Domain::Selection::EntityType::Item ||
        entry.getType() == Domain::Selection::EntityType::Ground) {
      has_entity_entries = true;
      break;
    }
  }

  if (has_entity_entries) {
    // Entity-level cut: only remove selected items/ground from tiles
    for (const auto &entry : entries) {
      if (entry.getType() == Domain::Selection::EntityType::Item ||
          entry.getType() == Domain::Selection::EntityType::Ground) {
        auto *item = static_cast<const Domain::Item *>(entry.entity_ptr);
        auto *tile = map->getTile(entry.getPosition());
        if (tile && item) {
          // Check if it's ground
          if (tile->getGround() == item) {
            tile->removeGround();
          } else {
            // Find the item index and remove it
            const auto &items = tile->getItems();
            if (auto it = std::ranges::find(items, item,
                                            &std::unique_ptr<Domain::Item>::get);
                it != items.end()) {
              tile->removeItem(std::distance(items.begin(), it));
            }
          }
        }
      }
    }
  } else {
    // Tile-level cut: remove entire tiles
    auto positions = selectionService.getPositions();
    for (const auto &pos : positions) {
      map->removeTile(pos);
    }
  }

  selectionService.clear();
  session.setModified(true);

  return count;
}

size_t ClipboardService::paste(EditorSession &session,
                               const Domain::Position &target_pos) {
  if (buffer_.empty()) {
    return 0;
  }

  // Instead of immediate action, invoke preview mode
  // The target_pos is ignored here - RME style paste attaches to mouse cursor
  session.startPaste(buffer_.getTiles());

  return buffer_.size();
}

bool ClipboardService::canPaste() const { return !buffer_.empty(); }

int32_t ClipboardService::getClipboardWidth() const {
  return buffer_.getWidth();
}

int32_t ClipboardService::getClipboardHeight() const {
  return buffer_.getHeight();
}

} // namespace MapEditor::AppLogic
