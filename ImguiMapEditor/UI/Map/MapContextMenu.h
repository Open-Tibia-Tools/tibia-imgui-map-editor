#pragma once
#include "Domain/Position.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace MapEditor {
namespace AppLogic {
    class EditorSession;
    class ClipboardService;
}
namespace Domain {
    struct Creature;
    struct Spawn;
    class Tile;
    class Item;
}
}

namespace MapEditor::UI {

enum class BrushPickMode : uint8_t {
    Smart,
    Raw,
    Ground,
    Doodad,
    Collection,
    Door,
    Wall,
    Carpet,
    Table,
    Creature,
    Spawn,
    House,
    HouseExit,
    Waypoint,
    OptionalBorder,
    ProtectionZone,
    NoPvp,
    NoLogout,
    PvpZone
};

/**
 * Right-click context menu for map tiles/items.
 * Rendered as ImGui popup.
 */
class MapContextMenu {
public:
    using PropertiesCallback = std::function<void(Domain::Item*)>;
    using GotoCallback = std::function<void(const Domain::Position&)>;
    // Extended to pass active item server ID (selected item if available)
    using BrowseTileCallback = std::function<void(const Domain::Position&, uint16_t item_server_id)>;
    using SelectBrushCallback = std::function<std::string(const Domain::Position&, const Domain::Item*, BrushPickMode)>;
    using CanSelectBrushCallback = std::function<bool(const Domain::Position&, const Domain::Item*, BrushPickMode)>;
    using RotateItemCallback = std::function<bool(const Domain::Position&, const Domain::Item*)>;
    using CanRotateItemCallback = std::function<bool(const Domain::Position&, const Domain::Item*)>;
    using SwitchDoorCallback = std::function<bool(const Domain::Position&, const Domain::Item*)>;
    using CanSwitchDoorCallback = std::function<bool(const Domain::Position&, const Domain::Item*)>;
    using DoorStateCallback = std::function<std::optional<bool>(const Domain::Position&, const Domain::Item*)>;
    using SpawnPropertiesCallback = std::function<void(Domain::Spawn*, const Domain::Position&)>;
    using CreaturePropertiesCallback =
        std::function<void(Domain::Creature*, const std::string&, const Domain::Position&)>;
    
    MapContextMenu();
    
    /**
     * Show the context menu at the given position.
     */
    void show(const Domain::Position& pos);
    
    /**
     * Render the context menu. Call each frame.
     */
    void render(
        AppLogic::EditorSession* session,
        AppLogic::ClipboardService* clipboard,
        PropertiesCallback on_properties = nullptr,
        GotoCallback on_goto = nullptr
    );
    
    void setBrowseTileCallback(BrowseTileCallback cb) { browse_tile_callback_ = std::move(cb); }
    void setSelectBrushCallback(SelectBrushCallback cb) { select_brush_callback_ = std::move(cb); }
    void setCanSelectBrushCallback(CanSelectBrushCallback cb) { can_select_brush_callback_ = std::move(cb); }
    void setRotateItemCallback(RotateItemCallback cb) { rotate_item_callback_ = std::move(cb); }
    void setCanRotateItemCallback(CanRotateItemCallback cb) { can_rotate_item_callback_ = std::move(cb); }
    void setSwitchDoorCallback(SwitchDoorCallback cb) { switch_door_callback_ = std::move(cb); }
    void setCanSwitchDoorCallback(CanSwitchDoorCallback cb) { can_switch_door_callback_ = std::move(cb); }
    void setDoorStateCallback(DoorStateCallback cb) { door_state_callback_ = std::move(cb); }
    void setSpawnPropertiesCallback(SpawnPropertiesCallback cb) {
      spawn_properties_callback_ = std::move(cb);
    }
    void setCreaturePropertiesCallback(CreaturePropertiesCallback cb) {
      creature_properties_callback_ = std::move(cb);
    }
    
    bool isOpen() const { return is_open_; }
    
private:
    void renderTileActions(AppLogic::EditorSession* session);
    void renderItemActions(AppLogic::EditorSession* session);
    void renderClipboardActions(
        AppLogic::EditorSession* session,
        AppLogic::ClipboardService* clipboard
    );
    void renderNavigationActions();
    void renderBrushSelectionActions(AppLogic::EditorSession* session);
    
    bool is_open_ = false;
    Domain::Position position_;
    const Domain::Tile* current_tile_ = nullptr;
    const Domain::Item* selected_item_ = nullptr;
    bool has_single_selection_context_ = false;
    PropertiesCallback properties_callback_;
    GotoCallback goto_callback_;
    BrowseTileCallback browse_tile_callback_;
    SelectBrushCallback select_brush_callback_;
    CanSelectBrushCallback can_select_brush_callback_;
    RotateItemCallback rotate_item_callback_;
    CanRotateItemCallback can_rotate_item_callback_;
    SwitchDoorCallback switch_door_callback_;
    CanSwitchDoorCallback can_switch_door_callback_;
    DoorStateCallback door_state_callback_;
    SpawnPropertiesCallback spawn_properties_callback_;
    CreaturePropertiesCallback creature_properties_callback_;
};

} // namespace MapEditor::UI
