#pragma once
#include "Creature.h"
#include "Item.h"
#include "Position.h"
#include "Spawn.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace MapEditor::IO {
    struct InvalidZoneState;
}

namespace MapEditor {
namespace Domain {

class Chunk; // Forward declaration

/**
 * Tile flags for special properties
 */
enum class TileFlag : uint16_t {
  None = 0,
  ProtectionZone = 1 << 0,   // 0x0001
  Deprecated = 1 << 1,       // 0x0002 — reserved/deprecated bit
  NoPvp = 1 << 2,            // 0x0004
  NoLogout = 1 << 3,         // 0x0008
  PvpZone = 1 << 4,          // 0x0010
  Refresh = 1 << 5           // 0x0020
};

inline TileFlag operator|(TileFlag a, TileFlag b) {
  return static_cast<TileFlag>(static_cast<uint16_t>(a) |
                               static_cast<uint16_t>(b));
}

inline TileFlag operator&(TileFlag a, TileFlag b) {
  return static_cast<TileFlag>(static_cast<uint16_t>(a) &
                               static_cast<uint16_t>(b));
}

inline TileFlag &operator|=(TileFlag &a, TileFlag b) {
  a = a | b;
  return a;
}

inline TileFlag &operator&=(TileFlag &a, TileFlag b) {
  a = a & b;
  return a;
}

inline bool hasFlag(TileFlag flags, TileFlag flag) {
  return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(flag)) != 0;
}

/**
 * Represents a single tile in the map
 * Contains ground and stacked items
 */
class Tile {
public:
  Tile() = default;
  explicit Tile(const Position &pos);
  ~Tile();

  // Move semantics
  Tile(Tile &&other) noexcept = default;
  Tile &operator=(Tile &&other) noexcept = default;

  // No copy - tiles should be moved or cloned explicitly
  Tile(const Tile &) = delete;
  Tile &operator=(const Tile &) = delete;

  // Position
  const Position &getPosition() const { return position_; }
  void setPosition(const Position &pos) { position_ = pos; }

  int32_t getX() const { return position_.x; }
  int32_t getY() const { return position_.y; }
  int16_t getZ() const { return position_.z; }

  // Ground item
  const Item *getGround() const { return ground_.get(); }
  Item *getGround() { return ground_.get(); }
  void setGround(std::unique_ptr<Item> item);
  std::unique_ptr<Item> removeGround();
  bool hasGround() const { return ground_ != nullptr; }

  // Stacked items
  const std::vector<std::unique_ptr<Item>> &getItems() const { return items_; }
  size_t getItemCount() const { return items_.size(); }
  const Item *getItem(size_t index) const;
  Item *getItem(size_t index);
  void addItem(std::unique_ptr<Item> item); // Sorts based on item properties
  void addItemDirect(
      std::unique_ptr<Item> item); // Appends without sorting (for undo/redo)
  std::unique_ptr<Item> removeItem(size_t index);
  void clearItems();
  void swapItems(size_t index1, size_t index2); // For reorder operations

  /**
   * Remove all items matching a predicate.
   * Used by brush undraw operations to remove specific items.
   * @param predicate Function returning true for items to remove
   * @return Number of items removed
   */
  template <typename Predicate> size_t removeItemsIf(Predicate predicate) {
    size_t removed = 0;
    auto it = items_.begin();
    while (it != items_.end()) {
      if (predicate(it->get())) {
        it = items_.erase(it);
        ++removed;
      } else {
        ++it;
      }
    }
    if (removed > 0) {
      markDirty();
    }
    return removed;
  }

  // Check if tile is empty
  bool isEmpty() const { return !ground_ && items_.empty(); }

  // Flags
  TileFlag getFlags() const { return flags_; }
  void setFlags(TileFlag flags) { flags_ = flags; }
  void setFlags(uint32_t flags) { flags_ = static_cast<TileFlag>(flags); }
  bool hasFlag(TileFlag flag) const { return Domain::hasFlag(flags_, flag); }
  void addFlag(TileFlag flag) { flags_ |= flag; }
  void removeFlag(TileFlag flag);

  // Helper to mark parent chunk dirty
  void markDirty();

  // House association
  uint32_t getHouseId() const { return house_id_; }
  void setHouseId(uint32_t id) { house_id_ = id; }
  bool isHouseTile() const { return house_id_ != 0; }

  // Spawn association
  void setSpawn(std::unique_ptr<Spawn>
                    spawn); // Defined in Tile.cpp (needs Chunk definition)
  std::unique_ptr<Spawn> removeSpawn();
  const Spawn *getSpawn() const { return spawn_.get(); }
  Spawn *getSpawn() { return spawn_.get(); }
  bool hasSpawn() const { return spawn_ != nullptr; }

  // Creature on this tile (per-tile storage like RME)
  const Creature *getCreature() const { return creature_.get(); }
  Creature *getCreature() { return creature_.get(); }
  void setCreature(std::unique_ptr<Creature> creature); // Defined in Tile.cpp
  std::unique_ptr<Creature> removeCreature() { return std::move(creature_); }
  bool hasCreature() const { return creature_ != nullptr; }

  // Clone the tile
  std::unique_ptr<Tile> clone() const;

  // Hangable hook support (for doors/pictures)
  // Checks if any item on this tile provides HOOK_SOUTH or HOOK_EAST support
  bool hasHookSouth() const;
  bool hasHookEast() const;

  // Opaque OTBM data preservation for unknown attributes/nodes
  const IO::InvalidZoneState* getOpaqueData() const { return opaque_data_.get(); }
  IO::InvalidZoneState* getOpaqueData() { return opaque_data_.get(); }
  void setOpaqueData(std::unique_ptr<IO::InvalidZoneState> data);
  bool hasOpaqueData() const { return opaque_data_ != nullptr; }

  // Parent Chunk (Performance optimization for dirty tracking)
  void setParentChunk(Chunk *chunk) { parent_chunk_ = chunk; }
  Chunk *getParentChunk() const { return parent_chunk_; }

private:
  Position position_;
  std::unique_ptr<Item> ground_;
  std::vector<std::unique_ptr<Item>> items_;
  TileFlag flags_ = TileFlag::None;
  uint32_t house_id_ = 0;

  // Creature spawn attached to this tile (from OTBM_SPAWNS or RME spawns.xml)
  std::unique_ptr<Spawn> spawn_;

  // Creature on this tile (per-tile storage like RME)
  std::unique_ptr<Creature> creature_;

  // Opaque OTBM data for preserving unknown attributes/nodes
  std::unique_ptr<IO::InvalidZoneState> opaque_data_;

  // Parent chunk for dirty notification (not owned)
  Chunk *parent_chunk_ = nullptr;
};

} // namespace Domain
} // namespace MapEditor
