#include "Rendering/Passes/TerrainPass.h"
#include "Core/Config.h"
#include "Rendering/Frame/FrameDataCollector.h"
#include "Rendering/Map/TileRenderer.h"
#include "Rendering/Passes/ShadeRenderer.hpp"
#include "Rendering/Passes/SpawnTintPass.h"
#include "Rendering/Tile/ChunkRenderingStrategy.h"
#include "Rendering/Visibility/ChunkVisibilityManager.h"
#include "Rendering/Visibility/FloorIterator.h"
#include "Rendering/Visibility/LODPolicy.h"
#include "Services/SpriteManager.h"
#include "Services/ViewSettings.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Rendering {

TerrainPass::TerrainPass(TileRenderer &tile_renderer,
                         ChunkVisibilityManager &visibility, SpriteBatch &batch,
                         Services::SpriteManager &sprite_manager,
                         FrameDataCollector &frame_data_collector)
    : tile_renderer_(tile_renderer), chunk_visibility_(visibility),
      sprite_batch_(batch), sprite_manager_(sprite_manager),
      frame_data_collector_(frame_data_collector) {

  // Initialize owned components
  shade_renderer_ = std::make_unique<ShadeRenderer>();
  spawn_renderer_ =
      std::make_unique<SpawnTintPass>(sprite_batch_, sprite_manager_);
  chunk_strategy_ = std::make_unique<ChunkRenderingStrategy>(
      tile_renderer_, sprite_batch_, sprite_manager);

  // Initialize state
  was_lod_active_ = false;
}

TerrainPass::~TerrainPass() = default;

void TerrainPass::setLODMode(bool enabled) {
  is_lod_active_ = enabled;
  tile_renderer_.setLODMode(enabled);
}

void TerrainPass::render(const RenderContext &context) {
  if (!context.view_settings)
    return;

  // Calculate floor range
  bool const show_all_floors = context.view_settings->show_all_floors;
  FloorRange floor_range = FloorIterator::calculateRangeWithToggle(
      context.current_floor, show_all_floors);

  // SMART EVICTION:
  // If floor or visibility settings changed, prune the cache to keep memory
  // usage low. We only keep cached chunks that are within the currently visible
  // floor range.
  if (context.current_floor != last_floor_ ||
      show_all_floors != was_show_all_floors_) {

    spdlog::info(
        "[TerrainPass] Floor changed to {} (Range: {} to {}). Pruning cache...",
        context.current_floor, floor_range.start_z, floor_range.super_end_z);

    // Prune everything outside the current visible range
    context.state.chunk_cache.prune(
        static_cast<int8_t>(floor_range.super_end_z),
        static_cast<int8_t>(floor_range.start_z));

    last_floor_ = context.current_floor;
    was_show_all_floors_ = show_all_floors;
  }

  // Get white pixel for shade rendering
  const AtlasRegion *white_pixel =
      sprite_manager_.getAtlasManager().getWhitePixel();

  // Begin sprite batch for this pass (Default to Sprite Mode for
  // Shade/Creatures)
  sprite_batch_.begin(context.mvp_matrix);

  // RME-style multi-floor rendering: back to front
  for (int map_z = floor_range.start_z; map_z >= floor_range.super_end_z;
       map_z--) {
    // Draw shade overlay at floor boundary
    bool const show_shade = context.view_settings->show_shade;
    if (white_pixel && shade_renderer_ &&
        FloorIterator::shouldDrawShade(map_z, floor_range, show_shade)) {
      // Note: Viewport width/height used for full-screen shade
      shade_renderer_->render(*context.sprite_batch, context.camera,
                              context.viewport_width, context.viewport_height,
                              *white_pixel);
    }

    // Only render tiles if this floor is within visible range
    if (FloorIterator::shouldRenderFloor(map_z, floor_range)) {
      // Render this floor's terrain (creatures and OnTop are rendered per-tile)
      // Note: Visibility update is handled inside renderMainFloor
      renderMainFloor(context, map_z);
      // NOTE: Creatures and OnTop items are now rendered IMMEDIATELY per-tile
      // inside queueTile() to maintain correct isometric depth with diagonal
      // iteration. Per-floor deferred rendering is no longer used.
    }
  }

  // End sprite batch - flushes to GPU
  sprite_batch_.end(sprite_manager_.getAtlasManager());
}

