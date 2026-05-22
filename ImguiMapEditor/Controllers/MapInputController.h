#pragma once
#include "Domain/Creature.h"
#include "Domain/Item.h"
#include "Domain/Position.h"
#include "Domain/SelectionSettings.h"
#include "Domain/Spawn.h"
#include <functional>
#include <glm/glm.hpp>
#include <memory>

namespace MapEditor {

namespace Domain {
class ChunkedMap;
} // namespace Domain
namespace Services {
class ClientDataService;
}
namespace Brushes {
class BrushController;
}

namespace AppLogic {

class EditorSession;
class ISelectionStrategy;
class ClipboardService;

/**
 * Handles all map-related mouse and keyboard input.
 * Extracted from MapPanel for separation of concerns.
 */
class MapInputController {
public:
  MapInputController(Domain::SelectionSettings &settings,
                     Services::ClientDataService *client_data);
  ~MapInputController();

  /**
   * Handle left mouse button click.
   * @param pos World position clicked
   * @param mods Key modifiers (Ctrl, Shift, Alt)
   * @param pixel_offset Sub-tile pixel offset for pixel-perfect mode
   * @param session Current editor session
   */
  void onLeftClick(const Domain::Position &pos, int mods,
                   const glm::vec2 &pixel_offset, EditorSession *session);

  /**
   * Check if something is selected at the given position.
   * Used to defer selection logic on mouse down (to allow dragging).
   */
  bool isSomethingSelectedAt(const Domain::Position &pos,
                             const glm::vec2 &pixel_offset,
                             EditorSession *session);

  /**
   * Handle left mouse drag start.
   */
  void onLeftDragStart(const Domain::Position &pos, int mods,
                       EditorSession *session);

  /**
   * Handle left mouse drag end (for item moving).
   */
  void onLeftDragEnd(const Domain::Position &pos, EditorSession *session);

  /**
   * Handle mouse move during drag (for brush painting or selection).
   */
  void onMouseMove(const Domain::Position &pos, EditorSession *session);

  /**
   * Handle right mouse click (context menu).
   */
  void onRightClick(const Domain::Position &pos, int mods,
                    const glm::vec2 &pixel_offset, EditorSession *session);

  /**
   * Handle double click (properties).
   */
  void onDoubleClick(const Domain::Position &pos, const glm::vec2 &pixel_offset,
                     EditorSession *session);

  using OpenItemPropertiesCallback = std::function<void(Domain::Item *)>;
  using OpenSpawnPropertiesCallback =
      std::function<void(Domain::Spawn *, const Domain::Position &)>;
  using OpenCreaturePropertiesCallback = std::function<void(
      Domain::Creature *, const std::string &, const Domain::Position &)>;

  void setOpenItemPropertiesCallback(OpenItemPropertiesCallback cb) {
    open_item_properties_callback_ = cb;
  }
  void setOpenSpawnPropertiesCallback(OpenSpawnPropertiesCallback cb) {
    open_spawn_properties_callback_ = cb;
  }
  void setOpenCreaturePropertiesCallback(OpenCreaturePropertiesCallback cb) {
    open_creature_properties_callback_ = cb;
  }

  /**
   * Check if context menu should be shown.
   */
  bool shouldShowContextMenu() const { return show_context_menu_; }
  void clearContextMenuFlag() { show_context_menu_ = false; }

  const Domain::Position &getContextMenuPosition() const {
    return context_menu_pos_;
  }

  void setClientDataService(Services::ClientDataService *client_data);

  /**
   * Set brush controller for painting operations.
   * When set and active, left click will apply brush instead of selecting.
   */
  void setBrushController(Brushes::BrushController *brush_controller) {
    brush_controller_ = brush_controller;
  }

  /**
   * Check if a brush is currently active.
   */
  bool hasBrush() const;
  bool toggleSelectionTool();

private:
  Domain::SelectionSettings &settings_;
  Services::ClientDataService *client_data_;

  std::unique_ptr<ISelectionStrategy> current_strategy_;
  bool last_was_pixel_perfect_ = false; // Track for dynamic strategy switching

  // Ensure correct strategy is active based on settings
  void ensureCorrectStrategy();

  // Drag state
  bool is_dragging_ = false;
  Domain::Position drag_start_pos_;

  // Brush drag state
  bool is_brush_dragging_ = false;
  Domain::Position last_brush_pos_;

  // Context menu state
  bool show_context_menu_ = false;
  Domain::Position context_menu_pos_;

  OpenItemPropertiesCallback open_item_properties_callback_;
  OpenSpawnPropertiesCallback open_spawn_properties_callback_;
  OpenCreaturePropertiesCallback open_creature_properties_callback_;

  // Brush painting (non-owning)
  Brushes::BrushController *brush_controller_ = nullptr;
};

} // namespace AppLogic
} // namespace MapEditor
