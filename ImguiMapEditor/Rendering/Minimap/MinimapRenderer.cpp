#include "MinimapRenderer.h"
#include "../../Core/Config.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <format>
#include <string>


namespace MapEditor {
namespace Rendering {

MinimapRenderer::MinimapRenderer() {
  display_buffer_.reserve(Config::Performance::MINIMAP_BUFFER_SIZE);
}

void MinimapRenderer::setDataSource(IMinimapDataSource *source) {
  if (data_source_ != source) {
    data_source_ = source;
    // Invalidate all floor caches
    for (auto &cache : floor_caches_) {
      cache.valid = false;
    }
    view_dirty_ = true;
  }
}

void MinimapRenderer::setViewCenter(int32_t x, int32_t y) {
  if (center_x_ != x || center_y_ != y) {
    center_x_ = x;
    center_y_ = y;
    view_dirty_ = true; // Just view changed, cache still valid
  }
}

void MinimapRenderer::setFloor(int16_t floor) {
  floor = std::clamp<int16_t>(floor, Config::Map::MIN_FLOOR,
                              Config::Map::MAX_FLOOR);
  if (floor_ != floor) {
    floor_ = floor;
    view_dirty_ = true; // Just view changed, cache still valid

    // Prune other floors to save memory
    pruneCache(floor_);
  }
}

void MinimapRenderer::zoomIn() {
  if (zoom_level_ > MIN_ZOOM_IN) {
    zoom_level_--;
    view_dirty_ = true;
  }
}

void MinimapRenderer::zoomOut() {
  if (zoom_level_ < MAX_ZOOM_OUT) {
    zoom_level_++;
    view_dirty_ = true;
  }
}

void MinimapRenderer::setZoomLevel(int level) {
  level = std::clamp(level, MIN_ZOOM_IN, MAX_ZOOM_OUT);
  if (zoom_level_ != level) {
    zoom_level_ = level;
    view_dirty_ = true;
  }
}

std::string MinimapRenderer::getZoomString() const {
  if (zoom_level_ <= 0) {
    int magnification = 1 << (-zoom_level_);
    return std::format("x{}", magnification);
  } else {
    int scale = 1 << zoom_level_;
    return std::format("1:{}", scale);
  }
}

float MinimapRenderer::getTilesPerPixel() const {
  if (zoom_level_ <= 0) {
    return std::pow(2.0f, static_cast<float>(zoom_level_));
  } else {
    return static_cast<float>(1 << zoom_level_);
  }
}

void MinimapRenderer::invalidateTile(int32_t x, int32_t y, int16_t z) {
  if (z < 0 || z >= NUM_FLOORS)
    return;

  auto &cache = floor_caches_[z];
  if (!cache.valid)
    return;

  // Update single pixel in cache
  int px = x - cache.origin_x;
  int py = y - cache.origin_y;

  if (px >= 0 && px < cache.width && py >= 0 && py < cache.height) {
    uint8_t color = data_source_ ? data_source_->getTileColor(x, y, z) : 0;
    cache.pixels[py * cache.width + px] =
        color > 0 ? MinimapColorTable::getColor(color) : 0xFF1A1A1A;
    view_dirty_ = true;
  }
}

void MinimapRenderer::rebuildCache() {
  for (auto &cache : floor_caches_) {
    cache.valid = false;
  }
  view_dirty_ = true;
}

void MinimapRenderer::buildFloorCache(int16_t floor) {
  if (!data_source_ || floor < 0 || floor >= NUM_FLOORS)
    return;

  auto &cache = floor_caches_[floor];

  // Get map bounds
  MinimapBounds map_bounds = data_source_->getMapBounds();
  int map_width = map_bounds.getWidth();
  int map_height = map_bounds.getHeight();

  // Limit cache size to prevent memory explosion
  constexpr int MAX_CACHE_SIZE = Config::Rendering::ATLAS_SIZE;
  int cache_width = std::min(map_width, MAX_CACHE_SIZE);
  int cache_height = std::min(map_height, MAX_CACHE_SIZE);

  if (cache_width <= 0 || cache_height <= 0) {
    spdlog::warn("Minimap: Invalid cache size {}x{}", cache_width,
                 cache_height);
    cache.valid = false;
    return;
  }

  // For large maps, CENTER the cache around current view position
  int32_t cache_origin_x, cache_origin_y;
  if (map_width <= MAX_CACHE_SIZE && map_height <= MAX_CACHE_SIZE) {
    // Small map - cache entire map
    cache_origin_x = map_bounds.min_x;
    cache_origin_y = map_bounds.min_y;
  } else {
    // Large map - center cache around current view
    cache_origin_x = center_x_ - cache_width / 2;
    cache_origin_y = center_y_ - cache_height / 2;

    // Clamp to map bounds
    cache_origin_x = std::clamp(cache_origin_x, map_bounds.min_x,
                                map_bounds.max_x - cache_width + 1);
    cache_origin_y = std::clamp(cache_origin_y, map_bounds.min_y,
                                map_bounds.max_y - cache_height + 1);
  }

  spdlog::info("Minimap buildFloorCache: floor={}, cache origin=({},{}), "
               "size={}x{}, center=({},{})",
               floor, cache_origin_x, cache_origin_y, cache_width, cache_height,
               center_x_, center_y_);

  cache.width = cache_width;
  cache.height = cache_height;
  cache.origin_x = cache_origin_x;
  cache.origin_y = cache_origin_y;
  cache.pixels.resize(cache_width * cache_height);

  // Fill cache with tile colors - ONE TIME COST
  const uint32_t bg_color =
      Config::Colors::MAP_BACKGROUND; // Dark gray background

  for (int y = 0; y < cache_height; ++y) {
    for (int x = 0; x < cache_width; ++x) {
      int32_t world_x = cache_origin_x + x;
      int32_t world_y = cache_origin_y + y;

      uint8_t color = data_source_->getTileColor(world_x, world_y, floor);
      cache.pixels[y * cache_width + x] =
          color > 0 ? MinimapColorTable::getColor(color) : bg_color;
    }
  }

  cache.valid = true;
  spdlog::info("Minimap: Floor {} cache built at ({},{}) size {}x{}", floor,
               cache_origin_x, cache_origin_y, cache_width, cache_height);
  cache.valid = true;
  spdlog::info("Minimap: Floor {} cache built at ({},{}) size {}x{}", floor,
               cache_origin_x, cache_origin_y, cache_width, cache_height);
}

void MinimapRenderer::pruneCache(int16_t keep_floor) {
  size_t bytes_freed = 0;
  int floors_cleared = 0;

  for (int z = 0; z < NUM_FLOORS; ++z) {
    if (z == keep_floor)
      continue;

    auto &cache = floor_caches_[z];
    if (cache.valid || !cache.pixels.empty()) {
      size_t size = cache.pixels.capacity() * sizeof(uint32_t);
      bytes_freed += size;

      // Force memory release
      std::vector<uint32_t>().swap(cache.pixels);
      cache.valid = false;
      floors_cleared++;
    }
  }

  if (floors_cleared > 0) {
    spdlog::info(
        "[MinimapRenderer] Pruned {} floors. Freed {:.2f} MB. Keeping floor {}",
        floors_cleared, bytes_freed / (1024.0 * 1024.0), keep_floor);
  }
}

void MinimapRenderer::update(int view_width, int view_height) {
  if (!data_source_)
    return;

  // Resize display texture if needed
  bool size_changed =
      (view_width != view_width_ || view_height != view_height_);
  if (size_changed) {
    view_width_ = view_width;
    view_height_ = view_height;

    if (!display_texture_.isValid() ||
        display_texture_.getWidth() != view_width ||
        display_texture_.getHeight() != view_height) {
      display_texture_.create(view_width, view_height);
    }
    display_buffer_.resize(view_width * view_height);
    view_dirty_ = true;
  }

  // Ensure current floor cache is built
  if (!floor_caches_[floor_].valid) {
    buildFloorCache(floor_);
  }

  if (!view_dirty_)
    return;

  // Calculate view bounds
  float tiles_per_pixel = getTilesPerPixel();
  float tiles_visible_x = view_width * tiles_per_pixel;
  float tiles_visible_y = view_height * tiles_per_pixel;

  view_bounds_.min_x = center_x_ - static_cast<int32_t>(tiles_visible_x / 2);
  view_bounds_.min_y = center_y_ - static_cast<int32_t>(tiles_visible_y / 2);
  view_bounds_.max_x =
      view_bounds_.min_x + static_cast<int32_t>(tiles_visible_x);
  view_bounds_.max_y =
      view_bounds_.min_y + static_cast<int32_t>(tiles_visible_y);

  // Render view from cache - FAST, just sampling cached pixels
  renderViewFromCache();

  display_texture_.updateFull(display_buffer_.data());
  view_dirty_ = false;
}

void MinimapRenderer::renderViewFromCache() {
  const auto &cache = floor_caches_[floor_];
  const uint32_t bg_color =
      Config::Colors::MAP_BACKGROUND; // Dark gray background

  if (!cache.valid) {
    std::fill(display_buffer_.begin(), display_buffer_.end(), bg_color);
    return;
  }

  float tiles_per_pixel = getTilesPerPixel();

  for (int py = 0; py < view_height_; ++py) {
    for (int px = 0; px < view_width_; ++px) {
      // Map display pixel to world coordinate
      float world_x_f =
          view_bounds_.min_x + px * tiles_per_pixel + tiles_per_pixel * 0.5f;
      float world_y_f =
          view_bounds_.min_y + py * tiles_per_pixel + tiles_per_pixel * 0.5f;

      // Map world coordinate to cache pixel
      int cache_x = static_cast<int>(world_x_f) - cache.origin_x;
      int cache_y = static_cast<int>(world_y_f) - cache.origin_y;

      if (cache_x >= 0 && cache_x < cache.width && cache_y >= 0 &&
          cache_y < cache.height) {
        display_buffer_[py * view_width_ + px] =
            cache.pixels[cache_y * cache.width + cache_x];
      } else {
        // Outside cache bounds - show background
        display_buffer_[py * view_width_ + px] = bg_color;
      }
    }
  }
}

void MinimapRenderer::screenToWorld(int screen_x, int screen_y,
                                    int32_t &world_x, int32_t &world_y) const {
  float tiles_per_pixel = getTilesPerPixel();
  world_x =
      view_bounds_.min_x +
      static_cast<int32_t>(screen_x * tiles_per_pixel + tiles_per_pixel * 0.5f);
  world_y =
      view_bounds_.min_y +
      static_cast<int32_t>(screen_y * tiles_per_pixel + tiles_per_pixel * 0.5f);
}

} // namespace Rendering
} // namespace MapEditor
