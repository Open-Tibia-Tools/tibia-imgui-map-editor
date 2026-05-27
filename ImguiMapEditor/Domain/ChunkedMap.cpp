#include "ChunkedMap.h"
#include <limits>
#include <algorithm>
#include <ranges>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Domain {

// ========== Chunk Implementation ==========

Tile *Chunk::getTile(int local_x, int local_y) const {
  if (local_x < 0 || local_x >= SIZE || local_y < 0 || local_y >= SIZE) {
    return nullptr;
  }
  return tiles_[toIndex(local_x, local_y)].get();
}

void Chunk::setTile(int local_x, int local_y, std::unique_ptr<Tile> tile) {
  if (local_x < 0 || local_x >= SIZE || local_y < 0 || local_y >= SIZE) {
    return;
  }

  int idx = toIndex(local_x, local_y);

  // Update count
  const bool had_tile = tiles_[idx] != nullptr;
  const bool has_tile = tile != nullptr;

  if (had_tile != has_tile) {
    non_empty_count_ += has_tile ? 1 : -1;
  }

  // Update spawn_count_
  // 1. Remove old tile spawn contribution
  if (tiles_[idx] && tiles_[idx]->hasSpawn()) {
    spawn_count_--;
  }
  // 2. Add new tile spawn contribution
  if (tile && tile->hasSpawn()) {
    spawn_count_++;
  }

  // Update creature_count_
  // 1. Remove old tile creature contribution
  if (tiles_[idx] && tiles_[idx]->hasCreature()) {
    creature_count_--;
  }
  // 2. Add new tile creature contribution
  if (tile && tile->hasCreature()) {
    creature_count_++;
  }

  if (tile) {
    tile->setParentChunk(this);
  }

  tiles_[idx] = std::move(tile);
  invalidateSpawns();
  setDirty(true);
}

std::unique_ptr<Tile> Chunk::removeTile(int local_x, int local_y) {
  if (local_x < 0 || local_x >= SIZE || local_y < 0 || local_y >= SIZE) {
    return nullptr;
  }

  int idx = toIndex(local_x, local_y);

  if (tiles_[idx]) {
    non_empty_count_--;
    invalidateSpawns();
    if (tiles_[idx]->hasSpawn())
      spawn_count_--;
    if (tiles_[idx]->hasCreature())
      creature_count_--;
    setDirty(true);
    return std::move(tiles_[idx]);
  }
  return nullptr;
}

std::vector<Tile *> Chunk::getNonEmptyTiles() const {
  std::vector<Tile *> result;
  result.reserve(non_empty_count_);

  for (const auto &tile : tiles_) {
    if (tile) {
      result.push_back(tile.get());
    }
  }

  return result;
}

const std::vector<Tile *> &Chunk::getSpawnTiles() const {
  if (spawns_dirty_) {
    updateSpawnCache();
  }
  return spawn_tiles_;
}

void Chunk::updateSpawnCache() const {
  spawn_tiles_.clear();
  for (const auto &tile : tiles_) {
    if (tile && tile->hasSpawn()) {
      spawn_tiles_.push_back(tile.get());
    }
  }
  spawns_dirty_ = false;
}

// ========== ChunkedFloor Implementation ==========

Tile *ChunkedFloor::getTile(int32_t x, int32_t y) const {
  int32_t chunk_x, chunk_y;
  int local_x, local_y;
  worldToChunk(x, y, chunk_x, chunk_y, local_x, local_y);

  Chunk *chunk = getChunk(chunk_x, chunk_y);
  if (!chunk) {
    return nullptr;
  }

  return chunk->getTile(local_x, local_y);
}

