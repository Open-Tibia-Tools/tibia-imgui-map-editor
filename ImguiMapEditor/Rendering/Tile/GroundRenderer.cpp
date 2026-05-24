#include "GroundRenderer.h"
#include "../../Core/Config.h"
#include "Services/ClientDataService.h"
#include "Services/SecondaryClientConstants.h"
#include "Services/SecondaryClientData.h"
#include "../ColorFilter.h"
#include "ItemRenderer.h"
namespace MapEditor {
namespace Rendering {

GroundRenderer::GroundRenderer(ItemRenderer &item_renderer,
                               Services::ClientDataService *client_data)
    : item_renderer_(item_renderer), client_data_(client_data) {}

bool GroundRenderer::queue(const Domain::Item *ground, float screen_x,
                           float screen_y, float size, int tile_x, int tile_y,
                           int tile_z, const AnimationTicks &anim_ticks,
                           const TileColor &color, float alpha,
                           bool is_selected,
                           const Services::ViewSettings *view_settings,
                           std::vector<uint32_t> &missing_sprites,
                           float *accumulated_elevation) {
  if (!ground)
    return false;

  const auto *item_type = ground->getType();
  if (!item_type && client_data_) {
    item_type = client_data_->getItemTypeByServerId(ground->getServerId());
  }

  // Secondary client fallback - ONLY if show_invalid_items is enabled
  bool is_from_secondary = false;
  bool const show_invalid = view_settings && view_settings->show_invalid_items;
  auto *sec_client = secondary_client_.get();

  if (!item_type && show_invalid && sec_client && sec_client->isActive()) {
    item_type = sec_client->getItemTypeByServerId(ground->getServerId());
    is_from_secondary = (item_type != nullptr);
  }

  // Item is invalid if:
  // 1. item_type is null (ID not in items.otb at all), OR
  // 2. item_type exists but has no client_id (gap entry in items.otb)
  bool const is_invalid = (item_type == nullptr) || !item_type->isValidForRendering();

  if (item_type && !item_type->sprite_ids.empty()) {
    // Check for blocking ground transparency
    float final_alpha = alpha;
    if (view_settings && view_settings->show_wall_outline) {
      bool const is_blocking_ground =
          item_type->hasFlag(Domain::ItemFlag::Unpassable) &&
          item_type->hasFlag(Domain::ItemFlag::BlockMissiles) &&
          !item_type->hasFlag(Domain::ItemFlag::Moveable) &&
          item_type->top_order == 0 &&
          !item_type->hasFlag(Domain::ItemFlag::FullTile);
      if (is_blocking_ground) {
        final_alpha = 0.5f;
      }
    }

    // Calculate final color
    TileColor final_color = color;

    // Selection tinting (RME-style: halve all channels)
    if (is_selected) {
      final_color.r *= 0.5f;
      final_color.g *= 0.5f;
      final_color.b *= 0.5f;
    }

    // Secondary client tinting (red tint)
    if (is_from_secondary && sec_client) {
      float const tint = sec_client->getTintIntensity();
      final_color.g *= (1.0f - tint);
      final_color.b *= (1.0f - tint);
      final_alpha *= sec_client->getAlphaMultiplier();
    }

    uint32_t const sprite_offset =
        is_from_secondary ? Services::SECONDARY_SPRITE_OFFSET : 0;

    // Delegate to ItemRenderer for actual sprite queueing
    item_renderer_.queueWithColor(
        item_type, screen_x, screen_y, size, tile_x, tile_y, tile_z, anim_ticks,
        missing_sprites, final_color.r, final_color.g, final_color.b,
        final_alpha, accumulated_elevation, ground, sprite_offset);
    return true;
  } else if (is_invalid && show_invalid) {
    item_renderer_.queueInvalidPlaceholder(
        screen_x, screen_y, size, alpha, Config::Colors::INVALID_GROUND_R,
        Config::Colors::INVALID_GROUND_G, Config::Colors::INVALID_GROUND_B);
    return true;
  }

  return false;
}

} // namespace Rendering
} // namespace MapEditor
