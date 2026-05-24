#include "Rendering/Passes/IngamePreviewRenderer.h"
#include "Core/Config.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "Services/ViewSettings.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Rendering {

IngamePreviewRenderer::IngamePreviewRenderer(
    TileRenderer &tile_renderer, SpriteBatch &sprite_batch,
    Services::SpriteManager &sprite_manager,
    Services::ClientDataService *client_data)
    : tile_renderer_(tile_renderer), sprite_batch_(sprite_batch),
      sprite_manager_(sprite_manager), client_data_(client_data) {
  // Initialize floor visibility calculator (OTClient algorithm)
  floor_calculator_ = std::make_unique<FloorVisibilityCalculator>(client_data_);

  // Initialize light system
  light_manager_ = std::make_unique<LightManager>(client_data_);

  if (!light_manager_->initialize()) {
    spdlog::warn("IngamePreviewRenderer: Failed to init light manager");
  }
}

void IngamePreviewRenderer::render(const Domain::ChunkedMap &map,
                                   int viewport_width, int viewport_height,
                                   float camera_x, float camera_y, int floor,
                                   float zoom,
                                   Services::ViewSettings *view_settings) {
  if (viewport_width < 1 || viewport_height < 1)
    return;

  // Clear the bound FBO
  glDisable(GL_SCISSOR_TEST);
  glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  updateProjection(viewport_width, viewport_height);

  // Initialize frame time if first run
  auto now = std::chrono::steady_clock::now();
  if (last_frame_time_.time_since_epoch().count() == 0) {
    last_frame_time_ = now;
  }
  double const dt = std::chrono::duration<double>(now - last_frame_time_).count();
  last_frame_time_ = now;

  // Calculate animation ticks
  int64_t const frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch())
                           .count();
  AnimationTicks anim_ticks = AnimationTicks::calculate(frame_time);

  // Camera position as integers for floor calculations
  int camera_z = floor;
  int const cam_x = static_cast<int>(camera_x);
  int const cam_y = static_cast<int>(camera_y);

  // Calculate visible floors using OTClient algorithm
  int const first_visible =
      floor_calculator_->calcFirstVisibleFloor(map, cam_x, cam_y, camera_z);
  int const last_visible = floor_calculator_->calcLastVisibleFloor(camera_z);

  // Teleport detection: check if we moved far or changed floors
  bool teleported = false;
  if (last_player_z_ == -1) {
    // First frame - initialize all fading progress
    for (int z = 0; z <= FloorConstants::MAX_Z; ++z) {
      fading_floor_progress_[z] =
          (z >= first_visible && z <= last_visible) ? 1.0 : 0.0;
    }
    teleported = true;
  } else {
    // Check for teleport (moved >= 3 tiles or significant floor change)
    int const dx = std::abs(cam_x - last_camera_x_);
    int const dy = std::abs(cam_y - last_camera_y_);
    int const dz = std::abs(camera_z - last_player_z_);

    if (dx >= Config::Preview::TELEPORT_THRESHOLD ||
        dy >= Config::Preview::TELEPORT_THRESHOLD ||
        dz >= Config::Preview::TELEPORT_THRESHOLD) {
      // Teleport detected - snap fading immediately
      for (int z = 0; z <= FloorConstants::MAX_Z; ++z) {
        fading_floor_progress_[z] =
            (z >= first_visible && z <= last_visible) ? 1.0 : 0.0;
      }
      teleported = true;
    }
  }

  // Update fading progress towards target (smooth fade when not teleporting)
  if (!teleported) {
    for (int z = 0; z <= FloorConstants::MAX_Z; ++z) {
      double const target = (z >= first_visible && z <= last_visible) ? 1.0 : 0.0;
      double current = fading_floor_progress_[z];

      if (current < target) {
        current += dt / Config::Preview::FADE_DURATION;
        if (current > target)
          current = target;
      } else if (current > target) {
        current -= dt / Config::Preview::FADE_DURATION;
        if (current < target)
          current = target;
      }
      fading_floor_progress_[z] = current;
    }
  }

  // Store current position for next frame
  last_player_z_ = camera_z;
  last_camera_x_ = cam_x;
  last_camera_y_ = cam_y;
  cached_first_visible_floor_ = first_visible;

  // Calculate visible tile range
  float tile_size = TileRenderer::TILE_SIZE;
  float const tiles_x = viewport_width / (tile_size * zoom);
  float const tiles_y = viewport_height / (tile_size * zoom);

  int const start_x = static_cast<int>(std::floor(camera_x - tiles_x / 2)) - 1;
  int const end_x = static_cast<int>(std::ceil(camera_x + tiles_x / 2)) + 2;
  int const start_y = static_cast<int>(std::floor(camera_y - tiles_y / 2)) - 1;
  int const end_y = static_cast<int>(std::ceil(camera_y + tiles_y / 2)) + 2;

  // Underground View Alignment (Z > 7)
  float underground_offset_x = 0.0f;
  float underground_offset_y = 0.0f;
  if (camera_z > FloorConstants::SEA_FLOOR) {
    int const off_x = (static_cast<int>(camera_x) % 2);
    int const off_y = (static_cast<int>(camera_y) % 2);
    underground_offset_x = static_cast<float>(off_x) * tile_size * zoom;
    underground_offset_y = static_cast<float>(off_y) * tile_size * zoom;
  }

  std::vector<uint32_t> missing_sprites;
  missing_sprites.reserve(64);

  glm::mat4 const mvp = projection_ * view_;
  sprite_batch_.begin(mvp);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Find render range based on fading progress
  int const render_start_z = last_visible;
  int render_end_z = 0;
  for (int z = 0; z <= last_visible; ++z) {
    if (fading_floor_progress_[z] > 0.001) {
      render_end_z = z;
      break;
    }
  }

  // Render floors from bottom (deepest) to top (surface)
  for (int z = render_start_z; z >= render_end_z; --z) {
    float floor_alpha = static_cast<float>(fading_floor_progress_[z]);
    if (floor_alpha <= 0.0f)
      continue;

    // Get visible chunks for this floor
    std::vector<Domain::Chunk *> visible_chunks;
    map.getVisibleChunks(start_x, start_y, end_x, end_y,
                         static_cast<int16_t>(z), visible_chunks);

    for (Domain::Chunk *chunk : visible_chunks) {
      if (!chunk)
        continue;

      // Transform world bounds to chunk-local space
      int const local_min_x = start_x - chunk->world_x;
      int const local_min_y = start_y - chunk->world_y;
      int const local_max_x = end_x - chunk->world_x;
      int const local_max_y = end_y - chunk->world_y;

      // Use Diagonal Region Iteration for correct depth sorting AND culling
      // (Fixes "Ingame Preview depth issues" and "Iterating invisible tiles")
      chunk->forEachTileDiagonalInRegion(
          local_min_x, local_min_y, local_max_x, local_max_y,
          [&](const Domain::Tile *tile, int lx, int ly) {
            // Reconstruct world coordinates from chunk + local
            int const tile_x = chunk->world_x + lx;
            int const tile_y = chunk->world_y + ly;

            // Calculate screen position
            float const screen_x =
                (tile_x - camera_x) * tile_size * zoom + viewport_width / 2.0f;

            // Z offset for floor perspective (OTClient style)
            float const z_offset =
                static_cast<float>(camera_z - z) * tile_size * zoom;

            float const final_screen_x = screen_x - z_offset + underground_offset_x;
            float const final_screen_y = (tile_y - camera_y) * tile_size * zoom +
                                   viewport_height / 2.0f - z_offset +
                                   underground_offset_y;

            // Use explicit coordinate override to avoid redundant getX/Y()
            tile_renderer_.queueTile(*tile, tile_x, tile_y, z, final_screen_x,
                                     final_screen_y, zoom, anim_ticks,
                                     missing_sprites, nullptr, floor_alpha);
          });
    }
  }

  // Request async load for missing sprites
  if (!missing_sprites.empty()) {
    sprite_manager_.requestSpritesAsync(missing_sprites);
  }

  sprite_batch_.end(sprite_manager_.getAtlasManager());
  glDisable(GL_BLEND);

  // Apply lighting if enabled
  bool const lighting_enabled =
      view_settings && view_settings->preview_lighting_enabled;
  if (lighting_enabled && light_manager_ && client_data_) {
    Domain::LightConfig config;
    config.enabled = true;
    config.ambient_level =
        static_cast<uint8_t>(view_settings->preview_ambient_light);
    config.ambient_color = 215;

    if (config.ambient_level != last_ambient_light_) {
      light_manager_->invalidateAll();
      last_ambient_light_ = config.ambient_level;
    }

    light_manager_->render(map, viewport_width, viewport_height, camera_x,
                           camera_y, zoom, floor, 
                           render_start_z, render_end_z,  // Floor range from fading calculation
                           config);
  }
}

void IngamePreviewRenderer::updateProjection(int width, int height) {
  projection_ = glm::ortho(0.0f, static_cast<float>(width),
                           static_cast<float>(height), 0.0f, -1.0f, 1.0f);
  view_ = glm::mat4(1.0f);
}

} // namespace Rendering
} // namespace MapEditor
