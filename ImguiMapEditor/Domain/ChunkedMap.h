#pragma once
#include "Core/Config.h"
#include "House.h"
#include "Tile.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace MapEditor {
namespace Domain {

/**
 * A 32x32 chunk of tiles providing spatial locality for cache-efficient
 * iteration.
 *
 * PERFORMANCE BENEFIT:
 * - All 1024 tiles in a chunk are contiguous in memory
 * - Iterating visible tiles hits L2 cache instead of thrashing
 * - Chunk-level culling skips 1024 tiles with one bounds check
 */
class Chunk {
public:
  static constexpr int SIZE = Config::Performance::CHUNK_SIZE;
  static constexpr int TILE_COUNT = SIZE * SIZE;

  // World coordinates of chunk's top-left corner
  int32_t world_x = 0;
  int32_t world_y = 0;

  /**
   * Get tile at local coordinates within chunk.
   * @param local_x 0 to SIZE-1
   * @param local_y 0 to SIZE-1
   * @return Tile pointer, or nullptr if empty
   */
  Tile *getTile(int local_x, int local_y) const;

  /**
   * Unsafe tile access for hot paths (renderer).
   * CAUTION: No bounds check! Caller must ensure 0 <= x,y < SIZE.
   * @param local_x 0 to SIZE-1
   * @param local_y 0 to SIZE-1
   */
  inline Tile *getTileUnsafe(int local_x, int local_y) const {
    // Direct array access without bounds check for max performance
    // toIndex is simple math, inlined here for clarity
    // int idx = local_y * SIZE + local_x;
    return tiles_[local_y * SIZE + local_x].get();
  }

  /**
   * Set tile at local coordinates.
   * @param local_x 0 to SIZE-1
   * @param local_y 0 to SIZE-1
   * @param tile Tile to store (nullptr to remove)
   */
  void setTile(int local_x, int local_y, std::unique_ptr<Tile> tile);

  /**
   * Remove tile at local coordinates and return it.
   */
  std::unique_ptr<Tile> removeTile(int local_x, int local_y);

  /**
   * Get all non-empty tiles in this chunk.
   * Returns pointers for cache-friendly iteration.
   */
  std::vector<Tile *> getNonEmptyTiles() const;

  /**
   * Get all tiles with spawns in this chunk.
   * Uses internal cache for performance.
   */
  const std::vector<Tile *> &getSpawnTiles() const;

  /**
   * Invalidate spawn cache. Called by Tile when spawn changes.
   */
  void invalidateSpawns() { spawns_dirty_ = true; }

  /**
   * Iterate over all non-empty tiles with callback (const version).
   * Callback receives const Tile* since this is a const method.
   */
  template <typename Func> void forEachTile(Func &&callback) const {
    for (const auto &tile : tiles_) {
      if (tile) {
        callback(static_cast<const Tile *>(tile.get()));
      }
    }
  }

  /**
   * Iterate over all non-empty tiles with explicit local coordinates (const
   * version). PERFORMANCE: Avoids accessing tile memory to retrieve X/Y
   * coordinates (cache miss reduction). Callback receives (const Tile*, int
   * local_x, int local_y).
   */
  template <typename Func> void forEachTileWithCoords(Func &&callback) const {
    for (int i = 0; i < TILE_COUNT; ++i) {
      const auto &tile = tiles_[i];
      if (tile) {
        // TILE_COUNT is SIZE*SIZE. SIZE is 32.
        // Compiler will optimize division/modulo by constant power of 2 into
        // bit shifts/masks
        callback(static_cast<const Tile *>(tile.get()), i % SIZE, i / SIZE);
      }
    }
  }

  /**
   * Iterate over all non-empty tiles in DIAGONAL order (OTClient parity).
   * ISOMETRIC DEPTH: Tiles at NW are drawn first, tiles at SE drawn last.
   * This ensures correct painter algorithm depth ordering where SE tiles
   * visually overlap NW tiles.
   * Callback receives (const Tile*, int local_x, int local_y).
   */
  template <typename Func> void forEachTileDiagonal(Func &&callback) const {
    // OTClient diagonal iteration pattern (mapview.cpp:430-445)
    // Iterates in diagonal waves from top-left to bottom-right
    const int numDiagonals = SIZE + SIZE - 1;
    for (int diagonal = 0; diagonal < numDiagonals; ++diagonal) {
      int advance = (diagonal >= SIZE) ? (diagonal - SIZE + 1) : 0;
      for (int iy = diagonal - advance, ix = advance; iy >= 0 && ix < SIZE;
           --iy, ++ix) {
        const auto &tile = tiles_[iy * SIZE + ix];
        if (tile) {
          callback(static_cast<const Tile *>(tile.get()), ix, iy);
        }
      }
    }
  }

