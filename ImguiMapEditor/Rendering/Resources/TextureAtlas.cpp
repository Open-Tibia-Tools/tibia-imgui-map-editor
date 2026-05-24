#include "Rendering/Resources/TextureAtlas.h"
#include <cstring>
#include <spdlog/spdlog.h>
#include <vector>

namespace MapEditor {
namespace Rendering {

TextureAtlas::TextureAtlas() = default;

TextureAtlas::~TextureAtlas() { release(); }

TextureAtlas::TextureAtlas(TextureAtlas &&other) noexcept
    : texture_id_(other.texture_id_), layer_count_(other.layer_count_),
      allocated_layers_(other.allocated_layers_),
      total_sprite_count_(other.total_sprite_count_),
      current_layer_(other.current_layer_), next_x_(other.next_x_),
      next_y_(other.next_y_) {
  other.texture_id_ = 0;
  other.layer_count_ = 0;
  other.allocated_layers_ = 0;
  other.total_sprite_count_ = 0;
  other.current_layer_ = 0;
  other.next_x_ = 0;
  other.next_y_ = 0;
}

TextureAtlas &TextureAtlas::operator=(TextureAtlas &&other) noexcept {
  if (this != &other) {
    release();
    texture_id_ = other.texture_id_;
    layer_count_ = other.layer_count_;
    allocated_layers_ = other.allocated_layers_;
    total_sprite_count_ = other.total_sprite_count_;
    current_layer_ = other.current_layer_;
    next_x_ = other.next_x_;
    next_y_ = other.next_y_;
    other.texture_id_ = 0;
    other.layer_count_ = 0;
    other.allocated_layers_ = 0;
    other.total_sprite_count_ = 0;
    other.current_layer_ = 0;
    other.next_x_ = 0;
    other.next_y_ = 0;
  }
  return *this;
}

bool TextureAtlas::initialize(int initial_layers) {
  if (texture_id_ != 0) {
    return true; // Already initialized
  }

  if (initial_layers < 1)
    initial_layers = 1;
  if (initial_layers > MAX_LAYERS)
    initial_layers = MAX_LAYERS;

  glGenTextures(1, &texture_id_);
  if (texture_id_ == 0) {
    spdlog::error("TextureAtlas: Failed to generate texture");
    return false;
  }

  glBindTexture(GL_TEXTURE_2D_ARRAY, texture_id_);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Allocate texture array storage
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, ATLAS_SIZE, ATLAS_SIZE,
               initial_layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  allocated_layers_ = initial_layers;
  layer_count_ = 1; // Start with one active layer
  current_layer_ = 0;
  next_x_ = 0;
  next_y_ = 0;

  spdlog::info("TextureAtlas created: {}x{} x {} layers, id={}", ATLAS_SIZE,
               ATLAS_SIZE, initial_layers, texture_id_);
  return true;
}

bool TextureAtlas::addLayer() {
  spdlog::warn("TextureAtlas::addLayer called - state: layer_count={}, "
               "allocated_layers={}, texture_id={}",
               layer_count_, allocated_layers_, texture_id_);

  if (layer_count_ >= MAX_LAYERS) {
    spdlog::error("TextureAtlas: Max layers ({}) reached", MAX_LAYERS);
    return false;
  }

  // If we need more layers than allocated, reallocate
  if (layer_count_ >= allocated_layers_) {
    int new_allocated = std::min(allocated_layers_ * 2, MAX_LAYERS);

    spdlog::warn(
        "TextureAtlas: EXPANSION TRIGGERED! {} -> {} layers (old_texture={})",
        allocated_layers_, new_allocated, texture_id_);

    // Create new larger texture array
    GLuint old_texture = texture_id_;
    GLuint new_texture;
    glGenTextures(1, &new_texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, new_texture);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, ATLAS_SIZE, ATLAS_SIZE,
                 new_allocated, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
      spdlog::error("TextureAtlas: glTexImage3D failed with GL error 0x{:04X}",
                    err);
      glDeleteTextures(1, &new_texture);
      return false;
    }

    // Copy existing layers to new texture using glCopyImageSubData (GL 4.3+)
    if (glCopyImageSubData) {
      spdlog::debug("TextureAtlas: Using glCopyImageSubData for layer copy");
      glCopyImageSubData(texture_id_, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
                         new_texture, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
                         ATLAS_SIZE, ATLAS_SIZE, allocated_layers_);
      err = glGetError();
      if (err != GL_NO_ERROR) {
        spdlog::error(
            "TextureAtlas: glCopyImageSubData failed with GL error 0x{:04X}",
            err);
      }
    } else {
      spdlog::warn("TextureAtlas: glCopyImageSubData not available, using FBO "
                   "fallback for layer-by-layer copy");
      // FBO-based fallback: read each layer individually via glReadPixels
      // This is required because glGetTexImage on GL_TEXTURE_2D_ARRAY only
      // reads the first layer, not all layers.
      static constexpr int RGBA_CHANNELS = 4;
      size_t const layer_size =
          static_cast<size_t>(ATLAS_SIZE) * ATLAS_SIZE * RGBA_CHANNELS;
      std::vector<uint8_t> buffer(layer_size);

      GLuint fbo;
      glGenFramebuffers(1, &fbo);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);

      for (int layer = 0; layer < allocated_layers_; ++layer) {
        // Attach old texture layer to FBO for reading
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  texture_id_, 0, layer);

        GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
          spdlog::error("TextureAtlas: FBO incomplete for layer {} (status "
                        "0x{:04X})",
                        layer, status);
          continue;
        }

