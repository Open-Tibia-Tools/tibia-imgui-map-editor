#pragma once

#include "Domain/HotkeyDefinitions.h"
#include <filesystem>
#include <string>

namespace MapEditor {
namespace IO {

/**
 * Reads and writes hotkey configuration from/to JSON files.
 * Operates on pure data (Domain::HotkeyBindingMap), not on Services.
 */
class HotkeyJsonReader {
public:
    /**
     * Load hotkey bindings from a JSON file into the binding map.
     * @return true on success, false on error (bindings unchanged on error)
     */
    static bool load(const std::filesystem::path& path, Domain::HotkeyBindingMap& bindings);
    
    /**
     * Save hotkey bindings from the binding map to a JSON file.
     * @return true on success
     */
    static bool save(const std::filesystem::path& path, const Domain::HotkeyBindingMap& bindings);
    
    /**
     * Convert key name string to GLFW key code.
     * @param name Key name like "A", "Ctrl", "PageUp", "F1"
     * @return GLFW key code, or -1 if unknown
     */
    static int parseKeyName(const std::string& name);
    
    /**
     * Convert modifier name to GLFW modifier bit.
     */
    static int parseModifier(const std::string& mod);
};

} // namespace IO
} // namespace MapEditor
