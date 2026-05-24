#include "TownPickController.h"
#include "../NotificationHelper.h"

namespace MapEditor::Presentation {

void TownPickController::processPickMode(const Context &ctx) {
  if (!ctx.dialog || !ctx.map_panel)
    return;
  if (!ctx.dialog->isPickingPosition())
    return;

  // Get mouse position
  ImVec2 const mouse_pos = ImGui::GetMousePos();

  // Check if click is within the map panel viewport
  glm::vec2 const vp_pos = ctx.map_panel->getViewportPos();
  glm::vec2 const vp_size = ctx.map_panel->getViewportSize();

  bool const in_viewport =
      mouse_pos.x >= vp_pos.x && mouse_pos.x <= vp_pos.x + vp_size.x &&
      mouse_pos.y >= vp_pos.y && mouse_pos.y <= vp_pos.y + vp_size.y;

  // Detect left click
  if (ImGui::IsMouseClicked(0) && in_viewport) {
    // Convert to tile position
    Domain::Position const tile_pos =
        ctx.map_panel->screenToTile(glm::vec2(mouse_pos.x, mouse_pos.y));
    ctx.dialog->setPickedPosition(tile_pos);

    showSuccess("Temple position set to (" + std::to_string(tile_pos.x) + ", " +
                    std::to_string(tile_pos.y) + ", " +
                    std::to_string(tile_pos.z) + ")",
                1500);
  }
}

} // namespace MapEditor::Presentation
