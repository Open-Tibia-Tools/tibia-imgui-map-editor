#pragma once
#include "Domain/ChunkedMap.h"
#include "Domain/Position.h"
#include <vector>
#include <string>
#include <functional>

namespace MapEditor {
namespace UI {

/**
 * Dialog for editing map towns (CRUD operations).
 *
 * Features:
 * - List all towns with selection
 * - Add/remove towns
 * - Edit town name and temple position
 * - Click-to-select temple position on map
 *
 * All changes are applied to the map when OK is clicked.
 * Cancel discards all changes.
 */
class EditTownsDialog {
public:
    enum class Result {
        None,       // Dialog still open
        Applied,    // User clicked OK - changes applied
        Cancelled   // User cancelled - no changes
    };

    /**
     * Callback for "Go To" functionality - navigate camera to position.
     */
    using GoToCallback = std::function<void(const Domain::Position& pos)>;

    /**
     * Callback for pick-position mode - enter mode where next map click sets temple.
     * Returns true if pick mode was activated.
     */
    using PickPositionCallback = std::function<bool()>;

    /**
     * Show the dialog with the given map.
     * Makes a copy of towns for editing.
     */
    void show(Domain::ChunkedMap* map);

    /**
     * Render the dialog. Call every frame.
     * @return Result of user action
     */
    Result render();

    /**
     * Check if dialog is open
     */
    bool isOpen() const { return is_open_; }

    /**
     * Set callback for Go To button
     */
    void setGoToCallback(GoToCallback cb) { on_go_to_ = std::move(cb); }

    /**
     * Set callback for pick position from map
     */
    void setPickPositionCallback(PickPositionCallback cb) { on_pick_position_ = std::move(cb); }

    /**
     * Set the picked position (called by Application when user clicks map in pick mode)
     */
    void setPickedPosition(const Domain::Position& pos);

    /**
     * Check if in position pick mode
     */
    bool isPickingPosition() const { return is_picking_position_; }

private:
    // Internal town representation for editing
    struct TownEntry {
        uint32_t id = 0;
        std::string name;
        Domain::Position temple_position{0, 0, 7};
        bool is_new = false;  // For tracking newly added towns
    };

    void loadTownsFromMap();
    void applyChangesToMap();
    void updateSelectionBuffers();
    bool canRemoveSelectedTown() const;

    bool should_open_ = false;
    bool is_open_ = false;
    bool is_picking_position_ = false;
    bool show_delete_confirm_ = false;

    Domain::ChunkedMap* map_ = nullptr;

    // Working copies (modifications applied on OK)
    std::vector<TownEntry> towns_;
    int selected_index_ = -1;

    // Edit buffers
    char name_buffer_[256] = {};
    int temple_x_ = 0;
    int temple_y_ = 0;
    int temple_z_ = 7;

    // Next available town ID
    uint32_t next_town_id_ = 1;

    // Callbacks
    GoToCallback on_go_to_;
    PickPositionCallback on_pick_position_;
};

} // namespace UI
} // namespace MapEditor