  /**
   * Iterate over non-empty tiles in DIAGONAL order restricted to a local region.
   * COMBINED OPTIMIZATION: Correct isometric depth + Viewport culling.
   *
   * @param min_x Start local x (inclusive)
   * @param min_y Start local y (inclusive)
   * @param max_x End local x (exclusive)
   * @param max_y End local y (exclusive)
   */
  template <typename Func>
  void forEachTileDiagonalInRegion(int min_x, int min_y, int max_x, int max_y,
                                   Func &&callback) const {
    // Clamp bounds to chunk size
    const int start_x = std::max(0, min_x);
    const int end_x = std::min(SIZE, max_x); // exclusive
    const int start_y = std::max(0, min_y);
    const int end_y = std::min(SIZE, max_y); // exclusive

    if (start_x >= end_x || start_y >= end_y)
      return;

    // Range of diagonals that can touch the region
    const int start_diag = start_x + start_y;
    const int end_diag = (end_x - 1) + (end_y - 1);

    for (int diagonal = start_diag; diagonal <= end_diag; ++diagonal) {
      // Determine the range of X for this diagonal within the region
      // ix must be >= start_x
      // ix must be >= diagonal - end_y + 1 (derived from iy < end_y)
      const int ix_min = std::max(start_x, diagonal - end_y + 1);

      // ix must be <= end_x - 1
      // ix must be <= diagonal - start_y (derived from iy >= start_y)
      const int ix_max = std::min(end_x - 1, diagonal - start_y);

      for (int ix = ix_min; ix <= ix_max; ++ix) {
        const int iy = diagonal - ix;
        const auto &tile = tiles_[iy * SIZE + ix];
        if (tile) {
          callback(static_cast<const Tile *>(tile.get()), ix, iy);
        }
      }
    }
  }

  /**
   * Iterate over non-empty tiles within a specific local region (const
   * version). PERFORMANCE: Significantly reduces iteration count for partially
   * visible chunks.
   * @param min_x Start local x (inclusive)
   * @param min_y Start local y (inclusive)
   * @param max_x End local x (exclusive)
   * @param max_y End local y (exclusive)
   */
  template <typename Func>
  void forEachTileInRegion(int min_x, int min_y, int max_x, int max_y,
                           Func &&callback) const {
    int start_x = std::max(0, min_x);
    int end_x = std::min(SIZE, max_x);
    int start_y = std::max(0, min_y);
    int end_y = std::min(SIZE, max_y);

    for (int y = start_y; y < end_y; ++y) {
      int row_offset = y * SIZE;
      for (int x = start_x; x < end_x; ++x) {
        const auto &tile = tiles_[row_offset + x];
        if (tile) {
          callback(static_cast<const Tile *>(tile.get()));
        }
      }
    }
  }

  /**
   * Iterate over all non-empty tiles with callback (mutable version).
   * Callback receives Tile* for modifying tiles.
   */
  template <typename Func> void forEachTileMutable(Func &&callback) {
    for (auto &tile : tiles_) {
      if (tile) {
        callback(tile.get());
      }
    }
  }

  /**
   * Check if chunk is empty (no tiles).
   */
  bool isEmpty() const { return non_empty_count_ == 0; }

  /**
   * Get number of non-empty tiles.
   */
  int getNonEmptyCount() const { return non_empty_count_; }

  /**
   * Check if chunk contains any spawns.
   * PERFORMANCE: Used to skip spawn iteration in renderer.
   */
  bool hasSpawns() const { return spawn_count_ > 0; }

  /**
   * Update spawn count (internal or for direct notification).
   * @param delta +1 or -1
   */
  void updateSpawnCount(int delta) { spawn_count_ += delta; }

  /**
   * Get number of creatures in this chunk.
   * PERFORMANCE: Used to skip spawn iteration in renderer.
   */
  int getCreatureCount() const { return creature_count_; }

  /**
   * Update creature count (called by Tile::setCreature).
   * @param delta +1 or -1
   */
  void updateCreatureCount(int delta) { creature_count_ += delta; }

