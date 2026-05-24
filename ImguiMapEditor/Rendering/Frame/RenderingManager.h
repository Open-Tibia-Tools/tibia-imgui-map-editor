#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace MapEditor {
namespace Rendering {
class MapRenderer;
class RenderState;
} // namespace Rendering
namespace Services {
class ClientDataService;
class SpriteManager;
} // namespace Services
} // namespace MapEditor

namespace MapEditor::Rendering {

/**
 * Manages the lifecycle of MapRenderer.
 * Provides clean ownership semantics and integrates with sprite loading.
 */
class RenderingManager {
public:
  /**
   * Factory method to create a MapRenderer.
   * Ensures rendering components are created in the rendering layer.
   * @param client_data Service providing item type and sprite data
   * @param sprite_manager Manages sprite loading and caching
   * @return Unique pointer to initialized MapRenderer
   */
  static std::unique_ptr<MapRenderer>
  createRenderer(Services::ClientDataService *client_data,
                 Services::SpriteManager *sprite_manager);

  /**
   * Take ownership of a newly created renderer.
   * Optionally wire sprite-loaded callback to invalidate cache.
   */
  void setRenderer(std::unique_ptr<MapRenderer> renderer,
                   Services::SpriteManager *sprite_manager = nullptr);

  /**
   * Get non-owning pointer to renderer. May be null.
   */
  MapRenderer *getRenderer() const { return renderer_.get(); }

  /**
   * Check if renderer is available.
   */
  bool hasRenderer() const { return renderer_ != nullptr; }

  /**
   * Invalidate all cached chunks (e.g., on sprite reload).
   */
  void invalidateCache();

  /**
   * Release renderer and clear resources.
   * Called during version switch or shutdown.
   */
  void release();

  // === Session Render State Management ===
  using SessionID = uint64_t;

  RenderState *createRenderState(SessionID session_id,
                                 Services::ClientDataService *client_data);
  void destroyRenderState(SessionID session_id);
  RenderState *getRenderState(SessionID session_id) const;

private:
  std::unique_ptr<MapRenderer> renderer_;

  // Render states for all active sessions, keyed by SessionID
  std::unordered_map<SessionID, std::unique_ptr<RenderState>> session_states_;
};

} // namespace MapEditor::Rendering
