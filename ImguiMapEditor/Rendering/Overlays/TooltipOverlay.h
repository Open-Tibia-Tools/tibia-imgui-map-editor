#pragma once
#include "../../Core/Config.h"
#include "Domain/ChunkedMap.h"
#include "OverlayCollector.h"
#include "TooltipBubbleRenderer.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <string>

namespace MapEditor {
namespace Rendering {

/**
 * Renders speech bubble tooltips on tiles with special attributes.
 * Single responsibility: tooltip visualization for action IDs, unique IDs,
 * door IDs, text, teleport destinations, and waypoints.
 */
class TooltipOverlay {
public:
  TooltipOverlay() = default;

  /**
   * Optimized rendering using pre-collected overlay entries.
   */
  void renderFromCollector(ImDrawList *draw_list,
                           const std::vector<OverlayEntry> &entries,
                           const glm::vec2 &camera_pos,
                           const glm::vec2 &viewport_pos,
                           const glm::vec2 &viewport_size, float zoom);

  /**
   * Render hover tooltip at mouse position (parchment style).
   */
  void renderHoverTooltip(ImDrawList *draw_list, Domain::ChunkedMap *map,
                          const glm::vec2 &mouse_pos_screen,
                          const glm::vec2 &mouse_pos_world, int floor,
                          const glm::vec2 &camera_pos,
                          const glm::vec2 &viewport_pos,
                          const glm::vec2 &viewport_size, float zoom);
  /**
   * Set LOD mode to enable/disable simplified rendering.
   */
  void setLODMode(bool enabled) { is_lod_active_ = enabled; }

private:
  bool is_lod_active_ = false;

  static void drawSpeechBubble(ImDrawList *draw_list, const glm::vec2 &tile_pos,
                        const std::string &text, bool is_waypoint, float zoom,
                        float scale);

  static void drawParchmentTooltip(ImDrawList *draw_list, const glm::vec2 &pos,
                            const std::string &text);

  static void drawParchmentTooltipColored(ImDrawList *draw_list, const glm::vec2 &pos,
                                   const std::string &text, ImU32 bg_color,
                                   ImU32 text_color);

  static constexpr float TILE_SIZE = Config::Rendering::TILE_SIZE;
};

} // namespace Rendering
} // namespace MapEditor
