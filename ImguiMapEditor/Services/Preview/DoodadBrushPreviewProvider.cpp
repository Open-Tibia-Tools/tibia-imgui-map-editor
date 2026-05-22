#include "DoodadBrushPreviewProvider.h"

#include "Brushes/Types/DoodadBrush.h"
#include "Services/BrushSettingsService.h"

namespace MapEditor::Services::Preview {

DoodadBrushPreviewProvider::DoodadBrushPreviewProvider(
    const Brushes::DoodadBrush &brush, BrushSettingsService *brushSettings)
    : brush_(brush), brushSettings_(brushSettings) {
  buildPreview();
}

bool DoodadBrushPreviewProvider::isActive() const { return true; }

Domain::Position DoodadBrushPreviewProvider::getAnchorPosition() const {
  return anchor_;
}

bool DoodadBrushPreviewProvider::checkSettingsChanged() const {
  if (!brushSettings_) {
    return false;
  }

  const auto offsets = brushSettings_->getBrushOffsets();
  return offsets != cachedOffsets_;
}

const std::vector<PreviewTileData> &DoodadBrushPreviewProvider::getTiles() const {
  if (checkSettingsChanged()) {
    needsRegen_ = true;
  }

  if (needsRegen_) {
    const_cast<DoodadBrushPreviewProvider *>(this)->buildPreview();
  }

  return tiles_;
}

PreviewBounds DoodadBrushPreviewProvider::getBounds() const { return bounds_; }

PreviewStyle DoodadBrushPreviewProvider::getStyle() const {
  return brushSettings_ && brushSettings_->getPreviewBorder()
             ? PreviewStyle::Outline
             : PreviewStyle::Ghost;
}

void DoodadBrushPreviewProvider::updateCursorPosition(const Domain::Position &cursor) {
  anchor_ = cursor;
  needsRegen_ = true;
}

void DoodadBrushPreviewProvider::regenerate() { buildPreview(); }

void DoodadBrushPreviewProvider::buildPreview() {
  tiles_ = brush_.buildPreviewTiles(anchor_, brushSettings_);
  bounds_ = PreviewBounds();
  needsRegen_ = false;
  cachedOffsets_ = brushSettings_ ? brushSettings_->getBrushOffsets()
                                  : std::vector<std::pair<int, int>>{{0, 0}};

  for (const auto &tile : tiles_) {
    bounds_.expand(tile.relativePosition);
  }
}

} // namespace MapEditor::Services::Preview
