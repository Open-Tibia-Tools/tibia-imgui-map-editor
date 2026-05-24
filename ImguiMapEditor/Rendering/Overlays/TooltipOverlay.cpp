#include "TooltipOverlay.h"
#include "Rendering/Utils/CoordUtils.h"
#include "Rendering/Visibility/LODPolicy.h"
#include <algorithm>
#include <cmath>
#include <sstream>


namespace MapEditor {
namespace Rendering {

void TooltipOverlay::renderFromCollector(
    ImDrawList *draw_list, const std::vector<OverlayEntry> &entries,
    const glm::vec2 &camera_pos, const glm::vec2 &viewport_pos,
    const glm::vec2 &viewport_size, float zoom) {
  if (entries.empty())
    return;
  if (entries.empty())
    return;

  // Controlled by LOD Policy
  // If LOD is active, we check the policy. If inactive, show by default.
  bool const should_show = !is_lod_active_ || LODPolicy::SHOW_TOOLTIPS;
  if (!should_show)
    return;

  float const scale = std::min(1.0f, std::max(0.4f, zoom));
  ImGui::SetWindowFontScale(scale);

  for (const auto &entry : entries) {
    const auto *tile = entry.tile; // Use const Tile* from OverlayEntry
    if (!tile)
      continue; // Should not happen given collector logic

    const auto &tile_pos = tile->getPosition();

    bool has_content = false;
    bool is_waypoint = false;

    std::string tooltip_text;
    tooltip_text.reserve(128);

    if (entry.waypoint_name) {
      has_content = true;
      is_waypoint = true;
      tooltip_text += "wp: ";
      tooltip_text += *entry.waypoint_name;
      tooltip_text += "\n";
    }

    // Ground Attributes
    if (tile->getGround()) {
      const auto *ground = tile->getGround();
      if (ground->getActionId() > 0 || ground->getUniqueId() > 0) {
        tooltip_text += "id: " + std::to_string(ground->getServerId()) + "\n";
        if (ground->getActionId() > 0)
          tooltip_text +=
              "aid: " + std::to_string(ground->getActionId()) + "\n";
        if (ground->getUniqueId() > 0)
          tooltip_text +=
              "uid: " + std::to_string(ground->getUniqueId()) + "\n";
        has_content = true;
      }
    }

    // Items Attributes
    for (const auto &item_ptr : tile->getItems()) {
      const auto *item = item_ptr.get();
      bool const item_has_attrs =
          (item->getActionId() > 0 || item->getUniqueId() > 0 ||
           item->getDoorId() > 0 || !item->getText().empty() ||
           item->getTeleportDestination() != nullptr);

      if (item_has_attrs) {
        has_content = true;
        tooltip_text += "id: " + std::to_string(item->getServerId()) + "\n";
        if (item->getActionId() > 0)
          tooltip_text += "aid: " + std::to_string(item->getActionId()) + "\n";
        if (item->getUniqueId() > 0)
          tooltip_text += "uid: " + std::to_string(item->getUniqueId()) + "\n";
        if (item->getDoorId() > 0)
          tooltip_text +=
              "door id: " + std::to_string(item->getDoorId()) + "\n";

        if (!item->getText().empty()) {
          tooltip_text += "text: " + item->getText() + "\n";
        }

        const auto *dest = item->getTeleportDestination();
        if (dest) {
          tooltip_text += "dest: " + std::to_string(dest->x) + ", " +
                          std::to_string(dest->y) + ", " +
                          std::to_string(dest->z) + "\n";
        }
      }
    }

    if (has_content) {
      // Use pre-calculated screen_pos from collector, but we need to adjust for
      // Parallax? TileRenderer calculates screen_x/y used for drawing sprites.
      // OverlayEntry stores that screen_x/y.
      // That position IS the render position (parallax applied).
      // BUT Sprite drawing subtracts cx*size etc.
      // Let's verify OverlayEntry's screen_pos.
      // In MapRenderer: screen_x = x * TILE_SIZE. Passed to queueTile.
      // In TileRenderer::queueTile: It's just raw world coordinate unless
      // adjusted. WAIT - MapRenderer passes `screen_x = x * TILE_SIZE`. This is
      // WORLD SPACE. It is NOT screen space. The `tileToScreen` logic typically
      // applies camera transform. In OverlayRenderer/TooltipRenderer:
      // `tileToScreen` applies camera, zoom, AND parallax.

      // So if we use OverlayEntry.screen_pos (which is world space), we need to
      // project it. `queueTile` received `screen_x` which was just `x * 32`.

      // Let's re-use `tileToScreen` helper from this class, but optimized.
      // Actually, `tileToScreen` needs `pos`. We have `pos`.
      // So we can just use `tileToScreen(tile_pos, ...)` and ignore stored
      // entry.screen_pos if it's raw world. PRO: Consistent with old renderer.
      // CON: Re-calculating position.
      // Cost: negligible (a few muls/adds) vs Map Iteration (massive).

      glm::vec2 const screen_pos = Utils::tileToScreen(
          tile_pos, camera_pos, viewport_pos, viewport_size, zoom);
      drawSpeechBubble(draw_list, screen_pos, tooltip_text, is_waypoint, zoom,
                       scale);
    }
  }

  ImGui::SetWindowFontScale(1.0f);
}

void TooltipOverlay::renderHoverTooltip(
    ImDrawList *draw_list, Domain::ChunkedMap *map,
    const glm::vec2 &mouse_pos_screen, const glm::vec2 &mouse_pos_world,
    int floor, const glm::vec2 &camera_pos, const glm::vec2 &viewport_pos,
    const glm::vec2 &viewport_size, float zoom) {
  if (!map)
    return;

  int const tx = static_cast<int>(std::floor(mouse_pos_world.x));
  int const ty = static_cast<int>(std::floor(mouse_pos_world.y));

  Domain::Position const pos(tx, ty, floor);
  auto tile = map->getTile(pos);

  if (!tile)
    return;

  std::stringstream ss;
  bool has_content = false;
  bool has_waypoint = false;

  // Check for waypoint using O(1) lookup
  const auto *wp = map->getWaypointAt(pos);
  if (wp) {
    ss << "wp: " << wp->name << "\n";
    has_content = true;
    has_waypoint = true;
  }

  // Ground tile
  if (tile->getGround()) {
    const auto *ground = tile->getGround();
    ss << "id: " << ground->getServerId() << "\n";
    if (ground->getActionId() > 0)
      ss << "aid: " << ground->getActionId() << "\n";
    if (ground->getUniqueId() > 0)
      ss << "uid: " << ground->getUniqueId() << "\n";
    has_content = true;
  }

  // Items
  const auto &items = tile->getItems();
  for (auto it = items.rbegin(); it != items.rend(); ++it) {
    const auto *item = it->get();
    ss << "\nid: " << item->getServerId();
    if (item->getCount() > 1)
      ss << " (x" << item->getCount() << ")";
    ss << "\n";

    if (item->getActionId() > 0)
      ss << "aid: " << item->getActionId() << "\n";
    if (item->getUniqueId() > 0)
      ss << "uid: " << item->getUniqueId() << "\n";
    if (item->getDoorId() > 0)
      ss << "door id: " << item->getDoorId() << "\n";

    std::string text = item->getText();
    if (!text.empty()) {
      if (text.length() > 30)
        text = text.substr(0, 30) + "...";
      ss << "text: " << text << "\n";
    }

    const Domain::Position *dest = item->getTeleportDestination();
    if (dest) {
      ss << "dest: " << dest->x << ", " << dest->y << ", " << dest->z << "\n";
    }

    has_content = true;
  }

  if (tile->hasSpawn()) {
    ss << "[SPAWN]\n";
    has_content = true;
  }

  if (has_content) {
    if (has_waypoint) {
      drawParchmentTooltipColored(draw_list, mouse_pos_screen, ss.str(),
                                  Config::Colors::TOOLTIP_WAYPOINT_BG,
                                  Config::Colors::TOOLTIP_WAYPOINT_TEXT);
    } else {
      drawParchmentTooltip(draw_list, mouse_pos_screen, ss.str());
    }

    // Draw hover tile outline
    glm::vec2 const tile_screen =
        Utils::tileToScreen(pos, camera_pos, viewport_pos, viewport_size, zoom);
    float const size = Config::Rendering::TILE_SIZE * zoom;
    draw_list->AddRect(ImVec2(tile_screen.x, tile_screen.y),
                       ImVec2(tile_screen.x + size, tile_screen.y + size),
                       Config::Colors::PIXEL_SELECT_BORDER, 0, 0, 2.0f);
  }
}

void TooltipOverlay::drawSpeechBubble(ImDrawList *draw_list,
                                      const glm::vec2 &tile_pos,
                                      const std::string &text, bool is_waypoint,
                                      float zoom, float scale) {
  float const tile_size = Config::Rendering::TILE_SIZE * zoom;
  float const center_x = tile_pos.x + tile_size / 2.0f;
  float const tile_top_y = tile_pos.y;

  float const max_text_width = 150.0f * scale;
  ImVec2 const text_size =
      ImGui::CalcTextSize(text.c_str(), nullptr, false, max_text_width);

  ImVec2 const padding(4 * scale, 2 * scale);
  float const bubble_width = text_size.x + padding.x * 2;
  float const bubble_height = text_size.y + padding.y * 2;

  float const pointer_size = 5.0f * scale;
  float const bubble_left = center_x - bubble_width / 2.0f;
  float const bubble_bottom = tile_top_y - pointer_size;
  float const bubble_top = bubble_bottom - bubble_height;

  ImU32 const bg_color = is_waypoint ? Config::Colors::TOOLTIP_WAYPOINT_BG
                               : Config::Colors::TOOLTIP_NORMAL_BG;
  ImU32 const border_color = Config::Colors::TOOLTIP_BORDER;
  ImU32 const text_color = Config::Colors::TOOLTIP_TEXT;

  draw_list->AddRectFilled(ImVec2(bubble_left, bubble_top),
                           ImVec2(bubble_left + bubble_width, bubble_bottom),
                           bg_color, 2.0f * scale);

  ImVec2 const p1(center_x - pointer_size, bubble_bottom);
  ImVec2 const p2(center_x + pointer_size, bubble_bottom);
  ImVec2 const p3(center_x, tile_top_y);
  draw_list->AddTriangleFilled(p1, p2, p3, bg_color);

  draw_list->AddRect(ImVec2(bubble_left, bubble_top),
                     ImVec2(bubble_left + bubble_width, bubble_bottom),
                     border_color, 2.0f * scale, 0, 1.0f * scale);

  draw_list->AddLine(p1, p3, border_color, 1.0f * scale);
  draw_list->AddLine(p2, p3, border_color, 1.0f * scale);

  draw_list->AddText(nullptr, 0.0f,
                     ImVec2(bubble_left + padding.x, bubble_top + padding.y),
                     text_color, text.c_str(), nullptr, max_text_width);
}

void TooltipOverlay::drawParchmentTooltip(ImDrawList *draw_list,
                                          const glm::vec2 &pos,
                                          const std::string &text) {
  ImVec2 const padding(10, 10);
  ImVec2 const text_size = ImGui::CalcTextSize(text.c_str());
  ImVec2 const size(text_size.x + padding.x * 2, text_size.y + padding.y * 2);

  ImVec2 const top_left(pos.x + 15, pos.y + 15);
  ImVec2 const bottom_right(top_left.x + size.x, top_left.y + size.y);

  ImU32 const bg_color = Config::Colors::PARCHMENT_BG;
  ImU32 const border_color = Config::Colors::PARCHMENT_BORDER;
  ImU32 const text_color = Config::Colors::PARCHMENT_TEXT;

  draw_list->AddRectFilled(top_left, bottom_right, bg_color, 4.0f);
  draw_list->AddRect(top_left, bottom_right, border_color, 4.0f, 0, 2.0f);
  draw_list->AddRect(ImVec2(top_left.x + 3, top_left.y + 3),
                     ImVec2(bottom_right.x - 3, bottom_right.y - 3),
                     IM_COL32(139, 69, 19, 100), 2.0f, 0, 1.0f);

  draw_list->AddText(ImVec2(top_left.x + padding.x, top_left.y + padding.y),
                     text_color, text.c_str());
}

void TooltipOverlay::drawParchmentTooltipColored(ImDrawList *draw_list,
                                                 const glm::vec2 &pos,
                                                 const std::string &text,
                                                 ImU32 bg_color,
                                                 ImU32 text_col) {
  ImVec2 const padding(10, 10);
  ImVec2 const text_size = ImGui::CalcTextSize(text.c_str());
  ImVec2 const size(text_size.x + padding.x * 2, text_size.y + padding.y * 2);

  ImVec2 const top_left(pos.x + 15, pos.y + 15);
  ImVec2 const bottom_right(top_left.x + size.x, top_left.y + size.y);

  ImU32 const border_color = IM_COL32(0, 80, 0, 255);

  draw_list->AddRectFilled(top_left, bottom_right, bg_color, 4.0f);
  draw_list->AddRect(top_left, bottom_right, border_color, 4.0f, 0, 2.0f);

  draw_list->AddText(ImVec2(top_left.x + padding.x, top_left.y + padding.y),
                     text_col, text.c_str());
}

} // namespace Rendering
} // namespace MapEditor
