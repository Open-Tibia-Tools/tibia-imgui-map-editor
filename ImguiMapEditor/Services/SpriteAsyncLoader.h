#pragma once

#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

// INTENTIONAL LAYER EXCEPTION: SpriteAsyncLoader is the designated boundary
// between CPU sprite data and GPU uploads. It owns Rendering::PixelBufferObject
// (the PBO fence-sync boundary) and receives AtlasManager/SpriteAtlasLUT
// references to drive glTexSubImage* uploads. All GL calls happen inside
// process() on the main thread via the frame-tick contract.
#include "Rendering/Core/PixelBufferObject.h"
#include "Rendering/Resources/AtlasManager.h"
#include "Rendering/Resources/SpriteAtlasLUT.h"
#include "SpriteLoadQueue.h"

namespace MapEditor {
namespace Services {

/**
 * Handles the async loading pipeline for sprites.
 * Orchestrates the flow of data from background threads (SpriteLoadQueue)
 * to GPU staging (PixelBufferObject) and finally to the Texture Atlas.
 *
 * Extracted from SpriteManager to separate pipeline orchestration from
 * high-level sprite management.
 */
class SpriteAsyncLoader {
public:
  using DataLoader = std::function<std::vector<uint8_t>(uint32_t)>;

  SpriteAsyncLoader() = default;
  ~SpriteAsyncLoader();

  // Non-copyable
  SpriteAsyncLoader(const SpriteAsyncLoader &) = delete;
  SpriteAsyncLoader &operator=(const SpriteAsyncLoader &) = delete;

  /**
   * Initialize the loader components (Queue, PBO).
   * @param worker_threads Number of background threads
   * @param loader Callback to load raw sprite data
   * @return true if successful
   */
  bool initialize(size_t worker_threads, DataLoader loader);

  /**
   * Process completed loads from the queue and upload to atlas.
   * Call once per frame.
   * @param atlas_manager Atlas to upload to
   * @param sprite_lut Optional LUT to update with new regions
   * @return Number of sprites uploaded this frame
   */
  size_t process(Rendering::AtlasManager &atlas_manager,
                 Rendering::SpriteAtlasLUT *sprite_lut);

  /**
   * Request async load of multiple sprites.
   * Filters out already pending requests.
   * @param sprite_ids List of IDs to load
   */
  void request(const std::vector<uint32_t> &sprite_ids);

  /**
   * Request async load of a single sprite.
   * @param sprite_id ID to load
   */
  void request(uint32_t sprite_id);

  /**
   * Check if a sprite is currently pending (queued or loading).
   */
  bool isPending(uint32_t sprite_id) const;

  /**
   * Get number of currently pending loads.
   */
  size_t getPendingCount() const;

  /**
   * Clear all pending state.
   */
  void clear();

  /**
   * Check if async loading is initialized.
   */
  bool isInitialized() const { return initialized_; }

private:
  std::unique_ptr<SpriteLoadQueue> load_queue_;
  std::unique_ptr<Rendering::PixelBufferObject> pbo_;
  std::unordered_set<uint32_t> pending_loads_;
  bool initialized_ = false;
};

} // namespace Services
} // namespace MapEditor
