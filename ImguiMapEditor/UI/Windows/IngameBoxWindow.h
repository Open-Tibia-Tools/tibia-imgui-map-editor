#pragma once
#include "Rendering/Core/Framebuffer.h"
#include "Domain/Position.h"
#include <memory>

namespace MapEditor {

namespace Domain {
    class ChunkedMap;
}

namespace Rendering {
    class MapRenderer;
    class IngamePreviewRenderer;
}

namespace Services {
    struct ViewSettings;
}

namespace AppLogic {
    class EditorSession;  // Forward declaration
}

namespace UI {

/**
 * Floating 15x11 tile preview window that follows the cursor.
 */
class IngameBoxWindow {
public:
    IngameBoxWindow();
    ~IngameBoxWindow() = default;
    
    /**
     * Render the ingame box window
     * @param map Current map being edited
     * @param renderer Map renderer for offscreen rendering
     * @param settings View settings
     * @param cursor_pos Current cursor position in tile coordinates
     */
    void render(Domain::ChunkedMap* map, 
                Rendering::MapRenderer* renderer,
                Services::ViewSettings& settings,
                const Domain::Position& cursor_pos,
                bool* p_open = nullptr);
    
    bool isOpen() const { return is_open_; }
    void setOpen(bool open) { is_open_ = open; }
    void toggle() { is_open_ = !is_open_; }
    
    // Window position control
    bool isFollowingCursor() const { return follow_cursor_; }
    void setFollowCursor(bool follow) { follow_cursor_ = follow; }
    
    // Session state save/restore for tab switching
    void saveState(AppLogic::EditorSession& session) const;
    void restoreState(const AppLogic::EditorSession& session);
    
private:
    void renderContent(Domain::ChunkedMap* map,
                       Rendering::MapRenderer* renderer,
                       const Domain::Position& center,
                       Services::ViewSettings& settings);
    
    bool is_open_ = false;
    bool follow_cursor_ = true;
    bool first_render_ = true;
    Domain::Position locked_position_{0, 0, 7};  // Locked center when not following
    
    // Configurable preview dimensions (in tiles)
    int preview_width_tiles_ = 15;
    int preview_height_tiles_ = 11;
    
    // Framebuffer for offscreen rendering
    std::unique_ptr<MapEditor::Rendering::Framebuffer> fbo_;
    
    // Renderer for the preview
    std::unique_ptr<Rendering::IngamePreviewRenderer> renderer_;
    Rendering::MapRenderer* current_map_renderer_ = nullptr;

};

} // namespace UI
} // namespace MapEditor
