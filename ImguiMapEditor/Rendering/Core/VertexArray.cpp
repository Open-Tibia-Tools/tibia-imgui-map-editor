#include "VertexArray.h"
#include <glad/glad.h>

namespace MapEditor {
namespace Rendering {

VertexArray::VertexArray() {
    glGenVertexArrays(1, &vao_);
}

VertexArray::~VertexArray() {
    release();
}

VertexArray::VertexArray(VertexArray&& other) noexcept
    : vao_(other.vao_)
    , vbo_(other.vbo_)
    , ebo_(other.ebo_)
    , index_count_(other.index_count_)
    , vbo_size_(other.vbo_size_)
{
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.index_count_ = 0;
    other.vbo_size_ = 0;
}

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
    if (this != &other) {
        release();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        ebo_ = other.ebo_;
        index_count_ = other.index_count_;
        vbo_size_ = other.vbo_size_;
        other.vao_ = 0;
        other.vbo_ = 0;
        other.ebo_ = 0;
        other.index_count_ = 0;
        other.vbo_size_ = 0;
    }
    return *this;
}

void VertexArray::bind() const {
    glBindVertexArray(vao_);
}

void VertexArray::unbind() {
    glBindVertexArray(0);
}

void VertexArray::setVertexBuffer(const void* data, size_t size, uint32_t stride,
                                   const std::vector<VertexAttribute>& attributes,
                                   bool dynamic) {
    bind();
    
    // Create VBO if needed
    if (vbo_ == 0) {
        glGenBuffers(1, &vbo_);
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), data,
                 dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    vbo_size_ = size;
    
    // Set up vertex attributes
    for (const auto& attr : attributes) {
        glEnableVertexAttribArray(attr.index);
        glVertexAttribPointer(
            attr.index,
            attr.size,
            attr.type,
            attr.normalized ? GL_TRUE : GL_FALSE,
            static_cast<GLsizei>(stride),
            reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.offset))
        );
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    unbind();
}

void VertexArray::updateVertexBuffer(const void* data, size_t size) const {
    if (vbo_ == 0 || size > vbo_size_) return;
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(size), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexArray::setIndexBuffer(const uint32_t* indices, size_t count, bool dynamic) {
    bind();
    
    // Create EBO if needed
    if (ebo_ == 0) {
        glGenBuffers(1, &ebo_);
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(count * sizeof(uint32_t)),
                 indices,
                 dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    index_count_ = count;
    
    unbind();
}

void VertexArray::updateIndexBuffer(const uint32_t* indices, size_t count) {
    if (ebo_ == 0) return;
    
    bind();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(count * sizeof(uint32_t)), indices);
    unbind();
    
    index_count_ = count;
}

void VertexArray::release() {
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    index_count_ = 0;
    vbo_size_ = 0;
}

} // namespace Rendering
} // namespace MapEditor