Tile *ChunkedFloor::getOrCreateTile(int32_t x, int32_t y) {
  int32_t chunk_x, chunk_y;
  int local_x, local_y;
  worldToChunk(x, y, chunk_x, chunk_y, local_x, local_y);

  Chunk *chunk = getOrCreateChunk(chunk_x, chunk_y);
  if (!chunk) {
    return nullptr;
  }

  Tile *tile = chunk->getTile(local_x, local_y);
  if (tile) {
    return tile;
  }

  // Create new tile
  // Use constructor instead of designated initializer as Position is not an aggregate
  Position pos(x, y, 0);

  auto new_tile = std::make_unique<Tile>(pos);
  Tile *ptr = new_tile.get();
  chunk->setTile(local_x, local_y, std::move(new_tile));

  return ptr;
}

void ChunkedFloor::setTile(int32_t x, int32_t y, std::unique_ptr<Tile> tile) {
  int32_t chunk_x, chunk_y;
  int local_x, local_y;
  worldToChunk(x, y, chunk_x, chunk_y, local_x, local_y);

  if (tile) {
    Chunk *chunk = getOrCreateChunk(chunk_x, chunk_y);
    if (chunk) {
      chunk->setTile(local_x, local_y, std::move(tile));
    }
  } else {
    // Removing tile
    Chunk *chunk = getChunk(chunk_x, chunk_y);
    if (chunk) {
      chunk->setTile(local_x, local_y, nullptr);
    }
  }
}

std::unique_ptr<Tile> ChunkedFloor::removeTile(int32_t x, int32_t y) {
  int32_t chunk_x, chunk_y;
  int local_x, local_y;
  worldToChunk(x, y, chunk_x, chunk_y, local_x, local_y);

  Chunk *chunk = getChunk(chunk_x, chunk_y);
  if (!chunk) {
    return nullptr;
  }

  return chunk->removeTile(local_x, local_y);
}

Chunk *ChunkedFloor::getChunk(int32_t chunk_x, int32_t chunk_y) const {
  uint64_t key = chunkKey(chunk_x, chunk_y);
  auto it = chunks_.find(key);
  if (it != chunks_.end()) {
    return it->second.get();
  }
  return nullptr;
}

Chunk *ChunkedFloor::getOrCreateChunk(int32_t chunk_x, int32_t chunk_y) {
  uint64_t key = chunkKey(chunk_x, chunk_y);
  auto it = chunks_.find(key);
  if (it != chunks_.end()) {
    return it->second.get();
  }

  // Create new chunk
  auto chunk = std::make_unique<Chunk>();
  chunk->world_x = chunk_x * Chunk::SIZE;
  chunk->world_y = chunk_y * Chunk::SIZE;

  Chunk *ptr = chunk.get();
  chunks_[key] = std::move(chunk);

  return ptr;
}

