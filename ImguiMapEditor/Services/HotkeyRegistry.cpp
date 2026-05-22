#include "HotkeyRegistry.h"
#include "IO/HotkeyJsonReader.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <filesystem>
#include <bit>

namespace MapEditor {
namespace Services {

void HotkeyRegistry::registerBinding(const HotkeyBinding& binding) {
    bindings_[binding.action_id] = binding;
}

const HotkeyBinding* HotkeyRegistry::findByAction(const std::string& action_id) const {
    auto it = bindings_.find(action_id);
    return it != bindings_.end() ? &it->second : nullptr;
}

const HotkeyBinding* HotkeyRegistry::findByKey(int key, int mods) const {
    const HotkeyBinding* bestMatch = nullptr;
    int maxModCount = -1;

    for (const auto& [id, binding] : bindings_) {
        // Check if this binding could match: same key and all required mods are pressed
        if (binding.key == key && (mods & binding.mods) == binding.mods) {
            // Select the most specific match (highest modifier count)
            int modCount = std::popcount(static_cast<unsigned int>(binding.mods));
            if (modCount > maxModCount) {
                bestMatch = &binding;
                maxModCount = modCount;
            }
        }
    }
    return bestMatch;
}

bool HotkeyRegistry::hasConflict(int key, int mods, const std::string& exclude_action) const {
    for (const auto& [id, binding] : bindings_) {
        if (id != exclude_action && binding.matches(key, mods)) {
            return true;
        }
    }
    return false;
}

std::vector<const HotkeyBinding*> HotkeyRegistry::getBindingsByCategory(const std::string& category) const {
    std::vector<const HotkeyBinding*> result;
    for (const auto& [id, binding] : bindings_) {
        if (binding.category == category) {
            result.push_back(&binding);
        }
    }
    return result;
}

std::string HotkeyRegistry::formatShortcut(const HotkeyBinding& binding) {
    std::ostringstream oss;
    
    if (binding.mods & GLFW_MOD_CONTROL) oss << "Ctrl+";
    if (binding.mods & GLFW_MOD_SHIFT) oss << "Shift+";
    if (binding.mods & GLFW_MOD_ALT) oss << "Alt+";
    
    // Convert GLFW key to string
    if (binding.key >= GLFW_KEY_A && binding.key <= GLFW_KEY_Z) {
        oss << static_cast<char>('A' + (binding.key - GLFW_KEY_A));
    } else if (binding.key >= GLFW_KEY_0 && binding.key <= GLFW_KEY_9) {
        oss << static_cast<char>('0' + (binding.key - GLFW_KEY_0));
    } else {
        switch (binding.key) {
            case GLFW_KEY_EQUAL: oss << "+"; break;
            case GLFW_KEY_MINUS: oss << "-"; break;
            case GLFW_KEY_PAGE_UP: oss << "PgUp"; break;
            case GLFW_KEY_PAGE_DOWN: oss << "PgDn"; break;
            case GLFW_KEY_DELETE: oss << "Del"; break;
            case GLFW_KEY_ESCAPE: oss << "Esc"; break;
            case GLFW_KEY_F1: oss << "F1"; break;
            case GLFW_KEY_F2: oss << "F2"; break;
            case GLFW_KEY_F5: oss << "F5"; break;
            default: oss << "?"; break;
        }
    }
    
    return oss.str();
}

namespace {

void mergeMissingBindings(HotkeyRegistry &registry,
                          const HotkeyRegistry &defaults) {
    for (const auto &[action_id, binding] : defaults.getAllBindings()) {
        if (!registry.findByAction(action_id)) {
            registry.registerBinding(binding);
        }
    }
}

} // namespace

HotkeyRegistry HotkeyRegistry::createDefaults() {
    HotkeyRegistry registry;
    
    // File operations
    registry.registerBinding({"NEW", GLFW_KEY_N, GLFW_MOD_CONTROL, "file"});
    registry.registerBinding({"OPEN", GLFW_KEY_O, GLFW_MOD_CONTROL, "file"});
    registry.registerBinding({"SAVE_AS", GLFW_KEY_S, GLFW_MOD_CONTROL | GLFW_MOD_ALT, "file"});
    registry.registerBinding({"CLOSE", GLFW_KEY_Q, GLFW_MOD_CONTROL, "file"});
    
    // Edit operations
    registry.registerBinding({"UNDO", GLFW_KEY_Z, GLFW_MOD_CONTROL, "edit"});
    registry.registerBinding({"REDO", GLFW_KEY_Y, GLFW_MOD_CONTROL, "edit"});
    registry.registerBinding({"CUT", GLFW_KEY_X, GLFW_MOD_CONTROL, "edit"});
    registry.registerBinding({"COPY", GLFW_KEY_C, GLFW_MOD_CONTROL, "edit"});
    registry.registerBinding({"PASTE", GLFW_KEY_V, GLFW_MOD_CONTROL, "edit"});
    registry.registerBinding({"PASTE_REPLACE", GLFW_KEY_V, GLFW_MOD_CONTROL | GLFW_MOD_SHIFT, "edit"});
    registry.registerBinding({"DELETE", GLFW_KEY_DELETE, 0, "edit"});
    registry.registerBinding({"SAVE", GLFW_KEY_S, GLFW_MOD_CONTROL, "edit"});
    
    // View/zoom
    registry.registerBinding({"ZOOM_IN", GLFW_KEY_EQUAL, GLFW_MOD_CONTROL, "view"});
    registry.registerBinding({"ZOOM_OUT", GLFW_KEY_MINUS, GLFW_MOD_CONTROL, "view"});
    registry.registerBinding({"ZOOM_RESET", GLFW_KEY_0, GLFW_MOD_CONTROL, "view"});
    registry.registerBinding({"SHOW_GRID", GLFW_KEY_G, GLFW_MOD_SHIFT, "view"});
    registry.registerBinding({"GHOST_ITEMS", GLFW_KEY_G, 0, "view"});
    registry.registerBinding({"GHOST_HIGHER_FLOORS", GLFW_KEY_L, GLFW_MOD_CONTROL, "view"});
    registry.registerBinding({"GHOST_LOWER_FLOORS", GLFW_KEY_L, GLFW_MOD_CONTROL | GLFW_MOD_SHIFT, "view"});
    registry.registerBinding({"SHOW_ALL_FLOORS", GLFW_KEY_W, GLFW_MOD_CONTROL, "view"});
    registry.registerBinding({"SHOW_SHADE", GLFW_KEY_F9, 0, "view"});
    
    // Overlay toggles
    registry.registerBinding({"SHOW_SPAWNS", GLFW_KEY_S, 0, "overlay"});
    registry.registerBinding({"SHOW_CREATURES", GLFW_KEY_F, 0, "overlay"});
    registry.registerBinding({"SHOW_BLOCKING", GLFW_KEY_O, 0, "overlay"});
    registry.registerBinding({"SHOW_SPECIAL", GLFW_KEY_E, 0, "overlay"});
    registry.registerBinding({"SHOW_HOUSES", GLFW_KEY_H, GLFW_MOD_CONTROL, "overlay"});
    registry.registerBinding({"HIGHLIGHT_ITEMS", GLFW_KEY_V, 0, "overlay"});
    registry.registerBinding({"HIGHLIGHT_LOCKED_DOORS", GLFW_KEY_U, 0, "overlay"});
    
    // Preview
    registry.registerBinding({"SHOW_INGAME_BOX", GLFW_KEY_I, GLFW_MOD_SHIFT, "preview"});
    registry.registerBinding({"SHOW_TOOLTIPS", GLFW_KEY_Y, 0, "preview"});
    registry.registerBinding({"SHOW_PREVIEW", GLFW_KEY_L, 0, "preview"});
    
    // Navigation
    registry.registerBinding({"FLOOR_UP", GLFW_KEY_PAGE_UP, 0, "navigation"});
    registry.registerBinding({"FLOOR_DOWN", GLFW_KEY_PAGE_DOWN, 0, "navigation"});

    // Brush actions
    registry.registerBinding({"BRUSH_REFRESH_CURRENT", GLFW_KEY_SPACE, GLFW_MOD_CONTROL, "brush"});
    registry.registerBinding({"BRUSH_TOGGLE_SELECTION_TOOL", GLFW_KEY_SPACE, 0, "brush"});
    registry.registerBinding({"BRUSH_RESTORE_LAST", GLFW_KEY_Q, 0, "brush"});
    registry.registerBinding({"BRUSH_VARIATION_PREV", GLFW_KEY_Z, 0, "brush"});
    registry.registerBinding({"BRUSH_VARIATION_NEXT", GLFW_KEY_X, 0, "brush"});
    registry.registerBinding({"ROTATE_ITEM", GLFW_KEY_R, 0, "brush"});

    for (int slot = 0; slot < 10; ++slot) {
        const auto slot_suffix = std::to_string(slot);
        registry.registerBinding({
            "BRUSH_SLOT_" + slot_suffix,
            GLFW_KEY_0 + slot,
            0,
            "brush",
        });
        registry.registerBinding({
            "BRUSH_STORE_SLOT_" + slot_suffix,
            GLFW_KEY_0 + slot,
            GLFW_MOD_CONTROL,
            "brush",
        });
    }
    
    // Selection
    registry.registerBinding({"SELECT_ALL", GLFW_KEY_A, GLFW_MOD_CONTROL, "selection"});
    registry.registerBinding({"DESELECT", GLFW_KEY_ESCAPE, 0, "selection"});
    
    // Search
    registry.registerBinding({"QUICK_SEARCH", GLFW_KEY_F, GLFW_MOD_CONTROL, "search"});
    registry.registerBinding({"ADVANCED_SEARCH", GLFW_KEY_F, GLFW_MOD_CONTROL | GLFW_MOD_SHIFT, "search"});
    
    // Map menu
    registry.registerBinding({"EDIT_TOWNS", GLFW_KEY_T, GLFW_MOD_CONTROL, "map"});
    registry.registerBinding({"MAP_PROPERTIES", GLFW_KEY_P, GLFW_MOD_CONTROL, "map"});
    
    return registry;
}

HotkeyRegistry HotkeyRegistry::loadOrCreateDefaults(const std::vector<std::string>& data_paths) {
    // Try to load from JSON first using IO::HotkeyJsonReader
    for (const auto& path : data_paths) {
        if (std::filesystem::exists(path)) {
            HotkeyRegistry registry;
            if (IO::HotkeyJsonReader::load(path, registry)) {
                mergeMissingBindings(registry, createDefaults());
                return registry;
            }
        }
    }
    
    // Fallback to defaults and save them
    spdlog::info("[HotkeyRegistry] No valid hotkeys.json found, generating defaults");
    HotkeyRegistry defaults = createDefaults();
    
    // Save to first valid path
    for (const auto& path : data_paths) {
        std::filesystem::path fs_path(path);
        if (std::filesystem::exists(fs_path.parent_path()) || fs_path.parent_path().empty()) {
            IO::HotkeyJsonReader::save(fs_path, defaults);
            break;
        }
    }
    
    return defaults;
}

} // namespace Services
} // namespace MapEditor
