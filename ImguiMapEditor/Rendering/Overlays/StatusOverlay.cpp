#include "StatusOverlay.h"
#include <cstdio>
#include "Core/Config.h"
#include <format>
#include <string>

namespace MapEditor {
namespace Rendering {

void StatusOverlay::render(ImDrawList *draw_list,
                           const Domain::ICoordinateTransformer &camera,
                           size_t selection_count, bool is_hovered,
                           float framerate) {
  if (!draw_list)
    return;

  // Calculate cursor tile position
  ImGuiIO  const&io = ImGui::GetIO();
  glm::vec2 const mouse_pos(io.MousePos.x, io.MousePos.y);

  // Check if mouse is within viewport bounds
  // Note: we accept the passed in 'is_hovered' state from the panel
  bool const mouse_in_viewport = is_hovered;

  Domain::Position cursor_tile;
  if (mouse_in_viewport) {
    cursor_tile = camera.screenToTile(mouse_pos);
  }

  // Format FPS with color warning
  const char* fps_icon = ICON_FA_GAUGE;
  if (framerate < Config::UI::FPS_WARNING_THRESHOLD) fps_icon = ICON_FA_TRIANGLE_EXCLAMATION; // Warning icon for low FPS
  else if (framerate > Config::UI::FPS_HIGH_PERF_THRESHOLD) fps_icon = ICON_FA_GAUGE_HIGH; // High performance icon

  // Build status text using std::format
  std::string status_str;

  if (selection_count > 0) {
      status_str += std::format("{} Selected: {} Tiles   ", ICON_FA_SQUARE_CHECK, selection_count);
  }

  if (mouse_in_viewport) {
      status_str += std::format("{} {}, {}, {}   ", ICON_FA_ARROW_POINTER, cursor_tile.x, cursor_tile.y, cursor_tile.z);
      status_str += std::format("{} Cam: {:.0f}, {:.0f}   ", ICON_FA_LOCATION_CROSSHAIRS, camera.getCameraPosition().x, camera.getCameraPosition().y);
  } else {
      status_str += std::format("{} Cam: {:.0f}, {:.0f}, {}   ", ICON_FA_LOCATION_CROSSHAIRS, camera.getCameraPosition().x, camera.getCameraPosition().y, camera.getCurrentFloor());
  }

  status_str += std::format("{} {:.0f}%   {} {:.1f} FPS", ICON_FA_MAGNIFYING_GLASS, camera.getZoom() * 100.0f, fps_icon, framerate);

  const char* pos_text = status_str.c_str();
  glm::vec2 const vp_pos = camera.getViewportPos();
  glm::vec2 const vp_size = camera.getViewportSize();
  ImVec2 const text_pos(vp_pos.x + 10, vp_pos.y + vp_size.y - 25);

  ImVec2 const text_size = ImGui::CalcTextSize(pos_text);
  draw_list->AddRectFilled(
      ImVec2(text_pos.x - 5, text_pos.y - 2),
      ImVec2(text_pos.x + text_size.x + 5, text_pos.y + text_size.y + 2),
      Config::Colors::INFO_TEXT_BG);

  draw_list->AddText(text_pos, Config::Colors::INFO_TEXT, pos_text);

  // Explicit copy button next to the text
  ImGui::SetCursorScreenPos(ImVec2(text_pos.x + text_size.x + 8, text_pos.y));
  if (mouse_in_viewport) {
    if (ImGui::SmallButton(ICON_FA_COPY "##CopyPos")) {
      std::string const clip_text = std::format("{}, {}, {}", cursor_tile.x, cursor_tile.y, cursor_tile.z);
      ImGui::SetClipboardText(clip_text.c_str());
      Presentation::showSuccess("Position copied to clipboard");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Copy cursor position (X, Y, Z)");
    }
  }

  // Tooltip for status text
  ImGui::SetCursorScreenPos(ImVec2(text_pos.x - 5, text_pos.y - 2));
  ImGui::InvisibleButton("##StatusOverlay",
                         ImVec2(text_size.x + 10, text_size.y + 4));
  if (ImGui::IsItemHovered()) {
    std::string tooltip_text;
    if (selection_count > 0) {
      tooltip_text += ICON_FA_SQUARE_CHECK " Selected Tiles Count\n";
    }
    tooltip_text += ICON_FA_ARROW_POINTER
        " Cursor Position (X, Y, Z)\n" ICON_FA_LOCATION_CROSSHAIRS
        " Camera Center (X, Y)\n" ICON_FA_MAGNIFYING_GLASS
        " Zoom Level\n" ICON_FA_GAUGE " Frames Per Second";

    ImGui::SetTooltip("%s", tooltip_text.c_str());
  }
}

} // namespace Rendering
} // namespace MapEditor
