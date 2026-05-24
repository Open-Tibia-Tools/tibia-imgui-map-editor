#pragma once
#include "Domain/ICoordinateTransformer.h"
#include "Domain/Position.h"
#include "Services/Selection/ISelectionObserver.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <vector>

namespace MapEditor {
namespace AppLogic {
class EditorSession;
}

namespace Rendering {

// Forward declaration
class ISelectionDataProvider;

/**
 * Renders selection highlights and drag selection boxes.
 *
 * Single responsibility: selection visualization.
 * Uses ISelectionDataProvider interface for clean separation from
 * SelectionService.
 *
 * Implements ISelectionObserver to receive selection change notifications
 * and track when re-rendering is needed.
 */
class SelectionOverlay : public Services::Selection::ISelectionObserver {
public:
  SelectionOverlay() = default;
  ~SelectionOverlay() override = default;

  /**
   * Render all selection visuals.
   * Gets selection data from EditorSession via ISelectionDataProvider.
   */
  void render(ImDrawList *draw_list, const Domain::ICoordinateTransformer &camera,
              const ISelectionDataProvider *selection_provider);

  /**
   * Render drag selection box.
   */
  void renderDragBox(ImDrawList *draw_list, const glm::vec2 &start_screen,
                     const glm::vec2 &current_screen);

  /**
   * Render lasso selection polygon outline.
   * @param current_mouse Current mouse position for preview line to next click
   */
  void renderLassoOverlay(ImDrawList *draw_list,
                          const std::vector<glm::vec2> &points,
                          const glm::vec2 &current_mouse);

  /**
   * Render drag selection dimensions text.
   */
  void renderDragDimensions(ImDrawList *draw_list,
                            const glm::vec2 &start_screen,
                            const glm::vec2 &current_screen,
                            const Domain::ICoordinateTransformer &camera, bool shif_pressed,
                            bool alt_pressed);

  // === ISelectionObserver interface ===

  /**
   * Called when selection changes. Sets dirty flag for potential optimization.
   */
  void onSelectionChanged(
      const std::vector<Domain::Selection::SelectionEntry> &added,
      const std::vector<Domain::Selection::SelectionEntry> &removed) override;

  /**
   * Called when selection is cleared.
   */
  void onSelectionCleared() override;

  /**
   * Check if selection has changed since last render.
   */
  bool isDirty() const { return dirty_; }

  /**
   * Clear the dirty flag after processing changes.
   */
  void clearDirty() { dirty_ = false; }

private:
  /**
   * Renders selection by iterating through selected entries.
   * Efficient for small selections.
   */
  void renderSelectionIterative(ImDrawList *draw_list,
                                const Domain::ICoordinateTransformer &camera,
                                const ISelectionDataProvider *provider,
                                int floor, float tile_screen_size);

  /**
   * Renders selection by iterating through visible viewport tiles.
   * Efficient for large selections (avoids O(N) iteration of selection set).
   */
  void renderSelectionViewport(ImDrawList *draw_list,
                               const Domain::ICoordinateTransformer &camera,
                               const ISelectionDataProvider *provider,
                               int floor, float tile_screen_size);

  // Dirty flag - set by observer callbacks, cleared after render
  bool dirty_ = false;
};

} // namespace Rendering
} // namespace MapEditor

