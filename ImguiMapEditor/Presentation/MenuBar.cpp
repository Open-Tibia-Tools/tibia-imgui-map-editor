#include "MenuBar.h"

#include <format>
#include <ranges>

#include <glad/glad.h>
#include <imgui.h>

#include "Application/MapTabManager.h"
#include "Input/Hotkeys.h"
#include "Services/RecentLocationsService.h"
#include "UI/Core/Theme.h"
#include "UI/Map/MapPanel.h"
#include "UI/Map/SelectionMenu.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"

namespace MapEditor {
namespace Presentation {

MenuBar::MenuBar(Services::ViewSettings &view_settings,
                 Domain::SelectionSettings &selection_settings,
                 UI::MapPanel *map_panel, AppLogic::MapTabManager *tab_manager)
    : view_settings_(view_settings), selection_settings_(selection_settings),
      map_panel_(map_panel), tab_manager_(tab_manager) {}

void MenuBar::render() {
  renderFileMenu();
  renderEditMenu();
  renderViewMenu();
  renderMapMenu();
  renderThemeMenu();
  renderSelectionMenu();
}

void MenuBar::renderFileMenu() {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem(ICON_FA_FILE " New Map", "Ctrl+N")) {
      if (on_new_map_)
        on_new_map_();
    }
    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Map", "Ctrl+O")) {
      if (on_open_map_)
        on_open_map_();
    }
    if (ImGui::MenuItem(ICON_FA_FOLDER_TREE " Open SEC Map (7.x)...")) {
      if (on_open_sec_map_)
        on_open_sec_map_();
    }

    bool has_session = tab_manager_ && tab_manager_->getActiveSession();
    if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save", "Ctrl+S", false,
                        has_session)) {
      if (on_save_map_)
        on_save_map_();
    }
    if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save As...", "Ctrl+Shift+S",
                        false, has_session)) {
      if (on_save_as_map_)
        on_save_as_map_();
    }
    if (ImGui::MenuItem(ICON_FA_XMARK " Close", "Ctrl+W", false, has_session)) {
      if (on_close_map_)
        on_close_map_();
    }

    bool has_any_tabs = tab_manager_ && tab_manager_->getTabCount() > 0;
    if (ImGui::MenuItem(ICON_FA_FOLDER_CLOSED " Close All", nullptr, false,
                        has_any_tabs)) {
      if (on_close_all_)
        on_close_all_();
    }

    ImGui::Separator();

    // Import submenu
    if (ImGui::BeginMenu(ICON_FA_FILE_IMPORT " Import", has_session)) {
      if (ImGui::MenuItem(ICON_FA_MAP " Import Map...")) {
        if (on_import_map_)
          on_import_map_();
      }
      if (ImGui::MenuItem(ICON_FA_GHOST " Import Monsters/NPC...")) {
        if (on_import_monsters_)
          on_import_monsters_();
      }
      ImGui::EndMenu();
    }

    ImGui::Separator();

    // Recent Files submenu
    renderRecentFilesSubmenu();

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_FA_GEAR " Preferences...")) {
      if (on_preferences_)
        on_preferences_();
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_FA_DOOR_OPEN " Exit")) {
      if (on_quit_)
        on_quit_();
    }
    ImGui::EndMenu();
  }
}

void MenuBar::renderRecentFilesSubmenu() {
  bool has_recent =
      recent_service_ && !recent_service_->getRecentMaps().empty();

  if (ImGui::BeginMenu(ICON_FA_CLOCK_ROTATE_LEFT " Recent Files", has_recent)) {
    if (!recent_service_) {
      ImGui::EndMenu();
      return;
    }

    const auto &recent_maps = recent_service_->getRecentMaps();

    int i = 0;
    for (const auto &entry : recent_maps | std::views::take(10)) {
      std::string filename = entry.path.filename().string();
      std::string full_path = entry.path.string();

      // Show filename with full path as tooltip
      ImGui::PushID(i++);
      if (ImGui::MenuItem(filename.c_str())) {
        if (on_open_recent_)
          on_open_recent_(entry.path);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", full_path.c_str());
      }
      ImGui::PopID();
    }

    if (!recent_maps.empty()) {
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_FA_TRASH " Clear Recent Files")) {
        recent_service_->clearRecentMaps();
      }
    }

    ImGui::EndMenu();
  }
}

