#pragma once
#include "Core/Config.h"
#include "Rendering/Backend/SpriteBatch.h"
#include "Rendering/Camera/ViewCamera.h"
#include "Rendering/Resources/TextureAtlas.h"


namespace MapEditor::Rendering {

/**
 * @brief Renders a semi-transparent shade overlay across the entire viewport.
 *
 * This is used to darken floors that are not the currently active floor,
 * improving visual separation.
 */
class ShadeRenderer {
public:
  /**
   * @brief Draws a full-screen quad with a specified alpha.
   * @param batch The sprite batch to draw with.
   * @param camera The view camera, used for position and zoom.
   * @param viewport_width Viewport width in pixels (passed from render layer).
   * @param viewport_height Viewport height in pixels (passed from render
   * layer).
   * @param white_pixel A 1x1 white texture region used to draw the colored
   * quad.
   * @param alpha The transparency of the shade overlay (0.0 to 1.0).
   */
  static void render(SpriteBatch &batch, const ViewCamera &camera, int viewport_width,
              int viewport_height, const AtlasRegion &white_pixel,
              float alpha = Config::Rendering::DEFAULT_SHADE_ALPHA) ;
};

} // namespace MapEditor::Rendering
