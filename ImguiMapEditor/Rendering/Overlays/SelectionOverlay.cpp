#include "SelectionOverlay.h"
#include "Core/Config.h"
#include "Domain/Selection/SelectionEntry.h"
#include "Rendering/Selection/ISelectionDataProvider.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>

namespace MapEditor {
namespace Rendering {

void SelectionOverlay::render(ImDrawList *draw_list,
                              const Domain::ICoordinateTransformer &camera,
                              const ISelectionDataProvider *provider) {
  if (!provider || provider->isEmpty()) {
    return;
  }

  int current_floor = camera.getCurrentFloor();
  float tile_screen_size = Config::Rendering::TILE_SIZE * camera.getZoom();

  // Heuristic: Choose the iteration strategy with fewer steps.
  // 1. Iterative: Costs O(SelectionSize) to filter by floor.
  // 2. Viewport: Costs O(VisibleTiles) to query the hash map.
  // We calculate visible tiles dynamically to handle zoom levels correctly.
  glm::vec2 vp_size = camera.getViewportSize();
  float cols = vp_size.x / tile_screen_size;
  float rows = vp_size.y / tile_screen_size;
  size_t visible_tiles_count = static_cast<size_t>(cols * rows);

  if (provider->getSelectionCount() > visible_tiles_count) {
    renderSelectionViewport(draw_list, camera, provider, current_floor,
                            tile_screen_size);
  } else {
    renderSelectionIterative(draw_list, camera, provider, current_floor,
                             tile_screen_size);
  }
}

void SelectionOverlay::renderDragBox(ImDrawList *draw_list,
                                     const glm::vec2 &start_screen,
                                     const glm::vec2 &current_screen) {
  float min_x = std::min(start_screen.x, current_screen.x);
  float max_x = std::max(start_screen.x, current_screen.x);
  float min_y = std::min(start_screen.y, current_screen.y);
  float max_y = std::max(start_screen.y, current_screen.y);

  // RME Style: White outline, NO fill.
  // Ideally dotted (stipped), but solid white is closest simple approximation
  // in ImGui for now.
  draw_list->AddRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y),
                     IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f);
}

void SelectionOverlay::renderLassoOverlay(ImDrawList *draw_list,
                                          const std::vector<glm::vec2> &points,
                                          const glm::vec2 &current_mouse) {
  if (points.empty())
    return;

  // Convert glm::vec2 to ImVec2 for ImGui
  std::vector<ImVec2> im_points;
  im_points.reserve(points.size() + 1);
  for (const auto &pt : points) {
    im_points.push_back(ImVec2(pt.x, pt.y));
  }

  // Draw polyline connecting all clicked vertices (open line, not closed)
  if (im_points.size() >= 2) {
    draw_list->AddPolyline(im_points.data(), static_cast<int>(im_points.size()),
                           IM_COL32(255, 255, 255, 255), ImDrawFlags_None,
                           1.5f);
  }

  // Draw preview line from last point to current mouse
  ImVec2 last_point = im_points.back();
  ImVec2 mouse_pos(current_mouse.x, current_mouse.y);
  draw_list->AddLine(last_point, mouse_pos, IM_COL32(255, 255, 255, 180), 1.0f);

  // Draw small circles at each vertex
  for (const auto &pt : im_points) {
    draw_list->AddCircleFilled(pt, 3.0f, IM_COL32(255, 255, 255, 255));
  }
}

void SelectionOverlay::renderDragDimensions(ImDrawList *draw_list,
                                            const glm::vec2 &start_screen,
                                            const glm::vec2 &current_screen,
                                             const Domain::ICoordinateTransformer &camera,
                                             bool shift_pressed,
                                             bool alt_pressed) {
  Domain::Position start_pos = camera.screenToTile(start_screen);
  Domain::Position end_pos = camera.screenToTile(current_screen);

  int width = std::abs(end_pos.x - start_pos.x) + 1;
  int height = std::abs(end_pos.y - start_pos.y) + 1;
  int total = width * height;

  std::string dim_text = std::to_string(width) + "x" + std::to_string(height) +
                         " (" + std::to_string(total) + ")";

  // Add interaction hints
  if (shift_pressed)
    dim_text += " [Add]";
  else if (alt_pressed)
    dim_text += " [Sub]";

  ImVec2 text_size = ImGui::CalcTextSize(dim_text.c_str());
  ImVec2 text_pos(current_screen.x + 15, current_screen.y + 15);

  draw_list->AddRectFilled(
      text_pos,
      ImVec2(text_pos.x + text_size.x + 4, text_pos.y + text_size.y + 4),
      IM_COL32(0, 0, 0, 200));
  draw_list->AddText(ImVec2(text_pos.x + 2, text_pos.y + 2),
                     IM_COL32(255, 255, 255, 255), dim_text.c_str());
}

