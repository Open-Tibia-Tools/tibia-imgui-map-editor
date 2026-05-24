#include "GridOverlay.h"
#include <cmath>

namespace MapEditor {
namespace Rendering {

void GridOverlay::render(ImDrawList *draw_list, const glm::vec2 &camera_pos,
                         const glm::vec2 &viewport_pos,
                         const glm::vec2 &viewport_size, float zoom) {
  if (!draw_list)
    return;

  float const tile_screen_size = Config::Rendering::TILE_SIZE * zoom;

  if (tile_screen_size < 8.0f)
    return;

  ImU32 const grid_color = Config::Colors::GRID_LINE;

  float const frac_x = camera_pos.x - std::floor(camera_pos.x);
  float const frac_y = camera_pos.y - std::floor(camera_pos.y);

  glm::vec2 const center = viewport_pos + viewport_size * 0.5f;
  glm::vec2 const grid_offset(-frac_x * tile_screen_size, -frac_y * tile_screen_size);

  int const tiles_x = static_cast<int>(viewport_size.x / tile_screen_size / 2) + 2;
  int const tiles_y = static_cast<int>(viewport_size.y / tile_screen_size / 2) + 2;

  // Vertical lines
  for (int i = -tiles_x; i <= tiles_x; ++i) {
    float const x = center.x + i * tile_screen_size + grid_offset.x;
    draw_list->AddLine(ImVec2(x, viewport_pos.y),
                       ImVec2(x, viewport_pos.y + viewport_size.y), grid_color);
  }

  // Horizontal lines
  for (int i = -tiles_y; i <= tiles_y; ++i) {
    float const y = center.y + i * tile_screen_size + grid_offset.y;
    draw_list->AddLine(ImVec2(viewport_pos.x, y),
                       ImVec2(viewport_pos.x + viewport_size.x, y), grid_color);
  }
}

} // namespace Rendering
} // namespace MapEditor
