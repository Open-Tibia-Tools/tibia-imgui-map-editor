#include "MapPanelInput.h"
#include "Application/EditorSession.h"
#include "Application/Selection/FloorScopeHelper.h"
#include "Application/Selection/LassoSelectionProcessor.hpp"
#include "Controllers/MapInputController.h"
#include "Core/Config.h"
#include "Domain/SelectionSettings.h"
#include "Brushes/BrushController.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace MapEditor {
namespace UI {

bool MapPanelInput::handleInput(
    MapViewCamera &camera, AppLogic::EditorSession *session,
    AppLogic::MapInputController *input_controller,
    const Domain::SelectionSettings *selection_settings, bool is_hovered,
    bool is_focused) {
  // Handle paste mode intercept
  if (session && session->isPasting()) {
    handlePasteMode(camera, session);
    handleMousePan(camera, is_focused);
    handleMouseZoom(camera);
    return true;
  }

  handleMousePan(camera, is_focused);
  handleMouseZoom(camera);
  handleFloorChange(camera, is_focused);

  if (is_hovered) {
    handleTileSelection(camera, session, input_controller, selection_settings,
                        is_hovered, is_focused);
  }

  return false;
}

void MapPanelInput::handlePasteMode(MapViewCamera &camera,
                                    AppLogic::EditorSession *session) {
  ImGuiIO &io = ImGui::GetIO();

  // Confirm paste on left click
  // Replace mode is set by Ctrl+Shift+V OR holding Shift during click
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    glm::vec2 mouse_pos(io.MousePos.x, io.MousePos.y);
    Domain::Position target_pos = camera.screenToTile(mouse_pos);
    bool replace_mode = session->isPasteReplaceMode() || io.KeyShift;
    session->confirmPaste(target_pos, replace_mode);
    return;
  }

  // Cancel paste on Escape or right click
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
      ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    session->cancelPaste();
  }
}

void MapPanelInput::handleMousePan(MapViewCamera &camera, bool is_focused) {
  ImGuiIO &io = ImGui::GetIO();

  // Middle mouse button for panning
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
    is_panning_ = true;
    pan_start_ = glm::vec2(io.MousePos.x, io.MousePos.y);
  }

  if (is_panning_) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      glm::vec2 current(io.MousePos.x, io.MousePos.y);
      glm::vec2 delta = (pan_start_ - current) /
                        (Config::Rendering::TILE_SIZE * camera.getZoom());

      glm::vec2 cam_pos = camera.getCameraPosition();
      camera.setCameraPosition(cam_pos.x + delta.x, cam_pos.y + delta.y);

      pan_start_ = current;
    } else {
      is_panning_ = false;
    }
  }

  // Arrow keys for panning
  if (is_focused) {
    float move_speed = 5.0f;
    glm::vec2 cam_pos = camera.getCameraPosition();
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
      camera.setCameraPosition(cam_pos.x - move_speed, cam_pos.y);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
      camera.setCameraPosition(cam_pos.x + move_speed, cam_pos.y);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
      camera.setCameraPosition(cam_pos.x, cam_pos.y - move_speed);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
      camera.setCameraPosition(cam_pos.x, cam_pos.y + move_speed);
  }
}

void MapPanelInput::handleMouseZoom(MapViewCamera &camera) {
  ImGuiIO &io = ImGui::GetIO();

  if (brush_controller_ && brush_controller_->hasBrush() && io.KeyAlt &&
      io.MouseWheel != 0.0f) {
    brush_controller_->adjustBrushSize(io.MouseWheel > 0.0f ? 1 : -1);
    return;
  }

  if (io.MouseWheel != 0 && !io.KeyCtrl) {
    glm::vec2 mouse_pos(io.MousePos.x, io.MousePos.y);
    camera.adjustZoom(io.MouseWheel, mouse_pos);
  }
}