void TerrainPass::renderMainFloor(const RenderContext &context, int floor) {
  // MEMORY OPTIMIZATION:
  // If we just switched from LOD Mode (Cache) to Dynamic Mode (No Cache),
  // we should clear the VBOs to free up RAM (approx 8GB for full map).
  if (was_lod_active_ && !is_lod_active_) {
    // Zoomed IN: Clear cache
    context.state.chunk_cache.clear();
  }
  was_lod_active_ = is_lod_active_;

  // Expand viewport bounds per floor (RME parallax effect)
  int const floor_diff =
      FloorIterator::calculateRangeWithToggle(
          context.current_floor, context.view_settings->show_all_floors)
          .start_z -
      floor;

  FloorRange const floor_range = FloorIterator::calculateRangeWithToggle(
      context.current_floor, context.view_settings->show_all_floors);
  int const floor_diff_val = floor_range.start_z - floor;

  VisibleBounds const floor_bounds =
      context.visible_bounds.withFloorOffset(floor_diff_val);

  // Calculate floor offset for parallax
  float const floor_offset =
      FloorIterator::getFloorOffset(context.current_floor, floor);

  float const zoom = context.camera.getZoom();

  // Configure tile renderer for current zoom
  tile_renderer_.setZoom(zoom);

  // Update cache policy
  context.state.last_zoom = zoom;

  // Query visibility for THIS floor specifically
  chunk_visibility_.update(context.map, floor_bounds,
                           static_cast<int8_t>(floor), floor_offset);

  // PRE-PASS: Collect all spawns before tile rendering
  frame_data_collector_.collectSpawns(context.map, floor, floor_bounds,
                                      context.state.overlay_collector,
                                      *context.view_settings);

  int tiles_rendered = 0;

  // Render Strategy Selection: Cached vs Dynamic
  if (is_lod_active_) {
    // === CACHED / TILE BATCH MODE ===
    // 1. Flush and end the current Sprite Batch (used for shade/overlays)
    sprite_batch_.end(sprite_manager_.getAtlasManager());

    // 2. Begin Tile Batch Mode (sets shader, binds Atlas/LUT/VAO ONCE)
    sprite_batch_.beginTileBatch(context.mvp_matrix,
                                 sprite_manager_.getAtlasManager(),
                                 sprite_manager_.getSpriteLUT());

    for (const VisibleChunk &vc : chunk_visibility_.getVisibleChunks()) {
      Domain::Chunk  const*chunk = vc.chunk;
      ChunkRenderingStrategy::Context const chunk_ctx(
          context.state, context.anim_ticks, context.missing_sprites_buffer,
          tiles_rendered, floor, floor_offset, *chunk);

      // Render using cached VBOs
      chunk_strategy_->renderCached(*chunk, chunk_ctx);
    }

    // 3. End Tile Batch Mode
    sprite_batch_.endTileBatch();

    // 4. Restart Sprite Batch Mode for subsequent rendering (Creatures, etc.)
    sprite_batch_.begin(context.mvp_matrix);

  } else {
    // === DYNAMIC / SPRITE MODE ===
    // Proceed using the existing Sprite Batch session
    for (const VisibleChunk &vc : chunk_visibility_.getVisibleChunks()) {
      Domain::Chunk  const*chunk = vc.chunk;
      ChunkRenderingStrategy::Context const chunk_ctx(
          context.state, context.anim_ticks, context.missing_sprites_buffer,
          tiles_rendered, floor, floor_offset, *chunk);

      // Render using dynamic sprite queuing (CPU heavy, GPU batched)
      chunk_strategy_->renderDynamic(*chunk, chunk_ctx);
    }
  }

  // Process Waypoints
  frame_data_collector_.collectWaypoints(context.map, floor, floor_bounds,
                                         context.state.overlay_collector,
                                         *context.view_settings, floor_offset);

  // Render Spawn Overlays (Radius Tints & Indicators)
  // Check LOD Policy: Should we show detailed spawn tints?
  // Re-using SHOW_SPAWN_LABELS policy for consistency (if labels are hidden,
  // tints should be too)
  bool const show_spawn_overlays = !is_lod_active_ || LODPolicy::SHOW_SPAWN_LABELS;

  if (show_spawn_overlays && context.view_settings->show_spawns &&
      context.view_settings->show_spawn_radius && spawn_renderer_) {

    spawn_renderer_->renderFromCollector(context.state.overlay_collector, floor,
                                         floor_offset, 1.0f);
  }
}
} // namespace Rendering
} // namespace MapEditor
