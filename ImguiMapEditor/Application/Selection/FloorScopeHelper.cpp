#include "FloorScopeHelper.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Item.h"
#include "Domain/Position.h"
#include "Domain/Selection/SelectionEntry.h"
#include "Domain/Tile.h"
#include "Services/Selection/SelectionService.h"

namespace MapEditor::AppLogic {

using namespace Domain::Selection;

FloorRange getFloorRange(Domain::SelectionFloorScope scope,
                         int16_t current_floor) {
  // RME constants
  constexpr int16_t GROUND_LAYER = 7;
  constexpr int16_t MAX_FLOOR = 15;

  switch (scope) {
  case Domain::SelectionFloorScope::CurrentFloor:
    // Only current floor
    return {current_floor, current_floor};

  case Domain::SelectionFloorScope::AllFloors:
    // From z=15 (deepest underground) down to current floor
    return {MAX_FLOOR, current_floor};

  case Domain::SelectionFloorScope::VisibleFloors:
    // Above ground (z <= 7): visible from z=7 to current
    // Underground (z > 7): visible from z=current+2 to current
    if (current_floor <= GROUND_LAYER) {
      return {GROUND_LAYER, current_floor};
    } else {
      return {std::min(MAX_FLOOR, static_cast<int16_t>(current_floor + 2)),
              current_floor};
    }
  }

  // Fallback: current floor only
  return {current_floor, current_floor};
}

void selectTileStackAcrossFloors(
    Domain::ChunkedMap *map,
    Services::Selection::SelectionService &selection_service,
    const Domain::Position &pos, Domain::SelectionFloorScope scope) {
  if (!map) {
    return;
  }

  // Get floor range from scope setting
  auto floor_range = getFloorRange(scope, pos.z);

  // Helper to create EntityId
  auto makeEntityId = [](const Domain::Position &tile_pos, EntityType type,
                         uint64_t local_id) {
    EntityId id;
    id.position = tile_pos;
    id.type = type;
    id.local_id = local_id;
    return id;
  };

  // Iterate over all floors in range
  for (int16_t z = floor_range.start_z; z >= floor_range.end_z; --z) {
    Domain::Position const tile_pos{pos.x, pos.y, z};
    if (Domain::Tile *tile = map->getTile(tile_pos)) {
      // Add ground if present
      if (const Domain::Item *ground = tile->getGround()) {
        selection_service.addEntity(
            SelectionEntry{makeEntityId(tile_pos, EntityType::Ground, 0),
                           ground, ground->getServerId()});
      }
      // Add all stacked items
      const auto &items = tile->getItems();
      for (const auto &item : items) {
        selection_service.addEntity(
            SelectionEntry{makeEntityId(tile_pos, EntityType::Item,
                                        reinterpret_cast<uint64_t>(item.get())),
                           item.get(), item->getServerId()});
      }
    }
  }
}

} // namespace MapEditor::AppLogic
