#include "CreatureBrush.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Creature.h"
#include "Domain/Spawn.h"
#include "Domain/Tile.h"
#include "Services/BrushSettingsService.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace MapEditor::Brushes {

CreatureBrush::CreatureBrush(const std::string &name,
                             const Domain::Outfit &outfit)
    : name_(name), outfit_(outfit) {}

void CreatureBrush::draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                         const DrawContext &ctx) {
  if (!tile)
    return;

  // Create new creature instance
  auto creature = std::make_unique<Domain::Creature>();
  creature->setName(name_);
  creature->setOutfit(outfit_);
  creature->setPosition(tile->getPosition());

  // Add to tile (Tile takes ownership)
  tile->setCreature(std::move(creature));

  // Auto-create spawn if enabled in settings
  if (ctx.brushSettings && ctx.brushSettings->getAutoCreateSpawn()) {
    // Check if this tile is within ANY existing spawn's radius
    bool withinExistingSpawn = false;

    // Search nearby tiles for spawns that cover this position
    Domain::Position const pos = tile->getPosition();
    int const maxRadius = 10; // Max possible spawn radius

    for (int dy = -maxRadius; dy <= maxRadius && !withinExistingSpawn; ++dy) {
      for (int dx = -maxRadius; dx <= maxRadius && !withinExistingSpawn; ++dx) {
        Domain::Tile *nearbyTile = map.getTile(pos.x + dx, pos.y + dy, pos.z);
        if (nearbyTile && nearbyTile->hasSpawn()) {
          const Domain::Spawn *spawn = nearbyTile->getSpawn();
          if (spawn) {
            // Check if current tile is within this spawn's radius
            int const spawnRadius = spawn->radius;
            if (std::abs(dx) <= spawnRadius && std::abs(dy) <= spawnRadius) {
              withinExistingSpawn = true;
            }
          }
        }
      }
    }

    // Only create spawn if not within any existing spawn
    if (!withinExistingSpawn) {
      int radius = ctx.brushSettings->getDefaultSpawnRadius();
      auto spawn = std::make_unique<Domain::Spawn>(tile->getPosition(), radius);
      tile->setSpawn(std::move(spawn));
      spdlog::debug(
          "[CreatureBrush] Auto-created spawn at ({},{},{}) with radius {}",
          tile->getPosition().x, tile->getPosition().y, tile->getPosition().z,
          radius);
    }
  }
}

void CreatureBrush::undraw(Domain::ChunkedMap &map, Domain::Tile *tile) {
  if (!tile)
    return;
  // Remove creature from tile
  tile->removeCreature();
}

} // namespace MapEditor::Brushes