void MapPanelInput::handleFloorChange(MapViewCamera &camera, bool is_focused) {
  ImGuiIO &io = ImGui::GetIO();

  // Ctrl + Scroll for floor change (inverted: scroll down = floor up, scroll up
  // = floor down)
  if (io.MouseWheel != 0 && io.KeyCtrl) {
    if (io.MouseWheel > 0) {
      camera.floorDown();
    } else {
      camera.floorUp();
    }
  }

  // Page Up/Down for floor change
  if (is_focused) {
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
      camera.floorUp();
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
      camera.floorDown();
  }
}

bool MapPanelInput::shouldShowBoxOverlay() const {
  if (!is_drag_selecting_ || !started_with_shift_)
    return false;

  // Get current mouse position
  ImGuiIO &io = ImGui::GetIO();
  float dx = io.MousePos.x - drag_start_screen_.x;
  float dy = io.MousePos.y - drag_start_screen_.y;
  float dist_sq = dx * dx + dy * dy;

  // STRICT: Requires BOTH time AND distance
  double elapsed = ImGui::GetTime() - drag_start_time_;
  bool time_met = elapsed > Config::Input::DRAG_DELAY_SECONDS;
  bool distance_met = dist_sq > Config::Input::DRAG_THRESHOLD_SQ;

  return time_met && distance_met;
}

bool MapPanelInput::shouldShowDragPreview() const {
  // Only for non-box-selection drags (item dragging)
  // Exclude lasso mode (any state) from drag preview
  if (!is_drag_selecting_ || started_with_shift_ ||
      lasso_mode_ != LassoMode::Inactive)
    return false;

  // Get current mouse position
  ImGuiIO &io = ImGui::GetIO();
  float dx = io.MousePos.x - drag_start_screen_.x;
  float dy = io.MousePos.y - drag_start_screen_.y;
  float dist_sq = dx * dx + dy * dy;

  // STRICT: Requires BOTH time AND distance
  double elapsed = ImGui::GetTime() - drag_start_time_;
  bool time_met = elapsed > Config::Input::DRAG_DELAY_SECONDS;
  bool distance_met = dist_sq > Config::Input::DRAG_THRESHOLD_SQ;

  return time_met && distance_met;
}

bool MapPanelInput::shouldShowLassoOverlay() const {
  // Show overlay whenever lasso is active (Drawing or Dragging mode)
  if (lasso_mode_ == LassoMode::Inactive)
    return false;

  // Need at least 1 point to show preview line
  return !lasso_points_.empty();
}

void MapPanelInput::handleTileSelection(
    MapViewCamera &camera, AppLogic::EditorSession *session,
    AppLogic::MapInputController *input_controller,
    const Domain::SelectionSettings *selection_settings, bool is_hovered,
    bool is_focused) {
  ImGuiIO &io = ImGui::GetIO();
  glm::vec2 mouse_pos(io.MousePos.x, io.MousePos.y);

  int mods = 0;
  if (io.KeyCtrl)
    mods |= GLFW_MOD_CONTROL;
  if (io.KeyShift)
    mods |= GLFW_MOD_SHIFT;
  if (io.KeyAlt)
    mods |= GLFW_MOD_ALT;

  // Right-click for context menu
  if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    handleRightClickInput(camera, session, input_controller, mouse_pos, mods);
    return;
  }

  // Update current mouse position for preview line rendering
  current_mouse_pos_ = mouse_pos;

  // Check Lasso Inputs
  if (lasso_mode_ != LassoMode::Inactive) {
    handleLassoInput(camera, session, selection_settings, mouse_pos,
                     is_hovered);
    return; // Lasso mode consumes all input
  }

  // Handle Lasso Start with Alt Key
  if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (io.KeyAlt && !io.KeyCtrl &&
        !(brush_controller_ && brush_controller_->hasBrush())) {
      lasso_mode_ = LassoMode::Drawing;
      lasso_points_.clear();
      lasso_points_.push_back(mouse_pos);
      lasso_drag_start_ = mouse_pos;
      last_lasso_click_time_ = ImGui::GetTime();
      lasso_is_ctrl_held_ = io.KeyCtrl;   // For additive mode
      lasso_is_shift_held_ = io.KeyShift; // For deselect mode (Ctrl+Alt+Shift)
      return;
    }
  }

  // Handle Normal Selection Input
  Domain::Position tile_pos = camera.screenToTile(mouse_pos);
  handleNormalSelectionInput(camera, session, input_controller,
                             selection_settings, is_hovered, is_focused,
                             mouse_pos, tile_pos, mods);
}

