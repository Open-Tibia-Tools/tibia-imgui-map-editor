#pragma once
#include "../../Core/Config.h"
#include "Domain/ChunkedMap.h"
#include "OverlayCollector.h"
#include "OverlaySpriteCache.h"
#include "Services/ClientDataService.h"
#include "Services/CreatureSimulator.h"
#include "Services/SpriteManager.h"
#include "Services/ViewSettings.h"
#include <glm/glm.hpp>
#include <imgui.h>

namespace MapEditor {
namespace Rendering {

/**
 * Renders spawn markers and creature sprites on the map overlay.
 * Single responsibility: spawn visualization.
 */
class SpawnLabelOverlay {
public:
  SpawnLabelOverlay() = default;

  /**
   * Optimized rendering using pre-collected overlay entries.
   * Now scans ALL visible tiles for creatures (stored per-tile).
   * Uses spawn_radii from collector for proper border rendering.
   */
  void renderFromCollector(
      ImDrawList *draw_list, const OverlayCollector *collector,
      Domain::ChunkedMap *map, Services::ClientDataService *client_data,
      Services::SpriteManager *sprite_manager,
      OverlaySpriteCache *overlay_cache, Services::CreatureSimulator *simulator,
      const Services::ViewSettings &settings, bool show_spawns,
      bool show_creatures, const glm::vec2 &camera_pos,
      const glm::vec2 &viewport_pos, const glm::vec2 &viewport_size, int floor,
      float zoom);

  /**
   * Set LOD mode to enable/disable simplified rendering.
   */
  void setLODMode(bool enabled) { is_lod_active_ = enabled; }

private:
  bool is_lod_active_ = false;

  /**
   * Render indicator for spawn tile (orange square with "SPAWN" text).
   */
  void renderSpawnIndicator(ImDrawList *draw_list, const glm::vec2 &screen_pos,
                            float size, float zoom) const;

  /**
   * Render solid border around spawn radius area.
   */
  void renderRadiusBorder(ImDrawList *draw_list,
                          const Domain::Position &spawn_pos, int radius,
                          const glm::vec2 &camera_pos,
                          const glm::vec2 &viewport_pos,
                          const glm::vec2 &viewport_size, float zoom,
                          int creature_count = 0) const;

  static constexpr float TILE_SIZE = Config::Rendering::TILE_SIZE;
};

} // namespace Rendering
} // namespace MapEditor
