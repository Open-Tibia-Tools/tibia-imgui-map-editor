/**
 * @file SpawnBrush.cpp
 * @brief Implementation of SpawnBrush for placing spawn points.
 */

#include "SpawnBrush.h"

#include "Domain/ChunkedMap.h"
#include "Domain/Spawn.h"
#include "Domain/Tile.h"
#include "Services/BrushSettingsService.h"
#include <algorithm>
#include <memory>

namespace MapEditor::Brushes {

SpawnBrush::SpawnBrush() = default;

void SpawnBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                      const DrawContext &ctx) {
  if (!tile)
    return;

  // Don't overwrite existing spawn
  if (tile->hasSpawn())
    return;

  // wx parity: spawn radius follows current brush size.
  int radius = 3;
  if (ctx.brushSettings) {
    radius = ctx.brushSettings->getStandardSize();
  } else if (settingsService_) {
    radius = settingsService_->getStandardSize();
  }
  radius = std::max(1, radius);

  // Create spawn at this tile's position
  auto spawn = std::make_unique<Domain::Spawn>(tile->getPosition(), radius);
  tile->setSpawn(std::move(spawn));
  tile->setSpawnBrushId(ctx.ownerBrushId);
  map.markChanged();
}

void SpawnBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile)
    return;

  if (tile->hasSpawn()) {
    tile->removeSpawn();
    tile->setSpawnBrushId(InvalidBrushId);
    map.markChanged();
  }
}

} // namespace MapEditor::Brushes