  // STATIC MESH CACHING (Phase 2 Optimization)
  // When dirty=true, rebuildChunkMesh() will regenerate static geometry.
  // Static = ground tiles, walls, non-animated decorations.
  // Animated items rendered separately via SpriteBatch.

  bool isDirty() const { return dirty_; }
  void setDirty(bool d = true) { dirty_ = d; }

  // GPU mesh handle for static geometry (0 = no cache)
  uint32_t cached_static_mesh_id = 0;
  int static_vertex_count = 0;

private:
  static int toIndex(int local_x, int local_y) {
    return local_y * SIZE + local_x;
  }

  // Rebuild the spawn cache if dirty
  void updateSpawnCache() const;

  // Dense array of tiles - cache-friendly!
  std::array<std::unique_ptr<Tile>, TILE_COUNT> tiles_;
  int non_empty_count_ = 0;
  int spawn_count_ = 0;
  int creature_count_ = 0;
  bool dirty_ = true; // Needs mesh rebuild

  // Spawn Cache
  mutable std::vector<Tile *> spawn_tiles_;
  mutable bool spawns_dirty_ = true;
};

/**
 * Spatial index for a single floor level using chunk-based storage.
 */
class ChunkedFloor {
public:
  /**
   * Get tile at world coordinates.
   */
  Tile *getTile(int32_t x, int32_t y) const;

  /**
   * Get or create tile at world coordinates.
   */
  Tile *getOrCreateTile(int32_t x, int32_t y);

  /**
   * Set tile at world coordinates.
   */
  void setTile(int32_t x, int32_t y, std::unique_ptr<Tile> tile);

  /**
   * Remove tile at world coordinates.
   */
  std::unique_ptr<Tile> removeTile(int32_t x, int32_t y);

  /**
   * Get chunk at chunk coordinates (not world coordinates).
   */
  Chunk *getChunk(int32_t chunk_x, int32_t chunk_y) const;

  /**
   * Get or create chunk at chunk coordinates.
   */
  Chunk *getOrCreateChunk(int32_t chunk_x, int32_t chunk_y);

  /**
   * Get all chunks that intersect a world-coordinate bounding box.
   * Appends non-null chunks to the output vector.
   */
  void getChunksInRegion(int32_t min_x, int32_t min_y, int32_t max_x,
                         int32_t max_y, std::vector<Chunk *> &out_result) const;

  /**
   * Iterate over all tiles on this floor (const version).
   */
  template <typename Func> void forEachTile(Func &&callback) const {
    for (const auto &[key, chunk] : chunks_) {
      chunk->forEachTile(callback);
    }
  }

  /**
   * Iterate over all chunks on this floor.
   * Callback receives (const Chunk*).
   */
  template <typename Func> void forEachChunk(Func &&callback) const {
    for (const auto &[key, chunk] : chunks_) {
      callback(chunk.get());
    }
  }

  /**
   * Iterate over all tiles on this floor (mutable version).
   */
  template <typename Func> void forEachTileMutable(Func &&callback) {
    for (auto &[key, chunk] : chunks_) {
      chunk->forEachTileMutable(callback);
    }
  }

  /**
   * Get total tile count.
   */
  size_t getTileCount() const;

  /**
   * Clear all chunks and tiles.
   */
  void clear();

private:
  static uint64_t chunkKey(int32_t chunk_x, int32_t chunk_y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(chunk_x)) << 32) |
           static_cast<uint64_t>(static_cast<uint32_t>(chunk_y));
  }

  static void worldToChunk(int32_t world_x, int32_t world_y, int32_t &chunk_x,
                           int32_t &chunk_y, int &local_x, int &local_y) {
    // Bitwise operations handle negative 2's complement numbers correctly
    // (floor division semantics for shift, modulo semantics for mask)
    chunk_x = world_x >> 5; // Divide by 32 (floored)
    chunk_y = world_y >> 5;
    local_x = world_x & 0x1F; // Mod 32 (always positive 0-31)
    local_y = world_y & 0x1F;
  }

  // Sparse chunk storage - most of 60k x 60k map is empty
  std::unordered_map<uint64_t, std::unique_ptr<Chunk>> chunks_;
};

