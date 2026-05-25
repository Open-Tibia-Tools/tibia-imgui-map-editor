// Config.h - Centralized Configuration Constants
// Single source of truth for all magic numbers and configuration values
#pragma once

#include <cstddef>
#include <cstdint>

namespace MapEditor::Config {

// ============================================================================
// RENDERING CONFIGURATION
// ============================================================================
namespace Rendering {
// Tile/Sprite dimensions (Tibia standard)
inline constexpr int TILE_DIMENSION = 32;
inline constexpr float TILE_SIZE = static_cast<float>(TILE_DIMENSION);
inline constexpr int TILE_SIZE_INT = TILE_DIMENSION;
inline constexpr uint32_t SPRITE_SIZE = TILE_DIMENSION;
inline constexpr uint32_t SPRITE_BYTES = SPRITE_SIZE * SPRITE_SIZE * 4; // RGBA

// Texture Atlas configuration
inline constexpr int ATLAS_SIZE = 4096;
inline constexpr int SPRITES_PER_ROW = ATLAS_SIZE / SPRITE_SIZE; // 128
inline constexpr int SPRITES_PER_LAYER =
    SPRITES_PER_ROW * SPRITES_PER_ROW; // 16384
inline constexpr int MAX_ATLAS_LAYERS = 64;

// Wall outline rendering
inline constexpr float WALL_OUTLINE_WIDTH = 2.0f;
inline constexpr float WALL_HOOK_COLOR_R = 1.0f;
inline constexpr float WALL_HOOK_COLOR_G = 0.65f;
inline constexpr float WALL_HOOK_COLOR_B = 0.0f;
inline constexpr float WALL_HOOK_COLOR_A = 0.4f;
inline constexpr float WALL_CONN_COLOR_R = 1.0f;
inline constexpr float WALL_CONN_COLOR_G = 1.0f;
inline constexpr float WALL_CONN_COLOR_B = 0.0f;
inline constexpr float WALL_CONN_COLOR_A = 0.8f;

// Viewport
inline constexpr struct {
  float r, g, b, a;
} VIEWPORT_CLEAR = {0.12f, 0.12f, 0.12f, 1.0f};

// Thresholds
inline constexpr float ANIMATION_ZOOM_THRESHOLD = 0.2f;
inline constexpr float GHOST_ITEM_ALPHA = 0.5f;
inline constexpr int GHOST_ALPHA_INT = 128;
inline constexpr float DEFAULT_SHADE_ALPHA = 0.5f;

// Colorized outfit sprite ID offset (ensures no collision with normal sprite
// IDs)
inline constexpr uint32_t COLORIZED_OUTFIT_OFFSET = 2'000'000'000;

// Level of Detail (LOD) Configuration
namespace LOD {
inline constexpr float THRESHOLD = 0.20f; // Global trigger for LOD mode
}
} // namespace Rendering

// ============================================================================
// CAMERA CONFIGURATION
// ============================================================================
namespace Camera {
inline constexpr float MIN_ZOOM = 0.025f;
inline constexpr float MAX_ZOOM = 4.0f;
inline constexpr float ZOOM_STEP = 0.1f;
inline constexpr float ZOOM_SENSITIVITY = 0.15f;
// Default camera position
inline constexpr float DEFAULT_CENTER_X = 1000.0f;
inline constexpr float DEFAULT_CENTER_Y = 1000.0f;
inline constexpr int16_t DEFAULT_CENTER_Z = 7;
} // namespace Camera

// ============================================================================
// MAP CONFIGURATION
// ============================================================================
namespace Map {
inline constexpr uint16_t DEFAULT_WIDTH = 1024;
inline constexpr uint16_t DEFAULT_HEIGHT = 1024;
inline constexpr int MIN_SIZE = 64;
inline constexpr int MAX_SIZE = 65535;
inline constexpr int16_t MIN_FLOOR = 0;
inline constexpr int16_t MAX_FLOOR = 15;
inline constexpr int GROUND_LAYER = 7;
} // namespace Map

// ============================================================================
// PERFORMANCE CONFIGURATION
// ============================================================================
namespace Performance {
// Sprite batching
inline constexpr size_t MAX_SPRITES_PER_BATCH = 1400000;
inline constexpr int MAX_ATLASES = 64;

// PBO upload
inline constexpr int MAX_SPRITES_PER_UPLOAD = 256;
inline constexpr size_t PBO_SIZE =
    Rendering::SPRITE_BYTES * MAX_SPRITES_PER_UPLOAD;
inline constexpr int PBO_COUNT = 2;

// Ring buffer
inline constexpr size_t RING_BUFFER_COUNT = 3;

// Multi-draw indirect
inline constexpr int MAX_DRAW_COMMANDS = 16;

// Chunk/cache sizing
inline constexpr int CHUNK_SIZE = 32;
inline constexpr int LIGHT_CACHE_SIZE = 32;
inline constexpr float CACHE_ZOOM_THRESHOLD = 0.2f;
inline constexpr float OVERLAY_ZOOM_THRESHOLD =
    0.2f; // Hide detailed overlays at very low zoom

// Reserve capacities
inline constexpr size_t WALL_VERTICES_RESERVE = 512;
inline constexpr size_t MINIMAP_BUFFER_SIZE = 256 * 256;
inline constexpr size_t TOOLTIP_TEXT_RESERVE = 128;
inline constexpr size_t MISSING_SPRITES_RESERVE = 64;

// Visible chunk estimate for sprite batch pre-allocation
inline constexpr size_t DEFAULT_VISIBLE_CHUNK_ESTIMATE = 100;

// Async sprite loading
inline constexpr size_t SPRITE_LOADER_THREADS = 4;

// Fence synchronization
inline constexpr int32_t MAX_FENCE_WAIT_RETRIES = 1000;
inline constexpr uint64_t FENCE_WAIT_TIMEOUT_NS = 1000000; // 1ms
} // namespace Performance

// ============================================================================
// INPUT CONFIGURATION
// ============================================================================
namespace Input {
// Drag requires BOTH time (0.1s) AND distance (10 pixels) to activate
inline constexpr float DRAG_THRESHOLD_SQ = 100.0f;       // 10 pixels squared
inline constexpr float SHIFT_DRAG_THRESHOLD_SQ = 100.0f; // 10 pixels squared
inline constexpr double DRAG_DELAY_SECONDS = 0.1;
// Lasso drag configuration
inline constexpr float LASSO_DRAG_POINT_DISTANCE_SQ = 64.0f; // 8 pixels squared
} // namespace Input

// ============================================================================
// UI CONFIGURATION
// ============================================================================
namespace UI {
// Path buffer for text inputs
inline constexpr int PATH_BUFFER_SIZE = 512;

// Dialog button widths
inline constexpr float DIALOG_BUTTON_WIDTH = 100.0f;
inline constexpr float TAB_BUTTON_WIDTH = 120.0f;

// Window sizes (width, height)
inline constexpr float WELCOME_WINDOW_W = 600.0f;
inline constexpr float WELCOME_WINDOW_H = 450.0f;
inline constexpr float MINIMAP_WINDOW_W = 250.0f;
inline constexpr float MINIMAP_WINDOW_H = 280.0f;
inline constexpr float ITEM_PROPS_WINDOW_W = 350.0f;
inline constexpr float ITEM_PROPS_WINDOW_H = 300.0f;
inline constexpr float PROJECT_DIALOG_W = 550.0f;
inline constexpr float PROJECT_DIALOG_H = 480.0f;
inline constexpr float PROJECT_DIALOG_OPEN_W = 900.0f;
inline constexpr float PROJECT_DIALOG_OPEN_H = 520.0f;

// Modal dialog dimensions (NewMap)
inline constexpr float NEW_MAP_DIALOG_W = 500.0f;
inline constexpr float NEW_MAP_DIALOG_H = 400.0f;
inline constexpr float MODAL_BUTTON_W = 120.0f;

// Action button dimensions
inline constexpr float ACTION_BUTTON_W = 180.0f;
inline constexpr float ACTION_BUTTON_H = 40.0f;
inline constexpr float FPS_WARNING_THRESHOLD = 30.0f;
inline constexpr float FPS_HIGH_PERF_THRESHOLD = 59.0f;

// Colors in ImVec4 (R, G, B, A)
inline constexpr struct {
  float r, g, b, a;
} MODIFIED_INDICATOR_COLOR = {1.0f, 0.8f, 0.2f, 1.0f}; // Warning Yellow

// Preview sizes
inline constexpr float PREVIEW_TILE_SIZE = Rendering::TILE_SIZE;
} // namespace UI

// ============================================================================
// DATA/FILE CONFIGURATION
// ============================================================================
namespace Data {
inline constexpr size_t MAX_RECENT_MAPS = 10;
inline constexpr size_t MAX_RECENT_CLIENTS = 5;
inline constexpr size_t MAX_RECENT_FILES = 10;
inline constexpr size_t FILE_BUFFER_SIZE =
    262144; // 256KB for large file performance
} // namespace Data

// ============================================================================
// INGAME PREVIEW CONFIGURATION
// ============================================================================
namespace Preview {
// Default dimensions
inline constexpr int WIDTH_TILES = 15;
inline constexpr int HEIGHT_TILES = 11;
inline constexpr int PIXEL_WIDTH =
    WIDTH_TILES * Rendering::TILE_SIZE_INT; // 480
inline constexpr int PIXEL_HEIGHT =
    HEIGHT_TILES * Rendering::TILE_SIZE_INT; // 352

// Dimension limits for user-configurable preview size
inline constexpr int MIN_WIDTH_TILES = 15;
inline constexpr int MIN_HEIGHT_TILES = 11;
inline constexpr int MAX_WIDTH_TILES = 30;
inline constexpr int MAX_HEIGHT_TILES = 22;

inline constexpr double FADE_DURATION = 0.5;
inline constexpr int TELEPORT_THRESHOLD = 3;
} // namespace Preview

// ============================================================================
// OVERLAY/SELECTION COLORS (ImU32 format: 0xAABBGGRR)
// ============================================================================
namespace Colors {
// Helper to create ImU32 color from RGBA at compile time
constexpr unsigned int MakeColor(int r, int g, int b, int a) {
  return ((unsigned int)a << 24) | ((unsigned int)b << 16) |
         ((unsigned int)g << 8) | (unsigned int)r;
}

// Selection overlay - box selection
inline constexpr unsigned int SELECTION_BOX_FILL = MakeColor(100, 150, 255, 40);
inline constexpr unsigned int SELECTION_BOX_BORDER =
    MakeColor(100, 150, 255, 200);

// Selection overlay - tile (smart mode)
inline constexpr unsigned int TILE_SELECT_FILL = MakeColor(0, 255, 255, 80);
inline constexpr unsigned int TILE_SELECT_BORDER = MakeColor(0, 255, 255, 200);

// Selection overlay - pixel-perfect mode
inline constexpr unsigned int PIXEL_SELECT_FILL = MakeColor(255, 255, 0, 60);
inline constexpr unsigned int PIXEL_SELECT_BORDER = MakeColor(255, 255, 0, 200);

// Map panel
inline constexpr unsigned int MAP_BACKGROUND = MakeColor(30, 30, 30, 255);
inline constexpr unsigned int GRID_LINE = MakeColor(60, 60, 60, 100);
inline constexpr unsigned int INFO_TEXT_BG = MakeColor(0, 0, 0, 180);
inline constexpr unsigned int INFO_TEXT = MakeColor(255, 255, 255, 255);

// Minimap
inline constexpr unsigned int MINIMAP_VIEWPORT = MakeColor(255, 255, 255, 200);

// Item preview
inline constexpr unsigned int VALID_PASTE_AREA = MakeColor(0, 255, 0, 100);
inline constexpr unsigned int GHOST_ITEM = MakeColor(255, 255, 255, 128);

// Waypoint renderer
inline constexpr unsigned int WAYPOINT_FLAME_INNER =
    MakeColor(100, 100, 255, 255);
inline constexpr unsigned int WAYPOINT_FLAME_OUTER =
    MakeColor(50, 50, 200, 200);
inline constexpr unsigned int WAYPOINT_FLAME_TIP =
    MakeColor(150, 150, 255, 255);

// Spawn renderer ("Option B - Static" - Gray/Purple theme)
inline constexpr unsigned int SPAWN_CIRCLE =
    MakeColor(255, 0, 255, 180); // Deprecated circular marker
inline constexpr unsigned int SPAWN_TEXT =
    MakeColor(255, 255, 255, 255); // White text

// Spawn tile indicator (Magenta Box)
inline constexpr unsigned int SPAWN_INDICATOR_FILL =
    MakeColor(200, 0, 200, 150); // Visible Magenta fill
inline constexpr unsigned int SPAWN_INDICATOR_BORDER =
    MakeColor(255, 100, 255, 255); // Light Magenta border
inline constexpr unsigned int SPAWN_INDICATOR_TEXT =
    MakeColor(255, 255, 255, 255); // White text

// Spawn radius border (Solid Magenta)
inline constexpr unsigned int SPAWN_RADIUS_BORDER =
    MakeColor(255, 0, 255, 255);                         // Solid Magenta
inline constexpr float SPAWN_RADIUS_BORDER_WIDTH = 3.0f; // Thicker border

// Spawn radius tint (Magenta Tint)
// R, G, B components for tinting (0-1.0 range used in shader/batch)
inline constexpr float SPAWN_RADIUS_TINT_R = 1.0f; // Magenta R
inline constexpr float SPAWN_RADIUS_TINT_G = 0.0f; // Magenta G
inline constexpr float SPAWN_RADIUS_TINT_B = 1.0f; // Magenta B
inline constexpr float SPAWN_RADIUS_TINT_FACTOR =
    0.30f; // Higher opacity (easier to see)

// Creature Count Badge
inline constexpr unsigned int SPAWN_BADGE_BG =
    MakeColor(255, 0, 255, 255); // Solid Magenta
inline constexpr unsigned int SPAWN_BADGE_TEXT =
    MakeColor(255, 255, 255, 255); // White text
inline constexpr float SPAWN_BADGE_FONT_SIZE = 24.0f;
inline constexpr float SPAWN_BADGE_PADDING_X = 6.0f;
inline constexpr float SPAWN_BADGE_PADDING_Y = 2.0f;

// Color unpacking helper (ImU32 -> float RGBA)
struct ColorF {
  float r, g, b, a;
};
constexpr ColorF UnpackColor(unsigned int color) {
  return {
      ((color >> 0) & 0xFF) / 255.0f,
      ((color >> 8) & 0xFF) / 255.0f,
      ((color >> 16) & 0xFF) / 255.0f,
      ((color >> 24) & 0xFF) / 255.0f
  };
}

// Tooltip colors
inline constexpr unsigned int TOOLTIP_WAYPOINT_BG = MakeColor(0, 200, 0, 220);
inline constexpr unsigned int TOOLTIP_NORMAL_BG = MakeColor(238, 232, 170, 220);
inline constexpr unsigned int TOOLTIP_BORDER = MakeColor(0, 0, 0, 200);
inline constexpr unsigned int TOOLTIP_TEXT = MakeColor(0, 0, 0, 255);
inline constexpr unsigned int TOOLTIP_WAYPOINT_TEXT = MakeColor(0, 100, 0, 255);
inline constexpr unsigned int PARCHMENT_BG = MakeColor(245, 230, 196, 255);
inline constexpr unsigned int PARCHMENT_BORDER = MakeColor(101, 67, 33, 255);
inline constexpr unsigned int PARCHMENT_TEXT = MakeColor(60, 40, 20, 255);

// Invalid item placeholder colors (float RGB for TileRenderer)
inline constexpr float INVALID_GROUND_R =
    0.9f; // RED for ground/attribute items
inline constexpr float INVALID_GROUND_G = 0.1f;
inline constexpr float INVALID_GROUND_B = 0.1f;

inline constexpr float INVALID_STACKED_R =
    0.9f; // YELLOW for stacked/child node items
inline constexpr float INVALID_STACKED_G = 0.9f;
inline constexpr float INVALID_STACKED_B = 0.1f;

inline constexpr float INVALID_CREATURE_R =
    1.0f; // MAGENTA for missing creature types
inline constexpr float INVALID_CREATURE_G = 0.0f;
inline constexpr float INVALID_CREATURE_B = 1.0f;
} // namespace Colors

// ============================================================================
// WINDOW DEFAULTS
// ============================================================================
namespace Window {
inline constexpr int DEFAULT_WIDTH = 1280;
inline constexpr int DEFAULT_HEIGHT = 720;
inline constexpr int DISPLAY_RECOVERY_DELAY_MS = 100;
} // namespace Window

// ============================================================================
// TOOLTIP BUBBLE RENDERER
// ============================================================================
namespace Tooltip {
inline constexpr size_t MAX_BUBBLES = 4096;
inline constexpr float MAX_WIDTH_BASE = 150.0f;
} // namespace Tooltip

// ============================================================================
// CREATURE SIMULATION CONFIGURATION
// ============================================================================
namespace Simulation {
inline constexpr float MOVE_CHANCE = 0.33f; // 33% chance to move each tick
inline constexpr float TICK_INTERVAL_SEC =
    0.7f; // 700ms between movement checks
inline constexpr float WALK_DURATION_SEC = 0.5f; // 500ms for walk animation
inline constexpr int DEFAULT_ROAM_RADIUS =
    3; // Default radius for creatures without spawn
inline constexpr float RANDOM_MOVE_INTERVAL_MIN = 0.5f;
inline constexpr float RANDOM_MOVE_INTERVAL_MAX = 1.0f;
} // namespace Simulation

} // namespace MapEditor::Config
