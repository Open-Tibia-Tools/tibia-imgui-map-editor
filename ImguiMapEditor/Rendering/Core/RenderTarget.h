#pragma once
#include "Framebuffer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MapEditor {
namespace Rendering {

/**
 * Manages render-to-texture output with automatic viewport handling.
 * Encapsulates Framebuffer + projection matrix + viewport state.
 *
 * Provides a clean RAII wrapper that combines:
 * - Framebuffer management (resize, bind/unbind)
 * - Orthographic projection matrix calculation
 * - Viewport dimensions tracking
 */
class RenderTarget {
public:
  RenderTarget() = default;
  ~RenderTarget() = default;

  // Non-copyable, movable
  RenderTarget(const RenderTarget &) = delete;
  RenderTarget &operator=(const RenderTarget &) = delete;
  RenderTarget(RenderTarget &&) noexcept = default;
  RenderTarget &operator=(RenderTarget &&) noexcept = default;

  /**
   * Resize the render target. Creates/recreates framebuffer if needed.
   * Also updates the projection matrix.
   * @return true if successful
   */
  bool resize(int width, int height);

  /**
   * Bind this render target for rendering.
   * Also sets the OpenGL viewport to match dimensions.
   */
  void bind();

  /**
   * Unbind (return to default framebuffer).
   */
  void unbind();

  /**
   * Begin a render pass: bind, clear viewport with given color.
   * Disables scissor test and clears color buffer.
   */
  void beginPass(const glm::vec4 &clear_color);

  /**
   * Enable alpha blending (SRC_ALPHA, ONE_MINUS_SRC_ALPHA).
   */
  static void enableBlending();

  /**
   * Disable blending.
   */
  static void disableBlending();

  // Getters (all const-correct)
  GLuint getTextureId() const;
  int getWidth() const { return width_; }
  int getHeight() const { return height_; }
  bool isValid() const;

  /**
   * Get the orthographic projection matrix for this render target.
   * Origin at top-left, Y increases downward.
   */
  const glm::mat4 &getProjection() const { return projection_; }

  /**
   * Get the view matrix (identity for 2D rendering).
   */
  const glm::mat4 &getView() const { return view_; }

private:
  Framebuffer framebuffer_;
  glm::mat4 projection_{1.0f};
  glm::mat4 view_{1.0f};
  int width_ = 0;
  int height_ = 0;
};

} // namespace Rendering
} // namespace MapEditor
