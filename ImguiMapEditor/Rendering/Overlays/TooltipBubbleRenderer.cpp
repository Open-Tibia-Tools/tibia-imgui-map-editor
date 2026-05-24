#include "TooltipBubbleRenderer.h"
#include "../../Core/Config.h"
#include "Rendering/Resources/ShaderLoader.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <cstring>

namespace MapEditor {
namespace Rendering {

TooltipBubbleRenderer::TooltipBubbleRenderer() {
    pending_bubbles_.reserve(MAX_BUBBLES);
    instances_.reserve(MAX_BUBBLES);
}

TooltipBubbleRenderer::~TooltipBubbleRenderer() {
    // RAII handles cleanup via DeferredVAOHandle/DeferredVBOHandle
}

bool TooltipBubbleRenderer::initialize() {
    // Load shader from external files
    shader_ = ShaderLoader::load("tooltip_bubble");
    if (!shader_ || !shader_->isValid()) {
        spdlog::error("TooltipBubbleRenderer: Failed to load shader: {}",
                      shader_ ? shader_->getError() : "file not found");
        return false;
    }

    // Create VAO and VBOs
    vao_.create();
    quad_vbo_.create();
    instance_vbo_.create();

    glBindVertexArray(vao_.get());

    // Unit quad vertices (position only)
    float quad_vertices[] = {
        0.0f, 0.0f,  // top-left
        1.0f, 0.0f,  // top-right
        1.0f, 1.0f,  // bottom-right
        0.0f, 0.0f,  // top-left
        1.0f, 1.0f,  // bottom-right
        0.0f, 1.0f   // bottom-left
    };

    // Upload quad geometry (static)
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_.get());
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Location 0: position (vec2)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Create instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_.get());
    glBufferData(GL_ARRAY_BUFFER, MAX_BUBBLES * sizeof(BubbleInstance), nullptr, GL_DYNAMIC_DRAW);

    // Instance attributes
    size_t const stride = sizeof(BubbleInstance);
    
    // Location 1: rect (vec4) - x, y, w, h
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    // Location 2: background color (vec4)
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    // Location 3: border color (vec4)
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    // Location 4: rounding (float)
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);

    initialized_ = true;
    spdlog::info("TooltipBubbleRenderer initialized");
    return true;
}

void TooltipBubbleRenderer::begin(const glm::mat4& projection, float scale) {
    projection_ = projection;
    current_scale_ = scale;
    pending_bubbles_.clear();
    instances_.clear();
    in_batch_ = true;
}

void TooltipBubbleRenderer::addBubble(float screen_x, float screen_y, float tile_size,
                                       const std::string& text, bool is_waypoint) {
    if (!in_batch_ || pending_bubbles_.size() >= MAX_BUBBLES) return;

    float const scale = current_scale_;
    float const max_text_width = Config::Tooltip::MAX_WIDTH_BASE * scale;
    
    // Calculate text size
    ImVec2 const text_size = ImGui::CalcTextSize(text.c_str(), nullptr, false, max_text_width);
    
    ImVec2 const padding(4.0f * scale, 2.0f * scale);
    float const bubble_width = text_size.x + padding.x * 2;
    float const bubble_height = text_size.y + padding.y * 2;
    
    float const pointer_size = 5.0f * scale;
    float const center_x = screen_x + tile_size / 2.0f;
    float const bubble_left = center_x - bubble_width / 2.0f;
    float const bubble_bottom = screen_y - pointer_size;
    float const bubble_top = bubble_bottom - bubble_height;
    
    // Create bubble instance
    BubbleInstance inst;
    inst.x = bubble_left;
    inst.y = bubble_top;
    inst.w = bubble_width;
    inst.h = bubble_height;
    
    // Colors based on type
    if (is_waypoint) {
        // Green for waypoints
        inst.bg_r = 0.0f; inst.bg_g = 0.78f; inst.bg_b = 0.0f; inst.bg_a = 0.86f;
        inst.border_r = 0.0f; inst.border_g = 0.0f; inst.border_b = 0.0f; inst.border_a = 0.78f;
    } else {
        // Parchment yellow for regular tooltips
        inst.bg_r = 0.93f; inst.bg_g = 0.91f; inst.bg_b = 0.67f; inst.bg_a = 0.86f;
        inst.border_r = 0.0f; inst.border_g = 0.0f; inst.border_b = 0.0f; inst.border_a = 0.78f;
    }
    
    inst.rounding = 2.0f * scale;
    
    // Store for rendering
    PendingBubble pb;
    pb.instance = inst;
    pb.text = text;
    pb.text_x = bubble_left + padding.x;
    pb.text_y = bubble_top + padding.y;
    pb.max_text_width = max_text_width;
    
    pending_bubbles_.push_back(pb);
    instances_.push_back(inst);
}

void TooltipBubbleRenderer::endBubbles() {
    if (!in_batch_ || !initialized_) return;
    in_batch_ = false;
    
    if (instances_.empty()) return;
    
    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_.get());
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                    instances_.size() * sizeof(BubbleInstance),
                    instances_.data());
    
    // Save OpenGL state
    GLboolean const scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean const blend_enabled = glIsEnabled(GL_BLEND);
    GLint blend_src, blend_dst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst);
    
    // Set up rendering state
    if (scissor_enabled) glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render all bubbles in one draw call
    shader_->use();
    shader_->setMat4("uMVP", projection_);
    
    glBindVertexArray(vao_.get());
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(instances_.size()));
    glBindVertexArray(0);
    
    // Restore state
    if (scissor_enabled) glEnable(GL_SCISSOR_TEST);
    if (!blend_enabled) glDisable(GL_BLEND);
    glBlendFunc(blend_src, blend_dst);
}

void TooltipBubbleRenderer::renderText(ImDrawList* draw_list) {
    if (!draw_list) return;
    
    ImU32 const text_color = IM_COL32(0, 0, 0, 255);
    
    for (const auto& bubble : pending_bubbles_) {
        draw_list->AddText(
            nullptr, 0.0f,
            ImVec2(bubble.text_x, bubble.text_y),
            text_color,
            bubble.text.c_str(), nullptr, bubble.max_text_width
        );
    }
}

} // namespace Rendering
} // namespace MapEditor
