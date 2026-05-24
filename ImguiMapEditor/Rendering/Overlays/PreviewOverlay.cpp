#include "PreviewOverlay.h"
#include "OutfitOverlay.h"
#include "OverlaySpriteCache.h"
#include "Services/SpriteManager.h"
#include <algorithm>
#include <cmath>

namespace MapEditor::Rendering {

void PreviewOverlay::render(
    ImDrawList *drawList, Services::ClientDataService *clientData,
    Services::SpriteManager *spriteManager, OverlaySpriteCache *spriteCache,
    const std::vector<Services::Preview::PreviewTileData> &tiles,
    const Domain::Position &anchorWorldPos, const glm::vec2 &cameraPos,
    const glm::vec2 &viewportPos, const glm::vec2 &viewportSize, float zoom,
    Services::Preview::PreviewStyle style) {

  if (!drawList || !clientData || !spriteCache || tiles.empty()) {
    return;
  }

  ImU32 const tintColor = getStyleColor(style);
  float const tileSizePx = Config::Rendering::TILE_SIZE * zoom;

  for (const auto &tile : tiles) {
    // Calculate world position from anchor + relative
    Domain::Position worldPos;
    worldPos.x = anchorWorldPos.x + tile.relativePosition.x;
    worldPos.y = anchorWorldPos.y + tile.relativePosition.y;
    worldPos.z =
        static_cast<int16_t>(anchorWorldPos.z + tile.relativePosition.z);

    // Bounds check for Z
    if (worldPos.z < 0 || worldPos.z > 15)
      continue;

    // Calculate screen position for culling
    glm::vec2 const screenPos =
        tileToScreen(worldPos, cameraPos, viewportPos, viewportSize, zoom);

    // Viewport culling (with margin for large items)
    if (!isInViewport(screenPos, viewportPos, viewportSize, tileSizePx * 3)) {
      continue;
    }

    renderTile(drawList, clientData, spriteManager, spriteCache, tile, worldPos,
               cameraPos, viewportPos, viewportSize, zoom, tintColor);
  }
}

void PreviewOverlay::renderCulled(
    ImDrawList *drawList, Services::ClientDataService *clientData,
    Services::SpriteManager *spriteManager, OverlaySpriteCache *spriteCache,
    const std::vector<Services::Preview::PreviewTileData> &tiles,
    const Domain::Position &anchorWorldPos,
    const Services::Preview::PreviewBounds &bounds, const glm::vec2 &cameraPos,
    const glm::vec2 &viewportPos, const glm::vec2 &viewportSize, float zoom,
    Services::Preview::PreviewStyle style) {

  // For now, just use regular render with viewport culling
  // Bounds parameter can be used for additional optimization later
  render(drawList, clientData, spriteManager, spriteCache, tiles,
         anchorWorldPos, cameraPos, viewportPos, viewportSize, zoom, style);
}

void PreviewOverlay::renderTile(
    ImDrawList *drawList, Services::ClientDataService *clientData,
    Services::SpriteManager *spriteManager, OverlaySpriteCache *spriteCache,
    const Services::Preview::PreviewTileData &tile,
    const Domain::Position &worldPos, const glm::vec2 &cameraPos,
    const glm::vec2 &viewportPos, const glm::vec2 &viewportSize, float zoom,
    ImU32 tintColor) {

  float accumulatedElevation = 0.0f;
  float const tileSizePx = Config::Rendering::TILE_SIZE * zoom;

  // Render items first
  for (const auto &item : tile.items) {
    renderItem(drawList, clientData, spriteCache, item, worldPos,
               accumulatedElevation, cameraPos, viewportPos, viewportSize, zoom,
               tintColor);
  }

  // Render creature if present - lookup outfit from CreatureType by name
  if (tile.creature_name.has_value() && clientData) {
    const std::string &creatureName = tile.creature_name.value();
    const Domain::CreatureType *creatureType =
        clientData->getCreatureType(creatureName);

    if (creatureType && creatureType->outfit.lookType > 0) {
      glm::vec2 const screenPos =
          tileToScreen(worldPos, cameraPos, viewportPos, viewportSize, zoom);

      // Use static OutfitOverlay instance (DRY - same as SpawnLabelOverlay.cpp)
      static OutfitOverlay s_outfit_renderer;
      s_outfit_renderer.render(drawList, creatureType->outfit, clientData,
                               spriteManager, spriteCache, screenPos, zoom, 2,
                               0, tintColor); // direction 2 = South, frame 0
    }
  }

  // Render spawn indicator if present - MAGENTA border like RME
  if (tile.has_spawn) {
    glm::vec2 const screenPos =
        tileToScreen(worldPos, cameraPos, viewportPos, viewportSize, zoom);

    // Magenta/Pink border color (matching RME spawn preview)
    ImU32 const magentaBorder = IM_COL32(255, 0, 255, 200);

    // Get spawn radius from tile data (0 means just this tile)
    int const radius = tile.spawn_radius;

    // Calculate full spawn area rectangle
    // Center tile is at screenPos, spawn extends radius tiles in all directions
    float const spawnWidth = (radius * 2 + 1) * tileSizePx;
    float const topLeftX = screenPos.x - (radius * tileSizePx);
    float const topLeftY = screenPos.y - (radius * tileSizePx);

    // Draw ONE continuous magenta border around entire spawn area
    drawList->AddRect(ImVec2(topLeftX, topLeftY),
                      ImVec2(topLeftX + spawnWidth, topLeftY + spawnWidth),
                      magentaBorder, 0.0f, 0, 2.0f);

    // Draw "SPAWN" text at center tile
    const char *text = "SPAWN";
    ImVec2 const textSize = ImGui::CalcTextSize(text);
    float const textX = screenPos.x + (tileSizePx - textSize.x) / 2;
    float const textY = screenPos.y + (tileSizePx - textSize.y) / 2;
    drawList->AddText(ImVec2(textX, textY), magentaBorder, text);
  }

  // Render zone color overlay if present (for Flag, Eraser, House, Waypoint
  // brushes)
  if (tile.zone_color != 0) {
    glm::vec2 const screenPos =
        tileToScreen(worldPos, cameraPos, viewportPos, viewportSize, zoom);

    // Extract ARGB components and convert to ImU32 (ABGR)
    ImU32 const fillColor = IM_COL32((tile.zone_color >> 16) & 0xFF,  // R
                               (tile.zone_color >> 8) & 0xFF,   // G
                               tile.zone_color & 0xFF,          // B
                               (tile.zone_color >> 24) & 0xFF); // A

    // Draw semi-transparent filled rectangle for the tile
    drawList->AddRectFilled(
        ImVec2(screenPos.x, screenPos.y),
        ImVec2(screenPos.x + tileSizePx, screenPos.y + tileSizePx), fillColor);

    // Draw a slightly brighter border
    ImU32 const borderColor =
        IM_COL32((tile.zone_color >> 16) & 0xFF, (tile.zone_color >> 8) & 0xFF,
                 tile.zone_color & 0xFF,
                 200); // More opaque border
    drawList->AddRect(
        ImVec2(screenPos.x, screenPos.y),
        ImVec2(screenPos.x + tileSizePx, screenPos.y + tileSizePx), borderColor,
        0.0f, 0, 1.0f);
  }
}

void PreviewOverlay::renderItem(
    ImDrawList *drawList, Services::ClientDataService *clientData,
    OverlaySpriteCache *spriteCache,
    const Services::Preview::PreviewItemData &item,
    const Domain::Position &worldPos, float &accumulatedElevation,
    const glm::vec2 &cameraPos, const glm::vec2 &viewportPos,
    const glm::vec2 &viewportSize, float zoom, ImU32 tintColor) {

  if (item.itemId == 0)
    return;

  auto *itemType = clientData->getItemTypeByServerId(item.itemId);
  if (!itemType || itemType->sprite_ids.empty())
    return;

  float const tileSizePx = Config::Rendering::TILE_SIZE * zoom;

  int const width = std::max(1, static_cast<int>(itemType->width));
  int const height = std::max(1, static_cast<int>(itemType->height));
  int const layers = std::max(1, static_cast<int>(itemType->layers));
  int const pat_x = std::max(1, static_cast<int>(itemType->pattern_x));
  int const pat_y = std::max(1, static_cast<int>(itemType->pattern_y));
  int const pat_z = std::max(1, static_cast<int>(itemType->pattern_z));

  // Determine sprite index based on item properties
  int subtypeIndex = -1;
  int fluidSubtype = -1;
  bool const isFluid = itemType->isFluidContainer() || itemType->isSplash();

  if (itemType->is_stackable) {
    int const count = item.subtype;
    if (count <= 1)
      subtypeIndex = 0;
    else if (count <= 2)
      subtypeIndex = 1;
    else if (count <= 3)
      subtypeIndex = 2;
    else if (count <= 4)
      subtypeIndex = 3;
    else if (count < 10)
      subtypeIndex = 4;
    else if (count < 25)
      subtypeIndex = 5;
    else if (count < 50)
      subtypeIndex = 6;
    else
      subtypeIndex = 7;
  } else if (isFluid) {
    fluidSubtype = item.subtype;
  }

  // Fast path for simple stackables
  bool const canUseFastPath = (subtypeIndex >= 0 && width == 1 && height == 1);
  if (canUseFastPath &&
      subtypeIndex < static_cast<int>(itemType->sprite_ids.size())) {
    uint32_t const spriteId = itemType->sprite_ids[subtypeIndex];
    if (spriteId > 0) {
      auto &texture = spriteCache->getTextureOrPlaceholder(spriteId);
      glm::vec2 const screenPos =
          tileToScreen(worldPos, cameraPos, viewportPos, viewportSize, zoom);

      float const elevOffset = accumulatedElevation + item.elevationOffset;

      float const w = texture.getWidth() * zoom;
      float const h = texture.getHeight() * zoom;
      float const offsetX = static_cast<float>(itemType->draw_offset_x) * zoom;
      float const offsetY = static_cast<float>(itemType->draw_offset_y) * zoom;

      float const drawX = screenPos.x + (tileSizePx - w) - offsetX - elevOffset;
      float const drawY = screenPos.y + (tileSizePx - h) - offsetY - elevOffset;

      drawList->AddImage((ImTextureID)(intptr_t)texture.id(),
                         ImVec2(drawX, drawY), ImVec2(drawX + w, drawY + h),
                         ImVec2(0, 0), ImVec2(1, 1), tintColor);
    }

    if (itemType->hasElevation()) {
      accumulatedElevation += static_cast<float>(itemType->elevation) * zoom;
    }
    return;
  }

  // Slow path for complex items (multi-tile, patterns, fluids)
  int patternX = 0;
  int patternY = 0;
  int patternZ = 0;

  if (isFluid && fluidSubtype >= 0) {
    patternX = (fluidSubtype % 4) % pat_x;
    patternY = (fluidSubtype / 4) % pat_y;
    patternZ = 0;
  } else {
    patternX = worldPos.x % pat_x;
    patternY = worldPos.y % pat_y;
    patternZ = worldPos.z % pat_z;
  }

  float const elevOffset = accumulatedElevation + item.elevationOffset;

  for (int cy = 0; cy < height; cy++) {
    for (int cx = 0; cx < width; cx++) {
      for (int layer = 0; layer < layers; layer++) {
        size_t spriteIndex = static_cast<size_t>(
            (((0 * pat_z + patternZ) * pat_y + patternY) * pat_x + patternX) *
                layers +
            layer);
        spriteIndex = (spriteIndex * height + cy) * width + cx;

        if (spriteIndex >= itemType->sprite_ids.size())
          continue;

        uint32_t const spriteId = itemType->sprite_ids[spriteIndex];
        if (spriteId == 0)
          continue;

        auto &texture = spriteCache->getTextureOrPlaceholder(spriteId);
        glm::vec2 const screenPos =
            tileToScreen(worldPos, cameraPos, viewportPos, viewportSize, zoom);

        float const w = texture.getWidth() * zoom;
        float const h = texture.getHeight() * zoom;
        float const offsetX = static_cast<float>(itemType->draw_offset_x) * zoom;
        float const offsetY = static_cast<float>(itemType->draw_offset_y) * zoom;

        float const drawX = screenPos.x + (tileSizePx - w) - offsetX -
                      cx * tileSizePx - elevOffset;
        float const drawY = screenPos.y + (tileSizePx - h) - offsetY -
                      cy * tileSizePx - elevOffset;

        drawList->AddImage((ImTextureID)(intptr_t)texture.id(),
                           ImVec2(drawX, drawY), ImVec2(drawX + w, drawY + h),
                           ImVec2(0, 0), ImVec2(1, 1), tintColor);
      }
    }
  }

  if (itemType->hasElevation()) {
    accumulatedElevation += static_cast<float>(itemType->elevation) * zoom;
  }
}

glm::vec2 PreviewOverlay::tileToScreen(const Domain::Position &pos,
                                       const glm::vec2 &cameraPos,
                                       const glm::vec2 &viewportPos,
                                       const glm::vec2 &viewportSize,
                                       float zoom) {

  float floorOffset = 0.0f;
  if (pos.z <= Config::Map::GROUND_LAYER) {
    floorOffset = static_cast<float>(Config::Map::GROUND_LAYER - pos.z) *
                  Config::Rendering::TILE_SIZE * zoom;
  }

  glm::vec2 localOffset(static_cast<float>(pos.x) - cameraPos.x,
                        static_cast<float>(pos.y) - cameraPos.y);
  localOffset *= Config::Rendering::TILE_SIZE * zoom;

  localOffset.x -= floorOffset;
  localOffset.y -= floorOffset;

  return viewportPos + viewportSize * 0.5f + localOffset;
}

ImU32 PreviewOverlay::getStyleColor(Services::Preview::PreviewStyle style) {
  switch (style) {
  case Services::Preview::PreviewStyle::Ghost:
    // Blue-tinted ghost color (same as original)
    return IM_COL32(180, 200, 255, 153); // ~60% alpha

  case Services::Preview::PreviewStyle::Outline:
    return IM_COL32(255, 255, 0, 200); // Yellow outline

  case Services::Preview::PreviewStyle::Tinted:
    return IM_COL32(160, 255, 160, 180); // Green tint

  default:
    return IM_COL32(180, 200, 255, 153);
  }
}

bool PreviewOverlay::isInViewport(const glm::vec2 &screenPos,
                                  const glm::vec2 &viewportPos,
                                  const glm::vec2 &viewportSize,
                                  float margin) {

  return screenPos.x >= viewportPos.x - margin &&
         screenPos.x <= viewportPos.x + viewportSize.x + margin &&
         screenPos.y >= viewportPos.y - margin &&
         screenPos.y <= viewportPos.y + viewportSize.y + margin;
}

} // namespace MapEditor::Rendering
