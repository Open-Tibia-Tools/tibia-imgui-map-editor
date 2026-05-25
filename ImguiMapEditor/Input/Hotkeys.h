#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace MapEditor {
namespace Input {

/**
 * Global hotkey definitions using GLFW key codes.
 * All View menu shortcuts are centralized here.
 */
namespace Hotkeys {

struct Binding {
  int key;
  int mods;
  const char *description;
};

// Zoom Controls
inline constexpr Binding ZOOM_IN{GLFW_KEY_EQUAL, GLFW_MOD_CONTROL, "Zoom In"};
inline constexpr Binding ZOOM_OUT{GLFW_KEY_MINUS, GLFW_MOD_CONTROL, "Zoom Out"};
inline constexpr Binding ZOOM_RESET{GLFW_KEY_0, GLFW_MOD_CONTROL, "Zoom 100%"};

// Display Toggles
inline constexpr Binding SHOW_GRID{GLFW_KEY_G, GLFW_MOD_SHIFT, "Show Grid"};
inline constexpr Binding GHOST_ITEMS{GLFW_KEY_G, 0, "Ghost Items"};
inline constexpr Binding GHOST_HIGHER_FLOORS{GLFW_KEY_L, GLFW_MOD_CONTROL,
                                             "Ghost Higher Floors"};
inline constexpr Binding GHOST_LOWER_FLOORS{
    GLFW_KEY_L, GLFW_MOD_CONTROL | GLFW_MOD_SHIFT, "Ghost Lower Floors"};
inline constexpr Binding SHOW_ALL_FLOORS{GLFW_KEY_W, GLFW_MOD_CONTROL,
                                         "Show All Floors"};
inline constexpr Binding SHOW_SHADE{GLFW_KEY_Q, 0, "Show Shade"};

// Overlay Toggles
inline constexpr Binding SHOW_SPAWNS{GLFW_KEY_S, 0, "Show Spawns"};
inline constexpr Binding SHOW_CREATURES{GLFW_KEY_F, 0, "Show Creatures"};
inline constexpr Binding SHOW_BLOCKING{GLFW_KEY_O, 0, "Show Pathing"};
inline constexpr Binding SHOW_SPECIAL{GLFW_KEY_E, 0, "Show Special Tiles"};
inline constexpr Binding SHOW_HOUSES{GLFW_KEY_H, GLFW_MOD_CONTROL,
                                     "Show Houses"};
inline constexpr Binding HIGHLIGHT_ITEMS{GLFW_KEY_V, 0, "Highlight Items"};

// Preview Window
inline constexpr Binding SHOW_INGAME_BOX{GLFW_KEY_I, GLFW_MOD_SHIFT,
                                         "Show Ingame Box"};
inline constexpr Binding SHOW_TOOLTIPS{GLFW_KEY_Y, 0, "Show Tooltips"};
inline constexpr Binding SHOW_PREVIEW{GLFW_KEY_L, 0, "Show Preview"};

// Floor Navigation
inline constexpr Binding FLOOR_UP{GLFW_KEY_PAGE_UP, 0, "Floor Up"};
inline constexpr Binding FLOOR_DOWN{GLFW_KEY_PAGE_DOWN, 0, "Floor Down"};

// Selection
inline constexpr Binding SELECT_ALL{GLFW_KEY_A, GLFW_MOD_CONTROL, "Select All"};
inline constexpr Binding DESELECT{GLFW_KEY_ESCAPE, 0, "Deselect"};

// Edit Operations
inline constexpr Binding UNDO{GLFW_KEY_Z, GLFW_MOD_CONTROL, "Undo"};
inline constexpr Binding REDO{GLFW_KEY_Y, GLFW_MOD_CONTROL, "Redo"};
inline constexpr Binding CUT{GLFW_KEY_X, GLFW_MOD_CONTROL, "Cut"};
inline constexpr Binding COPY{GLFW_KEY_C, GLFW_MOD_CONTROL, "Copy"};
inline constexpr Binding PASTE{GLFW_KEY_V, GLFW_MOD_CONTROL, "Paste"};
inline constexpr Binding PASTE_REPLACE{
    GLFW_KEY_V, GLFW_MOD_CONTROL | GLFW_MOD_SHIFT, "Paste (Replace)"};
inline constexpr Binding DELETE_SEL{GLFW_KEY_DELETE, 0, "Delete"};
inline constexpr Binding SAVE{GLFW_KEY_S, GLFW_MOD_CONTROL, "Save"};

// Search
inline constexpr Binding QUICK_SEARCH{GLFW_KEY_F, GLFW_MOD_CONTROL,
                                      "Quick Search"};
inline constexpr Binding ADVANCED_SEARCH{
    GLFW_KEY_F, GLFW_MOD_CONTROL | GLFW_MOD_SHIFT, "Advanced Search"};

/**
 * Helper to check if a key event matches a binding
 */
inline bool matches(const Binding &binding, int key, int mods) {
  return key == binding.key && (mods & binding.mods) == binding.mods;
}

/**
 * Format binding as string for menu display (e.g., "Ctrl+G")
 */
inline std::string formatShortcut(const Binding &binding) {
  std::string result;
  result.reserve(32);

  if (binding.mods & GLFW_MOD_CONTROL) {
    result += "Ctrl+";
  }
  if (binding.mods & GLFW_MOD_SHIFT) {
    result += "Shift+";
  }
  if (binding.mods & GLFW_MOD_ALT) {
    result += "Alt+";
  }

  // Convert key to character
  if (binding.key >= GLFW_KEY_A && binding.key <= GLFW_KEY_Z) {
    result += static_cast<char>('A' + (binding.key - GLFW_KEY_A));
  } else if (binding.key >= GLFW_KEY_0 && binding.key <= GLFW_KEY_9) {
    result += static_cast<char>('0' + (binding.key - GLFW_KEY_0));
  } else {
    static const std::unordered_map<int, std::string_view> key_names = {
        {GLFW_KEY_EQUAL, "+"},       {GLFW_KEY_MINUS, "-"},
        {GLFW_KEY_PAGE_UP, "PgUp"},  {GLFW_KEY_PAGE_DOWN, "PgDn"},
        {GLFW_KEY_DELETE, "Del"},    {GLFW_KEY_ESCAPE, "Esc"},
    };
    if (auto it = key_names.find(binding.key); it != key_names.end()) {
      result += it->second;
    }
  }

  return result;
}

} // namespace Hotkeys
} // namespace Input
} // namespace MapEditor
