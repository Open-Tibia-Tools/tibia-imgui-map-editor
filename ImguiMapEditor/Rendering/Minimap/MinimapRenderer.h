#pragma once
#include "IMinimapDataSource.h"
#include "MinimapColorTable.h"
#include "MinimapTexture.h"
#include <array>
#include <memory>
#include <vector>
#include <string>


namespace MapEditor {
namespace Rendering {

/**
 * Optimized minimap renderer with floor-level texture caching.
 *
 * PERFORMANCE STRATEGY:
 * - Pre-renders entire map for each floor into cached textures (one-time cost)
 * - Viewing just samples from cached textures (O(1) per frame)
 * - Only re-renders individual tiles when map is edited
 * - Camera movement = zero recalculation
 */
class MinimapRenderer {
public:
  static constexpr int NUM_FLOORS = 16;
  static constexpr int MIN_ZOOM_IN = -3; // x8 magnification
  static constexpr int MAX_ZOOM_OUT = 4; // 1:16 overview

  MinimapRenderer();
  ~MinimapRenderer() = default;

  /**
   * Set the data source and trigger full cache rebuild
   */
  void setDataSource(IMinimapDataSource *source);

  /**
   * Set view center - CHEAP, just updates view position
   */
  void setViewCenter(int32_t x, int32_t y);

  /**
   * Set current floor - CHEAP, just switches which cache to use
   */
  void setFloor(int16_t floor);
  int16_t getFloor() const { return floor_; }

  /**
   * Zoom controls
   */
  void zoomIn();
  void zoomOut();
  int getZoomLevel() const { return zoom_level_; }
  void setZoomLevel(int level); // For state restoration
  std::string getZoomString() const;

  // Center position accessors for state save/restore
  int32_t getCenterX() const { return center_x_; }
  int32_t getCenterY() const { return center_y_; }

  /**
   * Update single tile in cache (for map editing)
   */
  void invalidateTile(int32_t x, int32_t y, int16_t z);

  /**
   * Force rebuild of all floor caches
   */
  void rebuildCache();

  /**
   * Release memory for floors other than the active one.
   * Reduces RAM usage by ~1GB for large maps.
   */
  void pruneCache(int16_t keep_floor);

  /**
   * Render current view to display texture
   * Only blits from cache - very fast
   */
  void update(int view_width, int view_height);

  /**
   * Get the OpenGL texture for rendering
   */
  uint32_t getTextureId() const { return display_texture_.getTextureId(); }

  int getTextureWidth() const { return display_texture_.getWidth(); }
  int getTextureHeight() const { return display_texture_.getHeight(); }

  MinimapBounds getViewBounds() const { return view_bounds_; }

  void screenToWorld(int screen_x, int screen_y, int32_t &world_x,
                     int32_t &world_y) const;

private:
  void buildFloorCache(int16_t floor);
  void renderViewFromCache();
  float getTilesPerPixel() const;

  IMinimapDataSource *data_source_ = nullptr;

  // Cached floor textures - one per floor, covering entire map bounds
  struct FloorCache {
    std::vector<uint32_t> pixels;
    int width = 0;
    int height = 0;
    int32_t origin_x = 0;
    int32_t origin_y = 0;
    bool valid = false;
  };
  std::array<FloorCache, NUM_FLOORS> floor_caches_;

  // Display texture - what gets shown in ImGui
  MinimapTexture display_texture_;
  std::vector<uint32_t> display_buffer_;

  // View state
  int32_t center_x_ = 0;
  int32_t center_y_ = 0;
  int16_t floor_ = 7;
  int zoom_level_ = 0;

  // Display dimensions
  int view_width_ = 0;
  int view_height_ = 0;
  MinimapBounds view_bounds_;

  bool view_dirty_ = true; // Only for view changes, not cache rebuilds
};

} // namespace Rendering
} // namespace MapEditor
