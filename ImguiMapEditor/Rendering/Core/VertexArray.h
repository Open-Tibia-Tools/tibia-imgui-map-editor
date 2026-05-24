#pragma once

#include <glad/glad.h>
#include <vector>
#include <cstdint>

namespace MapEditor {
namespace Rendering {

/**
 * Vertex attribute layout specification
 */
struct VertexAttribute {
    uint32_t index;
    int32_t size;       // Number of components (1-4)
    GLenum type;        // GL_FLOAT, GL_INT, etc.
    bool normalized;
    uint32_t offset;
};

/**
 * RAII wrapper for OpenGL Vertex Array Object (VAO)
 * Manages VAO, VBO, and optional EBO
 */
class VertexArray {
public:
    VertexArray();
    ~VertexArray();
    
    // Move-only
    VertexArray(VertexArray&& other) noexcept;
    VertexArray& operator=(VertexArray&& other) noexcept;
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    
    void bind() const;
    static void unbind() ;
    
    /**
     * Set vertex buffer data with layout
     * @param data Vertex data
     * @param size Size in bytes
     * @param stride Bytes per vertex
     * @param attributes Vertex attribute layout
     * @param dynamic true for GL_DYNAMIC_DRAW, false for GL_STATIC_DRAW
     */
    void setVertexBuffer(const void* data, size_t size, uint32_t stride,
                         const std::vector<VertexAttribute>& attributes,
                         bool dynamic = false);
    
    /**
     * Update vertex buffer data (must be same size or smaller)
     */
    void updateVertexBuffer(const void* data, size_t size) const;
    
    /**
     * Set index buffer
     * @param indices Index data
     * @param count Number of indices
     * @param dynamic true for GL_DYNAMIC_DRAW
     */
    void setIndexBuffer(const uint32_t* indices, size_t count, bool dynamic = false);
    
    /**
     * Update index buffer data
     */
    void updateIndexBuffer(const uint32_t* indices, size_t count);
    
    // Accessors
    GLuint id() const { return vao_; }
    size_t getIndexCount() const { return index_count_; }
    bool hasIndices() const { return ebo_ != 0; }
    bool isValid() const { return vao_ != 0; }

private:
    void release();
    
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    size_t index_count_ = 0;
    size_t vbo_size_ = 0;
};

/**
 * Helper to create common vertex layouts
 */
namespace VertexLayouts {
    // Position only: vec3
    inline std::vector<VertexAttribute> position() {
        return {{0, 3, GL_FLOAT, false, 0}};
    }
    
    // Position + UV: vec3, vec2
    inline std::vector<VertexAttribute> positionUV() {
        return {
            {0, 3, GL_FLOAT, false, 0},
            {1, 2, GL_FLOAT, false, 3 * sizeof(float)}
        };
    }
    
    // Position + UV + Color: vec3, vec2, vec4
    inline std::vector<VertexAttribute> positionUVColor() {
        return {
            {0, 3, GL_FLOAT, false, 0},
            {1, 2, GL_FLOAT, false, 3 * sizeof(float)},
            {2, 4, GL_FLOAT, false, 5 * sizeof(float)}
        };
    }
    
    // 2D Position + UV: vec2, vec2
    inline std::vector<VertexAttribute> position2DUV() {
        return {
            {0, 2, GL_FLOAT, false, 0},
            {1, 2, GL_FLOAT, false, 2 * sizeof(float)}
        };
    }
}

} // namespace Rendering
} // namespace MapEditor
