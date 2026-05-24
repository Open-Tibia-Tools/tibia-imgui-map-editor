#pragma once
#include "Core/Config.h"
#include "Domain/ChunkedMap.h"
#include "Rendering/Animation/AnimationTicks.h"
#include "Rendering/Backend/IRenderer.h"
#include "Rendering/Backend/SpriteBatch.h"
#include "Rendering/Camera/ViewCamera.h"
#include "Rendering/Core/IRenderPass.h"
#include "Rendering/Core/RenderPipeline.h"
#include "Rendering/Core/RenderTarget.h"
#include "Rendering/Core/Shader.h"
#include "Rendering/Frame/FrameDataCollector.h"
#include "Rendering/Frame/RenderState.h"
#include "Rendering/Map/TileRenderer.h"
#include "Rendering/Overlays/OverlayCollector.h"
#include "Rendering/Passes/GhostFloorRenderer.h"
#include "Rendering/Passes/IngamePreviewRenderer.h"
#include "Rendering/Passes/ShadeRenderer.hpp"
#include "Rendering/Passes/WallOutlineRenderer.h"
#include "Rendering/Tile/ChunkRenderingStrategy.h"
#include "Rendering/Visibility/ChunkVisibilityManager.h"
#include "Rendering/Visibility/FloorIterator.h"
#include "Services/ViewSettings.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace MapEditor {

namespace Domain {}

namespace Services {
class ClientDataService;
class SpriteManager;
class CreatureSimulator;
} // namespace Services

namespace Rendering {

class TerrainPass;
class ISelectionDataProvider;

/**
 * Renders a Tibia map with isometric-style tile display.
 *
 * Orchestrates rendering using extracted components:
 * - TileRenderer: Tile/item sprite queueing
 * - FloorIterator: Multi-floor iteration with parallax (RME-style)
 * - IngamePreviewRenderer: Preview window rendering
 * - ViewCamera: Handles camera state and view matrices
 */
class MapRenderer : public IRenderer {
public:
  MapRenderer(Services::ClientDataService *client_data,
              Services::SpriteManager *sprite_manager);
  ~MapRenderer() override;

  MapRenderer(const MapRenderer &) = delete;
  MapRenderer &operator=(const MapRenderer &) = delete;

  bool initialize() override;

  /**
   * Render with explicit per-session state and external timing.
   * This is the preferred method for multi-map support.
   * @param map Map to render
   * @param state Per-session render state (cache, lighting, overlays)
   * @param viewport_width Viewport width in pixels
   * @param viewport_height Viewport height in pixels
   * @param anim_ticks Animation timing state
   */
  void render(const Domain::ChunkedMap &map, RenderState &state,
              int viewport_width, int viewport_height,
              const AnimationTicks &anim_ticks) override;

  // Camera control delegates
  void setCameraPosition(float x, float y) override;
  glm::vec2 getCameraPosition() const { return camera_.getPosition(); }
  void setZoom(float zoom) override;
  float getZoom() const { return camera_.getZoom(); }
  void setFloor(int floor) override;
  int getFloor() const { return camera_.getFloor(); }

  // Display options
  void setGridVisible(bool visible) { show_grid_ = visible; }
  bool isGridVisible() const { return show_grid_; }
  void setViewSettings(Services::ViewSettings *settings) override;

  /**
   * Set LOD mode to enable/disable simplified rendering and optimizations.
   */
  void setLODMode(bool enabled);

  // Selection provider (interface for clean separation)
  void setSelectionProvider(const ISelectionDataProvider *provider);

  // Creature simulation for walk animation
  void setCreatureSimulator(Services::CreatureSimulator *simulator);

  // Output
  uint32_t getTextureId() const override;

  /**
   * @brief Gets a constant reference to the view camera.
   * @return A constant reference to the `ViewCamera` instance.
   */
  const ViewCamera &getCamera() const { return camera_; }

  // Stats
  int getLastDrawCallCount() const override { return last_draw_calls_; }
  int getLastSpriteCount() const override { return last_sprite_count_; }
  int getLastChunkCount() const { return last_chunk_count_; }

  // Services Access
  Services::ClientDataService *getClientData() const { return client_data_; }
  Services::SpriteManager *getSpriteManager() const { return sprite_manager_; }

  // Overlay sprite cache for ImGui overlay rendering (preview, tooltips)
  // Delegates to SpriteManager which owns the cache
  OverlaySpriteCache *getOverlaySpriteCache() {
    return sprite_manager_ ? &sprite_manager_->getOverlaySpriteCache()
                           : nullptr;
  }

  // TileRenderer access for secondary client wiring
  TileRenderer &getTileRenderer() { return *tile_renderer_; }

  // SpriteBatch access for secondary renderers
  SpriteBatch *getSpriteBatch() { return sprite_batch_.get(); }
  const SpriteBatch *getSpriteBatch() const { return sprite_batch_.get(); }

private:
  void syncViewSettings();

  // --- render() helper methods (extracted for readability) ---

  /** Setup frame: updates camera, syncs settings, prepares render target.
      Returns false if viewport is invalid and rendering should abort. */
  bool setupFrame(int viewport_width, int viewport_height);

  // Services (not owned)
  Services::ClientDataService *client_data_ = nullptr;
  Services::SpriteManager *sprite_manager_ = nullptr;
  Services::ViewSettings *view_settings_ = nullptr;

  // OpenGL resources
  RenderTarget render_target_; // RAII wrapper for Framebuffer + projection
  std::unique_ptr<SpriteBatch> sprite_batch_;

  // Shared Services / Components (Owned by MapRenderer, shared with Passes)
  std::unique_ptr<TileRenderer> tile_renderer_;

  // Rendering Pipeline
  RenderPipeline render_pipeline_;

  // Camera component (handles position, zoom, matrix)
  ViewCamera camera_;
  bool show_grid_ = true;

  // Stats
  int last_draw_calls_ = 0;
  int last_sprite_count_ = 0;
  int last_chunk_count_ = 0;

  // Reusable buffer for missing sprites (avoids per-frame allocation)
  // Frame data collection (manages missing sprites and overlay collection)
  FrameDataCollector frame_data_collector_;

  // Chunk visibility manager (handles culling and screen position calculation)
  // Shared with TerrainPass and GhostFloorPass
  ChunkVisibilityManager chunk_visibility_;

  static constexpr float TILE_SIZE = Config::Rendering::TILE_SIZE;
};

} // namespace Rendering
} // namespace MapEditor