void MenuBar::renderEditMenu() {
  if (ImGui::BeginMenu("Edit")) {
    // Check if we have an active session with selection
    bool has_session = tab_manager_ && tab_manager_->getActiveSession();
    bool has_selection =
        has_session &&
        !tab_manager_->getActiveSession()->getSelectionService().isEmpty();
    bool can_paste = tab_manager_ && tab_manager_->getClipboard().canPaste();
    bool can_undo = has_session && tab_manager_->getActiveSession()->canUndo();
    bool can_redo = has_session && tab_manager_->getActiveSession()->canRedo();

    if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT " Undo", "Ctrl+Z", false,
                        can_undo)) {
      if (tab_manager_) {
        tab_manager_->getActiveSession()->undo();
      }
    }
    if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " Redo", "Ctrl+Y", false,
                        can_redo)) {
      if (tab_manager_) {
        tab_manager_->getActiveSession()->redo();
      }
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_FA_SCISSORS " Cut", "Ctrl+X", false,
                        has_selection)) {
      if (tab_manager_) {
        tab_manager_->getClipboard().cut(*tab_manager_->getActiveSession());
      }
    }
    if (ImGui::MenuItem(ICON_FA_COPY " Copy", "Ctrl+C", false, has_selection)) {
      if (tab_manager_) {
        tab_manager_->getClipboard().copy(*tab_manager_->getActiveSession());
      }
    }
    if (ImGui::MenuItem(ICON_FA_PASTE " Paste", "Ctrl+V", false, can_paste)) {
      if (tab_manager_) {
        auto *session = tab_manager_->getActiveSession();
        if (session) {
          auto &view = session->getViewState();
          Domain::Position target_pos{static_cast<int>(view.camera_x / 32.0f),
                                      static_cast<int>(view.camera_y / 32.0f),
                                      static_cast<int16_t>(view.current_floor)};
          tab_manager_->getClipboard().paste(*session, target_pos);
        }
      }
    }

    ImGui::Separator();

    // Note: Select All menu item removed - not needed per user feedback
    if (ImGui::MenuItem(ICON_FA_XMARK " Clear Selection", "Escape", false,
                        has_selection)) {
      if (tab_manager_) {
        tab_manager_->getActiveSession()->clearSelection();
      }
    }
    if (ImGui::MenuItem(ICON_FA_TRASH " Delete", "Delete", false,
                        has_selection)) {
      if (tab_manager_) {
        tab_manager_->getActiveSession()->deleteSelection();
      }
    }

    ImGui::EndMenu();
  }
}

