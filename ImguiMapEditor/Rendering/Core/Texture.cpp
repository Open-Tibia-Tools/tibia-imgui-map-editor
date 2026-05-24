#include "Texture.h"
#include <utility>
#include <glad/glad.h>

namespace MapEditor {
namespace Rendering {

Texture::Texture(uint32_t width, uint32_t height, const uint8_t* rgba_data)
    : width_(width)
    , height_(height)
{
    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 
                 static_cast<GLsizei>(width), 
                 static_cast<GLsizei>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture() {
    release();
}

Texture::Texture(Texture&& other) noexcept
    : id_(other.id_)
    , width_(other.width_)
    , height_(other.height_)
{
    other.id_ = 0;
    other.width_ = 0;
    other.height_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        release();
        id_ = other.id_;
        width_ = other.width_;
        height_ = other.height_;
        other.id_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

void Texture::bind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::unbind() {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::update(const uint8_t* rgba_data) const {
    if (id_ == 0) return;
    
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    static_cast<GLsizei>(width_),
                    static_cast<GLsizei>(height_),
                    GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture Texture::fromId(GLuint id, uint32_t width, uint32_t height) {
    Texture tex;
    tex.id_ = id;
    tex.width_ = width;
    tex.height_ = height;
    return tex;
}

void Texture::release() {
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

} // namespace Rendering
} // namespace MapEditor
