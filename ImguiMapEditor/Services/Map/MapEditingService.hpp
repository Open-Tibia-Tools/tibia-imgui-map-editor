#pragma once

#include "Domain/MapInstance.h"
#include "Domain/Selection/SelectionEntry.h"
#include <vector>

namespace MapEditor::Domain {
class ChunkedMap;
class Item;
class Creature;
class Spawn;
namespace History {
class HistoryManager;
}
} // namespace MapEditor::Domain

namespace MapEditor::Services {

namespace Selection {
class SelectionService;
}

/**
 * Service for performing complex map editing operations.
 * Extracts business logic from controllers to ensure Separation of Concerns.
 */
class MapEditingService {
public:
  MapEditingService() = default;

  /**
   * Move the selected items by the specified delta.
   * Handles the Two-Phase Move Algorithm to prevent data corruption.
   * Manages history recording and selection updates.
   *
   * @param map The map to modify.
   * @param selection_service The active selection service.
   * @param history_manager The history manager for undo/redo.
   * @param dx X delta.
   * @param dy Y delta.
   * @return true if the map was modified.
   */
  bool moveItems(Domain::ChunkedMap *map,
                 Services::Selection::SelectionService &selection_service,
                 Domain::History::HistoryManager &history_manager, int32_t dx,
                 int32_t dy);

private:
  struct PendingItemMove {
    Domain::Position from, to;
    std::unique_ptr<Domain::Item> item;
    bool is_ground;
  };
  struct PendingCreatureMove {
    Domain::Position from, to;
    std::unique_ptr<Domain::Creature> creature;
  };
  struct PendingSpawnMove {
    Domain::Position from, to;
    std::unique_ptr<Domain::Spawn> spawn;
  };

  struct MovedItemInfo {
    Domain::Position position;
    Domain::Selection::EntityType type;
    const Domain::Item *ptr;
    uint16_t server_id;
  };

  struct MoveContext {
      std::vector<PendingItemMove> pending_items;
      std::vector<PendingCreatureMove> pending_creatures;
      std::vector<PendingSpawnMove> pending_spawns;
      std::vector<MovedItemInfo> moved_info;
  };

  static void collectAffectedTiles(const std::vector<Domain::Selection::SelectionEntry>& entries,
                            int32_t dx, int32_t dy,
                            Domain::ChunkedMap* map,
                            Domain::History::HistoryManager& history_manager);

  static void extractMovables(const std::vector<Domain::Selection::SelectionEntry>& entries,
                       int32_t dx, int32_t dy,
                       Domain::ChunkedMap* map,
                       MoveContext& ctx);

  static void insertMovables(Domain::ChunkedMap* map, MoveContext& ctx);

  static void updateSelectionAfterMove(Services::Selection::SelectionService& selection_service,
                                const MoveContext& ctx);
};

} // namespace MapEditor::Services