void MapPanelInput::handleRightClickInput(
    const MapViewCamera &camera, AppLogic::EditorSession *session,
    AppLogic::MapInputController *input_controller,
    const glm::vec2 &mouse_pos, int mods) {
  Domain::Position pos = camera.screenToTile(mouse_pos);
  const glm::vec2 click_tile_screen = camera.tileToScreen(pos);
  const glm::vec2 pixel_offset =
      (mouse_pos - click_tile_screen) / camera.getZoom();

  if (input_controller && session) {
    input_controller->onRightClick(pos, mods, pixel_offset, session);
    show_context_menu_ = input_controller->shouldShowContextMenu();
    context_menu_pos_ = input_controller->getContextMenuPosition();
    input_controller->clearContextMenuFlag();
  } else {
    show_context_menu_ = true;
    context_menu_pos_ = pos;
  }
}

void MapPanelInput::handleLassoInput(
    const MapViewCamera &camera, AppLogic::EditorSession *session,
    const Domain::SelectionSettings *selection_settings,
    const glm::vec2 &mouse_pos, bool is_hovered) {
  // Escape cancels lasso
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    lasso_mode_ = LassoMode::Inactive;
    lasso_points_.clear();
    return;
  }

  // Enter finalizes lasso
  if (lasso_mode_ == LassoMode::Drawing &&
      ImGui::IsKeyPressed(ImGuiKey_Enter)) {
    finalizeLassoSelection(session, camera, selection_settings);
    return;
  }

  // Mouse down while in lasso mode
  if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    handleLassoClick(mouse_pos, session, camera, selection_settings);
  }

  // Dragging while in lasso mode = freeform segment
  if (is_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    handleLassoDrag(mouse_pos);
  }

  // Release after dragging = back to Drawing mode
  if (lasso_mode_ == LassoMode::Dragging &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    lasso_mode_ = LassoMode::Drawing;
  }
}

void MapPanelInput::handleLassoClick(const glm::vec2 &mouse_pos,
                                     AppLogic::EditorSession *session,
                                     const MapViewCamera &camera,
                                     const Domain::SelectionSettings *selection_settings) {
  double now = ImGui::GetTime();
  bool is_double_click =
      (now - last_lasso_click_time_) < 0.15; // 150ms for fast double-click
  last_lasso_click_time_ = now;

  if (is_double_click && lasso_points_.size() >= 2) {
    // Double-click = close polygon and select
    finalizeLassoSelection(session, camera, selection_settings);
  } else {
    // Single click = add vertex
    lasso_points_.push_back(mouse_pos);
    lasso_drag_start_ = mouse_pos;
  }
}

void MapPanelInput::handleLassoDrag(const glm::vec2 &mouse_pos) {
  float dx = mouse_pos.x - lasso_drag_start_.x;
  float dy = mouse_pos.y - lasso_drag_start_.y;
  float dist_sq = dx * dx + dy * dy;

  if (dist_sq > Config::Input::DRAG_THRESHOLD_SQ) {
    lasso_mode_ = LassoMode::Dragging;
    // Add point if distance from last point > threshold
    if (!lasso_points_.empty()) {
      float last_dx = mouse_pos.x - lasso_points_.back().x;
      float last_dy = mouse_pos.y - lasso_points_.back().y;
      float last_dist_sq = last_dx * last_dx + last_dy * last_dy;

      if (last_dist_sq > Config::Input::LASSO_DRAG_POINT_DISTANCE_SQ) {
        lasso_points_.push_back(mouse_pos);
      }
    }
  }
}

