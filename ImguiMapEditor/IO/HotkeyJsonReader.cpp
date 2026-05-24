#include "HotkeyJsonReader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>
#include <unordered_map>

namespace MapEditor {
namespace IO {

using json = nlohmann::json;

int HotkeyJsonReader::parseKeyName(const std::string& name) {
    // Static map for O(1) lookup of named keys
    static const std::unordered_map<std::string, int> keyMap = {
        // Navigation keys
        {"Escape", GLFW_KEY_ESCAPE}, {"Enter", GLFW_KEY_ENTER}, {"Tab", GLFW_KEY_TAB},
        {"Backspace", GLFW_KEY_BACKSPACE}, {"Insert", GLFW_KEY_INSERT}, {"Delete", GLFW_KEY_DELETE},
        {"Right", GLFW_KEY_RIGHT}, {"Left", GLFW_KEY_LEFT}, {"Down", GLFW_KEY_DOWN}, {"Up", GLFW_KEY_UP},
        {"PageUp", GLFW_KEY_PAGE_UP}, {"PageDown", GLFW_KEY_PAGE_DOWN},
        {"Home", GLFW_KEY_HOME}, {"End", GLFW_KEY_END},
        {"Space", GLFW_KEY_SPACE},
        // Lock keys
        {"CapsLock", GLFW_KEY_CAPS_LOCK}, {"ScrollLock", GLFW_KEY_SCROLL_LOCK},
        {"NumLock", GLFW_KEY_NUM_LOCK}, {"PrintScreen", GLFW_KEY_PRINT_SCREEN}, {"Pause", GLFW_KEY_PAUSE},
        // Function keys F1-F12
        {"F1", GLFW_KEY_F1}, {"F2", GLFW_KEY_F2}, {"F3", GLFW_KEY_F3}, {"F4", GLFW_KEY_F4},
        {"F5", GLFW_KEY_F5}, {"F6", GLFW_KEY_F6}, {"F7", GLFW_KEY_F7}, {"F8", GLFW_KEY_F8},
        {"F9", GLFW_KEY_F9}, {"F10", GLFW_KEY_F10}, {"F11", GLFW_KEY_F11}, {"F12", GLFW_KEY_F12},
        // Extended function keys F13-F25
        {"F13", GLFW_KEY_F13}, {"F14", GLFW_KEY_F14}, {"F15", GLFW_KEY_F15}, {"F16", GLFW_KEY_F16},
        {"F17", GLFW_KEY_F17}, {"F18", GLFW_KEY_F18}, {"F19", GLFW_KEY_F19}, {"F20", GLFW_KEY_F20},
        {"F21", GLFW_KEY_F21}, {"F22", GLFW_KEY_F22}, {"F23", GLFW_KEY_F23}, {"F24", GLFW_KEY_F24}, {"F25", GLFW_KEY_F25},
        // Keypad
        {"KP0", GLFW_KEY_KP_0}, {"KP1", GLFW_KEY_KP_1}, {"KP2", GLFW_KEY_KP_2}, {"KP3", GLFW_KEY_KP_3},
        {"KP4", GLFW_KEY_KP_4}, {"KP5", GLFW_KEY_KP_5}, {"KP6", GLFW_KEY_KP_6}, {"KP7", GLFW_KEY_KP_7},
        {"KP8", GLFW_KEY_KP_8}, {"KP9", GLFW_KEY_KP_9},
        {"KPDecimal", GLFW_KEY_KP_DECIMAL}, {"KPDivide", GLFW_KEY_KP_DIVIDE},
        {"KPMultiply", GLFW_KEY_KP_MULTIPLY}, {"KPSubtract", GLFW_KEY_KP_SUBTRACT},
        {"KPAdd", GLFW_KEY_KP_ADD}, {"KPEnter", GLFW_KEY_KP_ENTER}, {"KPEqual", GLFW_KEY_KP_EQUAL},
        // Special character keys (for explicit naming)
        {"Apostrophe", GLFW_KEY_APOSTROPHE}, {"Comma", GLFW_KEY_COMMA}, {"Period", GLFW_KEY_PERIOD},
        {"Slash", GLFW_KEY_SLASH}, {"Semicolon", GLFW_KEY_SEMICOLON}, {"Backslash", GLFW_KEY_BACKSLASH},
        {"LeftBracket", GLFW_KEY_LEFT_BRACKET}, {"RightBracket", GLFW_KEY_RIGHT_BRACKET},
        {"GraveAccent", GLFW_KEY_GRAVE_ACCENT}, {"Menu", GLFW_KEY_MENU}
    };

    // Handle single character keys
    if (name.length() == 1) {
        char c = name[0];
        if (c >= 'A' && c <= 'Z') return GLFW_KEY_A + (c - 'A');
        if (c >= 'a' && c <= 'z') return GLFW_KEY_A + (c - 'a');
        if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
        // Single-char special keys
        switch (c) {
            case ' ': return GLFW_KEY_SPACE;
            case '\'': return GLFW_KEY_APOSTROPHE;
            case ',': return GLFW_KEY_COMMA;
            case '-': return GLFW_KEY_MINUS;
            case '.': return GLFW_KEY_PERIOD;
            case '/': return GLFW_KEY_SLASH;
            case ';': return GLFW_KEY_SEMICOLON;
            case '=': case '+': return GLFW_KEY_EQUAL;
            case '[': return GLFW_KEY_LEFT_BRACKET;
            case '\\': return GLFW_KEY_BACKSLASH;
            case ']': return GLFW_KEY_RIGHT_BRACKET;
            case '`': return GLFW_KEY_GRAVE_ACCENT;
        }
    }
    
    // Named keys lookup
    auto it = keyMap.find(name);
    if (it != keyMap.end()) {
        return it->second;
    }
    
    return -1;
}

int HotkeyJsonReader::parseModifier(const std::string& mod) {
    if (mod == "Ctrl") return GLFW_MOD_CONTROL;
    if (mod == "Shift") return GLFW_MOD_SHIFT;
    if (mod == "Alt") return GLFW_MOD_ALT;
    return 0;
}

bool HotkeyJsonReader::load(const std::filesystem::path& path, Domain::HotkeyBindingMap& bindings) {
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::warn("[HotkeyJsonReader] Could not open {}", path.string());
        return false;
    }
    