        // Read pixels from FBO
        glReadPixels(0, 0, ATLAS_SIZE, ATLAS_SIZE, GL_RGBA, GL_UNSIGNED_BYTE,
                     buffer.data());

        // Upload to new texture
        glBindTexture(GL_TEXTURE_2D_ARRAY, new_texture);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, ATLAS_SIZE,
                        ATLAS_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        buffer.data());
      }

      glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &fbo);
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // Synchronize GPU before deleting old texture
    glFinish();

    // Delete old texture, use new one
    glDeleteTextures(1, &texture_id_);
    texture_id_ = new_texture;
    allocated_layers_ = new_allocated;
    version_++; // Increment version so SpriteBatch knows to rebind

    spdlog::warn("TextureAtlas: Expanded to {} layers (old_tex={}, new_tex={}, "
                 "version={})",
                 new_allocated, old_texture, new_texture, version_);
  }

  layer_count_++;
  current_layer_ = layer_count_ - 1;
  next_x_ = 0;
  next_y_ = 0;

  spdlog::debug(
      "TextureAtlas::addLayer complete - layer_count={}, current_layer={}",
      layer_count_, current_layer_);

  return true;
}

std::optional<AtlasRegion> TextureAtlas::addSprite(const uint8_t *rgba_data) {
  if (!isValid()) {
    spdlog::error("TextureAtlas::addSprite called on uninitialized atlas");
    return std::nullopt;
  }

  if (!rgba_data) {
    spdlog::error("TextureAtlas::addSprite called with null data");
    return std::nullopt;
  }

  // Check if current layer is full
  if (next_y_ >= SPRITES_PER_ROW) {
    if (!addLayer()) {
      return std::nullopt; // Can't add more layers
    }
  }

  // Calculate pixel position in current layer
  int const pixel_x = next_x_ * SPRITE_SIZE;
  int const pixel_y = next_y_ * SPRITE_SIZE;

  // Upload sprite data to texture array
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture_id_);
  glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, pixel_x, pixel_y, current_layer_,
                  SPRITE_SIZE, SPRITE_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                  rgba_data);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  // Calculate UV coordinates with half-texel inset
  const float texel_size = 1.0f / static_cast<float>(ATLAS_SIZE);
  const float half_texel = texel_size * 0.5f;

  AtlasRegion region;
  region.atlas_index = static_cast<uint32_t>(current_layer_);
  region.u_min = static_cast<float>(pixel_x) / ATLAS_SIZE + half_texel;
  region.v_min = static_cast<float>(pixel_y) / ATLAS_SIZE + half_texel;
  region.u_max =
      static_cast<float>(pixel_x + SPRITE_SIZE) / ATLAS_SIZE - half_texel;
  region.v_max =
      static_cast<float>(pixel_y + SPRITE_SIZE) / ATLAS_SIZE - half_texel;

  // Advance to next slot
  next_x_++;
  if (next_x_ >= SPRITES_PER_ROW) {
    next_x_ = 0;
    next_y_++;
  }
  total_sprite_count_++;

  return region;
}

std::optional<AtlasRegion>
TextureAtlas::addSpriteFromPBO(const uint8_t *pbo_offset) {
  if (!isValid()) {
    spdlog::error(
        "TextureAtlas::addSpriteFromPBO called on uninitialized atlas");
    return std::nullopt;
  }

  // Check if current layer is full
  if (next_y_ >= SPRITES_PER_ROW) {
    if (!addLayer()) {
      return std::nullopt;
    }
  }

  int const pixel_x = next_x_ * SPRITE_SIZE;
  int const pixel_y = next_y_ * SPRITE_SIZE;

  // Upload from PBO (GL_PIXEL_UNPACK_BUFFER must be bound!)
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture_id_);
  glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, pixel_x, pixel_y, current_layer_,
                  SPRITE_SIZE, SPRITE_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                  pbo_offset);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  const float texel_size = 1.0f / static_cast<float>(ATLAS_SIZE);
  const float half_texel = texel_size * 0.5f;

  AtlasRegion region;
  region.atlas_index = static_cast<uint32_t>(current_layer_);
  region.u_min = static_cast<float>(pixel_x) / ATLAS_SIZE + half_texel;
  region.v_min = static_cast<float>(pixel_y) / ATLAS_SIZE + half_texel;
  region.u_max =
      static_cast<float>(pixel_x + SPRITE_SIZE) / ATLAS_SIZE - half_texel;
  region.v_max =
      static_cast<float>(pixel_y + SPRITE_SIZE) / ATLAS_SIZE - half_texel;

  next_x_++;
  if (next_x_ >= SPRITES_PER_ROW) {
    next_x_ = 0;
    next_y_++;
  }
  total_sprite_count_++;

  return region;
}

void TextureAtlas::bind(uint32_t slot) const {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texture_id_);
}

void TextureAtlas::unbind() { glBindTexture(GL_TEXTURE_2D_ARRAY, 0); }

void TextureAtlas::release() {
  if (texture_id_ != 0) {
    glDeleteTextures(1, &texture_id_);
    texture_id_ = 0;
  }
  layer_count_ = 0;
  allocated_layers_ = 0;
  total_sprite_count_ = 0;
  current_layer_ = 0;
  next_x_ = 0;
  next_y_ = 0;
}

} // namespace Rendering
} // namespace MapEditor
