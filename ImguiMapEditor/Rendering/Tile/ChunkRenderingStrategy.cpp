#include "Rendering/Tile/ChunkRenderingStrategy.h"
#include "Core/Config.h"
#include "Rendering/Backend/SpriteBatch.h"

#include "Rendering/Map/TileRenderer.h"
#include "Rendering/Resources/AtlasManager.h"
#include "Services/SpriteManager.h"
#include <algorithm>

namespace {
constexpr float TILE_SIZE = MapEditor::Config::Rendering::TILE_SIZE;
}

namespace MapEditor {
namespace Rendering {

ChunkRenderingStrategy::Context::Context(RenderState &s,
                                         const AnimationTicks &ticks,
                                         std::vector<uint32_t> &missing,
                                         int &rendered, int z, float offset,
                                         const Domain::Chunk &chunk)
    : state(s), anim_ticks(ticks), missing_sprites(missing),
      tiles_rendered(rendered), floor_z(z), floor_offset(offset) {

  // Calculate coordinates once for the entire chunk
  chunk_wx = chunk.world_x;
  chunk_wy = chunk.world_y;
  chunk_screen_x = chunk_wx * TILE_SIZE - floor_offset;
  chunk_screen_y = chunk_wy * TILE_SIZE - floor_offset;
}

ChunkRenderingStrategy::ChunkRenderingStrategy(TileRenderer &tile_renderer,
                                               SpriteBatch &batch,
                                               Services::SpriteManager &sprites)
    : tile_renderer_(tile_renderer), sprite_batch_(batch),
      sprite_manager_(sprites) {}

void ChunkRenderingStrategy::renderFromCache(
    ChunkSpriteCache::CachedChunk *cached) {
  // ID-based rendering: GPU shader does ID→UV lookup via LUT
  // Always use GPU path - no CPU fallback needed
  if (cached->vbo.isValid()) {
    sprite_batch_.drawTileInstances(cached->vbo.get(), cached->tiles.size(),
                                    sprite_manager_.getAtlasManager(),
                                    sprite_manager_.getSpriteLUT());
  }
}

void ChunkRenderingStrategy::renderCached(const Domain::Chunk &chunk,
                                          const Context &ctx) {

  const int32_t chunk_x = chunk.world_x / Domain::Chunk::SIZE;
  const int32_t chunk_y = chunk.world_y / Domain::Chunk::SIZE;

  auto *cached = ctx.state.chunk_cache.getOrCreate(
      chunk_x, chunk_y, static_cast<int8_t>(ctx.floor_z));

  // FIX: Check floor_offset in addition to validity and generation.
  // floor_offset depends on current_floor for underground floors, so cached
  // positions become invalid when current_floor changes.
  const bool cache_valid =
      cached && cached->valid &&
      cached->generation >= ctx.state.chunk_cache.getGlobalGeneration() &&
      cached->floor_offset == ctx.floor_offset;

  if (!cache_valid) {
    generateCachedChunk(chunk, ctx, cached);
  }

  renderFromCache(cached);
}

void ChunkRenderingStrategy::generateCachedChunk(
    const Domain::Chunk &chunk, const Context &ctx,
    ChunkSpriteCache::CachedChunk *cached) {
  // Use TileInstance format (ID-based caching)
  cached->tiles.clear();
  cached->tiles.reserve(chunk.getNonEmptyCount() * 2);

  // Track missing sprites count BEFORE generation to detect new missing sprites
  const size_t missing_before = ctx.missing_sprites.size();

  // ISOMETRIC DIAGONAL ITERATION (OTClient parity)
  // Tiles at NW drawn first, tiles at SE drawn last for correct depth
  chunk.forEachTileDiagonal([&](const Domain::Tile *tile, int lx, int ly) {
    const int tile_x = ctx.chunk_wx + lx;
    const int tile_y = ctx.chunk_wy + ly;
    const float screen_x = ctx.chunk_screen_x + lx * TILE_SIZE;
    const float screen_y = ctx.chunk_screen_y + ly * TILE_SIZE;

    tile_renderer_.queueTileToTileCache(
        *tile, tile_x, tile_y, ctx.floor_z, screen_x, screen_y, ctx.anim_ticks,
        ctx.missing_sprites, cached->tiles, 1.0f);
    ctx.tiles_rendered++;
  });

  ctx.state.chunk_cache.uploadTiles(cached);

  // FIX: Only mark cache as valid if ALL sprites were available during
  // generation. If any sprites were missing, the chunk will be regenerated on
  // the next frame when those sprites may have finished loading asynchronously.
  const bool had_missing_sprites = ctx.missing_sprites.size() > missing_before;
  cached->valid = !had_missing_sprites;
  cached->floor_offset = ctx.floor_offset; // Store for cache invalidation check
  cached->generation = ctx.state.chunk_cache.getGlobalGeneration();
}

void ChunkRenderingStrategy::renderDynamic(const Domain::Chunk &chunk,
                                           const Context &ctx) {

  // ISOMETRIC DIAGONAL ITERATION (OTClient parity)
  // Tiles at NW drawn first, tiles at SE drawn last for correct depth
  chunk.forEachTileDiagonal([&](const Domain::Tile *tile, int lx, int ly) {
    // Calc coords incrementally using context
    const int tile_x = ctx.chunk_wx + lx;
    const int tile_y = ctx.chunk_wy + ly;
    const float screen_x = ctx.chunk_screen_x + lx * TILE_SIZE;
    const float screen_y = ctx.chunk_screen_y + ly * TILE_SIZE;

    // Pass explicit coords
    tile_renderer_.queueTile(*tile, tile_x, tile_y, ctx.floor_z, screen_x,
                             screen_y, 1.0f, ctx.anim_ticks,
                             ctx.missing_sprites, &ctx.state.overlay_collector);
    ctx.tiles_rendered++;
  });
}

void ChunkRenderingStrategy::renderEdge(const Domain::Chunk &chunk,
                                        const Context &ctx,
                                        const VisibleBounds &bounds) {
  // DEPRECATED: This method is replaced by renderCached/renderDynamic usage in
  // TerrainPass. It is left here temporarily to satisfy linker if referenced
  // elsewhere, but it should no longer be called.
}

} // namespace Rendering
} // namespace MapEditor
