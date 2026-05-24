#pragma once
#include "../../Core/Config.h"
#include "Domain/Position.h"
#include "OverlaySpriteCache.h"
#include "Services/ClientDataService.h"
#include "Services/Preview/PreviewTypes.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <vector>

namespace MapEditor::Services {
class SpriteManager;
}

namespace MapEditor::Rendering {

/**
 * Unified preview renderer for all preview types.
 *
 * Renders preview tiles to ImDrawList with ghost effect.
 * Reuses sprite rendering logic from ItemPreviewRenderer.
 *
 * Supports:
 * - Multi-tile items (width/height > 1)
 * - Elevation stacking
 * - Pattern variations (pattern_x, pattern_y, pattern_z)
 * - Stackables (count → sprite index)
 * - Fluids (subtype → pattern)
 * - Configurable ghost color tint
 */
class PreviewOverlay {
public:
  PreviewOverlay() = default;

  /**
   * Render all preview tiles.
   * @param drawList ImGui draw list to render to
   * @param clientData For item type lookup
   * @param spriteManager For sprite textures
   * @param tiles Preview tiles to render (relative positions)
   * @param anchorWorldPos World position of anchor (cursor tile)
   * @param cameraPos Camera center position
   * @param viewportPos Screen position of viewport
   * @param viewportSize Size of viewport in pixels
   * @param zoom Current zoom level
   * @param style Optional rendering style
   */
  void render(ImDrawList *drawList, Services::ClientDataService *clientData,
              Services::SpriteManager *spriteManager,
              OverlaySpriteCache *spriteCache,
              const std::vector<Services::Preview::PreviewTileData> &tiles,
              const Domain::Position &anchorWorldPos,
              const glm::vec2 &cameraPos, const glm::vec2 &viewportPos,
              const glm::vec2 &viewportSize, float zoom,
              Services::Preview::PreviewStyle style =
                  Services::Preview::PreviewStyle::Ghost);

  /**
   * Render tiles within specified bounds only (culling).
   */
  void
  renderCulled(ImDrawList *drawList, Services::ClientDataService *clientData,
               Services::SpriteManager *spriteManager,
               OverlaySpriteCache *spriteCache,
               const std::vector<Services::Preview::PreviewTileData> &tiles,
               const Domain::Position &anchorWorldPos,
               const Services::Preview::PreviewBounds &bounds,
               const glm::vec2 &cameraPos, const glm::vec2 &viewportPos,
               const glm::vec2 &viewportSize, float zoom,
               Services::Preview::PreviewStyle style =
                   Services::Preview::PreviewStyle::Ghost);

private:
  /**
   * Render a single preview tile.
   */
  void renderTile(ImDrawList *drawList, Services::ClientDataService *clientData,
                  Services::SpriteManager *spriteManager,
                  OverlaySpriteCache *spriteCache,
                  const Services::Preview::PreviewTileData &tile,
                  const Domain::Position &worldPos, const glm::vec2 &cameraPos,
                  const glm::vec2 &viewportPos, const glm::vec2 &viewportSize,
                  float zoom, ImU32 tintColor);

  /**
   * Render a single item sprite with ghost effect.
   * Handles multi-tile items, patterns, stackables, fluids.
   */
  void renderItem(ImDrawList *drawList, Services::ClientDataService *clientData,
                  OverlaySpriteCache *spriteCache,
                  const Services::Preview::PreviewItemData &item,
                  const Domain::Position &worldPos, float &accumulatedElevation,
                  const glm::vec2 &cameraPos, const glm::vec2 &viewportPos,
                  const glm::vec2 &viewportSize, float zoom, ImU32 tintColor);

  /**
   * Convert tile position to screen coordinates.
   */
  glm::vec2 tileToScreen(const Domain::Position &pos,
                         const glm::vec2 &cameraPos,
                         const glm::vec2 &viewportPos,
                         const glm::vec2 &viewportSize, float zoom);
  /**
   * Get tint color for preview style.
   */
  static ImU32 getStyleColor(Services::Preview::PreviewStyle style);

  /**
   * Check if tile is within viewport bounds.
   */
  bool isInViewport(const glm::vec2 &screenPos, const glm::vec2 &viewportPos,
                    const glm::vec2 &viewportSize, float tileSizePx);

};
} // namespace MapEditor::Rendering
