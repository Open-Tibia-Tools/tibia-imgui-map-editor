#include "SpawnTintPass.h"
#include "Rendering/Resources/AtlasManager.h"
#include "Rendering/Utils/CoordUtils.h"
#include "Services/SpriteManager.h"

namespace MapEditor {
namespace Rendering {

SpawnTintPass::SpawnTintPass(SpriteBatch &batch,
                             Services::SpriteManager &sprites)
    : sprite_batch_(batch), sprite_manager_(sprites) {}

void SpawnTintPass::queueRadiusOverlay(float screen_x, float screen_y,
                                       float size, float alpha,
                                       const OverlayCollector *collector,
                                       int tile_x, int tile_y, int tile_z,
                                       bool tile_has_spawn) {
  // Check if tile is within any spawn radius
  bool in_spawn_radius = tile_has_spawn;
  if (!in_spawn_radius && collector) {
    in_spawn_radius = collector->isWithinAnySpawnRadius(tile_x, tile_y, tile_z);
  }

  if (!in_spawn_radius) {
    return;
  }

  // Get white pixel for overlay drawing
  const AtlasRegion *white_pixel =
      sprite_manager_.getAtlasManager().getWhitePixel();
  if (!white_pixel) {
    return;
  }

  // TINT OVERLAY (using Config::Colors::SPAWN_RADIUS_TINT_*)
  float const overlay_alpha = Config::Colors::SPAWN_RADIUS_TINT_FACTOR * alpha;
  sprite_batch_.draw(screen_x, screen_y, size, size, *white_pixel,
                     Config::Colors::SPAWN_RADIUS_TINT_R,
                     Config::Colors::SPAWN_RADIUS_TINT_G,
                     Config::Colors::SPAWN_RADIUS_TINT_B, overlay_alpha);
}

void SpawnTintPass::queueCenterIndicator(float screen_x, float screen_y,
                                         float size, float alpha) {
  const AtlasRegion *white_pixel =
      sprite_manager_.getAtlasManager().getWhitePixel();
  if (!white_pixel) {
    return;
  }

  // Draw Config-defined spawn indicator fill
  // Use full size for the new box design, not small centered square
  auto color = Config::Colors::UnpackColor(Config::Colors::SPAWN_INDICATOR_FILL);
  float const overlay_alpha = color.a * alpha;

  sprite_batch_.draw(screen_x, screen_y, size, size, *white_pixel, color.r,
                     color.g, color.b, overlay_alpha);
}

void SpawnTintPass::renderFromCollector(const OverlayCollector &collector,
                                        int floor_z, float floor_offset,
                                        float alpha) {
  // Use constant TILE_SIZE (32.0f) because the MVP matrix handles Zoom.
  constexpr float tile_size = Config::Rendering::TILE_SIZE;

  // 1. Render Cyan Radius Tints
  for (const auto &entry : collector.spawn_radii) {
    if (entry.floor != floor_z)
      continue;

    // Calculate World Coordinates (Un-zoomed, but with parallax offset)
    // Same logic as ChunkRenderingStrategy
    float const center_world_x = entry.center_x * tile_size - floor_offset;
    float const center_world_y = entry.center_y * tile_size - floor_offset;

    // Radius is square (Chebyshev)
    // Full width = (radius * 2 + 1) tiles
    float const radius_px = (entry.radius * 2 + 1) * tile_size;

    // Top-left corner
    float const top_left_x = center_world_x - entry.radius * tile_size;
    float const top_left_y = center_world_y - entry.radius * tile_size;

    const AtlasRegion *white_pixel =
        sprite_manager_.getAtlasManager().getWhitePixel();
    if (white_pixel) {
      float const overlay_alpha = Config::Colors::SPAWN_RADIUS_TINT_FACTOR * alpha;
      sprite_batch_.draw(top_left_x, top_left_y, radius_px, radius_px,
                         *white_pixel, Config::Colors::SPAWN_RADIUS_TINT_R,
                         Config::Colors::SPAWN_RADIUS_TINT_G,
                         Config::Colors::SPAWN_RADIUS_TINT_B, overlay_alpha);
    }
  }

  // 2. Render Center Indicators
  for (const auto &entry : collector.spawns) {
    if (!entry.tile)
      continue;
    const auto &pos = entry.tile->getPosition();
    if (pos.z != floor_z)
      continue;

    float const world_x = pos.x * tile_size - floor_offset;
    float const world_y = pos.y * tile_size - floor_offset;

    queueCenterIndicator(world_x, world_y, tile_size, alpha);
  }
}

void SpawnTintPass::collectVisibleSpawns(const Domain::ChunkedMap &map,
                                         int floor_z,
                                         const VisibleBounds &bounds,
                                         OverlayCollector &collector,
                                         const Services::ViewSettings &settings,
                                         std::vector<Domain::Chunk *> &buffer) {
  // Check if spawn visualization is enabled
  // NOTE: We now link radius visibility directly to show_spawns for better UX
  // (ignoring show_spawn_radius flag)
  if (!settings.show_spawns) {
    return;
  }

  // EXTENDED area for spawns whose radius extends into viewport
  constexpr int kMaxSpawnRadius = 15;
  VisibleBounds const spawn_bounds = bounds.withFloorOffset(kMaxSpawnRadius);

  buffer.clear();
  map.getVisibleChunks(spawn_bounds.start_x, spawn_bounds.start_y,
                       spawn_bounds.end_x, spawn_bounds.end_y,
                       static_cast<int16_t>(floor_z), buffer);

  // Use optimized path: skip chunks without spawns, iterate only spawn tiles
  for (Domain::Chunk  const*chunk : buffer) {
    // Skip chunks with no spawns (O(1) check)
    if (!chunk->hasSpawns())
      continue;

    for (const Domain::Tile *tile : chunk->getSpawnTiles()) {
      if (!tile)
        continue;

      auto spawn = tile->getSpawn();
      if (spawn) {
        // OPTIMIZED: Use chunk-based creature counting instead of O(r²) tile lookups
        // Calculate chunks that overlap spawn radius
        const int32_t r = spawn->radius;
        const int32_t cx = tile->getX();
        const int32_t cy = tile->getY();
        const int16_t cz = tile->getZ();

        // Convert spawn bounds to chunk coordinates (>> 5 = / 32)
        const int32_t min_chunk_x = (cx - r) >> 5;
        const int32_t max_chunk_x = (cx + r) >> 5;
        const int32_t min_chunk_y = (cy - r) >> 5;
        const int32_t max_chunk_y = (cy + r) >> 5;

        // Sum creature counts from overlapping chunks - O(k) where k ≈ 1-4
        int32_t creature_count = 0;
        for (int32_t chunk_y = min_chunk_y; chunk_y <= max_chunk_y; ++chunk_y) {
          for (int32_t chunk_x = min_chunk_x; chunk_x <= max_chunk_x; ++chunk_x) {
            // PROFILER OPTIMIZATION 2025-02-15:
            // Replaced getVisibleChunks() inside loop with direct O(1) chunk access.
            // Avoids vector allocation and bounds calculations in hot loop.
            const auto radius_chunk = map.getChunk(chunk_x, chunk_y, cz);
            if (radius_chunk) {
              creature_count += radius_chunk->getCreatureCount();
            }
          }
        }

        collector.addSpawnRadius(cx, cy, cz, spawn->radius, creature_count);
      }
    }
  }
}

} // namespace Rendering
} // namespace MapEditor