void ChunkedFloor::getChunksInRegion(int32_t min_x, int32_t min_y,
                                     int32_t max_x, int32_t max_y,
                                     std::vector<Chunk *> &out_result) const {
  // Convert to chunk coordinates
  int32_t min_chunk_x = min_x >> 5; // floor(min_x / 32)
  int32_t min_chunk_y = min_y >> 5;
  int32_t max_chunk_x = max_x >> 5;
  int32_t max_chunk_y = max_y >> 5;

  // Validate bounds to prevent logic errors (inverted bounds)
  // If min > max, the region is empty.
  if (min_chunk_x > max_chunk_x || min_chunk_y > max_chunk_y) {
    return;
  }

  // Reserve approximate capacity
  // Note: bounds validation above ensures (max - min + 1) is always positive
  // (>= 1) Cast to size_t BEFORE multiplication to prevent signed integer
  // overflow with large coordinate ranges.
  size_t width = static_cast<size_t>(max_chunk_x - min_chunk_x + 1);
  size_t height = static_cast<size_t>(max_chunk_y - min_chunk_y + 1);

  // Check for potential overflow in multiplication (width * height)
  // If width * height would overflow size_t, we skip reservation to avoid
  // bad_alloc. This is extremely unlikely with realistic map sizes but robust
  // against edge cases.
  if (width > 0 && height > std::numeric_limits<size_t>::max() / width) {
    // Overflow would occur: skip reserve, vector will grow automatically if
    // needed
  } else {
    size_t region_area = width * height;
    size_t total_chunks = chunks_.size();

    // Optimization: Don't reserve more than the total number of existing chunks
    size_t reserve_count = std::min(region_area, total_chunks);

    if (out_result.capacity() < out_result.size() + reserve_count) {
      out_result.reserve(out_result.size() + reserve_count);
    }

    // BOLT OPTIMIZATION: Hybrid Iteration Strategy
    // If the query region (viewport) is significantly larger than the number of
    // populated chunks, it's faster to iterate the sparse map of existing
    // chunks than to check every coordinate in the dense grid. Factor 2
    // accounts for unordered_map iteration overhead vs hash lookup.
    if (total_chunks * 2 < region_area) {
      // Sparse Iteration: O(TotalChunks)
      // C++20: Iterate over values (chunks) directly using ranges
      for (const auto &chunk : chunks_ | std::views::values) {
        int32_t cx = chunk->world_x >> 5;
        int32_t cy = chunk->world_y >> 5;

        // Simple bounds check
        if (cx >= min_chunk_x && cx <= max_chunk_x && cy >= min_chunk_y &&
            cy <= max_chunk_y) {
          if (!chunk->isEmpty()) {
            out_result.push_back(chunk.get());
          }
        }
      }
    } else {
      // Dense Iteration: O(RegionArea)
      // C++20: Use iota views for coordinate ranges
      auto y_range = std::views::iota(min_chunk_y, max_chunk_y + 1);
      auto x_range = std::views::iota(min_chunk_x, max_chunk_x + 1);

      for (int32_t cy : y_range) {
        for (int32_t cx : x_range) {
          Chunk *chunk = getChunk(cx, cy);
          if (chunk && !chunk->isEmpty()) {
            out_result.push_back(chunk);
          }
        }
      }
    }
  }
}

size_t ChunkedFloor::getTileCount() const {
  size_t count = 0;
  for (const auto &[key, chunk] : chunks_) {
    count += chunk->getNonEmptyCount();
  }
  return count;
}

void ChunkedFloor::clear() { chunks_.clear(); }

// ========== ChunkedMap Implementation ==========

Tile *ChunkedMap::getTile(int32_t x, int32_t y, int16_t z) const {
  if (z < FLOOR_MIN || z > FLOOR_MAX) {
    return nullptr;
  }
  return floors_[z].getTile(x, y);
}

Tile *ChunkedMap::getTile(const Position &pos) const {
  return getTile(pos.x, pos.y, pos.z);
}

Tile *ChunkedMap::getOrCreateTile(int32_t x, int32_t y, int16_t z) {
  if (z < FLOOR_MIN || z > FLOOR_MAX) {
    return nullptr;
  }

  Tile *tile = floors_[z].getOrCreateTile(x, y);
  if (tile) {
    tile->setPosition(Position(x, y, z));
  }
  return tile;
}

Tile *ChunkedMap::getOrCreateTile(const Position &pos) {
  return getOrCreateTile(pos.x, pos.y, pos.z);
}

void ChunkedMap::setTile(int32_t x, int32_t y, int16_t z,
                         std::unique_ptr<Tile> tile) {
  if (z < FLOOR_MIN || z > FLOOR_MAX) {
    return;
  }

  if (tile) {
    tile->setPosition(Position(x, y, z));
  }

  floors_[z].setTile(x, y, std::move(tile));
}

void ChunkedMap::setTile(const Position &pos, std::unique_ptr<Tile> tile) {
  setTile(pos.x, pos.y, pos.z, std::move(tile));
}

std::unique_ptr<Tile> ChunkedMap::removeTile(int32_t x, int32_t y, int16_t z) {
  if (z < FLOOR_MIN || z > FLOOR_MAX) {
    return nullptr;
  }
  return floors_[z].removeTile(x, y);
}

std::unique_ptr<Tile> ChunkedMap::removeTile(const Position &pos) {
  return removeTile(pos.x, pos.y, pos.z);
}

