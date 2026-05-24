#include "Rendering/Backend/SpriteBatch.h"
#include "Rendering/Resources/ShaderLoader.h"
#include "Rendering/Resources/SpriteAtlasLUT.h"
#include "Services/SpriteManager.h"
#include <array>
#include <cstring>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <numeric>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace Rendering {

SpriteBatch::SpriteBatch() {
  // Reserve capacity upfront to avoid reallocations
  pending_sprites_.reserve(MAX_SPRITES_PER_BATCH);
}

SpriteBatch::~SpriteBatch() {
  // RAII handles cleanup automatically via DeferredVAOHandle/DeferredVBOHandle
  // ring_buffer_ cleanup handled by its destructor
}

SpriteBatch::SpriteBatch(SpriteBatch &&other) noexcept
    : shader_(std::move(other.shader_)), vao_(std::move(other.vao_)),
      quad_vbo_(std::move(other.quad_vbo_)),
      quad_ebo_(std::move(other.quad_ebo_)),
      ring_buffer_(std::move(other.ring_buffer_)),
      pending_sprites_(std::move(other.pending_sprites_)),
      projection_(other.projection_), in_batch_(other.in_batch_),
      mdi_renderer_(std::move(other.mdi_renderer_)), use_mdi_(other.use_mdi_),
      draw_call_count_(other.draw_call_count_),
      sprite_count_(other.sprite_count_) {
  other.in_batch_ = false;
  other.use_mdi_ = false;
  other.draw_call_count_ = 0;
  other.sprite_count_ = 0;
}

SpriteBatch &SpriteBatch::operator=(SpriteBatch &&other) noexcept {
  if (this != &other) {
    shader_ = std::move(other.shader_);
    vao_ = std::move(other.vao_);
    quad_vbo_ = std::move(other.quad_vbo_);
    quad_ebo_ = std::move(other.quad_ebo_);
    ring_buffer_ = std::move(other.ring_buffer_);
    pending_sprites_ = std::move(other.pending_sprites_);
    projection_ = other.projection_;
    in_batch_ = other.in_batch_;
    mdi_renderer_ = std::move(other.mdi_renderer_);
    use_mdi_ = other.use_mdi_;
    draw_call_count_ = other.draw_call_count_;
    sprite_count_ = other.sprite_count_;

    other.in_batch_ = false;
    other.use_mdi_ = false;
    other.draw_call_count_ = 0;
    other.sprite_count_ = 0;
  }
  return *this;
}

bool SpriteBatch::initialize() {
  // Load shader from external files
  shader_ = ShaderLoader::load("sprite_batch");
  if (!shader_ || !shader_->isValid()) {
    spdlog::error("SpriteBatch: Failed to load shader: {}",
                  shader_ ? shader_->getError() : "file not found");
    return false;
  }

  // Initialize ring buffer for instance data
  if (!ring_buffer_.initialize(sizeof(SpriteInstance), MAX_SPRITES_PER_BATCH)) {
    spdlog::error("SpriteBatch: Failed to initialize ring buffer");
    return false;
  }

  // Create VAO and VBOs using deferred RAII handles
  vao_.create();
  quad_vbo_.create();
  quad_ebo_.create();

  glBindVertexArray(vao_.get());

  // Unit quad vertices (position + texcoord)
  // Top-left origin, Y-down coordinate system
  float quad_vertices[] = {
      // pos      // texcoord
      0.0f, 0.0f, 0.0f, 0.0f, // top-left
      1.0f, 0.0f, 1.0f, 0.0f, // top-right
      1.0f, 1.0f, 1.0f, 1.0f, // bottom-right
      0.0f, 1.0f, 0.0f, 1.0f  // bottom-left
  };

  unsigned int quad_indices[] = {0, 1, 2, 2, 3, 0};

  // Upload quad geometry (static)
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_.get());
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices,
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ebo_.get());
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices,
               GL_STATIC_DRAW);

  // Vertex attributes for quad
  // Location 0: position (vec2)
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Location 1: texcoord (vec2)
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Bind ring buffer for instance attributes
  glBindBuffer(GL_ARRAY_BUFFER, ring_buffer_.getBufferId());

  // Location 2: rect (vec4) - x, y, w, h
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteInstance),
                        (void *)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1); // Per-instance

  // Location 3: uv (vec4) - u_min, v_min, u_max, v_max
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteInstance),
                        (void *)(4 * sizeof(float)));
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1); // Per-instance

  // Location 4: tint (vec4) - r, g, b, a
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteInstance),
                        (void *)(8 * sizeof(float)));
  glEnableVertexAttribArray(4);
  glVertexAttribDivisor(4, 1); // Per-instance

  // Location 5: atlas layer (float) - for texture array sampling
  glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(SpriteInstance),
                        (void *)(12 * sizeof(float)));
  glEnableVertexAttribArray(5);
  glVertexAttribDivisor(5, 1); // Per-instance

  glBindVertexArray(0);

  // Initialize multi-draw indirect if GL 4.3+ available
  if (mdi_renderer_.initialize()) {
    use_mdi_ = true;
    spdlog::info("SpriteBatch: Multi-draw indirect enabled (GL 4.3+)");
  }

  spdlog::info("SpriteBatch initialized with {} ring buffer",
               ring_buffer_.isPersistentlyMapped() ? "persistent-mapped"
                                                   : "orphaning");
  return true;
}

