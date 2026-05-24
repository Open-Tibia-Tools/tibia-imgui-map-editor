#include "Rendering/Passes/ShadeRenderer.hpp"
#include "Core/Config.h"

namespace MapEditor::Rendering {

void ShadeRenderer::render(SpriteBatch &batch, const ViewCamera &camera,
                           int viewport_width, int viewport_height,
                           const AtlasRegion &white_pixel, float alpha) {
  float const zoom = camera.getZoom();

  // Calculate world coordinates for full viewport shade
  // Screen (0,0) -> World
  float const shade_w = viewport_width / zoom;
  float const shade_h = viewport_height / zoom;
  float const shade_x = camera.getX() * Config::Rendering::TILE_SIZE - shade_w / 2.0f;
  float const shade_y = camera.getY() * Config::Rendering::TILE_SIZE - shade_h / 2.0f;

  // Draw semi-transparent black quad
  batch.draw(shade_x, shade_y, shade_w, shade_h, white_pixel, 0.0f, 0.0f, 0.0f,
             alpha);
}

} // namespace MapEditor::Rendering
