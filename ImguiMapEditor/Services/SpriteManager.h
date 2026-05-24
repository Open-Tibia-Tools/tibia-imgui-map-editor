#pragma once
#include "CreatureSpriteService.h"
#include "Domain/ItemType.h"
#include "IO/Readers/DatReaderBase.h"
#include "IO/SprReader.h"
#include "ItemCompositor.h"
// INTENTIONAL LAYER EXCEPTION: SpriteManager is the architectural bridge
// between async I/O and GPU resource management. It owns Rendering-layer
// types (AtlasManager, SpriteAtlasLUT, Texture) but does NOT issue GL calls
// itself — all GPU uploads are delegated to SpriteAsyncLoader::process()
// which runs on the main thread via the frame-tick contract.
#include "Rendering/Core/Texture.h"
#include "Rendering/Overlays/OverlaySpriteCache.h"
#include "Rendering/Resources/AtlasManager.h"
#include "Rendering/Resources/SpriteAtlasLUT.h"
#include "SpriteAsyncLoader.h"
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace MapEditor {
namespace Services {

/**
 * Manages sprite textures with async loading and atlas-based caching.
 *
 * ASYNC LOADING MODE (new performance optimization):
 * - Sprites are loaded on background threads
 * - Decoded data is uploaded via PBO (non-blocking GPU transfer)
 * - Call processAsyncLoads() each frame to complete pending uploads
 * - getSpriteRegion() returns nullptr for sprites still loading (use
 * placeholder)
 *
 * BATCHED RENDERING MODE:
 * - Sprites are packed into texture atlases (4096x4096)
 * - Use getSpriteRegion() + AtlasManager for batched rendering
 * - One draw call per atlas instead of per sprite
 *
 * LEGACY MODE (backwards compatible):
 * - getSprite() returns individual textures
 * - Use for single sprite rendering (e.g., UI previews)
 */
class SpriteManager {
public:
  /**
   * Create sprite manager with sprite reader
   * @param spr_reader Shared pointer to sprite reader
   */
  explicit SpriteManager(std::shared_ptr<IO::SprReader> spr_reader);
  ~SpriteManager();

  // Non-copyable
  SpriteManager(const SpriteManager &) = delete;
  SpriteManager &operator=(const SpriteManager &) = delete;

  // ========== ASYNC LOADING API (NEW) ==========

  /**
   * Initialize async loading subsystem.
   * Call once after construction.
   * @param worker_threads Number of background load threads (default: 4)
   * @return true if successful
   */
  bool initializeAsync(size_t worker_threads = 4);

  /**
   * Process completed async loads.
   * Call once per frame from main thread.
   * Uploads completed sprites to GPU via PBO.
   * @return Number of sprites uploaded this frame
   */
  size_t processAsyncLoads();

  /**
   * Synchronize the SpriteAtlasLUT with all sprites currently in the
   * AtlasManager. Call this after initializeAsync() to ensure pre-loaded
   * sprites are added to the LUT.
   */
  void syncLUTWithAtlas();

  /**
   * Request async load of multiple sprites.
   * Non-blocking - queues sprites for background loading.
   */
  void requestSpritesAsync(const std::vector<uint32_t> &sprite_ids);

  /**
   * Check if a sprite is currently being loaded.
   */
  bool isLoading(uint32_t sprite_id) const;

  /**
   * Get number of sprites currently queued for async load.
   */
  size_t getPendingLoadCount() const;

  /**
   * Set callback fired when sprites finish loading (for cache invalidation).
   */
  using SpritesLoadedCallback = std::function<void()>;
  void setSpritesLoadedCallback(SpritesLoadedCallback cb) {
    on_sprites_loaded_ = std::move(cb);
  }

  /**
   * Get the underlying SprReader (for creating local sprite caches).
   */
  std::shared_ptr<IO::SprReader> getSprReader() const { return spr_reader_; }

  // ========== BATCHED RENDERING API ==========

  /**
   * Get atlas region for a sprite (for batched rendering).
   * If sprite is not loaded:
   * - In async mode: queues load, returns nullptr (use placeholder)
   * - In sync mode: loads immediately (may stall)
   * @param sprite_id Sprite ID
   * @return Pointer to AtlasRegion, or nullptr if not yet loaded
   */
  const Rendering::AtlasRegion *getSpriteRegion(uint32_t sprite_id);

  /**
   * Preload a sprite to the atlas immediately.
   * Wraps internal loading logic to allow external services to force load a sprite.
   * Useful for optimization passes (e.g., ClientDataService pre-caching).
   * @param sprite_id Sprite ID
   * @return Pointer to AtlasRegion, or nullptr if loading failed
   */
  const Rendering::AtlasRegion *preloadSprite(uint32_t sprite_id);

  /**
   * Get the atlas manager for binding textures during batch rendering.
   */
  Rendering::AtlasManager &getAtlasManager() { return atlas_manager_; }
  const Rendering::AtlasManager &getAtlasManager() const {
    return atlas_manager_;
  }

  /**
   * Get the ItemCompositor for compositing multi-tile items.
   */
  ItemCompositor &getItemCompositor() { return *item_compositor_; }

  /**
   * Get the CreatureSpriteService for colorized outfits and creature
   * compositing.
   */
  CreatureSpriteService &getCreatureSpriteService() {
    return *creature_sprite_service_;
  }

  /**
   * Get the OverlaySpriteCache for ImGui overlay rendering (previews,
   * tooltips).
   */
  Rendering::OverlaySpriteCache &getOverlaySpriteCache() {
    return *overlay_sprite_cache_;
  }

  /**
   * Get the SpriteAtlasLUT for GPU-side ID→UV resolution.
   */
  Rendering::SpriteAtlasLUT &getSpriteLUT() { return sprite_lut_; }

  // ========== UTILITY API ==========

  /**
   * Get total sprites loaded into atlases
   */
  size_t getAtlasSpriteCount() const {
    return atlas_manager_.getTotalSpriteCount();
  }

  /**
   * Clear texture cache (frees GPU memory)
   */
  void clearCache();

  /**
   * Set secondary sprite reader provider for dual-client support.
   * Secondary sprites use IDs: original_id + SECONDARY_SPRITE_OFFSET (1
   * billion).
   * @param provider Callback that returns current secondary SprReader (may be
   * nullptr)
   */
  using SprReaderProvider = std::function<IO::SprReader *()>;
  void setSecondarySpriteReaderProvider(SprReaderProvider provider) {
    secondary_spr_reader_provider_ = std::move(provider);
  }

  /**
   * Check if secondary sprite reader is available.
   */
  bool hasSecondarySpriteReader() const {
    return secondary_spr_reader_provider_ &&
           secondary_spr_reader_provider_() != nullptr;
  }

  /**
   * Check if sprite exists
   */
  bool hasSprite(uint32_t sprite_id) const;

  /**
   * Get the AtlasRegion for the "invalid item" placeholder sprite.
   * This is a red 32x32 square used for items with no valid ItemType.
   * Renders inline with normal sprites for proper Z-order.
   * Created lazily on first access.
   */
  const Rendering::AtlasRegion *getInvalidItemPlaceholder();

private:
  const Rendering::AtlasRegion *loadSpriteToAtlas(uint32_t sprite_id);
  std::vector<uint8_t> loadSpriteData(uint32_t sprite_id);

  std::shared_ptr<IO::SprReader> spr_reader_;
  SprReaderProvider
      secondary_spr_reader_provider_; // Safe provider for dual-client support

  // Atlas manager for batched rendering
  Rendering::AtlasManager atlas_manager_;

  // Item compositor for multi-tile items
  std::unique_ptr<ItemCompositor> item_compositor_;

  // Creature sprite service for outfit coloring and creature compositing
  std::unique_ptr<CreatureSpriteService> creature_sprite_service_;

  // Overlay sprite cache for ImGui rendering (previews, tooltips)
  std::unique_ptr<Rendering::OverlaySpriteCache> overlay_sprite_cache_;

  // Async loading subsystem (delegated)
  std::unique_ptr<SpriteAsyncLoader> async_loader_;

  // GPU lookup table for ID→UV resolution in shader
  Rendering::SpriteAtlasLUT sprite_lut_;

  // Callback for cache invalidation when sprites load
  SpritesLoadedCallback on_sprites_loaded_;
};

} // namespace Services
} // namespace MapEditor
