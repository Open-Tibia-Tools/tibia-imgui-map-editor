#pragma once
#include "Domain/Creature.h"
#include "IO/Readers/DatReaderBase.h"
// INTENTIONAL LAYER EXCEPTION: CreatureSpriteService produces
// Rendering::Texture and Rendering::AtlasRegion objects for creature/outfit
// preview (UI) and GPU batch rendering. It composes colorized sprite data
// but delegates all GL uploads to the Rendering layer via AtlasManager.
#include "Rendering/Core/Texture.h"
#include "Rendering/Resources/AtlasManager.h"
#include <list>
#include <memory>
#include <unordered_map>

namespace MapEditor {

namespace IO {
class SprReader;
}

namespace Services {

/**
 * Service for colorized outfit sprites and composited creature textures.
 *
 * Handles:
 * - Single-sprite outfit colorization (for UI previews)
 * - Atlas-based colorized outfit regions (for map rendering)
 * - Multi-tile creature compositing with colorization
 */
class CreatureSpriteService {
public:
  /**
   * Create CreatureSpriteService with required dependencies.
   * @param spr_reader SprReader for raw sprite data access
   * @param atlas_manager AtlasManager for adding colorized sprites to atlas
   */
  CreatureSpriteService(std::shared_ptr<IO::SprReader> spr_reader,
                        Rendering::AtlasManager &atlas_manager);
  ~CreatureSpriteService() = default;

  // Non-copyable
  CreatureSpriteService(const CreatureSpriteService &) = delete;
  CreatureSpriteService &operator=(const CreatureSpriteService &) = delete;

  /**
   * Get colorized outfit texture using template-based coloring.
   * Results are cached by (baseSpriteId, templateSpriteId, colorHash).
   *
   * @param base_sprite_id Base sprite ID (layer 0)
   * @param template_sprite_id Template mask sprite ID (layer 1)
   * @param head Head color index (0-132)
   * @param body Body color index (0-132)
   * @param legs Legs color index (0-132)
   * @param feet Feet color index (0-132)
   * @return Texture pointer, or nullptr if failed
   */
  Rendering::Texture *getColorizedOutfitSprite(uint32_t base_sprite_id,
                                               uint32_t template_sprite_id,
                                               uint8_t head, uint8_t body,
                                               uint8_t legs, uint8_t feet);

  /**
   * Get atlas region for a colorized outfit sprite (for GPU batch rendering).
   * Results are cached by (baseSpriteId, templateSpriteId, colorHash).
   *
   * @param base_sprite_id Base sprite ID (layer 0)
   * @param template_sprite_id Template mask sprite ID (layer 1, 0 if none)
   * @param head Head color index (0-132)
   * @param body Body color index (0-132)
   * @param legs Legs color index (0-132)
   * @param feet Feet color index (0-132)
   * @return AtlasRegion pointer for batched rendering, or nullptr
   */
  const Rendering::AtlasRegion *
  getColorizedOutfitRegion(uint32_t base_sprite_id, uint32_t template_sprite_id,
                           uint8_t head, uint8_t body, uint8_t legs,
                           uint8_t feet);

  /**
   * Get composited, colorized creature texture for multi-tile outfits.
   * Handles creatures that span multiple tiles (1x1 to 4x4).
   * Results are cached by creature name hash + outfit colors.
   *
   * @param outfit_data ClientItem data for the outfit type
   * @param head Head color index
   * @param body Body color index
   * @param legs Legs color index
   * @param feet Feet color index
   * @return Composited texture, or nullptr if failed
   */
  Rendering::Texture *
  getCompositedCreatureTexture(const IO::ClientItem *outfit_data, uint8_t head,
                               uint8_t body, uint8_t legs, uint8_t feet);

  /**
   * Clear all cached textures.
   */
  void clearCache();

  /**
   * Get total number of cached entries.
   */
  size_t getCacheSize() const;

private:
  /**
   * Generate cache key for outfit coloring.
   */
  static uint64_t makeOutfitCacheKey(uint32_t base_id, uint32_t template_id,
                                     uint8_t head, uint8_t body, uint8_t legs,
                                     uint8_t feet);

  /**
   * Load and colorize a sprite.
   * @return Colorized RGBA data, or empty vector on failure
   */
  std::vector<uint8_t> colorizeSprite(uint32_t base_sprite_id,
                                      uint32_t template_sprite_id, uint8_t head,
                                      uint8_t body, uint8_t legs, uint8_t feet);

  std::shared_ptr<IO::SprReader> spr_reader_;
  Rendering::AtlasManager &atlas_manager_;

  // Colorized outfit texture cache
  std::unordered_map<uint64_t, std::unique_ptr<Rendering::Texture>>
      colorized_outfit_cache_;

  // Colorized outfit region cache (for GPU batch rendering)
  std::unordered_map<uint64_t, const Rendering::AtlasRegion *>
      colorized_outfit_region_cache_;

  // Composited creature texture cache with LRU eviction
  static constexpr size_t MAX_COMPOSITED_CACHE_SIZE = 1024;
  std::unordered_map<uint64_t, std::unique_ptr<Rendering::Texture>>
      composited_creature_cache_;
  // LRU tracking: front = most recently used, back = least recently used
  std::list<uint64_t> composited_lru_order_;
  std::unordered_map<uint64_t, std::list<uint64_t>::iterator>
      composited_lru_map_;

  // Counter for unique atlas sprite IDs
  uint32_t next_colorized_id_ = 0;
};

} // namespace Services
} // namespace MapEditor