void MapPanelInput::handleNormalSelectionInput(
    const MapViewCamera &camera, AppLogic::EditorSession *session,
    AppLogic::MapInputController *input_controller,
    const Domain::SelectionSettings *selection_settings, bool is_hovered,
    bool is_focused, const glm::vec2 &mouse_pos,
    const Domain::Position &tile_pos, int mods) {
  ImGuiIO &io = ImGui::GetIO();

  // Double click handling (non-lasso) - after ALT check
  if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    if (input_controller && session) {
      glm::vec2 click_tile_screen = camera.tileToScreen(tile_pos);
      glm::vec2 click_pixel_offset =
          (mouse_pos - click_tile_screen) / camera.getZoom();
      input_controller->onDoubleClick(tile_pos, click_pixel_offset, session);
    }
  }

  // Start tracking on left click (non-lasso)
  if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    handleSelectionMouseDown(camera, session, input_controller, mouse_pos, tile_pos, mods, io);
  }

  // Drag detection and logic
  handleDragState(camera, session, input_controller, selection_settings,
                  mouse_pos);

  // Escape to clear selection
  if (is_focused && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    if (session) {
      session->getSelectionService().clear();
    }
  }
}

void MapPanelInput::handleSelectionMouseDown(const MapViewCamera &camera,
                                             AppLogic::EditorSession *session,
                                             AppLogic::MapInputController *input_controller,
                                             const glm::vec2 &mouse_pos,
                                             const Domain::Position &tile_pos,
                                             [[maybe_unused]] int mods,
                                             const ImGuiIO &io) {
    is_drag_selecting_ = true;
    drag_start_screen_ = mouse_pos;
    drag_start_tile_ = tile_pos;
    drag_start_time_ = ImGui::GetTime();
    const bool has_brush = input_controller && input_controller->hasBrush();
    started_with_shift_ = io.KeyShift && !has_brush; // Box selection only without brush

    // FIX #1: Save modifiers at mouse-down time (not at release)
    mods_at_down_ = 0;
    if (io.KeyCtrl)
      mods_at_down_ |= GLFW_MOD_CONTROL;
    if (io.KeyShift)
      mods_at_down_ |= GLFW_MOD_SHIFT;
    if (io.KeyAlt)
      mods_at_down_ |= GLFW_MOD_ALT;

    // Immediate selection on mouse down (if not box selection)
    // IMPORTANT: For brush mode, do NOT paint here - let the drag stroke handle
    // it so the entire stroke is one atomic undo entry
    if (!started_with_shift_ && input_controller && session && !has_brush) {
      glm::vec2 click_tile_screen = camera.tileToScreen(drag_start_tile_);
      glm::vec2 click_pixel_offset =
          (drag_start_screen_ - click_tile_screen) / camera.getZoom();

      // Check if we are clicking on an ALREADY selected item
      // If so, do NOT select immediately (Wait for Drag or Up)
      // This allows "Drag Selection" to work without clearing selection on Down
      bool is_selected = input_controller->isSomethingSelectedAt(
          drag_start_tile_, click_pixel_offset, session);

      if (!is_selected) {
        // Only call onLeftClick on down if NOT Ctrl (Ctrl should only toggle on
        // UP)
        if (!(mods_at_down_ & GLFW_MOD_CONTROL)) {
          input_controller->onLeftClick(drag_start_tile_, mods_at_down_,
                                        click_pixel_offset, session);
        }
        // Enable drag for newly selected item (RME parity)
        skipped_selection_on_down_ = true;
      } else {
        // We skipped selection on Down. We MUST handle it on Up if no drag
        // occurs.
        skipped_selection_on_down_ = true;
      }
    }
}

