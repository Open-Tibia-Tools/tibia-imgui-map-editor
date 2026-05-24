#pragma once

#include "Core/Config.h"
#include <glm/glm.hpp>
#include <imgui.h>

namespace MapEditor {
namespace Rendering {

/**
 * Renders the map grid overlay.
 */
class GridOverlay {
public:
  GridOverlay() = default;

  static void render(ImDrawList *draw_list, const glm::vec2 &camera_pos,
              const glm::vec2 &viewport_pos, const glm::vec2 &viewport_size,
              float zoom);
};

} // namespace Rendering
} // namespace MapEditor