    try {
        json root = json::parse(file);
        
        if (!root.contains("bindings")) {
            spdlog::warn("[HotkeyJsonReader] No 'bindings' key in {}", path.string());
            return false;
        }
        
        bindings.clear();
        
        for (auto& [category, actions] : root["bindings"].items()) {
            for (auto& [action_id, binding_data] : actions.items()) {
                Domain::HotkeyBinding binding;
                binding.action_id = action_id;
                binding.category = category;
                
                // Parse key
                std::string key_name = binding_data.value("key", "");
                binding.key = parseKeyName(key_name);
                if (binding.key < 0) {
                    spdlog::warn("[HotkeyJsonReader] Unknown key '{}' for action {}", key_name, action_id);
                    continue;
                }
                
                // Parse modifiers
                binding.mods = 0;
                if (binding_data.contains("mods")) {
                    for (const auto& mod : binding_data["mods"]) {
                        binding.mods |= parseModifier(mod.get<std::string>());
                    }
                }
                
                bindings[binding.action_id] = binding;
            }
        }
        
        spdlog::info("[HotkeyJsonReader] Loaded {} hotkeys from {}", 
                     bindings.size(), path.string());
        return true;
        
    } catch (const json::exception& e) {
        spdlog::error("[HotkeyJsonReader] JSON parse error in {}: {}", path.string(), e.what());
        return false;
    }
}

bool HotkeyJsonReader::save(const std::filesystem::path& path, const Domain::HotkeyBindingMap& bindings) {
    json root;
    root["version"] = "1.0";
    root["bindings"] = json::object();
    
    for (const auto& [action_id, binding] : bindings) {
        // Build key name
        std::string key_name;
        if (binding.key >= GLFW_KEY_A && binding.key <= GLFW_KEY_Z) {
            key_name = std::string(1, 'A' + (binding.key - GLFW_KEY_A));
        } else if (binding.key >= GLFW_KEY_0 && binding.key <= GLFW_KEY_9) {
            key_name = std::string(1, '0' + (binding.key - GLFW_KEY_0));
        } else {
            switch (binding.key) {
                // Special characters
                case GLFW_KEY_SPACE: key_name = "Space"; break;
                case GLFW_KEY_APOSTROPHE: key_name = "'"; break;
                case GLFW_KEY_COMMA: key_name = ","; break;
                case GLFW_KEY_MINUS: key_name = "-"; break;
                case GLFW_KEY_PERIOD: key_name = "."; break;
                case GLFW_KEY_SLASH: key_name = "/"; break;
                case GLFW_KEY_SEMICOLON: key_name = ";"; break;
                case GLFW_KEY_EQUAL: key_name = "+"; break;
                case GLFW_KEY_LEFT_BRACKET: key_name = "["; break;
                case GLFW_KEY_BACKSLASH: key_name = "\\"; break;
                case GLFW_KEY_RIGHT_BRACKET: key_name = "]"; break;
                case GLFW_KEY_GRAVE_ACCENT: key_name = "`"; break;
                // Navigation keys
                case GLFW_KEY_ESCAPE: key_name = "Escape"; break;
                case GLFW_KEY_ENTER: key_name = "Enter"; break;
                case GLFW_KEY_TAB: key_name = "Tab"; break;
                case GLFW_KEY_BACKSPACE: key_name = "Backspace"; break;
                case GLFW_KEY_INSERT: key_name = "Insert"; break;
                case GLFW_KEY_DELETE: key_name = "Delete"; break;
                case GLFW_KEY_RIGHT: key_name = "Right"; break;
                case GLFW_KEY_LEFT: key_name = "Left"; break;
                case GLFW_KEY_DOWN: key_name = "Down"; break;
                case GLFW_KEY_UP: key_name = "Up"; break;
                case GLFW_KEY_PAGE_UP: key_name = "PageUp"; break;
                case GLFW_KEY_PAGE_DOWN: key_name = "PageDown"; break;
                case GLFW_KEY_HOME: key_name = "Home"; break;
                case GLFW_KEY_END: key_name = "End"; break;
                // Lock keys
                case GLFW_KEY_CAPS_LOCK: key_name = "CapsLock"; break;
                case GLFW_KEY_SCROLL_LOCK: key_name = "ScrollLock"; break;
                case GLFW_KEY_NUM_LOCK: key_name = "NumLock"; break;
                case GLFW_KEY_PRINT_SCREEN: key_name = "PrintScreen"; break;
                case GLFW_KEY_PAUSE: key_name = "Pause"; break;
                // Function keys F1-F12
                case GLFW_KEY_F1: key_name = "F1"; break;
                case GLFW_KEY_F2: key_name = "F2"; break;
                case GLFW_KEY_F3: key_name = "F3"; break;
                case GLFW_KEY_F4: key_name = "F4"; break;
                case GLFW_KEY_F5: key_name = "F5"; break;
                case GLFW_KEY_F6: key_name = "F6"; break;
                case GLFW_KEY_F7: key_name = "F7"; break;
                case GLFW_KEY_F8: key_name = "F8"; break;
                case GLFW_KEY_F9: key_name = "F9"; break;
                case GLFW_KEY_F10: key_name = "F10"; break;
                case GLFW_KEY_F11: key_name = "F11"; break;
                case GLFW_KEY_F12: key_name = "F12"; break;
                // Extended function keys F13-F25
                case GLFW_KEY_F13: key_name = "F13"; break;
                case GLFW_KEY_F14: key_name = "F14"; break;
                case GLFW_KEY_F15: key_name = "F15"; break;
                case GLFW_KEY_F16: key_name = "F16"; break;
                case GLFW_KEY_F17: key_name = "F17"; break;
                case GLFW_KEY_F18: key_name = "F18"; break;
                case GLFW_KEY_F19: key_name = "F19"; break;
                case GLFW_KEY_F20: key_name = "F20"; break;
                case GLFW_KEY_F21: key_name = "F21"; break;
                case GLFW_KEY_F22: key_name = "F22"; break;
                case GLFW_KEY_F23: key_name = "F23"; break;
                case GLFW_KEY_F24: key_name = "F24"; break;
                case GLFW_KEY_F25: key_name = "F25"; break;
                // Keypad
                case GLFW_KEY_KP_0: key_name = "KP0"; break;
                case GLFW_KEY_KP_1: key_name = "KP1"; break;
                case GLFW_KEY_KP_2: key_name = "KP2"; break;
                case GLFW_KEY_KP_3: key_name = "KP3"; break;
                case GLFW_KEY_KP_4: key_name = "KP4"; break;
                case GLFW_KEY_KP_5: key_name = "KP5"; break;
                case GLFW_KEY_KP_6: key_name = "KP6"; break;
                case GLFW_KEY_KP_7: key_name = "KP7"; break;
                case GLFW_KEY_KP_8: key_name = "KP8"; break;
                case GLFW_KEY_KP_9: key_name = "KP9"; break;
                case GLFW_KEY_KP_DECIMAL: key_name = "KPDecimal"; break;
                case GLFW_KEY_KP_DIVIDE: key_name = "KPDivide"; break;
                case GLFW_KEY_KP_MULTIPLY: key_name = "KPMultiply"; break;
                case GLFW_KEY_KP_SUBTRACT: key_name = "KPSubtract"; break;
                case GLFW_KEY_KP_ADD: key_name = "KPAdd"; break;
                case GLFW_KEY_KP_ENTER: key_name = "KPEnter"; break;
                case GLFW_KEY_KP_EQUAL: key_name = "KPEqual"; break;
                // Menu
                case GLFW_KEY_MENU: key_name = "Menu"; break;
                default: key_name = "?"; break;
            }
        }
        
        // Build mods array
        json mods = json::array();
        if (binding.mods & GLFW_MOD_CONTROL) mods.push_back("Ctrl");
        if (binding.mods & GLFW_MOD_SHIFT) mods.push_back("Shift");
        if (binding.mods & GLFW_MOD_ALT) mods.push_back("Alt");
        
        // Add to category
        if (!root["bindings"].contains(binding.category)) {
            root["bindings"][binding.category] = json::object();
        }
        root["bindings"][binding.category][action_id] = {
            {"key", key_name},
            {"mods", mods}
        };
    }
    
    std::ofstream file(path);
    if (!file.is_open()) {
        spdlog::error("[HotkeyJsonReader] Could not write to {}", path.string());
        return false;
    }
    
    file << root.dump(2);
    spdlog::info("[HotkeyJsonReader] Saved {} hotkeys to {}", 
                 bindings.size(), path.string());
    return true;
}

} // namespace IO
} // namespace MapEditor
