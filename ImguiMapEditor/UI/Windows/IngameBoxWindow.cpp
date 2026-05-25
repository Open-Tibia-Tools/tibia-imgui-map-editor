#include "IngameBoxWindow.h"
#include <cstdio>
#include <glad/glad.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include "Application/EditorSession.h"
#include "Core/Config.h"
#include "Domain/ChunkedMap.h"
#include "Rendering/Map/MapRenderer.h"
#include "Rendering/Passes/IngamePreviewRenderer.h"
#include "Services/ViewSettings.h"

namespace MapEditor {
namespace UI {

IngameBoxWindow::IngameBoxWindow() = default;

void IngameBoxWindow::render(Domain::ChunkedMap* map,
                              Rendering::MapRenderer* renderer,
                              Services::ViewSettings& settings,
                              const Domain::Position& cursor_pos,
                              bool* p_open) {
    
    // Use external visibility flag if provided, otherwise use internal one
    bool* open_ptr = p_open ? p_open : &is_open_;
    
    // Window flags for floating, resizable window
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    // Calculate current pixel dimensions based on tile settings
    int pixel_width = preview_width_tiles_ * Config::Rendering::TILE_SIZE_INT;
    int pixel_height = preview_height_tiles_ * Config::Rendering::TILE_SIZE_INT;
    
    if (first_render_) {
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(pixel_width) + 16, static_cast<float>(pixel_height) + 100));
        first_render_ = false;
    }
    
    // Let ImGui handle visibility - this ensures docking layout is restored
    // NOTE: Window title must be static - ImGui uses title as window ID
    if (!ImGui::Begin("Ingame Preview", open_ptr, flags)) {
        ImGui::End();
        return;
    }
    
    // === Toggle Buttons Row ===
    // Follow Selection toggle button (crosshairs icon)
    ImVec4 follow_color = follow_cursor_ ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, follow_color);
    if (ImGui::Button(ICON_FA_CROSSHAIRS "##follow")) {
        follow_cursor_ = !follow_cursor_;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Follow Selection");
    }
    
    ImGui::SameLine();
    
    // Enable Lighting toggle button (lightbulb icon)
    ImVec4 light_color = settings.preview_lighting_enabled ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, light_color);
    if (ImGui::Button(ICON_FA_LIGHTBULB "##lighting")) {
        settings.preview_lighting_enabled = !settings.preview_lighting_enabled;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable Lighting");
    }
    
    // Ambient slider (only when lighting is enabled)
    if (settings.preview_lighting_enabled) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::SliderInt("##ambient", &settings.preview_ambient_light, 0, 255);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Ambient Light Level");
        }
    }
    
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    
    // === Dimension Controls ===
    // X dimension
    ImGui::Text("X:");
    ImGui::SameLine();
    if (ImGui::SmallButton("-##x")) {
        if (preview_width_tiles_ > Config::Preview::MIN_WIDTH_TILES) {
            preview_width_tiles_--;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Decrease preview width");
    }

    ImGui::SameLine();
    ImGui::Text("%d", preview_width_tiles_);
    ImGui::SameLine();
    if (ImGui::SmallButton("+##x")) {
        if (preview_width_tiles_ < Config::Preview::MAX_WIDTH_TILES) {
            preview_width_tiles_++;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Increase preview width");
    }
    
    ImGui::SameLine();
    ImGui::Text("Y:");
    ImGui::SameLine();
    if (ImGui::SmallButton("-##y")) {
        if (preview_height_tiles_ > Config::Preview::MIN_HEIGHT_TILES) {
            preview_height_tiles_--;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Decrease preview height");
    }

    ImGui::SameLine();
    ImGui::Text("%d", preview_height_tiles_);
    ImGui::SameLine();
    if (ImGui::SmallButton("+##y")) {
        if (preview_height_tiles_ < Config::Preview::MAX_HEIGHT_TILES) {
            preview_height_tiles_++;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Increase preview height");
    }
    
    // Always sync when following
    if (follow_cursor_) {
        locked_position_ = cursor_pos;
    } else {
        // Arrow key navigation when not following (and window is focused)
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) locked_position_.x -= 1;
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) locked_position_.x += 1;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) locked_position_.y -= 1;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) locked_position_.y += 1;
        }
    }
    
    ImGui::Separator();
    
    if (map && renderer) {
        renderContent(map, renderer, locked_position_, settings);
        
        if (fbo_ && fbo_->isValid()) {
            ImVec2 content_size = ImGui::GetContentRegionAvail();
            float scale_x = content_size.x / static_cast<float>(pixel_width);
            float scale_y = content_size.y / static_cast<float>(pixel_height);
            float scale = std::min(scale_x, scale_y);
            
            ImVec2 image_size(static_cast<float>(pixel_width) * scale, static_cast<float>(pixel_height) * scale);
            
            float offset_x = (content_size.x - image_size.x) * 0.5f;
            if (offset_x > 0) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
            }
            
            ImGui::Image(
                (void*)(intptr_t)fbo_->getTextureId(),
                image_size,
                ImVec2(0, 1), ImVec2(1, 0)
            );
        } else {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Initializing preview...");
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "No map loaded");
    }
    
    ImGui::End();
}

