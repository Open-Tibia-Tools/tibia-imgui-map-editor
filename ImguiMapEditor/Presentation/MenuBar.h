#pragma once
#include "Services/ViewSettings.h"
#include "Domain/SelectionSettings.h"
#include "UI/Core/Theme.h"
#include <functional>
#include <filesystem>

// Forward declarations
namespace MapEditor {
namespace UI {
class MapPanel;
class SelectionMenu;
}
namespace AppLogic {
class MapTabManager;
}
namespace Services {
class RecentLocationsService;
}
}

namespace MapEditor {
namespace Presentation {

/**
 * Main menu bar for the editor.
 * 
 * Extracted from Application::render() to separate
 * UI presentation from application orchestration.
 */
class MenuBar {
public:
    using ActionCallback = std::function<void()>;
    using OpenRecentCallback = std::function<void(const std::filesystem::path&)>;
    
    MenuBar(Services::ViewSettings& view_settings, Domain::SelectionSettings& selection_settings, UI::MapPanel* map_panel,
            AppLogic::MapTabManager* tab_manager = nullptr);
    
    /**
     * Render the complete menu bar.
     * Call within ImGui::BeginMainMenuBar/EndMainMenuBar.
     */
    void render();
    
    // Action callbacks - set by Application orchestrator
    void setNewMapCallback(ActionCallback cb) { on_new_map_ = std::move(cb); }
    void setOpenMapCallback(ActionCallback cb) { on_open_map_ = std::move(cb); }
    void setOpenSecMapCallback(ActionCallback cb) { on_open_sec_map_ = std::move(cb); }
    void setSaveMapCallback(ActionCallback cb) { on_save_map_ = std::move(cb); }
    void setSaveAsMapCallback(ActionCallback cb) { on_save_as_map_ = std::move(cb); }
    void setCloseMapCallback(ActionCallback cb) { on_close_map_ = std::move(cb); }
    void setImportMapCallback(ActionCallback cb) { on_import_map_ = std::move(cb); }
    void setImportMonstersCallback(ActionCallback cb) { on_import_monsters_ = std::move(cb); }
    void setPreferencesCallback(ActionCallback cb) { on_preferences_ = std::move(cb); }
    void setQuitCallback(ActionCallback cb) { on_quit_ = std::move(cb); }
    void setCloseAllMapsCallback(ActionCallback cb) { on_close_all_ = std::move(cb); }
    
    // Recent files - callback receives the path to open
    void setOpenRecentCallback(OpenRecentCallback cb) { on_open_recent_ = std::move(cb); }
    void setRecentFilesService(Services::RecentLocationsService* service) { recent_service_ = service; }
    
    // Map menu callbacks
    void setEditTownsCallback(ActionCallback cb) { on_edit_towns_ = std::move(cb); }
    void setMapPropertiesCallback(ActionCallback cb) { on_map_properties_ = std::move(cb); }
    
    // Cleanup operations (each separate, all non-undoable)
    void setCleanInvalidItemsCallback(ActionCallback cb) { on_clean_invalid_items_ = std::move(cb); }
    void setCleanHouseItemsCallback(ActionCallback cb) { on_clean_house_items_ = std::move(cb); }
    
    // ID conversion operations
    void setConvertToServerIdCallback(ActionCallback cb) { on_convert_to_server_id_ = std::move(cb); }
    void setConvertToClientIdCallback(ActionCallback cb) { on_convert_to_client_id_ = std::move(cb); }
    
    // Theme persistence
    void setThemePtr(ThemeType* theme_ptr) { current_theme_ = theme_ptr; }
    
    // Selection settings access
    Domain::SelectionSettings& getSelectionSettings() { return selection_settings_; }
    
private:
    void renderFileMenu();
    void renderRecentFilesSubmenu();
    void renderEditMenu();
    void renderViewMenu();
    void renderMapMenu();
    void renderThemeMenu();
    void renderSelectionMenu();
    
    Services::ViewSettings& view_settings_;
    Domain::SelectionSettings& selection_settings_;
    UI::MapPanel* map_panel_;
    AppLogic::MapTabManager* tab_manager_;
    Services::RecentLocationsService* recent_service_ = nullptr;
    
    ActionCallback on_new_map_;
    ActionCallback on_open_map_;
    ActionCallback on_open_sec_map_;
    ActionCallback on_save_map_;
    ActionCallback on_save_as_map_;
    ActionCallback on_close_map_;
    ActionCallback on_import_map_;
    ActionCallback on_import_monsters_;
    ActionCallback on_preferences_;
    ActionCallback on_quit_;
    OpenRecentCallback on_open_recent_;
    ActionCallback on_close_all_;
    
    // Map menu callbacks
    ActionCallback on_edit_towns_;
    ActionCallback on_map_properties_;
    ActionCallback on_clean_invalid_items_;
    ActionCallback on_clean_house_items_;
    ActionCallback on_convert_to_server_id_;
    ActionCallback on_convert_to_client_id_;
    
    ThemeType* current_theme_ = nullptr;
};

} // namespace Presentation
} // namespace MapEditor

