#include "MapViewCamera.h"
#include "Core/Config.h"
#include "Rendering/Visibility/FloorIterator.h"
#include <algorithm>
#include <cmath>

namespace MapEditor {
namespace UI {

MapViewCamera::MapViewCamera() = default;

void MapViewCamera::setCameraPosition(float x, float y) {
  float tsize = Config::Rendering::TILE_SIZE;
  camera_pos_.x = x;
  camera_pos_.y = y;

  if (map_max_x_ > 0 && map_max_y_ > 0 && viewport_size_.x > 0) {
    float half_vp_x = viewport_size_.x / (2.0f * tsize * zoom_);
    float half_vp_y = viewport_size_.y / (2.0f * tsize * zoom_);
    float max_x = static_cast<float>(map_max_x_);
    float max_y = static_cast<float>(map_max_y_);

    // If viewport is wider than map, center the camera
    float min_x, min_y, clamp_max_x, clamp_max_y;
    if (half_vp_x * 2 >= max_x) {
      min_x = clamp_max_x = max_x / 2.0f;
    } else {
      min_x = half_vp_x;
      clamp_max_x = max_x - half_vp_x;
    }
    if (half_vp_y * 2 >= max_y) {
      min_y = clamp_max_y = max_y / 2.0f;
    } else {
      min_y = half_vp_y;
      clamp_max_y = max_y - half_vp_y;
    }

    camera_pos_.x = std::clamp(camera_pos_.x, min_x, clamp_max_x);
    camera_pos_.y = std::clamp(camera_pos_.y, min_y, clamp_max_y);
  } else {
    // No bounds set — just prevent negative
    camera_pos_.x = std::max(camera_pos_.x, 0.0f);
    camera_pos_.y = std::max(camera_pos_.y, 0.0f);
  }
}

void MapViewCamera::setCameraCenter(const Domain::Position &pos) {
  setCameraPosition(static_cast<float>(pos.x), static_cast<float>(pos.y));
  current_floor_ =
      std::clamp(pos.z, Config::Map::MIN_FLOOR, Config::Map::MAX_FLOOR);
}

void MapViewCamera::setCameraCenter(int32_t x, int32_t y, int16_t z) {
  setCameraCenter(Domain::Position(x, y, z));
}

Domain::Position MapViewCamera::getCameraCenter() const {
  return Domain::Position(static_cast<int32_t>(camera_pos_.x),
                          static_cast<int32_t>(camera_pos_.y), current_floor_);
}

void MapViewCamera::setZoom(float zoom) {
  zoom_ = std::clamp(zoom, Config::Camera::MIN_ZOOM, Config::Camera::MAX_ZOOM);
}

void MapViewCamera::adjustZoom(float delta, const glm::vec2 &pivot_screen) {
  // Get pivot position relative to viewport center
  glm::vec2 pivot_offset = pivot_screen - viewport_pos_ - viewport_size_ * 0.5f;

  // World position at pivot before zoom
  glm::vec2 world_before =
      camera_pos_ + pivot_offset / (Config::Rendering::TILE_SIZE * zoom_);

  // Apply zoom
  float zoom_factor = 1.0f + delta * Config::Camera::ZOOM_SENSITIVITY;
  float new_zoom = std::clamp(zoom_ * zoom_factor, Config::Camera::MIN_ZOOM,
                              Config::Camera::MAX_ZOOM);

  // World position at pivot after zoom
  glm::vec2 world_after =
      camera_pos_ + pivot_offset / (Config::Rendering::TILE_SIZE * new_zoom);

  // Adjust camera to keep pivot pointing at same world position
  camera_pos_ += world_before - world_after;
  zoom_ = new_zoom;

  float cx = camera_pos_.x, cy = camera_pos_.y;
  setCameraPosition(cx, cy);
}

void MapViewCamera::setCurrentFloor(int16_t floor) {
  current_floor_ =
      std::clamp(floor, Config::Map::MIN_FLOOR, Config::Map::MAX_FLOOR);
}

void MapViewCamera::floorUp() {
  if (current_floor_ > Config::Map::MIN_FLOOR) {
    current_floor_--;
  }
}

void MapViewCamera::floorDown() {
  if (current_floor_ < Config::Map::MAX_FLOOR) {
    current_floor_++;
  }
}

void MapViewCamera::setViewport(const glm::vec2 &pos, const glm::vec2 &size) {
  viewport_pos_ = pos;
  viewport_size_ = size;
}

Domain::Position
MapViewCamera::screenToTile(const glm::vec2 &screen_pos) const {
  // Get floor offset to compensate for parallax rendering
  // Tiles are rendered at (world_pos - floor_offset), so we add it back when
  // converting
  float floor_offset =
      Rendering::FloorIterator::getFloorOffset(current_floor_, current_floor_);
  float offset_tiles = floor_offset / Config::Rendering::TILE_SIZE;

  glm::vec2 local = screen_pos - viewport_pos_ - viewport_size_ * 0.5f;
  local /= (Config::Rendering::TILE_SIZE * zoom_);

  int32_t tile_x =
      static_cast<int32_t>(std::floor(camera_pos_.x + local.x + offset_tiles));
  int32_t tile_y =
      static_cast<int32_t>(std::floor(camera_pos_.y + local.y + offset_tiles));

  if (map_max_x_ > 0 && map_max_y_ > 0) {
    tile_x = std::clamp(tile_x, 0, map_max_x_ - 1);
    tile_y = std::clamp(tile_y, 0, map_max_y_ - 1);
  }

  return Domain::Position(tile_x, tile_y, current_floor_);
}

glm::vec2 MapViewCamera::tileToScreen(const Domain::Position &tile_pos) const {
  // Apply floor offset to match rendering parallax effect
  // Tiles on floors 0-6 are rendered shifted diagonally
  float floor_offset =
      Rendering::FloorIterator::getFloorOffset(current_floor_, tile_pos.z);
  float offset_tiles = floor_offset / Config::Rendering::TILE_SIZE;

  glm::vec2 offset(
      static_cast<float>(tile_pos.x) - camera_pos_.x - offset_tiles,
      static_cast<float>(tile_pos.y) - camera_pos_.y - offset_tiles);

  offset *= Config::Rendering::TILE_SIZE * zoom_;
  return viewport_pos_ + viewport_size_ * 0.5f + offset;
}

void MapViewCamera::setMapBounds(int32_t max_x, int32_t max_y) {
  map_max_x_ = max_x;
  map_max_y_ = max_y;
}

} // namespace UI
} // namespace MapEditor