void SelectionOverlay::renderSelectionIterative(
    ImDrawList *draw_list, const Domain::ICoordinateTransformer &camera,
    const ISelectionDataProvider *provider, int floor, float tile_screen_size) {

  // Check bounds for culling optimization
  glm::vec2 vp_pos_vec = camera.getViewportPos();
  glm::vec2 vp_size_vec = camera.getViewportSize();
  ImVec2 viewport_pos(vp_pos_vec.x, vp_pos_vec.y);
  ImVec2 viewport_size(vp_size_vec.x, vp_size_vec.y);

  // Use interface callback to iterate entries on floor
  provider->forEachEntryOnFloor(
      static_cast<int16_t>(floor),
      [&](const Domain::Position &pos, Domain::Selection::EntityType type) {
        glm::vec2 screen_pos = camera.tileToScreen(pos);

        // Simple Culling
        if (screen_pos.x + tile_screen_size < viewport_pos.x ||
            screen_pos.x > viewport_pos.x + viewport_size.x ||
            screen_pos.y + tile_screen_size < viewport_pos.y ||
            screen_pos.y > viewport_pos.y + viewport_size.y) {
          return; // Skip off-screen entries
        }

        // Render based on entity type
        if (type == Domain::Selection::EntityType::Spawn) {
          // Cyan highlight for spawn
          draw_list->AddRectFilled(ImVec2(screen_pos.x + 2, screen_pos.y + 2),
                                   ImVec2(screen_pos.x + tile_screen_size - 2,
                                          screen_pos.y + tile_screen_size - 2),
                                   Config::Colors::TILE_SELECT_FILL);
          draw_list->AddRect(ImVec2(screen_pos.x + 1, screen_pos.y + 1),
                             ImVec2(screen_pos.x + tile_screen_size - 1,
                                    screen_pos.y + tile_screen_size - 1),
                             Config::Colors::TILE_SELECT_BORDER, 0.0f, 0, 2.0f);
        }
        // Item and Ground tinting handled in TileRenderer - no overlay needed
      });
}

void SelectionOverlay::renderSelectionViewport(
    ImDrawList *draw_list, const Domain::ICoordinateTransformer &camera,
    const ISelectionDataProvider *provider, int floor, float tile_screen_size) {

  // Calculate visible tile range
  glm::vec2 vp_pos_vec = camera.getViewportPos();
  glm::vec2 vp_size_vec = camera.getViewportSize();
  ImVec2 viewport_pos(vp_pos_vec.x, vp_pos_vec.y);
  ImVec2 viewport_size(vp_size_vec.x, vp_size_vec.y);

  // Extend slightly to avoid clipping at edges
  Domain::Position top_left =
      camera.screenToTile(glm::vec2(viewport_pos.x, viewport_pos.y));
  Domain::Position bottom_right = camera.screenToTile(glm::vec2(
      viewport_pos.x + viewport_size.x, viewport_pos.y + viewport_size.y));

  int start_x = top_left.x - 1;
  int end_x = bottom_right.x + 1;
  int start_y = top_left.y - 1;
  int end_y = bottom_right.y + 1;

  // Iterate visible tiles
  for (int y = start_y; y <= end_y; ++y) {
    for (int x = start_x; x <= end_x; ++x) {
      Domain::Position pos(x, y, static_cast<int16_t>(floor));

      // Fast O(1) check via interface
      if (provider->hasSelectionAt(pos)) {
        // Check for Spawn selection at this position
        if (provider->hasSpawnSelectionAt(pos)) {
          glm::vec2 screen_pos = camera.tileToScreen(pos);
          draw_list->AddRectFilled(ImVec2(screen_pos.x + 2, screen_pos.y + 2),
                                   ImVec2(screen_pos.x + tile_screen_size - 2,
                                          screen_pos.y + tile_screen_size - 2),
                                   Config::Colors::TILE_SELECT_FILL);
          draw_list->AddRect(ImVec2(screen_pos.x + 1, screen_pos.y + 1),
                             ImVec2(screen_pos.x + tile_screen_size - 1,
                                    screen_pos.y + tile_screen_size - 1),
                             Config::Colors::TILE_SELECT_BORDER, 0.0f, 0, 2.0f);
        }
        // Item/Ground tinting handled in TileRenderer
      }
    }
  }
}

// === ISelectionObserver implementation ===

void SelectionOverlay::onSelectionChanged(
    const std::vector<Domain::Selection::SelectionEntry> &added,
    const std::vector<Domain::Selection::SelectionEntry> &removed) {
  // Mark as dirty when selection changes
  // Could use added/removed for incremental updates in the future
  dirty_ = true;
}

void SelectionOverlay::onSelectionCleared() {
  // Mark as dirty when selection is cleared
  dirty_ = true;
}

} // namespace Rendering
} // namespace MapEditor

