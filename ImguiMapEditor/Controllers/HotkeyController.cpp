#include "HotkeyController.h"
// glad must be included before GLFW
#include <glad/glad.h>
#include "Brushes/BrushController.h"
#include "Domain/Item.h"
#include "Domain/Selection/SelectionEntry.h"
#include "Services/HotkeyRegistry.h"
#include "Services/Selection/SelectionService.h"
#include "UI/Map/MapPanel.h"
#include "UI/Windows/IngameBoxWindow.h"
#include "Application/MapTabManager.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <optional>
#include <string_view>

namespace MapEditor {
namespace AppLogic {

namespace {

constexpr std::string_view kBrushRefreshAction{"BRUSH_REFRESH_CURRENT"};
constexpr std::string_view kBrushToggleSelectionAction{"BRUSH_TOGGLE_SELECTION_TOOL"};
constexpr std::string_view kBrushRestoreAction{"BRUSH_RESTORE_LAST"};
constexpr std::string_view kBrushVariationPrevAction{"BRUSH_VARIATION_PREV"};
constexpr std::string_view kBrushVariationNextAction{"BRUSH_VARIATION_NEXT"};
constexpr std::string_view kRotateItemAction{"ROTATE_ITEM"};
constexpr std::string_view kBrushSlotPrefix{"BRUSH_SLOT_"};
constexpr std::string_view kBrushStoreSlotPrefix{"BRUSH_STORE_SLOT_"};

std::optional<size_t> parseBrushSlotIndex(std::string_view action,
                                          std::string_view prefix) {
    if (!action.starts_with(prefix) || action.size() != prefix.size() + 1) {
        return std::nullopt;
    }

    const char slot_char = action.back();
    if (slot_char < '0' || slot_char > '9') {
        return std::nullopt;
    }

    return static_cast<size_t>(slot_char - '0');
}

} // namespace

HotkeyController::HotkeyController(Services::HotkeyRegistry& registry,
                                   Services::ViewSettings& view_settings,
                                   MapEditor::UI::MapPanel* map_panel,
                                   MapEditor::UI::IngameBoxWindow& ingame_box,
                                   MapTabManager& tab_manager)
    : registry_(registry)
    , view_settings_(view_settings)
    , map_panel_(map_panel)
    , ingame_box_(ingame_box)
    , tab_manager_(tab_manager)
{
}

void HotkeyController::processKey(int key, int mods, bool editor_active) {
    // Only process in editor mode
    if (!editor_active) {
        return;
    }
    
    // Don't process if ImGui wants text input (typing in a field)
    if (ImGui::GetIO().WantTextInput) {
        return;
    }
    
    // Look up action by key combination
    const auto* binding = registry_.findByKey(key, mods);
    if (!binding) {
        return;  // No binding for this key
    }
    
    spdlog::debug("[HOTKEY] Action: {}, Key: {}, Mods: {}", binding->action_id, key, mods);
    handleAction(binding->action_id);
}

bool HotkeyController::handleBrushAction(std::string_view action) {
    if (!brush_controller_) {
        return false;
    }

    if (action == kBrushRefreshAction) {
        brush_controller_->refreshCurrentBrush();
        return true;
    }

    if (action == kBrushToggleSelectionAction) {
        brush_controller_->toggleSelectionTool();
        return true;
    }

    if (action == kBrushRestoreAction) {
        brush_controller_->restoreLastBrush();
        return true;
    }

    if (action == kBrushVariationPrevAction) {
        brush_controller_->cycleBrushVariation(-1);
        return true;
    }

    if (action == kBrushVariationNextAction) {
        brush_controller_->cycleBrushVariation(1);
        return true;
    }

    if (action == kRotateItemAction) {
        if (rotateSelectedItem()) {
            if (auto *session = tab_manager_.getActiveSession()) {
                session->setModified(true);
            }
        }
        return true;
    }

    if (const auto slot = parseBrushSlotIndex(action, kBrushSlotPrefix)) {
        brush_controller_->recallBrushSlot(*slot);
        return true;
    }

    if (const auto slot = parseBrushSlotIndex(action, kBrushStoreSlotPrefix)) {
        brush_controller_->storeBrushSlot(*slot);
        return true;
    }

    return false;
}

bool HotkeyController::rotateSelectedItem() {
    auto *session = tab_manager_.getActiveSession();
    if (!session || !brush_controller_) {
        return false;
    }

    const auto &selection = session->getSelectionService();
    const auto entries = selection.getAllEntries();
    if (entries.size() != 1) {
        return false;
    }

    const auto &entry = entries.front();
    if (entry.getType() != Domain::Selection::EntityType::Item || !entry.entity_ptr) {
        return false;
    }

    const auto *item = static_cast<const Domain::Item *>(entry.entity_ptr);
    return brush_controller_->rotateItemAt(entry.getPosition(), item);
}

void HotkeyController::handleAction(const std::string& action) {
    if (handleBrushAction(action)) {
        return;
    }

    EditorSession* session = tab_manager_.getActiveSession();
    
    // Edit operations
    if (action == "UNDO") {
        if (session && session->canUndo()) {
            session->undo();
        }
    } else if (action == "REDO") {
        if (session && session->canRedo()) {
            session->redo();
        }
    } else if (action == "CUT") {
        if (session && !session->getSelectionService().isEmpty()) {
            tab_manager_.getClipboard().cut(*session);
        }
    } else if (action == "COPY") {
        if (session && !session->getSelectionService().isEmpty()) {
            tab_manager_.getClipboard().copy(*session);
        }
    } else if (action == "PASTE") {
        if (session) {
            Domain::Position target_pos;
            if (map_panel_) {
                target_pos = map_panel_->getCameraCenter();
            } else {
                auto& view = session->getViewState();
                target_pos = {
                    static_cast<int>(view.camera_x / 32.0f),
                    static_cast<int>(view.camera_y / 32.0f),
                    static_cast<int16_t>(view.current_floor)
                };
            }
            tab_manager_.getClipboard().paste(*session, target_pos);
        }
    } else if (action == "PASTE_REPLACE") {
        // Ctrl+Shift+V - paste with replace mode (clears destination tiles)
        if (session) {
            Domain::Position target_pos;
            if (map_panel_) {
                target_pos = map_panel_->getCameraCenter();
            } else {
                auto& view = session->getViewState();
                target_pos = {
                    static_cast<int>(view.camera_x / 32.0f),
                    static_cast<int>(view.camera_y / 32.0f),
                    static_cast<int16_t>(view.current_floor)
                };
            }
            // Start paste with replace_mode=true
            const auto& tiles = tab_manager_.getClipboard().getBuffer().getTiles();
            session->startPaste(tiles, true);
        }
    } else if (action == "DELETE") {
        if (session && !session->getSelectionService().isEmpty()) {
            session->deleteSelection();
        }
    } else if (action == "SAVE") {
        if (on_save_) on_save_();
    }
    // Selection (Note: SELECT_ALL removed - not needed per user feedback)
    else if (action == "DESELECT") {
        if (session) session->clearSelection();
    }
    // Search
    else if (action == "ADVANCED_SEARCH") {
        if (on_advanced_search_) on_advanced_search_();
    } else if (action == "QUICK_SEARCH") {
        if (on_quick_search_) on_quick_search_();
    }
    // Zoom
    else if (action == "ZOOM_IN") {
        view_settings_.zoomIn();
    } else if (action == "ZOOM_OUT") {
        view_settings_.zoomOut();
    } else if (action == "ZOOM_RESET") {
        view_settings_.zoomReset();
    }
    // Display toggles
    else if (action == "SHOW_GRID") {
        view_settings_.show_grid = !view_settings_.show_grid;
    } else if (action == "GHOST_ITEMS") {
        view_settings_.ghost_items = !view_settings_.ghost_items;
    } else if (action == "GHOST_HIGHER_FLOORS") {
        view_settings_.ghost_higher_floors = !view_settings_.ghost_higher_floors;
    } else if (action == "GHOST_LOWER_FLOORS") {
        view_settings_.ghost_lower_floors = !view_settings_.ghost_lower_floors;
    } else if (action == "SHOW_ALL_FLOORS") {
        view_settings_.show_all_floors = !view_settings_.show_all_floors;
    } else if (action == "SHOW_SHADE") {
        view_settings_.show_shade = !view_settings_.show_shade;
    }
    // Overlay toggles
    else if (action == "SHOW_SPAWNS") {
        view_settings_.show_spawns = !view_settings_.show_spawns;
    } else if (action == "SHOW_CREATURES") {
        view_settings_.show_creatures = !view_settings_.show_creatures;
    } else if (action == "SHOW_BLOCKING") {
        view_settings_.show_blocking = !view_settings_.show_blocking;
    } else if (action == "SHOW_SPECIAL") {
        view_settings_.show_special_tiles = !view_settings_.show_special_tiles;
    } else if (action == "SHOW_HOUSES") {
        view_settings_.show_houses = !view_settings_.show_houses;
    } else if (action == "HIGHLIGHT_ITEMS") {
        view_settings_.highlight_items = !view_settings_.highlight_items;
    } else if (action == "HIGHLIGHT_LOCKED_DOORS") {
        view_settings_.highlight_locked_doors = !view_settings_.highlight_locked_doors;
    }
    // Preview
    else if (action == "SHOW_INGAME_BOX") {
        view_settings_.show_ingame_box = !view_settings_.show_ingame_box;
        ingame_box_.setOpen(view_settings_.show_ingame_box);
    } else if (action == "SHOW_TOOLTIPS") {
        view_settings_.show_tooltips = !view_settings_.show_tooltips;
    } else if (action == "SHOW_PREVIEW") {
        // Animation toggle - currently maps to same as tooltips behavior
    }
    // Floor navigation
    else if (action == "FLOOR_UP") {
        view_settings_.floorUp();
        if (map_panel_) {
            map_panel_->setCurrentFloor(view_settings_.current_floor);
        }
    } else if (action == "FLOOR_DOWN") {
        view_settings_.floorDown();
        if (map_panel_) {
            map_panel_->setCurrentFloor(view_settings_.current_floor);
        }
    }
    // File operations
    else if (action == "NEW") {
        if (on_new_map_) on_new_map_();
    } else if (action == "OPEN") {
        if (on_open_map_) on_open_map_();
    } else if (action == "SAVE_AS") {
        if (on_save_as_map_) on_save_as_map_();
    } else if (action == "CLOSE") {
        if (on_close_map_) on_close_map_();
    }
    // Map menu
    else if (action == "EDIT_TOWNS") {
        if (on_edit_towns_) on_edit_towns_();
    } else if (action == "MAP_PROPERTIES") {
        if (on_map_properties_) on_map_properties_();
    }
}

} // namespace AppLogic
} // namespace MapEditor
