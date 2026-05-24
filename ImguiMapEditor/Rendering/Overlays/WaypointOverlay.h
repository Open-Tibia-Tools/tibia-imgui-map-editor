#pragma once
#include "../../Core/Config.h"
#include "Domain/ChunkedMap.h"
#include "OverlayCollector.h"
#include "Rendering/Visibility/ChunkVisibilityManager.h"
#include "Services/ViewSettings.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <string>


namespace MapEditor {
namespace Rendering {

/**
 * Renders waypoint markers (blue flames) on the map overlay.
 * Single responsibility: waypoint visualization.
 */
class WaypointOverlay {
public:
  WaypointOverlay() = default;

  /**
   * Render waypoints from pre-collected entries.
   */
  void renderFromCollector(ImDrawList *draw_list,
                           const std::vector<OverlayEntry> &entries,
                           const glm::vec2 &camera_pos,
                           const glm::vec2 &viewport_pos,
                           const glm::vec2 &viewport_size, float zoom);

  /**
   * Collect visible waypoints and add them to the overlay collector.
   * Extracts collection logic from MapRenderer.
   */
  static void collectVisibleWaypoints(const Domain::ChunkedMap &map,
                                      int floor_z, const VisibleBounds &bounds,
                                      OverlayCollector &collector,
                                      const Services::ViewSettings &settings,
                                      float floor_offset);

private:
  static void drawWaypointFlame(ImDrawList *draw_list, const glm::vec2 &screen_pos,
                         const std::string &name, float zoom);

  static constexpr float TILE_SIZE = Config::Rendering::TILE_SIZE;
};

} // namespace Rendering
} // namespace MapEditor