/**
 * Chunked map with spatial indexing for all 16 floor levels.
 *
 * PERFORMANCE vs std::unordered_map:
 * - O(1) chunk lookup instead of O(1) tile lookup
 * - Chunk contains 1024 tiles in contiguous memory
 * - Visible tile iteration is cache-friendly
 * - getChunksInRegion() enables viewport culling at chunk level
 */
class ChunkedMap {
public:
  static constexpr int16_t FLOOR_MIN = Config::Map::MIN_FLOOR;
  static constexpr int16_t FLOOR_MAX = Config::Map::MAX_FLOOR;
  static constexpr int16_t FLOOR_COUNT = FLOOR_MAX - FLOOR_MIN + 1;

  // ========== Tile Access ==========

  Tile *getTile(int32_t x, int32_t y, int16_t z) const;
  Tile *getTile(const Position &pos) const;

  Tile *getOrCreateTile(int32_t x, int32_t y, int16_t z);
  Tile *getOrCreateTile(const Position &pos);

  void setTile(int32_t x, int32_t y, int16_t z, std::unique_ptr<Tile> tile);
  void setTile(const Position &pos, std::unique_ptr<Tile> tile);

  std::unique_ptr<Tile> removeTile(int32_t x, int32_t y, int16_t z);
  std::unique_ptr<Tile> removeTile(const Position &pos);

  bool hasTile(const Position &pos) const;

  // ========== Chunk Access (for rendering) ==========

  /**
   * Get all chunks visible in a viewport region on a floor.
   * Appends non-null chunks to the output vector.
   * Use this in MapRenderer for cache-friendly tile iteration.
   */
  void getVisibleChunks(int32_t min_x, int32_t min_y, int32_t max_x,
                        int32_t max_y, int16_t floor,
                        std::vector<Chunk *> &out_result) const;

  /**
   * Get chunk directly from chunk coordinates.
   * PERFORMANCE: O(1) access to chunk for hot paths.
   */
  const Chunk *getChunk(int32_t chunk_x, int32_t chunk_y, int16_t z) const;

  /**
   * Notify map that a spawn was added or removed at position.
   * Updates the corresponding chunk's spawn count.
   */
  void notifySpawnChange(const Position &pos, bool added);

  // ========== Iteration ==========

  template <typename Func> void forEachTile(Func &&callback) const {
    for (const auto &floor : floors_) {
      floor.forEachTile(callback);
    }
  }

  /**
   * Iterate over all chunks in the map.
   * Callback receives (const Chunk* chunk, int16_t z).
   */
  template <typename Func> void forEachChunk(Func &&callback) const {
    for (int16_t z = FLOOR_MIN; z <= FLOOR_MAX; ++z) {
      floors_[z].forEachChunk([&](const Chunk *chunk) { callback(chunk, z); });
    }
  }

  template <typename Func> void forEachTileMutable(Func &&callback) {
    for (auto &floor : floors_) {
      floor.forEachTileMutable(callback);
    }
  }

  template <typename Func>
  void forEachTileOnFloor(int16_t floor, Func &&callback) const {
    if (floor >= FLOOR_MIN && floor <= FLOOR_MAX) {
      floors_[floor].forEachTile(callback);
    }
  }

  template <typename Func>
  void forEachTileOnFloorMutable(int16_t floor, Func &&callback) {
    if (floor >= FLOOR_MIN && floor <= FLOOR_MAX) {
      floors_[floor].forEachTileMutable(callback);
    }
  }

  // ========== Stats ==========

  size_t getTileCount() const;
  size_t getTileCount(int16_t floor) const;

  // ========== Management ==========

  void clear();

  /**
   * Create a deep copy of this map.
   * Used for operations that need to modify map data without affecting the
   * original (e.g., ID conversion for "Save As" operations).
   * @return New ChunkedMap with cloned tiles, towns, waypoints, and metadata
   */
  std::unique_ptr<ChunkedMap> clone() const;

  // ========== Metadata ==========

  void setSize(uint16_t width, uint16_t height);
  void setDescription(const std::string &description);
  void setFilename(const std::string &filename);
  void setName(const std::string &name);
  void setSpawnFile(const std::string &file) { spawn_file_ = file; }
  void setHouseFile(const std::string &file) { house_file_ = file; }

  uint16_t getWidth() const { return width_; }
  uint16_t getHeight() const { return height_; }
  const std::string &getDescription() const { return description_; }
  const std::string &getFilename() const { return filename_; }
  const std::string &getName() const { return name_; }
  const std::string &getSpawnFile() const { return spawn_file_; }
  const std::string &getHouseFile() const { return house_file_; }