bool ChunkedMap::hasTile(const Position &pos) const {
  return getTile(pos) != nullptr;
}

void ChunkedMap::getVisibleChunks(int32_t min_x, int32_t min_y, int32_t max_x,
                                  int32_t max_y, int16_t floor,
                                  std::vector<Chunk *> &out_result) const {
  if (floor < FLOOR_MIN || floor > FLOOR_MAX) {
    return;
  }
  floors_[floor].getChunksInRegion(min_x, min_y, max_x, max_y, out_result);
}

const Chunk *ChunkedMap::getChunk(int32_t chunk_x, int32_t chunk_y, int16_t z) const {
  if (z < FLOOR_MIN || z > FLOOR_MAX) {
    return nullptr;
  }
  return floors_[z].getChunk(chunk_x, chunk_y);
}

void ChunkedMap::notifySpawnChange(const Position &pos, bool added) {
  if (pos.z < FLOOR_MIN || pos.z > FLOOR_MAX)
    return;

  Tile *tile = getTile(pos);
  if (!tile)
    return; // Should not happen for added=true

  // We can access the chunk via the floor
  int32_t chunk_x = pos.x / Chunk::SIZE;
  int32_t chunk_y = pos.y / Chunk::SIZE;

  // Handle negative coords for chunk index
  if (pos.x < 0 && (pos.x % Chunk::SIZE) != 0)
    chunk_x--;
  if (pos.y < 0 && (pos.y % Chunk::SIZE) != 0)
    chunk_y--;

  Chunk *chunk = floors_[pos.z].getChunk(chunk_x, chunk_y);
  if (chunk) {
    chunk->updateSpawnCount(added ? 1 : -1);
  }
}

size_t ChunkedMap::getTileCount() const {
  size_t count = 0;
  for (const auto &floor : floors_) {
    count += floor.getTileCount();
  }
  return count;
}

size_t ChunkedMap::getTileCount(int16_t floor) const {
  if (floor < FLOOR_MIN || floor > FLOOR_MAX) {
    return 0;
  }
  return floors_[floor].getTileCount();
}

void ChunkedMap::clear() {
  for (auto &floor : floors_) {
    floor.clear();
  }

  width_ = 0;
  height_ = 0;
  description_.clear();
  filename_.clear();
  name_.clear();
  spawn_file_.clear();
  house_file_.clear();
  client_version_ = 0;
  towns_.clear();
  waypoints_.clear();
  waypoint_lookup_.clear();
  houses_.clear();
  version_ = {};
  has_changes_ = false;
}

std::unique_ptr<ChunkedMap> ChunkedMap::clone() const {
  auto cloned = std::make_unique<ChunkedMap>();

  // Copy metadata
  cloned->width_ = width_;
  cloned->height_ = height_;
  cloned->description_ = description_;
  cloned->filename_ = filename_;
  cloned->name_ = name_;
  cloned->spawn_file_ = spawn_file_;
  cloned->house_file_ = house_file_;
  cloned->client_version_ = client_version_;
  cloned->version_ = version_;

  // Deep copy all tiles using Tile::clone()
  for (int16_t z = FLOOR_MIN; z <= FLOOR_MAX; ++z) {
    floors_[z].forEachTile([&](const Tile *tile) {
      if (tile) {
        auto cloned_tile = tile->clone();
        cloned->setTile(tile->getPosition(), std::move(cloned_tile));
      }
    });
  }

  // Copy towns
  cloned->towns_ = towns_;

  // Copy waypoints
  cloned->waypoints_ = waypoints_;
  cloned->waypoint_lookup_ = waypoint_lookup_;

  // Deep copy houses
  for (const auto &[id, house] : houses_) {
    if (house) {
      auto cloned_house = std::make_unique<House>(house->id);
      cloned_house->name = house->name;
      cloned_house->town_id = house->town_id;
      cloned_house->rent = house->rent;
      cloned_house->entry_position = house->entry_position;
      cloned_house->is_guildhall = house->is_guildhall;
      cloned->houses_[id] = std::move(cloned_house);
    }
  }

  cloned->has_changes_ = false; // Clone starts as unmodified

  return cloned;
}

