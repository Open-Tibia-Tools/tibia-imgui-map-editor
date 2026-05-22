#pragma once
#include "Brushes/Core/IBrush.h"
#include "Brushes/Core/BrushId.h"
#include "Brushes/Data/BorderBlock.h"
#include "Domain/Position.h"
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MapEditor::Domain {
class Item;
class Tile;
class ChunkedMap;
}

namespace MapEditor::Services {
class ClientDataService;
}

namespace MapEditor::Brushes {

/**
 * Central registry for all brushes.
 * Manages brush ownership and lookup.
 *
 * NOTE: Tileset management is now handled by Domain::Tileset::TilesetRegistry.
 * This class only handles brush instances.
 */
class BrushRegistry {
public:
  struct BorderItemMetadata {
    uint16_t group = 0;
    EdgeType alignment = EdgeType::None;
  };

  BrushRegistry() = default;
  ~BrushRegistry() = default;

  // Non-copyable
  BrushRegistry(const BrushRegistry &) = delete;
  BrushRegistry &operator=(const BrushRegistry &) = delete;

  // ========== Brush Management ==========

  /**
   * Add a brush to the registry (takes ownership).
   */
  void addBrush(std::unique_ptr<IBrush> brush);
  void registerExternalBrush(IBrush *brush);

  /**
   * Get a brush by name. Returns nullptr if not found.
   */
  IBrush *getBrush(const std::string &name) const;

  /**
   * Get or create a RAW brush for an item ID.
   * RAW brushes are created on-demand and cached.
   */
  IBrush *getOrCreateRAWBrush(uint16_t itemId);

  void setClientDataService(Services::ClientDataService *clientData);
  Services::ClientDataService *getClientDataService() const {
    return clientData_;
  }

  void registerItemBinding(uint16_t itemId, IBrush *brush);
  void registerCreatureBinding(const std::string &creatureName, IBrush *brush);
  IBrush *getBrushForItem(uint16_t itemId) const;
  std::vector<IBrush *> getBrushesForItem(uint16_t itemId) const;
  IBrush *getBrushForCreature(const std::string &creatureName) const;
  IBrush *getOrCreatePlaceholderBrush(const std::string &name);
  IBrush *resolveBrushForTile(const Domain::Tile &tile) const;
  BrushId getBrushId(const IBrush *brush) const;
  IBrush *getBrushById(BrushId brushId) const;

  std::unique_ptr<Domain::Item> createItem(uint16_t itemId,
                                           uint16_t subtype = 1) const;

  void registerBorderTemplate(uint32_t id, BorderBlock border);
  const BorderBlock *getBorderTemplate(uint32_t id) const;
  const BorderItemMetadata *getBorderItemMetadata(uint16_t itemId) const;

  /**
   * Clear all brushes from the registry.
   */
  void clear();

  std::vector<IBrush *> getAllBrushes() const;

private:
  static std::string normalizeKey(const std::string &value);

  // Brush ownership
  std::map<std::string, std::unique_ptr<IBrush>> named_brushes_;
  std::map<uint16_t, std::unique_ptr<IBrush>> raw_brushes_; // ItemID -> Brush
  std::map<std::string, std::unique_ptr<IBrush>> placeholder_brushes_;
  std::vector<IBrush *> external_brushes_;
  std::unordered_map<uint16_t, IBrush *> item_bindings_;
  std::unordered_map<uint16_t, std::vector<IBrush *>> item_bindings_all_;
  std::unordered_map<std::string, IBrush *> creature_bindings_;
  std::unordered_map<uint32_t, BorderBlock> border_templates_;
  std::unordered_map<uint16_t, BorderItemMetadata> border_item_metadata_;
  std::unordered_map<const IBrush *, BrushId> brush_ids_;
  std::unordered_map<BrushId, IBrush *> brushes_by_id_;
  std::unordered_map<std::string, BrushId> brush_ids_by_name_;
  BrushId nextBrushId_ = 1;
  Services::ClientDataService *clientData_ = nullptr;
};

} // namespace MapEditor::Brushes
