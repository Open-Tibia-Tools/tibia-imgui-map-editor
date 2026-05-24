#include "MinimapWindow.h"
#include "Core/Config.h"
#include "Domain/ChunkedMap.h"
#include "Services/ClientDataService.h"
#include "Application/EditorSession.h"
#include "Input/Hotkeys.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>

namespace MapEditor {
namespace UI {

MinimapWindow::MinimapWindow() = default;

void MinimapWindow::setMap(Domain::ChunkedMap* map, Services::ClientDataService* clientData) {
    if (map && clientData) {
        data_source_ = std::make_unique<Rendering::ChunkedMapMinimapSource>(map, clientData);
        renderer_.setDataSource(data_source_.get());
    } else {
        data_source_.reset();
        renderer_.setDataSource(nullptr);
    }
}

void MinimapWindow::setViewportSyncCallback(ViewportSyncCallback callback) {
    viewport_sync_callback_ = std::move(callback);
}

void MinimapWindow::syncWithCamera(int32_t x, int32_t y, int16_t floor) {
    // Only sync when main camera position/floor actually changes
    bool position_changed = (x != main_camera_x_) || (y != main_camera_y_);
    bool floor_changed = (floor != last_synced_floor_);
    
    main_camera_x_ = x;
    main_camera_y_ = y;
    
    // Sync minimap when main map changes (not every frame)
    if (position_changed) {
        renderer_.setViewCenter(x, y);
    }
    if (floor_changed) {
        renderer_.setFloor(floor);
        last_synced_floor_ = floor;
    }
}

void MinimapWindow::render(bool* p_visible) {
    // Use external visibility flag if provided, otherwise use internal one
    bool* vis_ptr = p_visible ? p_visible : &visible_;
    
    if (p_visible && !*p_visible) return;

    ImGui::SetNextWindowSize(ImVec2(Config::UI::MINIMAP_WINDOW_W, Config::UI::MINIMAP_WINDOW_H), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Minimap", vis_ptr)) {
        ImGui::End();
        return;
    }
    
    renderToolbar();
    renderMinimapImage();
    
    ImGui::End();
}

void MinimapWindow::renderToolbar() {
    // Zoom controls with +/- buttons
    if (ImGui::SmallButton(ICON_FA_MAGNIFYING_GLASS_MINUS)) {
        renderer_.zoomOut();
    }
    ImGui::SameLine();
    ImGui::Text("%s", renderer_.getZoomString().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_MAGNIFYING_GLASS_PLUS)) {
        renderer_.zoomIn();
    }
    ImGui::SameLine();
    ImGui::TextDisabled(ICON_FA_CIRCLE_QUESTION);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zoom: + = closer view, - = wider view\nx2/x4 = magnified (multiple pixels per tile)\n1:2/1:4 = overview (multiple tiles per pixel)");
    }
    
    // Separator between zoom and floor controls
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    
    // Floor controls (inline with zoom)
    int16_t floor = renderer_.getFloor();
    ImGui::Text("F:%d", floor);
    ImGui::SameLine();
    
    // Floor Up (PageUp)
    ImGui::BeginDisabled(floor <= 0);
    if (ImGui::SmallButton(ICON_FA_ARROW_UP "##floor")) {
        renderer_.setFloor(floor - 1);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Go up a floor (%s)", Input::Hotkeys::formatShortcut(Input::Hotkeys::FLOOR_UP).c_str());
    }

    ImGui::SameLine();

    // Floor Down (PageDown)
    ImGui::BeginDisabled(floor >= 15);
    if (ImGui::SmallButton(ICON_FA_ARROW_DOWN "##floor")) {
        renderer_.setFloor(floor + 1);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Go down a floor (%s)", Input::Hotkeys::formatShortcut(Input::Hotkeys::FLOOR_DOWN).c_str());
    }

    // Sync button (only if desynced)
    // Desync logic: if minimap center is significantly different from main camera
    bool is_desynced = (renderer_.getCenterX() != main_camera_x_ || renderer_.getCenterY() != main_camera_y_);

    if (is_desynced) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (ImGui::SmallButton(ICON_FA_LOCATION_CROSSHAIRS "##Sync")) {
            renderer_.setViewCenter(main_camera_x_, main_camera_y_);
            renderer_.setFloor(last_synced_floor_);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Center minimap on camera");
        }
    }
    
    ImGui::Separator();
}