void MapPanelInput::handleDragState(
    const MapViewCamera &camera, AppLogic::EditorSession *session,
    AppLogic::MapInputController *input_controller,
    const Domain::SelectionSettings *selection_settings,
    const glm::vec2 &mouse_pos) {
  if (is_drag_selecting_) {
    float dx = mouse_pos.x - drag_start_screen_.x;
    float dy = mouse_pos.y - drag_start_screen_.y;
    float dist_sq = dx * dx + dy * dy;

    double elapsed = ImGui::GetTime() - drag_start_time_;
    bool time_met = elapsed > Config::Input::DRAG_DELAY_SECONDS;

    // Skip time delay for brush mode - brushes should paint immediately
    bool has_brush = input_controller && input_controller->hasBrush();

    // STRICT: Drag requires BOTH time (0.1s) AND distance (10px) - no
    // exceptions
    bool distance_met = dist_sq > Config::Input::DRAG_THRESHOLD_SQ;
    const bool smart_pick_chord =
        (mods_at_down_ & GLFW_MOD_CONTROL) != 0 &&
        (mods_at_down_ & GLFW_MOD_ALT) != 0;
    bool should_trigger_drag =
        has_brush && !smart_pick_chord ? distance_met
                                       : (distance_met && time_met);

    // Item drag/brush (not box selection)
    if (should_trigger_drag && !started_with_shift_ && input_controller &&
        session) {
      if (!drag_notified_) {
        spdlog::debug("[DRAG] Calling onLeftDragStart at ({},{},{})",
                      drag_start_tile_.x, drag_start_tile_.y,
                      drag_start_tile_.z);
        input_controller->onLeftDragStart(drag_start_tile_, mods_at_down_, session);
        drag_notified_ = true;
      }
      // Update mouse position for continuous brush painting
      Domain::Position current_tile = camera.screenToTile(mouse_pos);
      input_controller->onMouseMove(current_tile, session);
    }
  }

  // Finish drag/click on release
  if (is_drag_selecting_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      handleDragRelease(camera, session, input_controller, selection_settings, mouse_pos);
  }
}