void MenuBar::renderViewMenu() {
  using namespace Input::Hotkeys;

  if (ImGui::BeginMenu("View")) {
    // Zoom controls
    if (ImGui::BeginMenu("Zoom")) {
      if (ImGui::MenuItem(ICON_FA_MAGNIFYING_GLASS_PLUS " Zoom In",
                          formatShortcut(ZOOM_IN).c_str())) {
        view_settings_.zoomIn();
      }
      if (ImGui::MenuItem(ICON_FA_MAGNIFYING_GLASS_MINUS " Zoom Out",
                          formatShortcut(ZOOM_OUT).c_str())) {
        view_settings_.zoomOut();
      }
      if (ImGui::MenuItem(ICON_FA_MAGNIFYING_GLASS " Zoom 100%",
                          formatShortcut(ZOOM_RESET).c_str())) {
        view_settings_.zoomReset();
      }
      ImGui::EndMenu();
    }

    ImGui::Separator();

    // Display toggles - Core
    ImGui::MenuItem("Show Grid", formatShortcut(SHOW_GRID).c_str(),
                    &view_settings_.show_grid);
    ImGui::MenuItem("Show All Floors", formatShortcut(SHOW_ALL_FLOORS).c_str(),
                    &view_settings_.show_all_floors);
    ImGui::MenuItem("Ghost Items", formatShortcut(GHOST_ITEMS).c_str(),
                    &view_settings_.ghost_items);
    ImGui::MenuItem("Ghost Higher Floors",
                    formatShortcut(GHOST_HIGHER_FLOORS).c_str(),
                    &view_settings_.ghost_higher_floors);
    ImGui::MenuItem("Ghost Lower Floors",
                    formatShortcut(GHOST_LOWER_FLOORS).c_str(),
                    &view_settings_.ghost_lower_floors);
    ImGui::MenuItem("Show Shade", formatShortcut(SHOW_SHADE).c_str(),
                    &view_settings_.show_shade);

    ImGui::Separator();

    // Overlay toggles
    ImGui::MenuItem("Show Wall Outlines", nullptr,
                    &view_settings_.show_wall_outline);
    ImGui::Separator();

    ImGui::MenuItem("Show Ingame Box", formatShortcut(SHOW_INGAME_BOX).c_str(),
                    &view_settings_.show_ingame_box);
    ImGui::MenuItem("Show As Minimap", nullptr,
                    &view_settings_.show_minimap_window);
    ImGui::MenuItem("Browse Tile", nullptr, &view_settings_.show_browse_tile);
    ImGui::MenuItem(ICON_FA_PAINTBRUSH " Brush Settings", nullptr,
                    &view_settings_.show_brush_settings);
    ImGui::MenuItem(ICON_FA_MAGNIFYING_GLASS " Search Results", "Ctrl+Shift+F",
                    &view_settings_.show_search_results);

    ImGui::Separator();

    // Creature/Spawn toggles
    ImGui::MenuItem("Show Creatures", formatShortcut(SHOW_CREATURES).c_str(),
                    &view_settings_.show_creatures);
    ImGui::MenuItem("Show Spawns", formatShortcut(SHOW_SPAWNS).c_str(),
                    &view_settings_.show_spawns);
    ImGui::MenuItem("Show Special Tiles", formatShortcut(SHOW_SPECIAL).c_str(),
                    &view_settings_.show_special_tiles);
    ImGui::MenuItem("Show Pathing", formatShortcut(SHOW_BLOCKING).c_str(),
                    &view_settings_.show_blocking);
    ImGui::MenuItem("Show Houses", formatShortcut(SHOW_HOUSES).c_str(),
                    &view_settings_.show_houses);

    ImGui::Separator();

    // Tools
    ImGui::MenuItem("Highlight Items", formatShortcut(HIGHLIGHT_ITEMS).c_str(),
                    &view_settings_.highlight_items);
    ImGui::MenuItem("Highlight Locked Doors", nullptr,
                    &view_settings_.highlight_locked_doors);
    ImGui::MenuItem("Show Tooltips", formatShortcut(SHOW_TOOLTIPS).c_str(),
                    &view_settings_.show_tooltips);
    ImGui::MenuItem("Show Waypoints", nullptr, &view_settings_.show_waypoints);

    ImGui::Separator();

    // Floor selection
    if (ImGui::BeginMenu("Floor")) {
      for (int floor : std::views::iota(0, 16)) {
        std::string label = std::format("Floor {}", floor);
        if (ImGui::RadioButton(label.c_str(),
                               view_settings_.current_floor == floor)) {
          view_settings_.current_floor = static_cast<int16_t>(floor);
          if (map_panel_) {
            map_panel_->setCurrentFloor(floor);
          }
        }
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenu();
  }
}

void MenuBar::renderThemeMenu() {
  if (ImGui::BeginMenu("Theme")) {
    for (const auto &theme : AVAILABLE_THEMES) {
      bool is_current = current_theme_ && (*current_theme_ == theme.type);
      if (ImGui::MenuItem(theme.name, nullptr, is_current, true)) {
        ApplyTheme(theme.type);
        // Update persistent theme setting
        if (current_theme_) {
          *current_theme_ = theme.type;
        }
      }
      if (ImGui::IsItemHovered() && theme.description) {
        ImGui::SetTooltip("%s", theme.description);
      }
    }
    ImGui::EndMenu();
  }
}

void MenuBar::renderSelectionMenu() {
  // Delegate to SelectionMenu to avoid duplicate code
  // SelectionMenu provides: Selection Type, Floor Scope, Select All, Deselect
  UI::SelectionMenu selection_menu(selection_settings_);
  selection_menu.render(tab_manager_ ? tab_manager_->getActiveSession()
                                     : nullptr);
}

void MenuBar::renderMapMenu() {
  bool has_session = tab_manager_ && tab_manager_->getActiveSession();

  if (ImGui::BeginMenu("Map")) {
    // Edit Towns
    if (ImGui::MenuItem(ICON_FA_CITY " Edit Towns...", nullptr, false,
                        has_session)) {
      if (on_edit_towns_)
        on_edit_towns_();
    }

    ImGui::Separator();

    // Cleanup submenu - each operation separate
    if (ImGui::BeginMenu(ICON_FA_BROOM " Clean Up", has_session)) {
      // Invalid items
      if (ImGui::MenuItem(ICON_FA_TRASH " Remove Invalid Items...")) {
        if (on_clean_invalid_items_)
          on_clean_invalid_items_();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Remove items with IDs not in client data.\n"
                          "WARNING: This action CANNOT be undone!");
      }

      // House items
      if (ImGui::MenuItem(ICON_FA_HOUSE " Remove House Items...")) {
        if (on_clean_house_items_)
          on_clean_house_items_();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Remove moveable items from house tiles.\n"
                          "WARNING: This action CANNOT be undone!");
      }

      ImGui::EndMenu();
    }

    // Convert Map ID submenu
    if (ImGui::BeginMenu(ICON_FA_RIGHT_LEFT " Convert Map ID", has_session)) {
      if (ImGui::MenuItem(ICON_FA_SERVER " To Server ID...")) {
        if (on_convert_to_server_id_)
          on_convert_to_server_id_();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Convert all item IDs in the map to Server "
                          "IDs.\nWill prompt to save as a new file.");
      }

      if (ImGui::MenuItem(ICON_FA_DESKTOP " To Client ID...")) {
        if (on_convert_to_client_id_)
          on_convert_to_client_id_();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Convert all item IDs in the map to Client "
                          "IDs.\nWill prompt to save as a new file.");
      }

      ImGui::EndMenu();
    }

    ImGui::Separator();

    // Highlight invalid items (view-only, non-destructive)
    if (ImGui::MenuItem(ICON_FA_EYE " Highlight Invalid Items", nullptr,
                        view_settings_.show_invalid_items, has_session)) {
      view_settings_.show_invalid_items = !view_settings_.show_invalid_items;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show items with invalid IDs in red overlay");
    }

    ImGui::Separator();

    // Properties
    if (ImGui::MenuItem(ICON_FA_SLIDERS " Properties...", nullptr, false,
                        has_session)) {
      if (on_map_properties_)
        on_map_properties_();
    }

    ImGui::EndMenu();
  }
}

} // namespace Presentation
} // namespace MapEditor
