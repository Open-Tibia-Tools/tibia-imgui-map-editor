#pragma once

#include "BrushId.h"
#include "Domain/Outfit.h"
#include <cstdint>
#include <string>
#include <utility>

namespace MapEditor::Domain {
class ChunkedMap;
class Tile;
class Item;
struct Position;
} // namespace MapEditor::Domain

namespace MapEditor::Services {
class BrushSettingsService;
class ClientDataService;
} // namespace MapEditor::Services

namespace MapEditor::Brushes {

class BrushRegistry;

// ============================================================================
// Brush Type Enumeration
// ============================================================================

/**
 * Identifies the type of brush for filtering and specialized handling.
 * Extended to include all RME brush types for migration compatibility.
 */
enum class BrushType {
  Raw,            // Single item by ID
  Doodad,         // Decorations with variations/composites
  Ground,         // Ground tiles with auto-bordering
  Wall,           // Wall tiles with alignment
  WallDecoration, // Wall overlay decorations
  Table,          // Table-like objects with alignment
  Carpet,         // Carpet tiles with alignment
  Door,           // Door items (subset of wall)
  Creature,       // Creature placement
  Spawn,          // Spawn point placement
  House,          // House zone assignment
  HouseExit,      // House exit point
  Waypoint,       // Named waypoints
  Flag,           // Zone flags (PZ, noPVP, etc.)
  OptionalBorder, // Gravel/mountain optional borders
  Eraser,         // Removes items
  Placeholder     // For missing/undefined brushes
};

enum class BrushPreviewKind : uint8_t {
  None,
  ServerItem,
  ClientSprite,
  Creature,
  Symbolic,
};

struct BrushPreviewDescriptor {
  BrushPreviewKind kind = BrushPreviewKind::None;
  uint32_t numericId = 0;
  Domain::Outfit outfit {};
  BrushType symbolicType = BrushType::Placeholder;
  std::string label;

  [[nodiscard]] static BrushPreviewDescriptor serverItem(uint32_t itemId) {
    return {.kind = BrushPreviewKind::ServerItem, .numericId = itemId};
  }

  [[nodiscard]] static BrushPreviewDescriptor clientSprite(uint32_t spriteId) {
    return {.kind = BrushPreviewKind::ClientSprite, .numericId = spriteId};
  }

  [[nodiscard]] static BrushPreviewDescriptor creature(
      const Domain::Outfit &creatureOutfit) {
    return {.kind = BrushPreviewKind::Creature, .outfit = creatureOutfit};
  }

  [[nodiscard]] static BrushPreviewDescriptor symbolic(BrushType type,
                                                       std::string text = {}) {
    return {.kind = BrushPreviewKind::Symbolic,
            .symbolicType = type,
            .label = std::move(text)};
  }

  [[nodiscard]] bool isExplicit() const {
    return kind != BrushPreviewKind::None;
  }
};

[[nodiscard]] inline BrushPreviewDescriptor
defaultPreviewDescriptor(BrushType type, uint32_t lookId, std::string label) {
  if (lookId != 0) {
    return BrushPreviewDescriptor::serverItem(lookId);
  }
  return BrushPreviewDescriptor::symbolic(type, std::move(label));
}

// ============================================================================
// Draw Context
// ============================================================================

/**
 * Parameters passed to brush draw operations.
 */
struct DrawContext {
  int variation = 0;       // Which size/variant to use (for alternates)
  uint32_t modifiers = 0;  // GLFW modifier bitmask
  bool isDragging = false; // Part of a drag stroke
  bool specialAction = false; // wx-style single-tile special click behavior
  bool forcePlace = false; // Ignore blocking/duplicate checks
  Services::BrushSettingsService *brushSettings = nullptr; // For spawn settings
  Services::ClientDataService *clientData = nullptr;
  BrushRegistry *brushRegistry = nullptr;
  BrushId ownerBrushId = InvalidBrushId;
};

// ============================================================================
// IBrush Interface
// ============================================================================

/**
 * Abstract interface for all brush types.
 *
 * All brushes implement this interface, allowing BrushController to handle
 * them polymorphically. This replaces the old BrushDefinition struct approach.
 *
 * RME Compatibility:
 * - draw() matches RME's Brush::draw()
 * - undraw() matches RME's Brush::undraw()
 * - ownsItem() supports brush-specific erasing behavior
 */
class IBrush {
public:
  virtual ~IBrush() = default;

  // ─── Identity ─────────────────────────────────────────────────────────

  /**
   * Get the brush name (used for lookup and display).
   */
  virtual const std::string &getName() const = 0;

  /**
   * Get the brush type for filtering.
   */
  virtual BrushType getType() const = 0;

  /**
   * Get the preview sprite ID.
   */
  virtual uint32_t getLookId() const = 0;

  /**
   * Get explicit preview information for palette rendering.
   */
  virtual BrushPreviewDescriptor getPreviewDescriptor() const {
    return defaultPreviewDescriptor(getType(), getLookId(), getName());
  }

  // ─── Capabilities ─────────────────────────────────────────────────────

  /**
   * Check if the brush can draw at the given position.
   * Override to implement blocking checks, etc.
   */
  virtual bool canDraw(const Domain::ChunkedMap &map,
                       const Domain::Position &pos) const {
    return true;
  }

  /**
   * Whether the brush supports drag-painting.
   */
  virtual bool isDraggable() const { return true; }

  /**
   * Whether this brush should be shown in palette/context-discovery views.
   * Collection brushes and palette-bound brushes may opt in explicitly.
   */
  virtual bool visibleInPalette() const { return false; }

  /**
   * Mark the brush as visible in palette/discovery views.
   */
  virtual void flagAsVisible() {}

  /**
   * Whether this brush belongs to a collection tileset.
   */
  virtual bool hasCollection() const { return false; }

  /**
   * Mark the brush as part of a collection tileset.
   */
  virtual void setCollection() {}

  /**
   * Whether placing this brush should trigger border recalculation.
   */
  virtual bool needsBorderUpdate() const { return false; }

  // ─── Variations (for brushes with alternates) ─────────────────────────

  /**
   * Get the number of size variations available.
   * Returns 1 for brushes without alternates.
   */
  virtual size_t getMaxVariation() const { return 1; }

  /**
   * Set the current variation index.
   */
  virtual void setVariation(size_t /*index*/) {}

  // ─── Core Operations ──────────────────────────────────────────────────

  /**
   * Draw the brush at the given tile.
   * This is called when the user left-clicks or drags.
   *
   * @param map The map being modified
   * @param tile The target tile
   * @param ctx Draw parameters (variation, dragging state, etc.)
   */
  virtual void draw(Domain::ChunkedMap &map, Domain::Tile *tile,
                    const DrawContext &ctx) = 0;

  /**
   * Undraw (erase) the brush from the given tile.
   * This is called when the user right-clicks.
   * The brush should remove items it "owns" from the tile.
   *
   * @param map The map being modified
   * @param tile The target tile
   */
  virtual void undraw(Domain::ChunkedMap &map, Domain::Tile *tile) = 0;

  // ─── Ownership (for undraw-by-brush-type) ─────────────────────────────

  /**
   * Check if this brush "owns" the given item.
   * Used by undraw() to determine which items to remove.
   *
   * For raw brushes: item ID matches brush's item ID.
   */
  virtual bool ownsItem(const Domain::Item *item) const { return false; }
};

} // namespace MapEditor::Brushes
