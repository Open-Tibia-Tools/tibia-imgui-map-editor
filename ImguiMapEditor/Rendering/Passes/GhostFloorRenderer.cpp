#include "Rendering/Passes/GhostFloorRenderer.h"
#include "Domain/ChunkedMap.h"
#include "Rendering/Backend/SpriteBatch.h"
#include "Rendering/Map/TileRenderer.h"
#include "Rendering/Tile/ChunkRenderingStrategy.h"
#include "Rendering/Visibility/FloorIterator.h"
#include "Services/SpriteManager.h"
#include "Services/ViewSettings.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

namespace MapEditor {
namespace Rendering {

GhostFloorRenderer::GhostFloorRenderer(TileRenderer &tile_renderer,
                                       SpriteBatch &batch,
                                       ChunkVisibilityManager &visibility,
                                       Services::SpriteManager &sprites)
    : tile_renderer_(tile_renderer), sprite_batch_(batch),
      chunk_visibility_(visibility), sprite_manager_(sprites) {
  // Initialize chunk strategy
  chunk_strategy_ = std::make_unique<ChunkRenderingStrategy>(
      tile_renderer_, sprite_batch_, sprite_manager_);
}

GhostFloorRenderer::~GhostFloorRenderer() = default;

void GhostFloorRenderer::render(const RenderContext &context) {
  if (!context.view_settings)
    return;

  // Delegate blending to pipeline/pass manager (usually enabled by default or
  // MapRenderer)

  // Ghost higher floor
  int const ghost_higher = FloorIterator::getGhostHigherFloor(
      context.current_floor, context.view_settings->ghost_higher_floors);

  if (ghost_higher >= 0) {
    renderSingleFloor(
        context.map, context.state, context.visible_bounds,
        context.current_floor, ghost_higher, context.camera.getZoom(),
        FloorIterator::GHOST_ALPHA,
        glm::vec2(context.viewport_width, context.viewport_height),
        context.camera.getPosition(), context.mvp_matrix, context.anim_ticks,
        context.missing_sprites_buffer);
  }

  // Ghost lower floor
  int const ghost_lower = FloorIterator::getGhostLowerFloor(
      context.current_floor, context.view_settings->ghost_lower_floors);

  if (ghost_lower >= 0) {
    renderSingleFloor(
        context.map, context.state, context.visible_bounds,
        context.current_floor, ghost_lower, context.camera.getZoom(),
        FloorIterator::GHOST_ALPHA,
        glm::vec2(context.viewport_width, context.viewport_height),
        context.camera.getPosition(), context.mvp_matrix, context.anim_ticks,
        context.missing_sprites_buffer);
  }
}

void GhostFloorRenderer::renderSingleFloor(
    const Domain::ChunkedMap &map, RenderState &state,
    const VisibleBounds &bounds, int current_floor, int ghost_floor, float zoom,
    float alpha, const glm::vec2 &viewport_size, const glm::vec2 &camera_pos,
    const glm::mat4 &mvp, const AnimationTicks &anim_ticks,
    std::vector<uint32_t> &missing_sprites) {
  // Calculate floor offset for ghost floor (parallax effect)
  float const floor_offset =
      FloorIterator::getFloorOffset(current_floor, ghost_floor);

  // Update chunk visibility for ghost floor
  chunk_visibility_.update(map, bounds, static_cast<int8_t>(ghost_floor),
                           floor_offset);

  // OPTIMIZATION: Use ChunkRenderingStrategy's Cached Mode.
  // This automatically handles cache generation (uploading static geometry once)
  // and rendering from VBOs, preventing the 6.4MB/frame upload overhead of the
  // previous dynamic fallback path.
  //
  // Note: We force 'renderCached' here because ghost floors are effectively static
  // (animations are usually ignored or acceptable to be static in ghost view).
  // If we used dynamic mode, we'd be uploading thousands of sprites every frame.

  int tiles_rendered = 0;

  // Begin Tile Batch (Cached)
  sprite_batch_.beginTileBatch(mvp, sprite_manager_.getAtlasManager(),
                               sprite_manager_.getSpriteLUT());
  sprite_batch_.setGlobalTint(1.0f, 1.0f, 1.0f, alpha);

  for (const VisibleChunk &vc : chunk_visibility_.getVisibleChunks()) {
    Domain::Chunk  const*chunk = vc.chunk;

    ChunkRenderingStrategy::Context const chunk_ctx(
        state, anim_ticks, missing_sprites, tiles_rendered, ghost_floor,
        floor_offset, *chunk);

    // Render using cached VBOs (generates if missing)
    chunk_strategy_->renderCached(*chunk, chunk_ctx);
  }

  sprite_batch_.endTileBatch();
}

} // namespace Rendering
} // namespace MapEditor
