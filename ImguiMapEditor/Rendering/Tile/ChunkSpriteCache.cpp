#include "Rendering/Tile/ChunkSpriteCache.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Rendering {

ChunkSpriteCache::CachedChunk *
ChunkSpriteCache::getOrCreate(int32_t chunk_x, int32_t chunk_y, int8_t floor) {

  uint64_t const key = makeKey(chunk_x, chunk_y, floor);
  auto &entry = cache_[key]; // Creates if not exists
  return &entry;
}

const ChunkSpriteCache::CachedChunk *
ChunkSpriteCache::get(int32_t chunk_x, int32_t chunk_y, int8_t floor) const {

  uint64_t const key = makeKey(chunk_x, chunk_y, floor);
  auto it = cache_.find(key);
  if (it != cache_.end() && it->second.valid) {
    return &it->second;
  }
  return nullptr;
}

void ChunkSpriteCache::invalidate(int32_t chunk_x, int32_t chunk_y,
                                  int8_t floor) {
  uint64_t const key = makeKey(chunk_x, chunk_y, floor);
  auto it = cache_.find(key);
  if (it != cache_.end()) {
    it->second.valid = false;
    it->second.generation++;
  }
}

void ChunkSpriteCache::invalidateAll() {
  global_generation_++;
  for (auto &[key, entry] : cache_) {
    entry.valid = false;
  }
}

void ChunkSpriteCache::clear() {
  // Explicitly destroy GPU resources
  for (auto &[key, entry] : cache_) {
    entry.vbo.reset(); // Releases VBO handle
  }
  cache_.clear();
  global_generation_++;
}

void ChunkSpriteCache::prune(int8_t min_z, int8_t max_z) {
  size_t before = cache_.size();
  // Iterate and remove entries outside range
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (it->second.z < min_z || it->second.z > max_z) {
      it->second.vbo.reset(); // Free GPU resource
      it = cache_.erase(it);  // Free CPU resource
    } else {
      ++it;
    }
  }
  size_t after = cache_.size();
  if (before != after) {
    spdlog::info(
        "[ChunkSpriteCache] Pruned {} chunks (Range: {} to {}). Size: {} -> {}",
        before - after, min_z, max_z, before, after);
  }
}

void ChunkSpriteCache::uploadTiles(CachedChunk *chunk) {
  if (!chunk || chunk->tiles.empty())
    return;

  // Create VBO if needed
  if (!chunk->vbo.isValid()) {
    chunk->vbo.create();
    chunk->vbo_capacity = 0;
  }

  glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo.get());

  size_t const required_size = chunk->tiles.size() * sizeof(TileInstance);

  // If buffer is too small, reallocate
  if (required_size > chunk->vbo_capacity) {
    glBufferData(GL_ARRAY_BUFFER, required_size, chunk->tiles.data(),
                 GL_STATIC_DRAW);
    chunk->vbo_capacity = required_size;
  } else {
    // Reuse existing buffer
    glBufferSubData(GL_ARRAY_BUFFER, 0, required_size, chunk->tiles.data());
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

size_t ChunkSpriteCache::getTotalSprites() const {
  size_t total = 0;
  for (const auto &[key, entry] : cache_) {
    if (entry.valid) {
      total += entry.tiles.size();
    }
  }
  return total;
}

} // namespace Rendering
} // namespace MapEditor