void SpriteBatch::begin(const glm::mat4 &projection) {
  projection_ = projection;
  pending_sprites_.clear();
  in_batch_ = true;
  mode_ = BatchMode::Sprites; // [UB FIX] Ensure mode is reset to Sprites (in case previous tile batch wasn't closed correctly)
  draw_call_count_ = 0;
  sprite_count_ = 0;

  // Reset state cache for new frame
  last_bound_vao_ = 0;

  // Set shader state once at beginning
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  shader_->use();
  shader_->setMat4("uMVP", projection_);
  shader_->setInt("uTextureArray", 0);
  shader_->setVec4("uGlobalTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

void SpriteBatch::setGlobalTint(float r, float g, float b, float a) {
  if (!in_batch_)
    return;

  if (mode_ == BatchMode::Sprites) {
    // Flush pending sprites before changing state
    // The design assumes setGlobalTint is called for a whole batch or section.
    // If pending_sprites_ is not empty, we MUST flush it before changing
    // uniform, otherwise previous sprites get new tint. BUT we don't have
    // AtlasManager here. For GhostFloorRenderer, we set tint ONCE per floor
    // (before any draws). So for now, we assume this is called when
    // pending_sprites_ is empty, or warn.
    if (!pending_sprites_.empty()) {
      spdlog::warn("SpriteBatch::setGlobalTint called with pending sprites! "
                   "Flush batch first or set tint before drawing.");
    }
    shader_->setVec4("uGlobalTint", glm::vec4(r, g, b, a));
  } else {
    // Tile Mode: Apply to tile shader
    // In Tile Mode, drawTileInstances executes immediately, so setting tint
    // affects next draw call.
    if (tile_shader_) {
      tile_shader_->setVec4("uGlobalTint", glm::vec4(r, g, b, a));
    }
  }
}

void SpriteBatch::ensureCapacity(size_t capacity) {
  if (pending_sprites_.capacity() < capacity) {
    pending_sprites_.reserve(capacity);
  }
}

void SpriteBatch::draw(float x, float y, float w, float h,
                       const AtlasRegion &region) {
  draw(x, y, w, h, region, 1.0f, 1.0f, 1.0f, 1.0f);
}

void SpriteBatch::draw(float x, float y, float w, float h,
                       const AtlasRegion &region, float r, float g, float b,
                       float a) {
  if (!in_batch_) {
    spdlog::warn("SpriteBatch::draw called outside begin/end");
    return;
  }

  SpriteInstance &inst = pending_sprites_.emplace_back();
  inst.x = x;
  inst.y = y;
  inst.w = w;
  inst.h = h;
  inst.u_min = region.u_min;
  inst.v_min = region.v_min;
  inst.u_max = region.u_max;
  inst.v_max = region.v_max;
  inst.r = r;
  inst.g = g;
  inst.b = b;
  inst.a = a;
  inst.atlas_layer = static_cast<float>(region.atlas_index);
  inst._pad1 = inst._pad2 = inst._pad3 = 0.0f;
}

void SpriteBatch::flush(const AtlasManager &atlas_manager) {
  if (pending_sprites_.empty())
    return;

  atlas_manager.bind(0);

  // Optimization: avoid redundant VAO binding
  if (last_bound_vao_ != vao_.get()) {
    glBindVertexArray(vao_.get());
    last_bound_vao_ = vao_.get();
  }

  glBindBuffer(GL_ARRAY_BUFFER, ring_buffer_.getBufferId());

  size_t const total_sprites = pending_sprites_.size();
  size_t processed_sprites = 0;
  size_t const max_per_batch = ring_buffer_.getMaxElements();

  while (processed_sprites < total_sprites) {
    size_t const batch_size =
        std::min(total_sprites - processed_sprites, max_per_batch);

    void *buffer_ptr = ring_buffer_.waitAndMap(batch_size);
    if (!buffer_ptr)
      break;

    memcpy(buffer_ptr, pending_sprites_.data() + processed_sprites,
           batch_size * sizeof(SpriteInstance));
    ring_buffer_.finishWrite();

    size_t const section_offset = ring_buffer_.getCurrentSectionOffset();

    // Update attribute pointers for ring buffer
    // Note: Using glVertexAttribPointer repeatedly inside render loop is not
    // ideal but safe. For static drawing we do this anyway.
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteInstance),
                          (void *)(section_offset + 0));
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteInstance),
                          (void *)(section_offset + 16));
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteInstance),
                          (void *)(section_offset + 32));
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(SpriteInstance),
                          (void *)(section_offset + 48));

    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0,
                            static_cast<GLsizei>(batch_size));

    draw_call_count_++;
    ring_buffer_.signalFinished();

    processed_sprites += batch_size;
    sprite_count_ += static_cast<int>(batch_size);
  }

  pending_sprites_.clear();
}

