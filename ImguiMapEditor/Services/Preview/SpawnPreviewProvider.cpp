/**
 * @file SpawnPreviewProvider.cpp
 * @brief Implementation of SpawnPreviewProvider for spawn radius preview.
 */

#include "SpawnPreviewProvider.h"
#include "Services/BrushSettingsService.h"

namespace MapEditor {
namespace Services {
namespace Preview {

SpawnPreviewProvider::SpawnPreviewProvider(BrushSettingsService *brushSettings)
    : brushSettings_(brushSettings) {
  buildSquarePreview();
}

bool SpawnPreviewProvider::isActive() const { return true; }

Domain::Position SpawnPreviewProvider::getAnchorPosition() const {
  return anchor_;
}

const std::vector<PreviewTileData> &SpawnPreviewProvider::getTiles() const {
  // Check if radius changed
  if (brushSettings_) {
    int const currentRadius = brushSettings_->getDefaultSpawnRadius();
    if (currentRadius != cachedRadius_) {
      cachedRadius_ = currentRadius;
      needsRegen_ = true;
    }
  }

  if (needsRegen_) {
    const_cast<SpawnPreviewProvider *>(this)->regenerate();
  }
  return tiles_;
}

PreviewBounds SpawnPreviewProvider::getBounds() const { return bounds_; }

void SpawnPreviewProvider::updateCursorPosition(
    const Domain::Position &cursor) {
  anchor_ = cursor;
}

void SpawnPreviewProvider::regenerate() {
  buildSquarePreview();
  needsRegen_ = false;
}

void SpawnPreviewProvider::buildSquarePreview() {
  tiles_.clear();
  bounds_ = PreviewBounds();

  int radius = 3;
  if (brushSettings_) {
    radius = brushSettings_->getDefaultSpawnRadius();
  }
  cachedRadius_ = radius;

  // Only generate CENTER tile - the renderer will draw the full border
  // based on the spawn_radius value
  PreviewTileData center(0, 0, 0);
  center.has_spawn = true;
  center.spawn_radius = radius; // Pass radius to renderer
  tiles_.push_back(center);

  // Bounds cover the full spawn area
  bounds_.expand(-radius, -radius, 0);
  bounds_.expand(radius, radius, 0);
}

} // namespace Preview
} // namespace Services
} // namespace MapEditor
