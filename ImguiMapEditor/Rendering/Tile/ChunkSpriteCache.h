#pragma once
#include "Rendering/Backend/SpriteBatch.h"
#include "Rendering/Backend/TileInstance.h"
#include "Rendering/Core/GLHandle.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace MapEditor {
namespace Rendering {

/**
 * Cache for per-chunk generated TileInstance arrays.
 *
 * When a chunk hasn't changed (no tile edits), we skip tile iteration
 * and directly render from cached VBO. TileInstance stores sprite_id
 * instead of UVs - resolution happens in shader via SpriteAtlasLUT.
 *
 * Cache key: (chunk_x, chunk_y, floor) packed into 64-bit integer.
 */
class ChunkSpriteCache {
public:
  struct CachedChunk {
    std::vector<TileInstance> tiles; // ID-based cache (new architecture)
    DeferredVBOHandle vbo;           // Handle to GPU buffer (created lazily)
    size_t vbo_capacity = 0;         // Current capacity in bytes
    uint64_t generation = 0;         // Incremented when chunk content changes
    float floor_offset =
        0.0f;     // Floor offset used when generating (for cache validation)
    int8_t z = 0; // Store Z floor for smart eviction
    bool valid = false;

    CachedChunk() = default;
    CachedChunk(CachedChunk &&) = default;
    CachedChunk &operator=(CachedChunk &&) = default;
    // Non-copyable due to DeferredVBOHandle
    CachedChunk(const CachedChunk &) = delete;
    CachedChunk &operator=(const CachedChunk &) = delete;
  };

  /**
   * Get or create cache entry for a chunk.
   * @return Pointer to cached chunk (never null after call)
   */
  CachedChunk *getOrCreate(int32_t chunk_x, int32_t chunk_y, int8_t floor);

  /**
   * Get existing cache entry (returns null if not cached).
   */
  const CachedChunk *get(int32_t chunk_x, int32_t chunk_y, int8_t floor) const;

  /**
   * Invalidate a specific chunk's cache.
   * Called when a tile in this chunk is modified.
   */
  void invalidate(int32_t chunk_x, int32_t chunk_y, int8_t floor);

  /**
   * Invalidate all cached chunks.
   * Called on map load, zoom change, major settings change.
   */
  void invalidateAll();

  /**
   * Clear all cached data and release GPU resources.
   * Called when switching to Dynamic Mode (Zoom > 20%) to free RAM.
   */
  void clear();

  /**
   * Remove chunks outside the given floor range.
   * Called when switching floors to evict invisible layers.
   */
  void prune(int8_t min_z, int8_t max_z);

  /**
   * Upload TileInstance data to the chunk's VBO (new ID-based format).
   * Creates VBO if needed.
   */
  static void uploadTiles(CachedChunk *chunk);

  /**
   * Get current global generation counter.
   * Changes when invalidateAll() is called.
   */
  uint64_t getGlobalGeneration() const { return global_generation_; }

  /**
   * Get cache statistics.
   */
  size_t getCacheSize() const { return cache_.size(); }
  size_t getTotalSprites() const;

private:
  static uint64_t makeKey(int32_t chunk_x, int32_t chunk_y, int8_t floor) {
    // Pack into 64 bits: 24 bits x, 24 bits y, 8 bits floor
    uint64_t key = 0;
    key |= (static_cast<uint64_t>(chunk_x + 0x800000) & 0xFFFFFF) << 32;
    key |= (static_cast<uint64_t>(chunk_y + 0x800000) & 0xFFFFFF) << 8;
    key |= static_cast<uint64_t>(floor & 0xFF);
    return key;
  }

  std::unordered_map<uint64_t, CachedChunk> cache_;
  uint64_t global_generation_ = 0;
};

} // namespace Rendering
} // namespace MapEditor