void SpriteBatch::end(const AtlasManager &atlas_manager) {
  if (!in_batch_) {
    spdlog::warn("SpriteBatch::end called without begin");
    return;
  }

  in_batch_ = false;

  // Flush remaining dynamic sprites
  flush(atlas_manager);

  glBindVertexArray(0);
  glDisable(GL_BLEND);
}

void SpriteBatch::drawTileInstances(GLuint vbo, size_t count,
                                    const AtlasManager &atlas_manager,
                                    SpriteAtlasLUT &lut) {
  if (!in_batch_)
    return;
  if (vbo == 0 || count == 0)
    return;

  // Ensure tile shader is initialized
  if (!ensureTileShaderInitialized())
    return;

  // Check if we're already in tile batch mode (beginTileBatch was called)
  // If so, shader and textures are already bound - just draw
  bool const needs_full_setup = (mode_ != BatchMode::Tiles);

  if (needs_full_setup) {
    // STANDALONE MODE: Called without beginTileBatch (e.g., GhostFloorRenderer)
    // Must do full setup here for backward compatibility

    // Flush any pending UV-based sprites first
    flush(atlas_manager);

    // Switch to tile shader
    tile_shader_->use();
    tile_shader_->setMat4("uMVP", projection_);
    tile_shader_->setInt("uTextureArray", 0);
    tile_shader_->setInt("uUseSSBO", lut.usesSSBO() ? 1 : 0);
    tile_shader_->setVec4("uGlobalTint", glm::vec4(1.0f));
    tile_shader_->setVec4("uPlaceholderColor",
                          glm::vec4(1.0f, 0.0f, 1.0f, 0.5f));

    // Bind atlas texture and LUT
    atlas_manager.bind(0);
    if (lut.usesSSBO()) {
      lut.bind(0);
    } else {
      lut.bind(1);
      tile_shader_->setInt("uSpriteLUT", 1);
    }

    // Bind tile VAO
    glBindVertexArray(tile_vao_.get());
    // [UB FIX] Update last_bound_vao_ to prevent subsequent flush() calls from assuming old VAO is still bound
    last_bound_vao_ = tile_vao_.get();
  }

  // Bind instance VBO and set up vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  // TileInstance layout: x,y,w,h (16), sprite_id (4), flags (4), rgba (16), pad
  // (8) = 48
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TileInstance),
                        (void *)0);
  glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(TileInstance),
                         (void *)(4 * sizeof(float)));
  glVertexAttribIPointer(5, 1, GL_UNSIGNED_INT, sizeof(TileInstance),
                         (void *)(4 * sizeof(float) + sizeof(uint32_t)));
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(TileInstance),
                        (void *)(4 * sizeof(float) + 2 * sizeof(uint32_t)));

  glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0,
                          static_cast<GLsizei>(count));

  draw_call_count_++;
  sprite_count_ += static_cast<int>(count);

  if (needs_full_setup) {
    // STANDALONE MODE: Restore original shader state
    shader_->use();
    shader_->setMat4("uMVP", projection_);
    shader_->setInt("uTextureArray", 0);
    shader_->setVec4("uGlobalTint", glm::vec4(1.0f));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Note: We don't restore VAO here, relying on next flush/draw to set it.
    // However, last_bound_vao_ was updated above, so next flush will see mismatch and rebind correctly.
    // If we rely on default VAO (0) being bound, we should ideally bind 0 here, but SpriteBatch
    // usually manages specific VAOs. Setting last_bound_vao_ prevents the corruption.
    // For extra safety, we can invalidate it to force rebind next time:
    last_bound_vao_ = 0;
  }
}

