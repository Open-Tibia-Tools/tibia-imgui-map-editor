#include "RawBrushPreviewProvider.h"
#include "Services/BrushSettingsService.h"

namespace MapEditor {
namespace Services {
namespace Preview {

namespace {

[[nodiscard]] bool containsOffset(
    std::span<const std::pair<int, int>> offsets, int x, int y) {
  for (const auto &[dx, dy] : offsets) {
    if (dx == x && dy == y) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::vector<std::pair<int, int>>
buildPreviewOffsets(const BrushSettingsService *brushSettings) {
  if (!brushSettings) {
    return {{0, 0}};
  }

  auto offsets = brushSettings->getBrushOffsets();
  if (!brushSettings->getPreviewBorder() || offsets.size() <= 1) {
    return offsets;
  }

  std::vector<std::pair<int, int>> borderOffsets;
  borderOffsets.reserve(offsets.size());
  for (const auto &[dx, dy] : offsets) {
    const bool isPerimeter =
        !containsOffset(offsets, dx - 1, dy) ||
        !containsOffset(offsets, dx + 1, dy) ||
        !containsOffset(offsets, dx, dy - 1) ||
        !containsOffset(offsets, dx, dy + 1);
    if (isPerimeter) {
      borderOffsets.emplace_back(dx, dy);
    }
  }

  return borderOffsets.empty() ? offsets : borderOffsets;
}

} // namespace

RawBrushPreviewProvider::RawBrushPreviewProvider(
    uint32_t itemId, uint16_t subtype, BrushSettingsService *brushSettings)
    : itemId_(itemId), subtype_(subtype), brushSettings_(brushSettings) {
  buildPreview();
}

bool RawBrushPreviewProvider::isActive() const { return itemId_ > 0; }

Domain::Position RawBrushPreviewProvider::getAnchorPosition() const {
  return anchor_;
}

bool RawBrushPreviewProvider::checkSettingsChanged() const {
  if (!brushSettings_) {
    return false;
  }
  // Compare effective preview offsets so Preview Border changes regenerate too.
  auto offsets = buildPreviewOffsets(brushSettings_);
  return offsets != cachedOffsets_;
}

const std::vector<PreviewTileData> &RawBrushPreviewProvider::getTiles() const {
  // Check if brush settings changed since last build
  if (checkSettingsChanged()) {
    needsRegen_ = true;
  }

  // Lazy regeneration if needed
  if (needsRegen_) {
    const_cast<RawBrushPreviewProvider *>(this)->buildPreview();
  }
  return tiles_;
}

PreviewBounds RawBrushPreviewProvider::getBounds() const { return bounds_; }

PreviewStyle RawBrushPreviewProvider::getStyle() const {
  return brushSettings_ && brushSettings_->getPreviewBorder()
             ? PreviewStyle::Outline
             : PreviewStyle::Ghost;
}

void RawBrushPreviewProvider::updateCursorPosition(
    const Domain::Position &cursor) {
  anchor_ = cursor;
}

void RawBrushPreviewProvider::regenerate() { buildPreview(); }

void RawBrushPreviewProvider::buildPreview() {
  tiles_.clear();
  bounds_ = PreviewBounds();
  needsRegen_ = false;

  if (itemId_ == 0) {
    cachedOffsets_.clear();
    return;
  }

  auto offsets = buildPreviewOffsets(brushSettings_);

  // Cache actual offsets for change detection
  cachedOffsets_ = offsets;

  // Create a preview tile for each offset
  for (const auto &[dx, dy] : offsets) {
    PreviewTileData tile(dx, dy, 0); // Relative position
    tile.addItem(itemId_, subtype_);
    tiles_.push_back(std::move(tile));

    // Update bounds
    bounds_.expand(dx, dy, 0);
  }
}

} // namespace Preview
} // namespace Services
} // namespace MapEditor
