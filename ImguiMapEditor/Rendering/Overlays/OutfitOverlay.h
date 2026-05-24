#pragma once
#include "../../Core/Config.h"
#include "Domain/Outfit.h"
#include "OverlaySpriteCache.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "Utils/SpriteUtils.h"
#include <glm/glm.hpp>
#include <imgui.h>

namespace MapEditor {
namespace Rendering {

/**
 * Renders creature outfits with proper sprite composition.
 * Handles:
 * - Multi-layer sprite rendering (base + addons)
 * - Direction-based sprite selection
 * - Name label rendering
 *
 * Single responsibility: outfit sprite composition and rendering.
 */
class OutfitOverlay {
public:
  OutfitOverlay() = default;

  /**
   * Render an outfit at the given screen position.
   * @param draw_list ImGui draw list
   * @param outfit Outfit data (lookType, colors, addons)
   * @param client_data For outfit sprite lookup
   * @param sprite_manager For colorized outfit textures
   * @param overlay_cache For fallback uncolored textures
   * @param screen_pos Screen position (top-left of tile)
   * @param zoom Current zoom level
   * @param direction Facing direction (0=N, 1=E, 2=S, 3=W), default South
   * @param animation_frame Current animation frame
   * @param tint Color tint for lighting (default white = no tint)
   * @return true if rendered successfully, false if fallback needed
   */
  static bool render(ImDrawList *draw_list, const Domain::Outfit &outfit,
              Services::ClientDataService *client_data,
              Services::SpriteManager *sprite_manager,
              OverlaySpriteCache *overlay_cache, const glm::vec2 &screen_pos,
              float zoom, uint8_t direction = 2, int animation_frame = 0,
              ImU32 tint = IM_COL32(255, 255, 255, 255));

  /**
   * Render creature name label above sprite.
   */
  static void renderName(ImDrawList *draw_list, const std::string &name,
                  const glm::vec2 &center, float sprite_height, float zoom);

private:
  static constexpr float TILE_SIZE = Config::Rendering::TILE_SIZE;
};

} // namespace Rendering
} // namespace MapEditor
