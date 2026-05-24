#pragma once
#include "Domain/ChunkedMap.h"
#include "Domain/Position.h"
#include "Domain/SelectionSettings.h"
#include "MapPanelInput.h"
#include "MapViewCamera.h"
#include "Presentation/IUIComponent.h"
#include "Rendering/Overlays/OverlayManager.h"
#include "Rendering/Overlays/OverlayRenderer.h"
#include "Rendering/Overlays/SelectionOverlay.h"
#include "Services/CreatureSimulator.h"
#include "Services/ViewSettings.h"
#include <glm/glm.hpp>
#include <memory>

namespace MapEditor {
namespace Rendering {
class MapRenderer;
class RenderState;     // Forward declaration
struct AnimationTicks; // Forward declaration for render overload
} // namespace Rendering
namespace AppLogic {
class EditorSession;
class MapInputController;
} // namespace AppLogic
namespace Brushes {
class BrushController;
}
namespace Services {
class ClientDataService;
}

namespace UI {

/**
 * Map canvas panel for displaying and interacting with the map.
 * Coordinates camera, input, and overlay rendering through extracted
 * sub-components.
 *
 * Implements IUIComponent for basic visibility control.
 */
class MapPanel : public Presentation::IUIComponent {
public:
  MapPanel();
  ~MapPanel() override = default;

  void render(Domain::ChunkedMap *map, Rendering::RenderState &state,
              Rendering::MapRenderer *renderer);

  /**
   * Render overload for ChunkedMap with explicit animation timing.
   * @param map Map to render
   * @param state Render state (cache, lighting)
   * @param renderer Renderer instance
   * @param anim_ticks Optional external animation ticks (removes timing logic
   * from renderer)
   */
  void render(Domain::ChunkedMap *map, Rendering::RenderState &state,
              Rendering::MapRenderer *renderer,
              const Rendering::AnimationTicks *anim_ticks);

  // Camera access (delegates to MapViewCamera)
  glm::vec2 getCameraPosition() const { return camera_.getCameraPosition(); }
  int16_t getCurrentFloor() const {
    return static_cast<int16_t>(camera_.getCurrentFloor());
  }
  void setCameraPosition(float x, float y) { camera_.setCameraPosition(x, y); }
  void setCameraCenter(const Domain::Position &pos) {
    camera_.setCameraCenter(pos);
  }
  void setCameraCenter(int32_t x, int32_t y, int16_t z) {
    camera_.setCameraCenter(x, y, z);
  }
  float getZoom() const { return camera_.getZoom(); }
  void setZoom(float zoom) { camera_.setZoom(zoom); }
  void setCurrentFloor(int16_t floor) { camera_.setCurrentFloor(floor); }
  void floorUp() { camera_.floorUp(); }
  void floorDown() { camera_.floorDown(); }

  Domain::Position getCameraCenter() const { return camera_.getCameraCenter(); }

  // Viewport info
  glm::vec2 getViewportSize() const { return camera_.getViewportSize(); }
  glm::vec2 getViewportPos() const { return camera_.getViewportPos(); }

  // Coordinate transforms (delegates to MapViewCamera)
  Domain::Position screenToTile(const glm::vec2 &screen_pos) const {
    return camera_.screenToTile(screen_pos);
  }
  glm::vec2 tileToScreen(const Domain::Position &tile_pos) const {
    return camera_.tileToScreen(tile_pos);
  }

  // Grid visibility
  bool isGridVisible() const { return show_grid_; }
  void setGridVisible(bool visible) { show_grid_ = visible; }

  // Dependency injection
  void setViewSettings(Services::ViewSettings &settings) {
    view_settings_ = &settings;
  }
  void setEditorSession(AppLogic::EditorSession *session);
  void setInputController(AppLogic::MapInputController *controller) {
    input_controller_ = controller;
  }
  void setClientDataService(Services::ClientDataService *client_data) {
    client_data_ = client_data;
  }
  void setSelectionSettings(Domain::SelectionSettings *settings) {
    selection_settings_ = settings;
  }
  void setBrushController(Brushes::BrushController *brush_controller) {
    brush_controller_ = brush_controller;
  }

  // Context menu state
  bool shouldShowContextMenu() const { return input_.shouldShowContextMenu(); }
  void clearContextMenuFlag() { input_.clearContextMenuFlag(); }
  Domain::Position getContextMenuPosition() const {
    return input_.getContextMenuPosition();
  }

  // IUIComponent interface
  void render() override { /* No-op: use render(map, renderer) instead */ }
  void setVisible(bool visible) override { is_visible_ = visible; }
  bool isVisible() const override { return is_visible_; }

private:
  template <typename MapType>
  void renderInternal(MapType *map, Rendering::RenderState &state,
                      Rendering::MapRenderer *renderer,
                      const Rendering::AnimationTicks *anim_ticks = nullptr);

  void renderBackground();
  void renderGrid();
  void renderOverlay();

  // Sub-components (composition)
  MapViewCamera camera_;
  MapPanelInput input_;

  // Overlay Manager (handles all overlays: selection, grid, etc.)
  std::unique_ptr<Rendering::OverlayManager> overlay_manager_;

  // Display options
  bool show_grid_ = true;

  // Dependencies (not owned)
  Services::ViewSettings *view_settings_ = nullptr;
  AppLogic::EditorSession *session_ = nullptr;
  AppLogic::MapInputController *input_controller_ = nullptr;
  Services::ClientDataService *client_data_ = nullptr;
  Domain::SelectionSettings *selection_settings_ = nullptr;
  Brushes::BrushController *brush_controller_ = nullptr;

  // State
  bool is_hovered_ = false;
  bool is_focused_ = false;
  bool is_visible_ = true;
  bool drag_preview_active_ = false; // Track if drag preview is active
};

} // namespace UI
} // namespace MapEditor
