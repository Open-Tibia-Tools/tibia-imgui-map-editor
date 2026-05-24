#pragma once
#include "Domain/ChunkedMap.h"
#include "Domain/Position.h"
#include "Rendering/Minimap/MinimapRenderer.h"
#include "Rendering/Minimap/ChunkedMapMinimapSource.h"
#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_set>

namespace MapEditor {
namespace AppLogic { class MapTabManager; }

namespace UI {

class EditTownsDialog {
public:
    enum class Result {
        None,
        Applied,
        Cancelled
    };

    using GoToCallback = std::function<void(const Domain::Position& pos)>;
    using PickPositionCallback = std::function<bool()>;
    using TileToScreenFunc = std::function<glm::vec2(const Domain::Position&)>;

    void show();

    Result render();

    bool isOpen() const { return is_open_; }

    void setTabManager(AppLogic::MapTabManager* tab_mgr) { tab_manager_ = tab_mgr; }
    void setGoToCallback(GoToCallback cb) { on_go_to_ = std::move(cb); }
    void setPickPositionCallback(PickPositionCallback cb) { on_pick_position_ = std::move(cb); }
    void setTileToScreenFunc(TileToScreenFunc fn) { tile_to_screen_ = std::move(fn); }

    void setPickedPosition(const Domain::Position& pos);

    bool isPickingPosition() const { return is_picking_position_; }

private:
    struct TownEntry {
        uint32_t id = 0;
        std::string name;
        Domain::Position temple_position{0, 0, 7};
        bool is_new = false;
    };

    void refreshFromActiveMap();
    void loadTownsFromMap();
    void applyChangesToMap();
    void syncEditBuffers();
    void markModified(size_t index);
    bool canRemoveSelectedTown() const;
    uint32_t getNextTownId() const;
    void updateMinimapView(const Domain::Position& pos);

    bool is_open_ = false;
    bool is_picking_position_ = false;
    bool show_delete_confirm_ = false;

    AppLogic::MapTabManager* tab_manager_ = nullptr;

    Domain::ChunkedMap* map_ = nullptr;
    Domain::ChunkedMap* last_map_ = nullptr;

    std::vector<TownEntry> towns_;
    std::unordered_set<size_t> modified_indices_;
    int selected_index_ = -1;

    char name_buffer_[256] = {};
    int temple_x_ = 0;
    int temple_y_ = 0;
    int temple_z_ = 7;

    Domain::Position last_minimap_pos_{-1, -1, -1};
    int last_selected_index_ = -2;

    Rendering::MinimapRenderer minimap_renderer_;
    std::unique_ptr<Rendering::ChunkedMapMinimapSource> minimap_source_;

    GoToCallback on_go_to_;
    PickPositionCallback on_pick_position_;
    TileToScreenFunc tile_to_screen_;

    float apply_flash_time_ = 0.0f;
};

} // namespace UI
} // namespace MapEditor
