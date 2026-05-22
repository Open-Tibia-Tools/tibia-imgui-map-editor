#pragma once
#include "Domain/Position.h"
#include "MapViewCamera.h"
#include <glm/glm.hpp>
#include <vector>

struct ImGuiIO; // Forward declaration

namespace MapEditor {
namespace AppLogic {
class EditorSession;
class MapInputController;
} // namespace AppLogic
namespace Brushes {
class BrushController;
}
namespace Domain {
struct SelectionSettings;
}

namespace UI {

// Lasso selection mode state machine
enum class LassoMode {
  Inactive, // No lasso active
  Drawing,  // Click-to-add-vertex mode
  Dragging  // Freeform curve drawing
};

/**
 * Handles all user input for the map panel.
 * Delegates actions to EditorSession and MapInputController.
 *
 * Single responsibility: input processing and delegation.
 */
class MapPanelInput {
public:
  MapPanelInput() = default;

  /**
   * Process all input for current frame.
   * @return true if input was handled and should block further processing
   */
  bool handleInput(MapViewCamera &camera, AppLogic::EditorSession *session,
                   AppLogic::MapInputController *input_controller,
                   const Domain::SelectionSettings *selection_settings,
                   bool is_hovered, bool is_focused);

  // Drag selection state accessors
  bool isDragSelecting() const { return is_drag_selecting_; }
  bool isBoxSelection() const { return started_with_shift_; }
  // Returns true only if box selection is active AND thresholds met
  bool shouldShowBoxOverlay() const;
  // Returns true only if item drag preview should be shown (thresholds met)
  bool shouldShowDragPreview() const;
  glm::vec2 getDragStartScreen() const { return drag_start_screen_; }
  Domain::Position getDragStartTile() const { return drag_start_tile_; }
  double getDragStartTime() const { return drag_start_time_; }

  // Lasso selection state accessors
  LassoMode getLassoMode() const { return lasso_mode_; }
  bool isLassoActive() const { return lasso_mode_ != LassoMode::Inactive; }
  bool shouldShowLassoOverlay() const;
  const std::vector<glm::vec2> &getLassoPoints() const { return lasso_points_; }
  glm::vec2 getCurrentMousePos() const { return current_mouse_pos_; }

  // Context menu state
  bool shouldShowContextMenu() const { return show_context_menu_; }
  void clearContextMenuFlag() { show_context_menu_ = false; }
  Domain::Position getContextMenuPosition() const { return context_menu_pos_; }

private:
  void handlePasteMode(MapViewCamera &camera, AppLogic::EditorSession *session);

  void handleMousePan(MapViewCamera &camera, bool is_focused);
  void handleMouseZoom(MapViewCamera &camera);
  void handleFloorChange(MapViewCamera &camera, bool is_focused);
  void handleTileSelection(MapViewCamera &camera,
                           AppLogic::EditorSession *session,
                           AppLogic::MapInputController *input_controller,
                           const Domain::SelectionSettings *selection_settings,
                           bool is_hovered, bool is_focused);

  // -- Refactored Helpers --
  void handleRightClickInput(const MapViewCamera &camera,
                             AppLogic::EditorSession *session,
                             AppLogic::MapInputController *input_controller,
                             const glm::vec2 &mouse_pos, int mods);

  void handleLassoInput(const MapViewCamera &camera, AppLogic::EditorSession *session,
                        const Domain::SelectionSettings *selection_settings,
                        const glm::vec2 &mouse_pos, bool is_hovered);

  // Sub-helpers for Lasso
  void handleLassoClick(const glm::vec2 &mouse_pos, AppLogic::EditorSession *session,
                        const MapViewCamera &camera,
                        const Domain::SelectionSettings *selection_settings);
  void handleLassoDrag(const glm::vec2 &mouse_pos);

  void handleNormalSelectionInput(
      const MapViewCamera &camera, AppLogic::EditorSession *session,
      AppLogic::MapInputController *input_controller,
      const Domain::SelectionSettings *selection_settings, bool is_hovered,
      bool is_focused, const glm::vec2 &mouse_pos,
      const Domain::Position &tile_pos, int mods);

  // Sub-helpers for Normal Selection
  void handleSelectionMouseDown(const MapViewCamera &camera,
                                AppLogic::EditorSession *session,
                                AppLogic::MapInputController *input_controller,
                                const glm::vec2 &mouse_pos,
                                const Domain::Position &tile_pos, int mods,
                                const ImGuiIO &io);

  void handleDragState(const MapViewCamera &camera, AppLogic::EditorSession *session,
                       AppLogic::MapInputController *input_controller,
                       const Domain::SelectionSettings *selection_settings,
                       const glm::vec2 &mouse_pos);

  // Sub-helpers for Drag
  void handleDragRelease(const MapViewCamera &camera, AppLogic::EditorSession *session,
                         AppLogic::MapInputController *input_controller,
                         const Domain::SelectionSettings *selection_settings,
                         const glm::vec2 &mouse_pos);
  // ------------------------

  // Panning state
  bool is_panning_ = false;
  glm::vec2 pan_start_{0, 0};

  // Drag selection state
  bool is_drag_selecting_ = false;
  bool started_with_shift_ = false;
  glm::vec2 drag_start_screen_{0, 0};
  double drag_start_time_ = 0.0;
  bool drag_notified_ = false;
  Domain::Position drag_start_tile_;

  // Lasso selection state (GIMP-style)
  LassoMode lasso_mode_ = LassoMode::Inactive;
  std::vector<glm::vec2> lasso_points_;
  double last_lasso_click_time_ = 0.0; // For double-click detection
  glm::vec2 lasso_drag_start_{0, 0};   // Where current drag started
  glm::vec2 current_mouse_pos_{0, 0};  // For preview line rendering
  bool lasso_is_ctrl_held_ = false;    // For additive mode
  bool lasso_is_shift_held_ = false;   // For deselect mode (Ctrl+Alt+Shift)

  // Helper to finalize lasso selection
  void
  finalizeLassoSelection(AppLogic::EditorSession *session,
                         const MapViewCamera &camera,
                         const Domain::SelectionSettings *selection_settings);

  // Context menu state
  bool show_context_menu_ = false;
  Domain::Position context_menu_pos_;

  // Brush size modifier support (non-owning)
  Brushes::BrushController *brush_controller_ = nullptr;

  // Deferred selection state
  bool skipped_selection_on_down_ = false;
  int mods_at_down_ = 0; // Store modifiers when click started (for Shift+Click)

public:
  void setBrushController(Brushes::BrushController *brush_controller) {
    brush_controller_ = brush_controller;
  }
};

} // namespace UI
} // namespace MapEditor
