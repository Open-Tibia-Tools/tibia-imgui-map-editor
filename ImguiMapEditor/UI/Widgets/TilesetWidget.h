#pragma once
#include "../../Brushes/BrushRegistry.h"
#include "../../Domain/Tileset/Tileset.h"
#include "../../Domain/Tileset/TilesetRegistry.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../Utils/BrushPreviewResolver.h"

namespace MapEditor {

namespace Services {
class ClientDataService;
class SpriteManager;
} // namespace Services

namespace Brushes {
class BrushController;
}

namespace UI {

/**
 * Callback when a brush is selected from the palette.
 * @param itemId Server ID of selected item (for RAW brush)
 * @param brushName Name of selected brush (empty for RAW)
 */
using BrushSelectedCallback =
    std::function<void(uint32_t itemId, const std::string &brushName)>;

/**
 * Widget to display tileset palettes with brush/item selection.
 *
 * Hierarchy:
 * 1. Tileset Dropdown (all available tilesets)
 * 2. Grid (Items/Brushes from selected tileset)
 */
class TilesetWidget {
public:
  TilesetWidget();
  ~TilesetWidget();

  /**
   * Initialize with required services.
   */
  void initialize(Services::ClientDataService *clientData,
                  Services::SpriteManager *spriteManager,
                  Brushes::BrushController *brushController,
                  Domain::Tileset::TilesetRegistry &tilesetRegistry);

  /**
   * Set callback for brush selection.
   */
  void setOnBrushSelected(BrushSelectedCallback callback) {
    onBrushSelected_ = callback;
  }

  /**
   * Render the widget.
   * @param p_visible Optional visibility pointer for ImGui
   */
  void render(bool *p_visible = nullptr);

  // Window visibility
  bool isVisible() const { return visible_; }
  void setVisible(bool visible) { visible_ = visible; }
  void toggleVisible() { visible_ = !visible_; }

  // Icon size control (32-128 pixels)
  float getIconSize() const { return iconSize_; }
  void setIconSize(float size);

private:
  void renderTilesetDropdown();
  void renderItemGrid();
  void renderIconSizeSlider();
  void syncActiveBrushSelection();
  [[nodiscard]] Utils::ResolvedBrushPreview
  getBrushPreview(const Brushes::IBrush *brush) const;
  [[nodiscard]] const Domain::Tileset::Tileset *
  findTilesetForBrush(const Brushes::IBrush *brush) const;
  void updateSelectedTilesetIndex();

  // Services (non-owning)
  Services::ClientDataService *clientData_ = nullptr;
  Services::SpriteManager *spriteManager_ = nullptr;
  Brushes::BrushController *brushController_ = nullptr;
  Domain::Tileset::TilesetRegistry *tilesetRegistry_ = nullptr;
  const Domain::Tileset::Tileset *currentTileset_ = nullptr;

  // State
  bool visible_ = true;
  float iconSize_ = 48.0f;
  char filterBuffer_[128] = "";
  bool filterDirty_ = true;
  std::vector<const Brushes::IBrush *> filteredBrushes_;

  // Selection
  int selectedTilesetIdx_ = 0;
  const Brushes::IBrush *selectedBrush_ = nullptr; // Currently highlighted brush
  const Brushes::IBrush *syncedActiveBrush_ = nullptr;

  // Callback
  BrushSelectedCallback onBrushSelected_;
};

} // namespace UI
} // namespace MapEditor
