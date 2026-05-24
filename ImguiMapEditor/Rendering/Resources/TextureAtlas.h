#pragma once

#include "Core/Config.h"
#include <cstdint>
#include <glad/glad.h>
#include <optional>


namespace MapEditor {
namespace Rendering {

/**
 * Represents a region within a texture atlas array where a sprite is stored.
 * Contains UV coordinates and layer for rendering.
 */
struct AtlasRegion {
  uint32_t atlas_index = 0; // Which layer in the texture array
  float u_min = 0.0f;       // UV left
  float v_min = 0.0f;       // UV top
  float u_max = 1.0f;       // UV right
  float v_max = 1.0f;       // UV bottom
};

/**
 * RAII wrapper for a GL_TEXTURE_2D_ARRAY.
 * Manages multiple 4096x4096 layers in a single texture array.
 * Each layer holds 16384 32x32 sprites.
 *
 * This enables single-draw-call rendering by sampling layer via atlas_layer.
 */
class TextureAtlas {
public:
  static constexpr int ATLAS_SIZE = Config::Rendering::ATLAS_SIZE;
  static constexpr int SPRITE_SIZE = Config::Rendering::SPRITE_SIZE;
  static constexpr int SPRITES_PER_ROW = Config::Rendering::SPRITES_PER_ROW;
  static constexpr int SPRITES_PER_LAYER = Config::Rendering::SPRITES_PER_LAYER;
  static constexpr int MAX_LAYERS = Config::Rendering::MAX_ATLAS_LAYERS;

  TextureAtlas();
  ~TextureAtlas();

  // Non-copyable (GPU resource)
  TextureAtlas(const TextureAtlas &) = delete;
  TextureAtlas &operator=(const TextureAtlas &) = delete;

  // Moveable
  TextureAtlas(TextureAtlas &&other) noexcept;
  TextureAtlas &operator=(TextureAtlas &&other) noexcept;

  /**
   * Initialize the texture array on the GPU.
   * Must be called before addSprite().
   * @param initial_layers Number of layers to pre-allocate (default: 1)
   * @return true if successful
   */
  bool initialize(int initial_layers = 1);

  /**
   * Add a 32x32 sprite to the atlas array.
   * Automatically adds new layers as needed.
   * @param rgba_data Pointer to 32*32*4 bytes of RGBA pixel data
   * @return AtlasRegion with layer and UV coordinates, or nullopt on failure
   */
  std::optional<AtlasRegion> addSprite(const uint8_t *rgba_data);

  /**
   * Add a sprite from a bound PBO (Pixel Buffer Object).
   * GL_PIXEL_UNPACK_BUFFER must be bound before calling.
   * @param pbo_offset Offset into the PBO (cast as pointer)
   * @return AtlasRegion with layer and UV coordinates, or nullopt on failure
   */
  std::optional<AtlasRegion> addSpriteFromPBO(const uint8_t *pbo_offset);

  /**
   * Bind the texture array to a texture slot.
   * @param slot Texture unit (0-15)
   */
  void bind(uint32_t slot = 0) const;

  /**
   * Unbind texture from current slot.
   */
  static void unbind() ;

  /**
   * Get number of layers in use.
   */
  int getLayerCount() const { return layer_count_; }

  /**
   * Get total number of sprites across all layers.
   */
  int getTotalSpriteCount() const { return total_sprite_count_; }

  /**
   * Get OpenGL texture ID.
   */
  GLuint id() const { return texture_id_; }

  /**
   * Check if atlas is valid (initialized).
   */
  bool isValid() const { return texture_id_ != 0; }

  /**
   * Get version number. Incremented when texture object changes (e.g., during
   * expansion). Used by SpriteBatch to detect stale texture bindings.
   */
  uint64_t getVersion() const { return version_; }

private:
  bool addLayer();
  void release();

  GLuint texture_id_ = 0;
  int layer_count_ = 0;        // Number of layers allocated
  int allocated_layers_ = 0;   // Number of layers in GPU memory
  int total_sprite_count_ = 0; // Total sprites across all layers
  int current_layer_ = 0;      // Current layer being filled
  int next_x_ = 0;             // Next slot X in current layer
  int next_y_ = 0;             // Next slot Y in current layer
  uint64_t version_ = 0;       // Incremented on texture object change
};

} // namespace Rendering
} // namespace MapEditor
