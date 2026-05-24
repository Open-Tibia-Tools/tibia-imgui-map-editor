#include "OutfitOverlay.h"
#include "../../Core/OutfitColors.h"
#include "Core/Config.h"
#include "OverlaySpriteCache.h"
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Rendering {

bool OutfitOverlay::render(ImDrawList *draw_list, const Domain::Outfit &outfit,
                           Services::ClientDataService *client_data,
                           Services::SpriteManager *sprite_manager,
                           OverlaySpriteCache *overlay_cache,
                           const glm::vec2 &screen_pos, float zoom,
                           uint8_t direction, int animation_frame, ImU32 tint) {
  if (!draw_list || !client_data || !sprite_manager)
    return false;
  if (outfit.lookType == 0)
    return false;

  // Get outfit data from DAT
  const IO::ClientItem *outfit_data =
      client_data->getOutfitData(outfit.lookType);
  if (!outfit_data || outfit_data->sprite_ids.empty()) {
    return false;
  }

  // Get dimensions
  int const width = std::max<int>(1, outfit_data->width);
  int const height = std::max<int>(1, outfit_data->height);
  int const layers = std::max<int>(1, outfit_data->layers);
  int const pattern_x = std::max<int>(1, outfit_data->pattern_x); // directions
  int const pattern_y = std::max<int>(1, outfit_data->pattern_y); // addons
  int const pattern_z = std::max<int>(1, outfit_data->pattern_z); // mount (0/1)
  int const frames = std::max<int>(1, outfit_data->frames);

  float const tile_size = Config::Rendering::TILE_SIZE * zoom;

  // Apply displacement offset from DAT (center creature within tile)
  float const offset_x =
      outfit_data->has_offset ? outfit_data->offset_x * zoom : 0.0f;
  float const offset_y =
      outfit_data->has_offset ? outfit_data->offset_y * zoom : 0.0f;

  // Direction maps to pattern_x (0=S, 1=E, 2=N, 3=W in Tibia)
  int dir = 2 % pattern_x; // Default to South-facing
  if (direction < pattern_x) {
    dir = direction;
  }
  // Use animation_frame modulo available frames (clamp to valid range)
  int const frame = (frames > 1) ? (animation_frame % frames) : 0;
  int const mount_z = 0; // no mount

  // Check if outfit has template layer (layers >= 2)
  bool const has_template = (layers >= 2);

  bool any_rendered = false;

  // Render base outfit (addon pattern 0) and addons
  for (int addon_y = 0; addon_y < pattern_y; ++addon_y) {
    // Skip addons we don't have (addon_y=0 is base, 1=addon1, 2=addon2)
    if (addon_y > 0 && !(outfit.lookAddons & (1 << (addon_y - 1)))) {
      continue;
    }

    // Render all parts of this outfit layer (for multi-tile outfits)
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        // Get base sprite index (layer 0)
        uint32_t const base_sprite_idx = Utils::SpriteUtils::getSpriteIndex(
            outfit_data, w, h, 0, dir, addon_y, mount_z, frame);

        if (base_sprite_idx >= outfit_data->sprite_ids.size()) {
          continue;
        }

        uint32_t const base_sprite_id = outfit_data->sprite_ids[base_sprite_idx];
        if (base_sprite_id == 0)
          continue;

        Rendering::Texture  const*texture = nullptr;

        if (has_template) {
          // Get template sprite index (layer 1)
          uint32_t const template_sprite_idx = Utils::SpriteUtils::getSpriteIndex(
              outfit_data, w, h, 1, dir, addon_y, mount_z, frame);
          uint32_t template_sprite_id = 0;
          if (template_sprite_idx < outfit_data->sprite_ids.size()) {
            template_sprite_id = outfit_data->sprite_ids[template_sprite_idx];
          }

          // Get colorized texture
          texture = sprite_manager->getCreatureSpriteService()
                        .getColorizedOutfitSprite(
                            base_sprite_id, template_sprite_id,
                            static_cast<uint8_t>(outfit.lookHead),
                            static_cast<uint8_t>(outfit.lookBody),
                            static_cast<uint8_t>(outfit.lookLegs),
                            static_cast<uint8_t>(outfit.lookFeet));
        }

        // Fall back to uncolored sprite if colorization failed or no template
        if (!texture) {
          texture = &overlay_cache->getTextureOrPlaceholder(base_sprite_id);
        }

        if (!texture)
          continue;

        // Position: multi-tile sprites draw offset by (width-w-1) and
        // (height-h-1) tiles Apply displacement offset from DAT to center
        // creature properly
        float draw_x = screen_pos.x - offset_x + (width - w - 1) * tile_size;
        float draw_y = screen_pos.y - offset_y + (height - h - 1) * tile_size;

        // Multi-tile: adjust base position so creature is centered
        if (width > 1 || height > 1) {
          draw_x -= (width - 1) * tile_size;
          draw_y -= (height - 1) * tile_size;
        }

        draw_list->AddImage(
            (ImTextureID)(intptr_t)texture->id(), ImVec2(draw_x, draw_y),
            ImVec2(draw_x + tile_size, draw_y + tile_size), ImVec2(0, 0),
            ImVec2(1, 1), // UV coords (full texture)
            tint          // Apply lighting tint
        );

        any_rendered = true;
      }
    }
  }

  return any_rendered;
}

void OutfitOverlay::renderName(ImDrawList *draw_list, const std::string &name,
                               const glm::vec2 &center, float sprite_height,
                               float zoom) {
  if (!draw_list || name.empty())
    return;

  if (!draw_list || name.empty())
    return;

  // Zoom check removed - controlled by caller (SpawnLabelOverlay + LOD Policy)

  ImVec2 const text_size = ImGui::CalcTextSize(name.c_str());

  // Position above the sprite
  ImVec2 const text_pos(center.x - text_size.x / 2.0f,
                  center.y - sprite_height / 2.0f - text_size.y - 4.0f);

  // Background for readability
  float const padding = 2.0f;
  draw_list->AddRectFilled(
      ImVec2(text_pos.x - padding, text_pos.y - 1),
      ImVec2(text_pos.x + text_size.x + padding, text_pos.y + text_size.y + 1),
      IM_COL32(0, 0, 0, 180),
      2.0f // Rounded corners
  );

  // Draw text
  draw_list->AddText(text_pos, Config::Colors::SPAWN_TEXT, name.c_str());
}

} // namespace Rendering
} // namespace MapEditor
