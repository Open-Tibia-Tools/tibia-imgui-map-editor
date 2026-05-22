#pragma once

#include "Core/Config.h"
#include <cstdint>

namespace MapEditor {

namespace Services {
class ConfigService;
}

namespace Services {

/**
 * Centralized view settings for the map editor.
 * All display toggles are stored here and persisted via ConfigService.
 */
struct ViewSettings {
  // === Core Display ===
  bool show_grid = true;
  bool show_all_floors = false;
  bool ghost_items = false;
  bool ghost_higher_floors = false;
  bool ghost_lower_floors = false; // Render floor+1 at reduced alpha
  bool show_shade = true;

  // === Overlay Toggles ===
  bool show_spawns = true;
  bool show_creatures = true;
  bool show_spawn_radius = true; // Show spawn radius ground tint
  bool simulate_creatures =
      false;                  // Animate creatures walking within spawn radius
  bool show_blocking = false; // Pathing overlay
  bool show_special_tiles = true; // PZ, PVPZONE, etc.
  bool always_show_zones = false; // Always show zones regardless of selection
  bool show_houses = false;
  bool highlight_items = false;
  bool highlight_locked_doors = true;
  bool show_invalid_items = false; // Red overlay on items with invalid IDs

  // === Preview ===
  bool show_ingame_box = false; // 15x11 floating preview window
  bool show_tooltips = true;

  // === Lighting Settings ===
  bool map_lighting_enabled = false;     // Enable lighting in main map viewport
  int map_ambient_light = 128;           // 0 = dark, 255 = full bright
  bool preview_lighting_enabled = false; // Enable lighting in ingame preview
  int preview_ambient_light = 128;       // 0 = dark, 255 = full bright

  // === Placeholders (menu only, no rendering yet) ===
  bool show_minimap_window = false;
  bool show_browse_tile = false;    // Browse Tile dockable window
  bool show_brush_settings = true;  // Tool Options dockable window
  bool show_search_results = false; // Search Results dockable window
  bool show_waypoints = false;
  bool show_wall_hooks = false;
  bool show_wall_outline =
      false; // Orange blocking ground overlay + yellow wall lines
  bool show_towns = false;

  // === Zoom and Floor ===
  float zoom = 1.0f;
  int16_t current_floor = 7; // Ground floor
  float camera_x = 500.0f;   // Camera position for MapPanel sync
  float camera_y = 500.0f;

  // Zoom limits
  static constexpr float MIN_ZOOM = Config::Camera::MIN_ZOOM;
  static constexpr float MAX_ZOOM = Config::Camera::MAX_ZOOM;
  static constexpr float ZOOM_STEP = Config::Camera::ZOOM_STEP;

  // Persistence
  void loadFromConfig(const ConfigService &config);
  void saveToConfig(ConfigService &config) const;

  // Helpers
  void zoomIn();
  void zoomOut();
  void zoomReset();
  void floorUp();
  void floorDown();
};

} // namespace Services
} // namespace MapEditor