void MinimapWindow::renderMinimapImage() {
    // Get available space for minimap
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    int width = static_cast<int>(content_region.x);
    int height = static_cast<int>(content_region.y);
    
    if (width <= 0 || height <= 0) return;
    
    // Update renderer with current size
    renderer_.update(width, height);
    
    // Render the minimap texture
    auto tex_id = renderer_.getTextureId();
    if (tex_id != 0) {
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        
        // Draw minimap with dark background
        ImGui::Image(
            (void*)(intptr_t)tex_id,
            content_region,
            ImVec2(0, 0), ImVec2(1, 1)
        );
        
        ImGuiIO& io = ImGui::GetIO();
        bool is_hovered = ImGui::IsItemHovered();
        
        // Scroll handling (when hovered)
        if (is_hovered && io.MouseWheel != 0) {
            if (io.KeyCtrl) {
                // Ctrl+scroll: floor change (inverted: scroll down = floor up, scroll up = floor down)
                int16_t floor = renderer_.getFloor();
                if (io.MouseWheel > 0 && floor < 15) {
                    renderer_.setFloor(floor + 1);
                } else if (io.MouseWheel < 0 && floor > 0) {
                    renderer_.setFloor(floor - 1);
                }
            } else {
                // Scroll without modifier: zoom
                if (io.MouseWheel > 0) {
                    renderer_.zoomIn();
                } else {
                    renderer_.zoomOut();
                }
            }
        }
        
        // Mouse interaction: Ctrl+click to jump main viewport, regular click/drag for pan
        if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (io.KeyCtrl) {
                // Ctrl+click: jump main viewport to this position
                handleMouseClick();
            } else {
                // Start drag for panning
                is_dragging_ = true;
                drag_start_screen_ = io.MousePos;
                Rendering::MinimapBounds bounds = renderer_.getViewBounds();
                drag_start_center_x_ = (bounds.min_x + bounds.max_x) / 2;
                drag_start_center_y_ = (bounds.min_y + bounds.max_y) / 2;
            }
        }
        
        // Handle dragging
        if (is_dragging_) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                Rendering::MinimapBounds bounds = renderer_.getViewBounds();
                float world_width = static_cast<float>(bounds.max_x - bounds.min_x);
                float world_height = static_cast<float>(bounds.max_y - bounds.min_y);
                
                if (world_width > 0 && world_height > 0) {
                    ImVec2 delta = ImVec2(drag_start_screen_.x - io.MousePos.x, 
                                          drag_start_screen_.y - io.MousePos.y);
                    int32_t dx = static_cast<int32_t>(delta.x / width * world_width);
                    int32_t dy = static_cast<int32_t>(delta.y / height * world_height);
                    
                    renderer_.setViewCenter(drag_start_center_x_ + dx, drag_start_center_y_ + dy);
                }
            } else {
                is_dragging_ = false;
            }
        }
        
        // Draw viewport rectangle overlay
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        Rendering::MinimapBounds bounds = renderer_.getViewBounds();
        
        // Calculate main viewport position on minimap using view bounds
        // Map world coordinates to screen coordinates
        float world_width = static_cast<float>(bounds.max_x - bounds.min_x);
        float world_height = static_cast<float>(bounds.max_y - bounds.min_y);
        
        if (world_width > 0 && world_height > 0) {
            float cx = cursor_pos.x + (main_camera_x_ - bounds.min_x) / world_width * width;
            float cy = cursor_pos.y + (main_camera_y_ - bounds.min_y) / world_height * height;
            
            // Clamp to visible area
            if (cx >= cursor_pos.x && cx <= cursor_pos.x + width &&
                cy >= cursor_pos.y && cy <= cursor_pos.y + height) {
                const ImU32 white = Config::Colors::MINIMAP_VIEWPORT;
                draw_list->AddLine(ImVec2(cx - 5, cy), ImVec2(cx + 5, cy), white, 1.0f);
                draw_list->AddLine(ImVec2(cx, cy - 5), ImVec2(cx, cy + 5), white, 1.0f);
            }
        }
    } else {
        // No texture - show placeholder
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No map loaded");
    }
}

void MinimapWindow::handleMouseClick() {
    if (!viewport_sync_callback_ || !data_source_) return;
    
    // Get click position relative to minimap
    ImVec2 mouse_pos = ImGui::GetMousePos();
    ImVec2 item_min = ImGui::GetItemRectMin();
    
    int screen_x = static_cast<int>(mouse_pos.x - item_min.x);
    int screen_y = static_cast<int>(mouse_pos.y - item_min.y);
    
    // Convert to world coordinates
    int32_t world_x, world_y;
    renderer_.screenToWorld(screen_x, screen_y, world_x, world_y);
    
    // Clamp to valid map bounds to prevent sending invalid coordinates
    auto bounds = data_source_->getMapBounds();
    world_x = std::clamp(world_x, bounds.min_x, bounds.max_x);
    world_y = std::clamp(world_y, bounds.min_y, bounds.max_y);
    
    // Call sync callback with valid coordinates
    viewport_sync_callback_(world_x, world_y, renderer_.getFloor());
    
    // Also update our center
    renderer_.setViewCenter(world_x, world_y);
}

void MinimapWindow::saveState(AppLogic::EditorSession& session) {
    auto& state = session.getMinimapState();
    state.center_x = renderer_.getCenterX();
    state.center_y = renderer_.getCenterY();
    state.floor = renderer_.getFloor();
    state.zoom_level = renderer_.getZoomLevel();
}

void MinimapWindow::restoreState(const AppLogic::EditorSession& session) {
    const auto& state = session.getMinimapState();
    renderer_.setViewCenter(state.center_x, state.center_y);
    renderer_.setFloor(state.floor);
    renderer_.setZoomLevel(state.zoom_level);
}

MinimapWindow::~MinimapWindow() = default;

} // namespace UI
} // namespace MapEditor