void MapPanelInput::handleDragRelease(
    const MapViewCamera &camera, AppLogic::EditorSession *session,
    AppLogic::MapInputController *input_controller,
    const Domain::SelectionSettings *selection_settings,
    const glm::vec2 &mouse_pos) {

    is_drag_selecting_ = false;

    Domain::Position end_tile = camera.screenToTile(mouse_pos);

    float dx = mouse_pos.x - drag_start_screen_.x;
    float dy = mouse_pos.y - drag_start_screen_.y;
    float dist_sq = dx * dx + dy * dy;

    double elapsed = ImGui::GetTime() - drag_start_time_;
    bool time_met = elapsed > Config::Input::DRAG_DELAY_SECONDS;

    // IMPORTANT: Match drag start logic - include has_brush check
    // This ensures brush strokes properly call endStroke() via onLeftDragEnd()
    bool has_brush = input_controller && input_controller->hasBrush();
    // STRICT: Drag requires BOTH time (0.1s) AND distance (10px)
    // The drag_notified_ flag tells us if we already started a drag,
    // but we still need to complete the drag properly on release
    bool distance_met = dist_sq > Config::Input::DRAG_THRESHOLD_SQ;
    bool is_drag = has_brush ? distance_met : (distance_met && time_met);

    // If we already notified the drag started, we must end it properly
    // but ONLY if the user actually moved (distance_met)
    if (drag_notified_ && distance_met) {
      is_drag = true;
    }

    if (started_with_shift_ && is_drag) {
      // Box selection
      if (session) {
        // FIX: Only clear if CTRL is NOT held (CTRL+SHIFT+Drag = additive)
        if (!(mods_at_down_ & GLFW_MOD_CONTROL)) {
          session->getSelectionService().clear();
        }

        int32_t min_x = std::min(drag_start_tile_.x, end_tile.x);
        int32_t max_x = std::max(drag_start_tile_.x, end_tile.x);
        int32_t min_y = std::min(drag_start_tile_.y, end_tile.y);
        int32_t max_y = std::max(drag_start_tile_.y, end_tile.y);

        // Use FloorScopeHelper to determine floor range (RME-style)
        int16_t current_floor = camera.getCurrentFloor();
        Domain::SelectionFloorScope scope =
            selection_settings ? selection_settings->floor_scope
                               : Domain::SelectionFloorScope::CurrentFloor;
        auto floor_range = AppLogic::getFloorRange(scope, current_floor);

        // Iterate over all floors in range (descending: start to end)
        for (int16_t z = floor_range.start_z; z >= floor_range.end_z; --z) {
          session->selectRegion(min_x, min_y, max_x, max_y, z);
        }
      }
    } else if (is_drag) {
      // Item drag or brush stroke end
      if (input_controller && session) {
        if (!drag_notified_) {
          spdlog::debug("[DRAG] Late onLeftDragStart at release");
          input_controller->onLeftDragStart(drag_start_tile_, mods_at_down_,
                                            session);
        }
        spdlog::debug("[DRAG] Calling onLeftDragEnd at ({},{},{})", end_tile.x,
                      end_tile.y, end_tile.z);
        input_controller->onLeftDragEnd(end_tile, session);
      }
    } else {
      // Single click - handle shift+click selection or brush single-click
      // painting
      if (input_controller && session) {
        bool has_brush = input_controller->hasBrush();
        const bool smart_pick_chord =
            (mods_at_down_ & GLFW_MOD_CONTROL) != 0 &&
            (mods_at_down_ & GLFW_MOD_ALT) != 0;
        if (smart_pick_chord) {
          glm::vec2 click_tile_screen = camera.tileToScreen(drag_start_tile_);
          glm::vec2 click_pixel_offset =
              (drag_start_screen_ - click_tile_screen) / camera.getZoom();
          input_controller->onLeftClick(drag_start_tile_, mods_at_down_,
                                        click_pixel_offset, session);
        } else if (has_brush) {
          // Brush single-click: treat as minimal stroke
          input_controller->onLeftDragStart(drag_start_tile_, mods_at_down_, session);
          input_controller->onLeftDragEnd(drag_start_tile_, session);
        } else if (started_with_shift_ || skipped_selection_on_down_) {
          glm::vec2 click_tile_screen = camera.tileToScreen(drag_start_tile_);
          glm::vec2 click_pixel_offset =
              (drag_start_screen_ - click_tile_screen) / camera.getZoom();
          // FIX #2: Use saved mods from mouse-down (not current mods at
          // release)
          input_controller->onLeftClick(drag_start_tile_, mods_at_down_,
                                        click_pixel_offset, session);
        }
        skipped_selection_on_down_ = false;
      }
    }

    started_with_shift_ = false;
    drag_notified_ = false;
}

void MapPanelInput::finalizeLassoSelection(
    AppLogic::EditorSession *session, const MapViewCamera &camera,
    const Domain::SelectionSettings *selection_settings) {
  // Delegate logic to LassoSelectionProcessor
  using Application::Selection::LassoSelectionProcessor;
  LassoSelectionProcessor::SelectionMode mode;

  if (lasso_is_ctrl_held_ && lasso_is_shift_held_) {
    mode = LassoSelectionProcessor::SelectionMode::Subtract;
  } else if (lasso_is_ctrl_held_ || lasso_is_shift_held_) {
    mode = LassoSelectionProcessor::SelectionMode::Add;
  } else {
    mode = LassoSelectionProcessor::SelectionMode::Replace;
  }

  LassoSelectionProcessor::process(session, camera, selection_settings,
                                   lasso_points_, mode);

  lasso_mode_ = LassoMode::Inactive;
  lasso_points_.clear();
}

} // namespace UI
} // namespace MapEditor