void IngameBoxWindow::renderContent(Domain::ChunkedMap* map,
                                     Rendering::MapRenderer* renderer,
                                     const Domain::Position& center,
                                     Services::ViewSettings& settings) {
    // Initialize framebuffer if needed
    if (!fbo_) {
        fbo_ = std::make_unique<Rendering::Framebuffer>();
    }
    
    // Initialize preview renderer if needed or if map renderer changed
    if (!renderer_ || current_map_renderer_ != renderer) {
        // If the main renderer is gone, or its resources are unavailable, we can't create the preview renderer.
        if (!renderer || !renderer->getSpriteBatch() || !renderer->getSpriteManager() || !renderer->getClientData()) {
            if (renderer) { // Only log error if renderer existed but resources were missing
                spdlog::error("IngameBoxWindow: Failed to initialize renderer - missing resources");
            }
            renderer_.reset();
            current_map_renderer_ = nullptr;
            return;
        }

        // All dependencies are available, create the renderer.
        renderer_ = std::make_unique<Rendering::IngamePreviewRenderer>(
            renderer->getTileRenderer(),
            *renderer->getSpriteBatch(),
            *renderer->getSpriteManager(),
            renderer->getClientData()
        );
        current_map_renderer_ = renderer;
    }

    // Calculate pixel dimensions based on current tile settings
    int pixel_width = preview_width_tiles_ * Config::Rendering::TILE_SIZE_INT;
    int pixel_height = preview_height_tiles_ * Config::Rendering::TILE_SIZE_INT;
    
    // Resize framebuffer to match preview size
    if (!fbo_->resize(pixel_width, pixel_height)) {
        spdlog::error("IngameBoxWindow: Failed to resize framebuffer");
        return;
    }
    
    // Bind our FBO and render directly to it
    fbo_->bind();
    
    // Render
    renderer_->render(*map, pixel_width, pixel_height,
                                  static_cast<float>(center.x), 
                                  static_cast<float>(center.y),
                                  center.z, 1.0f, &settings);  // Pass settings for lighting
    
    fbo_->unbind();
}

void IngameBoxWindow::saveState(AppLogic::EditorSession& session) {
    auto& state = session.getIngamePreviewState();
    state.is_open = is_open_;
    state.follow_cursor = follow_cursor_;
    state.locked_x = locked_position_.x;
    state.locked_y = locked_position_.y;
    state.locked_z = locked_position_.z;
    state.width_tiles = preview_width_tiles_;
    state.height_tiles = preview_height_tiles_;
}

void IngameBoxWindow::restoreState(const AppLogic::EditorSession& session) {
    const auto& state = session.getIngamePreviewState();
    is_open_ = state.is_open;
    follow_cursor_ = state.follow_cursor;
    locked_position_.x = state.locked_x;
    locked_position_.y = state.locked_y;
    locked_position_.z = state.locked_z;
    preview_width_tiles_ = state.width_tiles;
    preview_height_tiles_ = state.height_tiles;
}

} // namespace UI
} // namespace MapEditor
