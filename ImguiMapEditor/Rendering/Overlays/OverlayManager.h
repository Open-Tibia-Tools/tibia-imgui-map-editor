#pragma once
#include <imgui.h>
#include <memory>
#include <vector>

// Forward declarations
namespace MapEditor {
namespace UI {
class MapViewCamera;
} // namespace UI
namespace AppLogic {
class EditorSession;
}
namespace Services::Selection {
class SelectionService;
}
namespace Rendering {
class GridOverlay;
class StatusOverlay;
class SelectionOverlay;
class OverlayRenderer;
class OverlaySpriteCache;
class OverlayCollector;
class PreviewOverlay;
} // namespace Rendering
namespace Domain {
class ChunkedMap;
}
} // namespace MapEditor

namespace MapEditor {
namespace Rendering {

class OverlayManager {
public:
  OverlayManager();
  ~OverlayManager();

  // Prevent copying
  OverlayManager(const OverlayManager &) = delete;
  OverlayManager &operator=(const OverlayManager &) = delete;

  /**
   * Render all active overlays.
   *
   * @param draw_list ImGui draw list to render to
   * @param camera The current map view camera
   * @param session The active editor session
   * @param is_hovered Whether the map window is hovered
   * @param framerate Current application framerate
   * @param input_state Helper struct or arguments for input-dependent overlays
   * (optional, for now passing args directly)
   */
  static void render(ImDrawList *draw_list, const UI::MapViewCamera &camera,
              const AppLogic::EditorSession *session, bool is_hovered,
              float framerate);

  // Accessors for specific overlays
  GridOverlay &getGridOverlay();
  StatusOverlay &getStatusOverlay();
  SelectionOverlay &getSelectionOverlay();
  PreviewOverlay &getPreviewOverlay();
  OverlayRenderer &getOverlayRenderer();

  /**
   * Set the Level of Detail (LOD) mode.
   * When enabled (low zoom), overlays may simplify or hide themselves.
   */
  void setLODMode(bool enabled);

  /**
   * Bind SelectionService for observer pattern.
   * Registers SelectionOverlay as an observer of selection changes.
   * Call with nullptr to unbind.
   */
  void bindSelectionService(Services::Selection::SelectionService *service);

private:
  std::unique_ptr<GridOverlay> grid_overlay_;
  std::unique_ptr<StatusOverlay> status_overlay_;
  std::unique_ptr<SelectionOverlay> selection_overlay_;
  std::unique_ptr<PreviewOverlay> preview_overlay_;
  std::unique_ptr<OverlayRenderer> overlay_renderer_;

  // Track bound selection service for cleanup
  Services::Selection::SelectionService *bound_selection_service_ = nullptr;
};

} // namespace Rendering
} // namespace MapEditor
