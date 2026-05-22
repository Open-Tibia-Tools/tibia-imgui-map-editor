#include "EraserBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Tile.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Brushes {

EraserBrush::EraserBrush() = default;

void EraserBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                       const DrawContext &ctx) {
  if (!tile)
    return;

  // Erase ground if enabled
  if (eraseGround_ && tile->hasGround()) {
    tile->removeGround();
    tile->setOptionalBorder(false);
    tile->setGroundBrushId(InvalidBrushId);
    tile->setOptionalBorderBrushId(InvalidBrushId);
  }

  // Erase stacked items if enabled
  if (eraseItems_) {
    tile->clearItems();
  }

  // Erase creature if enabled
  if (eraseCreatures_ && tile->hasCreature()) {
    tile->removeCreature();
    tile->setCreatureBrushId(InvalidBrushId);
  }

  // Erase spawn if enabled
  if (eraseSpawns_ && tile->hasSpawn()) {
    tile->removeSpawn();
    tile->setSpawnBrushId(InvalidBrushId);
  }

  tile->setFlags(Domain::TileFlag::None);
  tile->setZoneBrushId(Domain::TileFlag::ProtectionZone, InvalidBrushId);
  tile->setZoneBrushId(Domain::TileFlag::NoPvp, InvalidBrushId);
  tile->setZoneBrushId(Domain::TileFlag::NoLogout, InvalidBrushId);
  tile->setZoneBrushId(Domain::TileFlag::PvpZone, InvalidBrushId);
  tile->setZoneBrushId(Domain::TileFlag::Refresh, InvalidBrushId);

  map.removeWaypointAt(tile->getPosition());
  tile->setWaypointBrushId(InvalidBrushId);
  tile->setHouseId(0);
  tile->setHouseBrushId(InvalidBrushId);
  tile->setHouseExitBrushId(InvalidBrushId);
  tile->setHouseExitHouseId(0);
  map.markChanged();

  spdlog::trace("[EraserBrush] Erased at ({},{},{})", tile->getPosition().x,
                tile->getPosition().y, tile->getPosition().z);
}

void EraserBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  // Eraser doesn't have undraw - history system handles undo
  // This is intentionally empty
}

} // namespace MapEditor::Brushes