bool SpriteBatch::ensureTileShaderInitialized() {
  if (tile_shader_initialized_)
    return true;

  tile_shader_ = ShaderLoader::load("tile_batch");
  if (!tile_shader_ || !tile_shader_->isValid()) {
    spdlog::error("SpriteBatch: Failed to load tile_batch shader: {}",
                  tile_shader_ ? tile_shader_->getError() : "file not found");
    return false;
  }

  // Create VAO for TileInstance layout
  tile_vao_.create();
  glBindVertexArray(tile_vao_.get());

  // Bind quad VBO for vertex positions
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_.get());

  // Location 0: position (vec2)
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // Location 1: texcoord (vec2)
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Bind EBO
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ebo_.get());

  // Instance attributes will be set per-draw from the tile VBO
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1);
  glEnableVertexAttribArray(4);
  glVertexAttribDivisor(4, 1);
  glEnableVertexAttribArray(5);
  glVertexAttribDivisor(5, 1);

  glBindVertexArray(0);
  tile_shader_initialized_ = true;
  spdlog::info("SpriteBatch: Tile shader and VAO initialized");
  return true;
}

void SpriteBatch::beginTileBatch(const glm::mat4 &projection,
                                 const AtlasManager &atlas_manager,
                                 SpriteAtlasLUT &lut) {
  projection_ = projection;
  in_batch_ = true;
  mode_ = BatchMode::Tiles;
  draw_call_count_ = 0;
  sprite_count_ = 0;

  // Ensure shader and VAO are initialized
  if (!ensureTileShaderInitialized())
    return;

  // Enable blending for tile rendering
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Set up tile shader (ONCE for entire batch)
  tile_shader_->use();
  tile_shader_->setMat4("uMVP", projection_);
  tile_shader_->setInt("uTextureArray", 0);
  tile_shader_->setInt("uUseSSBO", lut.usesSSBO() ? 1 : 0);
  tile_shader_->setVec4("uGlobalTint", glm::vec4(1.0f));
  tile_shader_->setVec4("uPlaceholderColor", glm::vec4(1.0f, 0.0f, 1.0f, 0.5f));

  // Bind atlas texture and LUT (ONCE for entire batch)
  atlas_manager.bind(0);
  if (lut.usesSSBO()) {
    lut.bind(0);
  } else {
    lut.bind(1);
    tile_shader_->setInt("uSpriteLUT", 1);
  }

  // Bind tile VAO (ONCE for entire batch)
  glBindVertexArray(tile_vao_.get());
}

void SpriteBatch::endTileBatch() {
  if (!in_batch_ || mode_ != BatchMode::Tiles) {
    return;
  }

  in_batch_ = false;
  mode_ = BatchMode::Sprites;

  glBindVertexArray(0);
  glDisable(GL_BLEND);
}

} // namespace Rendering
} // namespace MapEditor
