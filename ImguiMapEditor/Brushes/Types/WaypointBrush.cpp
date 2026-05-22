#include "WaypointBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Tile.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Brushes {

WaypointBrush::WaypointBrush() = default;

void WaypointBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                         const DrawContext &ctx) {
  if (!tile)
    return;
  if (waypointName_.empty())
    return; // No waypoint name set

  for (const auto &existing : map.getWaypoints()) {
    if (existing.name == waypointName_ && existing.position != tile->getPosition()) {
      if (auto *previousTile = map.getTile(existing.position)) {
        previousTile->setWaypointBrushId(InvalidBrushId);
      }
      break;
    }
  }

  map.upsertWaypoint(waypointName_, tile->getPosition());
  tile->setWaypointBrushId(ctx.ownerBrushId);

  spdlog::trace("[WaypointBrush] Set waypoint '{}' at ({},{},{})",
                waypointName_, tile->getPosition().x, tile->getPosition().y,
                tile->getPosition().z);
}

void WaypointBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile)
    return;

  map.removeWaypointAt(tile->getPosition());
  tile->setWaypointBrushId(InvalidBrushId);

  spdlog::trace("[WaypointBrush] Removed waypoint from ({},{},{})",
                tile->getPosition().x, tile->getPosition().y,
                tile->getPosition().z);
}

} // namespace MapEditor::Brushes
