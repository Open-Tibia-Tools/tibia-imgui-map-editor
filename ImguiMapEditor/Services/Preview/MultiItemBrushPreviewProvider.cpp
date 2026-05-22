#include "MultiItemBrushPreviewProvider.h"

#include "Services/BrushSettingsService.h"

namespace MapEditor::Services::Preview {

MultiItemBrushPreviewProvider::MultiItemBrushPreviewProvider(
    std::vector<PreviewTileData> prototypeTiles,
    BrushSettingsService *brushSettings, bool repeatByBrushShape)
    : prototypeTiles_(std::move(prototypeTiles)),
      brushSettings_(brushSettings), repeatByBrushShape_(repeatByBrushShape) {
  buildPreview();
}

const std::vector<PreviewTileData> &MultiItemBrushPreviewProvider::getTiles() const {
  if (checkSettingsChanged()) {
    needsRegen_ = true;
  }
  if (needsRegen_) {
    const_cast<MultiItemBrushPreviewProvider *>(this)->buildPreview();
  }
  return tiles_;
}

void MultiItemBrushPreviewProvider::regenerate() { buildPreview(); }

PreviewStyle MultiItemBrushPreviewProvider::getStyle() const {
  return brushSettings_ && brushSettings_->getPreviewBorder()
             ? PreviewStyle::Outline
             : PreviewStyle::Ghost;
}

bool MultiItemBrushPreviewProvider::checkSettingsChanged() const {
  if (!repeatByBrushShape_ || !brushSettings_) {
    return false;
  }
  return cachedOffsets_ != brushSettings_->getBrushOffsets();
}

void MultiItemBrushPreviewProvider::buildPreview() {
  tiles_.clear();
  bounds_ = PreviewBounds{};
  needsRegen_ = false;

  if (prototypeTiles_.empty()) {
    cachedOffsets_.clear();
    return;
  }

  if (!repeatByBrushShape_ || !brushSettings_) {
    tiles_ = prototypeTiles_;
    for (const auto &tile : tiles_) {
      bounds_.expand(tile.relativePosition);
    }
    return;
  }

  cachedOffsets_ = brushSettings_->getBrushOffsets();
  for (const auto &[dx, dy] : cachedOffsets_) {
    for (const auto &prototype : prototypeTiles_) {
      auto tile = prototype;
      tile.relativePosition.x += dx;
      tile.relativePosition.y += dy;
      tiles_.push_back(tile);
      bounds_.expand(tile.relativePosition);
    }
  }
}

} // namespace MapEditor::Services::Preview