// ========== Metadata ==========

void ChunkedMap::setSize(uint16_t width, uint16_t height) {
  width_ = width;
  height_ = height;
  markChanged();
}

void ChunkedMap::setDescription(const std::string &description) {
  description_ = description;
  markChanged();
}

void ChunkedMap::setFilename(const std::string &filename) {
  filename_ = filename;
}

void ChunkedMap::setName(const std::string &name) {
  name_ = name;
  markChanged();
}

// ========== Creation ==========

void ChunkedMap::createNew(uint16_t width, uint16_t height,
                           uint32_t client_version) {
  createNew(width, height, client_version, 2, 0, 0, "");
}

void ChunkedMap::createNew(uint16_t width, uint16_t height,
                           uint32_t client_version, uint32_t otbm_version,
                           uint32_t items_major, uint32_t items_minor,
                           const std::string &description) {
  clear();
  width_ = width;
  height_ = height;
  client_version_ = client_version;
  version_.client_version = client_version;
  version_.otbm_version = otbm_version;
  version_.items_major_version = items_major;
  version_.items_minor_version = items_minor;
  description_ = description;
  name_ = "New Map";
  has_changes_ = true;
}

// ========== Towns & Waypoints ==========

void ChunkedMap::addTown(uint32_t id, const std::string &name,
                         const Position &temple_pos) {
  towns_.push_back({id, name, temple_pos});
  markChanged();
}

void ChunkedMap::removeTown(uint32_t id) {
  // Use C++20 std::erase_if for clean removal
  if (std::erase_if(towns_, [id](const Town &t) { return t.id == id; }) > 0) {
    markChanged();
  }
}

void ChunkedMap::updateTown(uint32_t id, const std::string &name,
                            const Position &temple) {
  // Use C++20 ranges for clearer intent
  auto it = std::ranges::find(towns_, id, &Town::id);
  if (it != towns_.end()) {
    it->name = name;
    it->temple_position = temple;
    markChanged();
  }
}

ChunkedMap::Town *ChunkedMap::getTown(uint32_t id) {
  auto it = std::ranges::find(towns_, id, &Town::id);
  return (it != towns_.end()) ? &(*it) : nullptr;
}

const ChunkedMap::Town *ChunkedMap::getTown(uint32_t id) const {
  auto it = std::ranges::find(towns_, id, &Town::id);
  return (it != towns_.end()) ? &(*it) : nullptr;
}

uint32_t ChunkedMap::getNextTownId() const {
  if (towns_.empty()) {
    return 1;
  }
  // Use C++20 ranges::max_element with projection
  return std::ranges::max_element(towns_, std::ranges::less{}, &Town::id)->id +
         1;
}

bool ChunkedMap::hasTownWithHouses(uint32_t town_id) const {
  // Use C++20 ranges::any_of
  return std::ranges::any_of(houses_ | std::views::values,
                             [town_id](const auto &house) {
                               return house && house->town_id == town_id;
                             });
}

void ChunkedMap::addWaypoint(const std::string &name, const Position &pos) {
  waypoints_.push_back({name, pos});
  // Add to O(1) lookup map - index of the waypoint we just added
  uint64_t key = positionToKey(pos);
  waypoint_lookup_[key] = waypoints_.size() - 1;
  markChanged();
}

void ChunkedMap::addHouse(std::unique_ptr<House> house) {
  if (house) {
    houses_[house->id] = std::move(house);
    markChanged();
  }
}

House *ChunkedMap::getHouse(uint32_t id) {
  auto it = houses_.find(id);
  if (it != houses_.end()) {
    return it->second.get();
  }
  return nullptr;
}

} // namespace Domain
} // namespace MapEditor
