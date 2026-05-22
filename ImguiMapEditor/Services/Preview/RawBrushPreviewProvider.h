#pragma once
#include "IPreviewProvider.h"

namespace MapEditor {
namespace Services {
class BrushSettingsService;
} // namespace Services
} // namespace MapEditor

namespace MapEditor {
namespace Services {
namespace Preview {

/**
 * Preview provider for RAW brush (item placement).
 *
 * Generates preview tiles based on current brush settings:
 * - Uses BrushSettingsService to get brush positions
 * - Each position gets a copy of the item
 * - Supports regeneration when brush size/shape changes
 */
class RawBrushPreviewProvider : public IPreviewProvider {
public:
  /**
   * Create RAW brush preview.
   * @param itemId Server ID of item to preview
   * @param subtype Optional subtype (stack count, fluid type)
   * @param brushSettings Brush settings service for size/shape (optional)
   */
  explicit RawBrushPreviewProvider(
      uint32_t itemId, uint16_t subtype = 0,
      BrushSettingsService *brushSettings = nullptr);

  /**
   * Set brush settings service for size/shape.
   * Called if settings weren't available at construction.
   */
  void setBrushSettingsService(BrushSettingsService *service) {
    brushSettings_ = service;
    needsRegen_ = true;
  }

  // IPreviewProvider interface
  bool isActive() const override;
  Domain::Position getAnchorPosition() const override;
  const std::vector<PreviewTileData> &getTiles() const override;
  PreviewBounds getBounds() const override;
  void updateCursorPosition(const Domain::Position &cursor) override;
  PreviewStyle getStyle() const override;

  // Regeneration support for brush size changes
  bool needsRegeneration() const override { return needsRegen_; }
  void regenerate() override;

  /**
   * Get the item ID being previewed.
   */
  uint32_t getItemId() const { return itemId_; }

  /**
   * Get the subtype being previewed.
   */
  uint16_t getSubtype() const { return subtype_; }

  /**
   * Mark preview as needing regeneration.
   * Called by BrushSettingsService when settings change.
   */
  void markNeedsRegeneration() { needsRegen_ = true; }

private:
  uint32_t itemId_;
  uint16_t subtype_;
  BrushSettingsService *brushSettings_ = nullptr;
  Domain::Position anchor_{0, 0, 0};
  std::vector<PreviewTileData> tiles_;
  PreviewBounds bounds_;
  mutable bool needsRegen_ = false;

  // Cached settings for change detection (actual offsets, not just count)
  mutable std::vector<std::pair<int, int>> cachedOffsets_;

  void buildPreview();
  bool checkSettingsChanged() const;
};

} // namespace Preview
} // namespace Services
} // namespace MapEditor
