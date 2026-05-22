#pragma once
#include "Services/ViewSettings.h"
#include <functional>
#include <string>
#include <string_view>

// Forward declarations
namespace MapEditor {
namespace UI {
class MapPanel;
class IngameBoxWindow;
}
namespace Services {
class HotkeyRegistry;
}
namespace Brushes {
class BrushController;
}
namespace AppLogic {
class MapTabManager;
}
}

namespace MapEditor {
namespace AppLogic {

/**
 * Handles keyboard hotkey processing for the editor.
 * 
 * Uses HotkeyRegistry for configurable key bindings loaded from JSON.
 */
class HotkeyController {
public:
    using ActionCallback = std::function<void()>;
    
    HotkeyController(Services::HotkeyRegistry& registry,
                     Services::ViewSettings& view_settings,
                     UI::MapPanel* map_panel,
                     UI::IngameBoxWindow& ingame_box,
                     MapTabManager& tab_manager);
    
    /**
     * Process a key press event.
     * @param key GLFW key code
     * @param mods GLFW modifier bits
     * @param editor_active Whether the editor is in active state
     */
    void processKey(int key, int mods, bool editor_active);
    
    // Action callbacks for operations that require Application orchestration
    void setSaveCallback(ActionCallback cb) { on_save_ = std::move(cb); }
    void setQuickSearchCallback(ActionCallback cb) { on_quick_search_ = std::move(cb); }
    void setAdvancedSearchCallback(ActionCallback cb) { on_advanced_search_ = std::move(cb); }
    void setNewMapCallback(ActionCallback cb) { on_new_map_ = std::move(cb); }
    void setOpenMapCallback(ActionCallback cb) { on_open_map_ = std::move(cb); }
    void setSaveAsMapCallback(ActionCallback cb) { on_save_as_map_ = std::move(cb); }
    void setCloseMapCallback(ActionCallback cb) { on_close_map_ = std::move(cb); }
    void setEditTownsCallback(ActionCallback cb) { on_edit_towns_ = std::move(cb); }
    void setMapPropertiesCallback(ActionCallback cb) { on_map_properties_ = std::move(cb); }
    void setBrushController(Brushes::BrushController* brush_controller) {
        brush_controller_ = brush_controller;
    }
    
private:
    void handleAction(const std::string& action);
    bool handleBrushAction(std::string_view action);
    bool rotateSelectedItem();
    
    Services::HotkeyRegistry& registry_;
    Services::ViewSettings& view_settings_;
    UI::MapPanel* map_panel_;
    UI::IngameBoxWindow& ingame_box_;
    MapTabManager& tab_manager_;
    Brushes::BrushController* brush_controller_ = nullptr;
    
    ActionCallback on_save_;
    ActionCallback on_quick_search_;
    ActionCallback on_advanced_search_;
    ActionCallback on_new_map_;
    ActionCallback on_open_map_;
    ActionCallback on_save_as_map_;
    ActionCallback on_close_map_;
    ActionCallback on_edit_towns_;
    ActionCallback on_map_properties_;
};

} // namespace AppLogic
} // namespace MapEditor
