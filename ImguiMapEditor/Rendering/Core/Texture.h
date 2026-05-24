#pragma once

#include <glad/glad.h>
#include <cstdint>

namespace MapEditor {
namespace Rendering {

/**
 * RAII wrapper for OpenGL texture
 * Automatically releases GPU resources on destruction
 */
class Texture {
public:
    Texture() = default;
    
    /**
     * Create texture from RGBA data
     * @param width Texture width in pixels
     * @param height Texture height in pixels
     * @param rgba_data Pointer to RGBA pixel data (4 bytes per pixel)
     */
    Texture(uint32_t width, uint32_t height, const uint8_t* rgba_data);
    
    ~Texture();
    
    // Move-only (no copying GPU resources)
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    
    // Bind texture to a texture slot
    void bind(uint32_t slot = 0) const;
    static void unbind() ;
    
    // Accessors
    GLuint id() const { return id_; }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    bool isValid() const { return id_ != 0; }
    
    // Update texture data (must be same size)
    void update(const uint8_t* rgba_data) const;
    
    // Create from existing OpenGL texture ID (takes ownership)
    static Texture fromId(GLuint id, uint32_t width, uint32_t height);

private:
    void release();
    
    GLuint id_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace Rendering
} // namespace MapEditor