  // ========== Creation ==========

  void createNew(uint16_t width, uint16_t height, uint32_t client_version);

  // ========== Towns & Waypoints ==========

  // Town and Waypoint data structures
  struct Town {
    uint32_t id;
    std::string name;
    Position temple_position;
  };

  struct Waypoint {
    std::string name;
    Position position;
  };

  void addTown(uint32_t id, const std::string &name,
               const Position &temple_pos);
  void addWaypoint(const std::string &name, const Position &pos);
  bool upsertWaypoint(const std::string &name, const Position &pos);
  bool removeWaypointAt(const Position &pos);
  bool removeWaypointByName(const std::string &name);

  // Town CRUD operations
  void removeTown(uint32_t id);
  void updateTown(uint32_t id, const std::string &name, const Position &temple);
  Town *getTown(uint32_t id);
  const Town *getTown(uint32_t id) const;
  uint32_t getNextTownId() const;
  bool hasTownWithHouses(uint32_t town_id) const;

  // ========== Houses ==========
  void addHouse(std::unique_ptr<House> house);
  House *getHouse(uint32_t id);
  const std::unordered_map<uint32_t, std::unique_ptr<House>> &
  getHouses() const {
    return houses_;
  }

  // ========== Version Info ==========
  struct MapVersion {
    uint32_t otbm_version = 0;
    uint32_t client_version = 0;
    uint32_t items_major_version = 0; // OTB major version from OTBM header
    uint32_t items_minor_version = 0; // OTB minor version from OTBM header
  };

  const MapVersion &getVersion() const { return version_; }
  void setVersion(const MapVersion &v) {
    version_ = v;
    markChanged();
  }

  // ========== Modification Tracking ==========
  bool hasChanges() const { return has_changes_; }
  void markChanged() {
    has_changes_ = true;
    revision_++;
  }
  void clearChanges() { has_changes_ = false; }

  /**
   * Get revision counter. Incremented on every modification.
   * Used by renderers to detect if cached geometry is stale.
   */
  uint32_t getRevision() const { return revision_; }

  const std::vector<Town> &getTowns() const { return towns_; }
  std::vector<Town> &getTowns() { return towns_; } // Mutable for dialog editing
  const std::vector<Waypoint> &getWaypoints() const { return waypoints_; }
  Waypoint *getWaypointAtMutable(const Position &pos) {
    uint64_t key = positionToKey(pos);
    auto it = waypoint_lookup_.find(key);
    if (it == waypoint_lookup_.end() || it->second >= waypoints_.size()) {
      return nullptr;
    }
    return &waypoints_[it->second];
  }

  /**
   * O(1) lookup of waypoint at a specific position.
   * @return Pointer to waypoint if one exists at this position, nullptr
   * otherwise.
   */
  const Waypoint *getWaypointAt(const Position &pos) const {
    uint64_t key = positionToKey(pos);
    auto it = waypoint_lookup_.find(key);
    if (it != waypoint_lookup_.end()) {
      size_t idx = it->second;
      if (idx < waypoints_.size()) {
        return &waypoints_[idx];
      }
    }
    return nullptr;
  }

private:
  std::array<ChunkedFloor, FLOOR_COUNT> floors_;

  // Metadata
  uint16_t width_ = 0;
  uint16_t height_ = 0;
  std::string description_;
  std::string filename_;
  std::string name_;
  std::string spawn_file_;
  std::string house_file_;
  uint32_t client_version_ = 0;

  // Towns & Waypoints
  std::vector<Town> towns_;
  std::vector<Waypoint> waypoints_;
  // O(1) position lookup. Stores an index into `waypoints_` to avoid pointer
  // invalidation when the vector reallocates.
  std::unordered_map<uint64_t, size_t> waypoint_lookup_;
  std::unordered_map<uint32_t, std::unique_ptr<House>> houses_;

  // Version and state
  MapVersion version_;
  bool has_changes_ = false;
  uint32_t revision_ = 0;

  static uint64_t positionToKey(const Position &pos) {
    // Use the safe 64-bit packing logic from Position
    // Supports negative coordinates and large maps (up to +/- 134 million)
    return pos.pack();
  }
};

} // namespace Domain
} // namespace MapEditor
