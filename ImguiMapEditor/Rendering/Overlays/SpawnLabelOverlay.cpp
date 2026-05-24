#include "SpawnLabelOverlay.h"
#include "../ColorFilter.h"
#include "Domain/Creature.h"
#include "OutfitOverlay.h"
#include "Rendering/Utils/CoordUtils.h"
#include "Rendering/Visibility/LODPolicy.h"
#include <cmath>
#include <format>

namespace MapEditor {
namespace Rendering {

// Static outfit renderer instance for reuse
static OutfitOverlay s_outfit_renderer;

void SpawnLabelOverlay::renderFromCollector(
    ImDrawList *draw_list, const OverlayCollector *collector,
    Domain::ChunkedMap *map, Services::ClientDataService *client_data,
    Services::SpriteManager *sprite_manager, OverlaySpriteCache *overlay_cache,
    Services::CreatureSimulator *simulator,
    const Services::ViewSettings &settings, bool show_spawns,
    bool show_creatures, const glm::vec2 &camera_pos,
    const glm::vec2 &viewport_pos, const glm::vec2 &viewport_size, int floor,
    float zoom) {
  if (!map || !collector)
    return;

  float const tile_size_px = TILE_SIZE * zoom;

  // First pass: Render spawn indicators (orange SPAWN boxes) and radius borders
  // Only if show_spawns is enabled
  if (show_spawns) {
    // Render spawn INDICATORS for spawn centers collected during tile iteration
    for (const auto &entry : collector->spawns) {
      if (!entry.tile || !entry.tile->hasSpawn())
        continue;

      auto spawn = entry.tile->getSpawn();
      if (!spawn)
        continue;

      const Domain::Position &spawn_pos = entry.tile->getPosition();
      if (spawn_pos.z != floor)
        continue;

      // Render SPAWN indicator on spawn tile
      glm::vec2 const screen_pos = Utils::tileToScreen(
          spawn_pos, camera_pos, viewport_pos, viewport_size, zoom);
      renderSpawnIndicator(draw_list, screen_pos, tile_size_px, zoom);

      // Selection highlight: yellow border when spawn is selected
      if (spawn->isSelected()) {
        draw_list->AddRect(ImVec2(screen_pos.x - 2, screen_pos.y - 2),
                           ImVec2(screen_pos.x + tile_size_px + 2,
                                  screen_pos.y + tile_size_px + 2),
                           IM_COL32(255, 255, 0, 255), 4.0f, 0, 3.0f);
      }
    }

    // Render spawn RADIUS BORDERS using spawn_radii (may include spawns outside
    // viewport whose radius extends into the visible area)
    for (const auto &spawn_entry : collector->spawn_radii) {
      if (spawn_entry.floor != floor)
        continue;

      // Create position from spawn_radii entry
      Domain::Position const spawn_pos(spawn_entry.center_x, spawn_entry.center_y,
                                 spawn_entry.floor);

      // Render solid border around spawn radius + Creature Count Badge
      renderRadiusBorder(draw_list, spawn_pos, spawn_entry.radius, camera_pos,
                         viewport_pos, viewport_size, zoom,
                         spawn_entry.creature_count);
    }
  }

  // Calculate visible tile range from viewport
  int const tiles_x = static_cast<int>(std::ceil(viewport_size.x / tile_size_px)) + 2;
  int const tiles_y = static_cast<int>(std::ceil(viewport_size.y / tile_size_px)) + 2;
  int const start_x = static_cast<int>(camera_pos.x) - tiles_x / 2 - 1;
  int const start_y = static_cast<int>(camera_pos.y) - tiles_y / 2 - 1;
  int const end_x = start_x + tiles_x + 2;
  int const end_y = start_y + tiles_y + 2;

  // NOTE: When simulation is DISABLED, creature sprites are rendered via GPU
  // pipeline in TileRenderer::queueTile() When simulation is ENABLED, we render
  // creatures here in the overlay pass for proper Z-order (creatures at
  // simulated position which may differ from spawn tile)
  bool const simulate = simulator && simulator->isEnabled();

  if (show_creatures && client_data) {
    for (int y = start_y; y < end_y; ++y) {
      for (int x = start_x; x < end_x; ++x) {
        Domain::Position const pos(x, y, floor);
        auto *tile = map->getTile(pos);
        if (!tile || !tile->hasCreature())
          continue;

        Domain::Creature  const*creature = tile->getCreature();
        if (!creature)
          continue;

        // Get creature position - may be different from tile position if
        // simulating
        Domain::Position creature_pos = pos;
        uint8_t direction = static_cast<uint8_t>(creature->direction);
        int animation_frame = 0;
        float walk_offset_x = 0.0f;
        float walk_offset_y = 0.0f;

        if (simulate) {
          auto *state = simulator->getOrCreateState(creature, pos, map);
          if (state) {
            creature_pos = state->current_pos;
            direction = state->direction;
            animation_frame = state->animation_frame;
            walk_offset_x = state->walk_offset_x * tile_size_px;
            walk_offset_y = state->walk_offset_y * tile_size_px;
          }
        }

        // Calculate screen position from simulated position
        glm::vec2 screen_pos = Utils::tileToScreen(
            creature_pos, camera_pos, viewport_pos, viewport_size, zoom);
        screen_pos.x += walk_offset_x;
        screen_pos.y += walk_offset_y;

        // NOTE: Creature SPRITES are now rendered via GPU pipeline in
        // TileRenderer with per-floor two-pass rendering for proper floor
        // occlusion. SpawnRenderer ONLY handles name labels (which are ImGui
        // overlay).

        // Render name label (controlled by LOD Policy)
        // If LOD is active, we check the policy. If inactive, show by default.
        bool const should_show_name =
            !is_lod_active_ || LODPolicy::SHOW_CREATURE_NAMES;

        if (should_show_name) {
          glm::vec2 const center(screen_pos.x + tile_size_px / 2.0f,
                           screen_pos.y + tile_size_px / 2.0f);
          s_outfit_renderer.renderName(draw_list, creature->name, center,
                                       tile_size_px, zoom);
        }
      }
    }
  }
}

void SpawnLabelOverlay::renderSpawnIndicator(ImDrawList *draw_list,
                                             const glm::vec2 &screen_pos,
                                             float size, float zoom) const {
  // Option B: Gray Transparent Box with Light Gray Border
  draw_list->AddRectFilled(ImVec2(screen_pos.x, screen_pos.y),
                           ImVec2(screen_pos.x + size, screen_pos.y + size),
                           Config::Colors::SPAWN_INDICATOR_FILL, 4.0f);

  draw_list->AddRect(ImVec2(screen_pos.x, screen_pos.y),
                     ImVec2(screen_pos.x + size, screen_pos.y + size),
                     Config::Colors::SPAWN_INDICATOR_BORDER, 4.0f, 0, 1.0f);

  // "SPAWN" text in center
  // Controlled by LOD Policy
  bool const should_show_text = !is_lod_active_ || LODPolicy::SHOW_SPAWN_LABELS;

  if (should_show_text) {
    const char *text = "SPAWN";
    ImVec2 const textSize = ImGui::CalcTextSize(text);
    glm::vec2 const center(screen_pos.x + size / 2.0f, screen_pos.y + size / 2.0f);
    draw_list->AddText(
        ImVec2(center.x - textSize.x / 2, center.y - textSize.y / 2),
        Config::Colors::SPAWN_INDICATOR_TEXT, text);
  }
}

void SpawnLabelOverlay::renderRadiusBorder(
    ImDrawList *draw_list, const Domain::Position &spawn_pos, int radius,
    const glm::vec2 &camera_pos, const glm::vec2 &viewport_pos,
    const glm::vec2 &viewport_size, float zoom,
    int creature_count) const { // Added creature_count param
  if (radius <= 0)
    return;

  float const tile_size = TILE_SIZE * zoom;

  // Calculate top-left corner of spawn radius area
  Domain::Position const top_left(spawn_pos.x - radius, spawn_pos.y - radius,
                            spawn_pos.z);
  glm::vec2 const screen_top_left = Utils::tileToScreen(
      top_left, camera_pos, viewport_pos, viewport_size, zoom);

  // Calculate the full width/height of the radius box (diameter + 1 for center
  // tile)
  float const box_width = (radius * 2 + 1) * tile_size;
  float const box_height = (radius * 2 + 1) * tile_size;

  // Draw solid border around the entire radius area (Purple)
  draw_list->AddRect(
      ImVec2(screen_top_left.x, screen_top_left.y),
      ImVec2(screen_top_left.x + box_width, screen_top_left.y + box_height),
      Config::Colors::SPAWN_RADIUS_BORDER, 0.0f, 0,
      Config::Colors::SPAWN_RADIUS_BORDER_WIDTH);

  // Render Creature Count Badge if count > 0
  if (creature_count > 0 && (!is_lod_active_ || LODPolicy::SHOW_SPAWN_LABELS)) {
    std::string const count_str = std::format("{}", creature_count);
    const char *text = count_str.c_str();

    // Use larger font for visibility
    float const font_size = Config::Colors::SPAWN_BADGE_FONT_SIZE;
    ImFont *font = ImGui::GetFont();
    // Calculate text size with specific font size
    ImVec2 const textSize = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);

    float const badge_pad_x = Config::Colors::SPAWN_BADGE_PADDING_X;
    float const badge_pad_y = Config::Colors::SPAWN_BADGE_PADDING_Y;

    // Bottom-Right corner position
    float const badge_width = textSize.x + (badge_pad_x * 2);
    float const badge_height = textSize.y + (badge_pad_y * 2);

    float const badge_x = screen_top_left.x + box_width - badge_width;
    float const badge_y = screen_top_left.y + box_height - badge_height;

    ImVec2 const badge_min(badge_x, badge_y);
    ImVec2 const badge_max(screen_top_left.x + box_width,
                     screen_top_left.y + box_height);

    draw_list->AddRectFilled(badge_min, badge_max,
                             Config::Colors::SPAWN_BADGE_BG);

    // Use font-specific AddText overload
    draw_list->AddText(font, font_size,
                       ImVec2(badge_x + badge_pad_x, badge_y + badge_pad_y),
                       Config::Colors::SPAWN_BADGE_TEXT, text);
  }
}

} // namespace Rendering
} // namespace MapEditor
