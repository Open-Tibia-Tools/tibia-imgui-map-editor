#pragma once
#include "Domain/ChunkedMap.h"
#include "Rendering/Overlays/OverlayCollector.h"
#include "Rendering/Visibility/ChunkVisibilityManager.h"
#include "Services/ViewSettings.h"
#include <cstdint>
#include <vector>

namespace MapEditor {

namespace Services {
class SpriteManager;
}

namespace Domain {
class Chunk;
}

namespace Rendering {

/**
 * Consolidates per-frame data collection and buffer management.
 * Extracted from MapRenderer for single responsibility.
 *
 * Lifecycle:
 *   1. beginFrame() - clears buffers
 *   2. collectSpawns() / collectWaypoints() - populate overlays
 *   3. endFrame() - trigger async sprite loading
 */
class FrameDataCollector {
public:
  FrameDataCollector() = default;

  /**
   * Clear all buffers at start of frame.
   */
  void beginFrame();

  /**
   * Collect visible spawns for radius overlay.
   * Delegates to SpawnOverlayRenderer::collectVisibleSpawns().
   */
  void collectSpawns(const Domain::ChunkedMap &map, int floor_z,
                     const VisibleBounds &bounds, OverlayCollector &collector,
                     const Services::ViewSettings &settings);

  /**
   * Collect visible waypoints for overlay rendering.
   * Delegates to WaypointRenderer::collectVisibleWaypoints().
   */
  static void collectWaypoints(const Domain::ChunkedMap &map, int floor_z,
                        const VisibleBounds &bounds,
                        OverlayCollector &collector,
                        const Services::ViewSettings &settings,
                        float floor_offset);

  /**
   * Trigger async loading of missing sprites.
   * Call at end of frame after all tile rendering.
   */
  void endFrame(Services::SpriteManager *sprites);

  /**
   * Get buffer for tile renderers to report missing sprites.
   * Buffer is cleared by beginFrame().
   */
  std::vector<uint32_t> &getMissingSpriteBuffer() { return missing_sprites_; }

private:
  std::vector<uint32_t> missing_sprites_;
  std::vector<Domain::Chunk *> chunk_buffer_; // Reusable for spawn queries
};

} // namespace Rendering
} // namespace MapEditor
