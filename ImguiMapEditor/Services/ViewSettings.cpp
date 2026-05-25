#include "ViewSettings.h"
#include "ConfigService.h"
#include <algorithm>

namespace MapEditor {
namespace Services {

void ViewSettings::loadFromConfig(const ConfigService &config) {
  // Core Display
  show_grid = config.get<bool>("view.show_grid", true);
  show_all_floors = config.get<bool>("view.show_all_floors", false);
  ghost_items = config.get<bool>("view.ghost_items", false);
  ghost_higher_floors = config.get<bool>("view.ghost_higher_floors", false);
  ghost_lower_floors = config.get<bool>("view.ghost_lower_floors", false);
  show_shade = config.get<bool>("view.show_shade", true);

  // Overlays
  show_spawns = config.get<bool>("view.show_spawns", true);
  show_creatures = config.get<bool>("view.show_creatures", true);
  show_spawn_radius = config.get<bool>("view.show_spawn_radius", true);
  show_blocking = config.get<bool>("view.show_blocking", false);
  show_special_tiles = config.get<bool>("view.show_special_tiles", true);
  show_houses = config.get<bool>("view.show_houses", false);
  highlight_items = config.get<bool>("view.highlight_items", false);
  highlight_locked_doors =
      config.get<bool>("view.highlight_locked_doors", true);
  show_invalid_items = config.get<bool>("view.show_invalid_items", false);

  // Preview
  show_ingame_box = config.get<bool>("view.show_ingame_box", false);
  show_tooltips = config.get<bool>("view.show_tooltips", true);

  // Lighting Settings
  map_lighting_enabled = config.get<bool>("view.map_lighting_enabled", false);
  map_ambient_light = config.get<int>("view.map_ambient_light", 255);
  preview_lighting_enabled =
      config.get<bool>("view.preview_lighting_enabled", false);
  preview_ambient_light = config.get<int>("view.preview_ambient_light", 255);

  show_minimap_window = config.get<bool>("view.show_minimap_window", false);
  show_waypoints = config.get<bool>("view.show_waypoints", false);
  show_wall_hooks = config.get<bool>("view.show_wall_hooks", false);
  show_wall_outline = config.get<bool>("view.show_wall_outline", false);
  show_towns = config.get<bool>("view.show_towns", false);
  always_show_zones = config.get<bool>("view.always_show_zones", false);

  // Zoom (don't persist floor - that's per-session)
  zoom = config.get<float>("view.zoom", 1.0f);
}

void ViewSettings::saveToConfig(ConfigService &config) const {
  // Core Display
  config.set("view.show_grid", show_grid);
  config.set("view.show_all_floors", show_all_floors);
  config.set("view.ghost_items", ghost_items);
  config.set("view.ghost_higher_floors", ghost_higher_floors);
  config.set("view.ghost_lower_floors", ghost_lower_floors);
  config.set("view.show_shade", show_shade);

  // Overlays
  config.set("view.show_spawns", show_spawns);
  config.set("view.show_creatures", show_creatures);
  config.set("view.show_spawn_radius", show_spawn_radius);
  config.set("view.show_blocking", show_blocking);
  config.set("view.show_special_tiles", show_special_tiles);
  config.set("view.show_houses", show_houses);
  config.set("view.highlight_items", highlight_items);
  config.set("view.highlight_locked_doors", highlight_locked_doors);
  config.set("view.show_invalid_items", show_invalid_items);

  // Preview
  config.set("view.show_ingame_box", show_ingame_box);
  config.set("view.show_tooltips", show_tooltips);

  // Lighting Settings
  config.set("view.map_lighting_enabled", map_lighting_enabled);
  config.set("view.map_ambient_light", map_ambient_light);
  config.set("view.preview_lighting_enabled", preview_lighting_enabled);
  config.set("view.preview_ambient_light", preview_ambient_light);

  config.set("view.show_minimap_window", show_minimap_window);
  config.set("view.show_waypoints", show_waypoints);
  config.set("view.show_wall_hooks", show_wall_hooks);
  config.set("view.show_wall_outline", show_wall_outline);
  config.set("view.show_towns", show_towns);
  config.set("view.always_show_zones", always_show_zones);

  // Zoom
  config.set("view.zoom", zoom);

  config.save();
}

void ViewSettings::zoomIn() { zoom = std::min(MAX_ZOOM, zoom + ZOOM_STEP); }

void ViewSettings::zoomOut() { zoom = std::max(MIN_ZOOM, zoom - ZOOM_STEP); }

void ViewSettings::zoomReset() { zoom = 1.0f; }

void ViewSettings::floorUp() {
  if (current_floor > 0) {
    current_floor--;
  }
}

void ViewSettings::floorDown() {
  if (current_floor < 15) {
    current_floor++;
  }
}

} // namespace Services
} // namespace MapEditor
