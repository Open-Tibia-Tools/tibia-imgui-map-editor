#pragma once

#include "Domain/HotkeyDefinitions.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace MapEditor {
namespace Services {

/**
 * Runtime storage and lookup for hotkey bindings.
 * 
 * Separation of concerns:
 * - Domain/HotkeyDefinitions: pure data struct (HotkeyBinding)
 * - This class handles runtime storage and lookup
 * - IO/HotkeyJsonReader handles file loading/saving
 * - Controllers/HotkeyController handles key event processing
 */
class HotkeyRegistry {
public:
    HotkeyRegistry() = default;
    
    /**
     * Register a hotkey binding.
     */
    void registerBinding(const Domain::HotkeyBinding& binding);
    
    /**
     * Find binding by action ID.
     * @return nullptr if not found
     */
    const Domain::HotkeyBinding* findByAction(const std::string& action_id) const;
    
    /**
     * Find binding by key combination.
     * @return nullptr if no binding matches
     */
    const Domain::HotkeyBinding* findByKey(int key, int mods) const;
    
    /**
     * Check if a key combination conflicts with existing bindings.
     * @param exclude_action Action ID to exclude from conflict check
     */
    bool hasConflict(int key, int mods, const std::string& exclude_action = "") const;
    
    /**
     * Get all bindings in a category.
     */
    std::vector<const Domain::HotkeyBinding*> getBindingsByCategory(const std::string& category) const;
    
    /**
     * Get all bindings.
     */
    const Domain::HotkeyBindingMap& getAllBindings() const { return bindings_; }
    
    /**
     * Clear all bindings.
     */
    void clear() { bindings_.clear(); }
    
    /**
     * Create registry with default bindings (from current Hotkeys.h values).
     */
    static HotkeyRegistry createDefaults();
    
    /**
     * Load registry from JSON file, or return defaults if file not found.
     * @param data_paths List of paths to search for hotkeys.json
     */
    static HotkeyRegistry loadOrCreateDefaults(const std::vector<std::string>& data_paths = {
        "data/hotkeys.json", "../data/hotkeys.json", "hotkeys.json"
    });
    
    /**
     * Format a binding as display string (e.g., "Ctrl+S").
     */
    static std::string formatShortcut(const Domain::HotkeyBinding& binding);
    
private:
    Domain::HotkeyBindingMap bindings_;
};

} // namespace Services
} // namespace MapEditor
