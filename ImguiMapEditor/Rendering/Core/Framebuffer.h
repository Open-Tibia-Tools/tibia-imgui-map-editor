#pragma once

#include <cstdint>
#include <glad/glad.h>

namespace MapEditor {
namespace Rendering {

/**
 * Simple OpenGL framebuffer for off-screen rendering
 * Renders to a texture that can be displayed in ImGui
 */
class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();
    
    // Non-copyable, but movable
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;
    
    /**
     * Create or resize the framebuffer
     * @return true if successful
     */
    bool resize(int width, int height);
    
    /**
     * Bind this framebuffer for rendering
     */
    void bind() const;
    
    /**
     * Unbind (return to default framebuffer)
     */
    static void unbind();
    
    /**
     * Get the color attachment texture ID for ImGui::Image()
     */
    GLuint getTextureId() const { return color_texture_; }
    
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    bool isValid() const { return fbo_ != 0; }

private:
    void cleanup();
    
    GLuint fbo_ = 0;
    GLuint color_texture_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace Rendering
} // namespace MapEditor
