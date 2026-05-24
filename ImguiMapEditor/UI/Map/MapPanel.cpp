#include "MapPanel.h"
#include "Application/EditorSession.h"
#include "Brushes/BrushController.h"
#include "Controllers/MapInputController.h"
#include "Core/Config.h"
#include "Presentation/NotificationHelper.h"
#include "Rendering/Map/MapRenderer.h"
#include "Rendering/Overlays/GridOverlay.h"
#include "Rendering/Overlays/OverlayManager.h"
#include "Rendering/Overlays/OverlayRenderer.h"
#include "Rendering/Overlays/PreviewOverlay.h"
#include "Rendering/Overlays/SelectionOverlay.h"
#include "Rendering/Overlays/StatusOverlay.h"
#include "Rendering/Visibility/LODPolicy.h"
#include "Services/Preview/DragPreviewProvider.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace MapEditor {
namespace UI {

MapPanel::MapPanel() {
  overlay_manager_ = std::make_unique<Rendering::OverlayManager>();
}

void MapPanel::setEditorSession(AppLogic::EditorSession *session) {
  session_ = session;
  // Register SelectionOverlay as observer of SelectionService
  if (overlay_manager_) {
    overlay_manager_->bindSelectionService(
        session ? &session->getSelectionService() : nullptr);
  }
}

void MapPanel::render(Domain::ChunkedMap *map, Rendering::RenderState &state,
                      Rendering::MapRenderer *renderer) {
  renderInternal(map, state, renderer);
}

void MapPanel::render(Domain::ChunkedMap *map, Rendering::RenderState &state,
                      Rendering::MapRenderer *renderer,
                      const Rendering::AnimationTicks *anim_ticks) {
  renderInternal(map, state, renderer, anim_ticks);
}

template <typename MapType>
void MapPanel::renderInternal(MapType *map, Rendering::RenderState &state,
                              Rendering::MapRenderer *renderer,
                              const Rendering::AnimationTicks *anim_ticks) {
  // Calculate viewport
  ImVec2 const cursor_pos = ImGui::GetCursorScreenPos();
  ImVec2 const avail_size = ImGui::GetContentRegionAvail();

  float const x = std::floor(cursor_pos.x);
  float const y = std::floor(cursor_pos.y);
  float const w = std::floor(avail_size.x);
  float const h = std::floor(avail_size.y);

  camera_.setViewport(glm::vec2(x, y), glm::vec2(w, h));

  // Handle input
  is_hovered_ = ImGui::IsWindowHovered();
  is_focused_ = ImGui::IsWindowFocused();

  if (is_hovered_) {
    input_.handleInput(camera_, session_, input_controller_,
                       selection_settings_, is_hovered_, is_focused_);
  }

  if (map && renderer) {
    // Sync with ViewSettings
    if (view_settings_) {
      view_settings_->zoom = camera_.getZoom();
      view_settings_->current_floor = camera_.getCurrentFloor();
      view_settings_->camera_x = camera_.getCameraPosition().x;
      view_settings_->camera_y = camera_.getCameraPosition().y;
    }

    // Update renderer camera
    renderer->setCameraPosition(camera_.getCameraPosition().x,
                                camera_.getCameraPosition().y);

    // Pass selection provider to renderer (session owns the adapter)
    if (session_) {
      renderer->setSelectionProvider(session_->getSelectionProvider());
      renderer->setCreatureSimulator(&session_->getCreatureSimulator());
    } else {
      renderer->setSelectionProvider(nullptr);
      renderer->setCreatureSimulator(nullptr);
    }

    // Push LOD state to renderer and overlays
    bool const is_lod_active = Rendering::LODPolicy::isLodActive(camera_.getZoom());
    renderer->setLODMode(is_lod_active);
    overlay_manager_->setLODMode(is_lod_active);

    // Render map to framebuffer
    // Use session's RenderState for ChunkedMap (per-tab caching)
    // Use session's RenderState for ChunkedMap (per-tab caching)
    if constexpr (std::is_same_v<MapType, Domain::ChunkedMap>) {
      // Always render if map is valid, using provided state
      renderer->render(*map, state,
                       static_cast<int>(camera_.getViewportSize().x),
                       static_cast<int>(camera_.getViewportSize().y),
                       anim_ticks ? *anim_ticks : Rendering::AnimationTicks{});
    }

    // Display rendered texture
    uint32_t const tex_id = renderer->getTextureId();
    if (tex_id != 0) {
      ImGui::SetCursorScreenPos(
          ImVec2(camera_.getViewportPos().x, camera_.getViewportPos().y));
      ImGui::Image(
          (void *)(intptr_t)tex_id,
          ImVec2(camera_.getViewportSize().x, camera_.getViewportSize().y),
          ImVec2(0, 1), ImVec2(1, 0));
    }
  } else {
    renderBackground();
  }

  // Grid
  bool const grid_visible = view_settings_ ? view_settings_->show_grid : show_grid_;
  if (grid_visible) {
    overlay_manager_->getGridOverlay().render(
        ImGui::GetWindowDrawList(), camera_.getCameraPosition(),
        camera_.getViewportPos(), camera_.getViewportSize(), camera_.getZoom());
  }

  // Selection overlay
  if ((session_ && !session_->getSelectionService().isEmpty()) ||
      input_.isDragSelecting()) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    // Use session's selection provider (session owns the adapter)
    overlay_manager_->getSelectionOverlay().render(draw_list, camera_,
                                                   session_->getSelectionProvider());

    // Draw drag selection box if shift-selecting AND time delay has been met
    if (input_.isDragSelecting() && input_.shouldShowBoxOverlay()) {
      ImGuiIO  const&io = ImGui::GetIO();
      glm::vec2 const current_mouse(io.MousePos.x, io.MousePos.y);
      overlay_manager_->getSelectionOverlay().renderDragBox(
          draw_list, input_.getDragStartScreen(), current_mouse);

      overlay_manager_->getSelectionOverlay().renderDragDimensions(
          draw_list, input_.getDragStartScreen(), current_mouse, camera_,
          io.KeyShift, io.KeyAlt);
    }
  }

  // Draw lasso selection polygon outline (GIMP-style) - OUTSIDE selection block
  // so it's always visible when lasso is active
  if (input_.shouldShowLassoOverlay()) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    const auto &points = input_.getLassoPoints();
    if (!points.empty()) {
      overlay_manager_->getSelectionOverlay().renderLassoOverlay(
          draw_list, points, input_.getCurrentMousePos());
    }
  }

  // Overlay rendering (spawns, waypoints, tooltips)
  if (map && overlay_manager_ && view_settings_) {
    // Use session's creature simulator if available
    Services::CreatureSimulator *simulator = nullptr;
    if (session_) {
      simulator = &session_->getCreatureSimulator();
    }
    // No fallback simulator (creature_simulator_ removed from class)

    if constexpr (std::is_same_v<MapType, Domain::ChunkedMap>) {

      // NOTE: Simulation update logic removed from View!
      // It is now handled by Application::updateEditor -> EditorSession

      // Get overlay collector directly from session render state (MapRenderer
      // is stateless)
      const Rendering::OverlayCollector *overlay_collector =
          &state.overlay_collector;

      overlay_manager_->getOverlayRenderer().render(
          map, renderer ? renderer->getClientData() : nullptr,
          renderer ? renderer->getSpriteManager() : nullptr,
          renderer ? renderer->getOverlaySpriteCache() : nullptr, simulator,
          *view_settings_, camera_.getViewportPos(), camera_.getViewportSize(),
          camera_.getCameraPosition(), camera_.getZoom(),
          camera_.getCurrentFloor(), overlay_collector);

      // Drag preview via unified PreviewService
      // Use MapPanelInput's threshold check for proper separation of concerns
      bool const has_brush = brush_controller_ && brush_controller_->hasBrush();
      if (input_.shouldShowDragPreview() && session_ && !has_brush) {
        auto &previewService = session_->getPreviewService();

        // Create the drag preview provider if not already set
        if (!drag_preview_active_) {
          auto provider =
              std::make_unique<Services::Preview::DragPreviewProvider>(
                  session_->getSelectionService(), map,
                  input_.getDragStartTile());
          previewService.setProvider(std::move(provider));
          drag_preview_active_ = true;
        }

        // Update cursor position for preview
        ImGuiIO  const&io = ImGui::GetIO();
        glm::vec2 const current_mouse(io.MousePos.x, io.MousePos.y);
        Domain::Position const mouse_tile = camera_.screenToTile(current_mouse);
        previewService.updateCursor(mouse_tile);
      } else {
        // Clear drag preview when drag ends or conditions not met
        if (drag_preview_active_ && session_) {
          session_->getPreviewService().clearPreview();
          drag_preview_active_ = false;
        }
      }

      // NOTE: Paste preview is now handled by the unified PreviewService below
      // (PastePreviewProvider is created in EditorSession::startPaste)

      // Unified brush preview (via PreviewService)
      if (session_ && session_->getPreviewService().hasPreview() &&
          is_hovered_) {
        ImGuiIO  const&io = ImGui::GetIO();
        glm::vec2 const current_mouse(io.MousePos.x, io.MousePos.y);
        Domain::Position const mouse_tile = camera_.screenToTile(current_mouse);

        auto &previewService = session_->getPreviewService();
        previewService.updateCursor(mouse_tile);

        // Note: The previous logic calling view_model->getPastePreview() etc.
        // was invalid and has been replaced by the PreviewService logic here.

        overlay_manager_->getPreviewOverlay().render(
            ImGui::GetWindowDrawList(),
            renderer ? renderer->getClientData() : nullptr,
            renderer ? renderer->getSpriteManager() : nullptr,
            renderer ? renderer->getOverlaySpriteCache() : nullptr,
            previewService.getPreviewTiles(),
            previewService.getAnchorPosition(), camera_.getCameraPosition(),
            camera_.getViewportPos(), camera_.getViewportSize(),
            camera_.getZoom(), previewService.getStyle());
      }
    }
  }

  renderOverlay();
}

template void MapPanel::renderInternal<Domain::ChunkedMap>(
    Domain::ChunkedMap *, Rendering::RenderState &, Rendering::MapRenderer *,
    const Rendering::AnimationTicks *);

void MapPanel::renderBackground() {
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  glm::vec2 const pos = camera_.getViewportPos();
  glm::vec2 const size = camera_.getViewportSize();

  draw_list->AddRectFilled(ImVec2(pos.x, pos.y),
                           ImVec2(pos.x + size.x, pos.y + size.y),
                           Config::Colors::MAP_BACKGROUND);
}

void MapPanel::renderOverlay() {
  ImDrawList *draw_list = ImGui::GetWindowDrawList();

  // Status overlay (coordinates, selection count, FPS)
  size_t const selection_count =
      session_ ? session_->getSelectionService().size() : 0;
  overlay_manager_->getStatusOverlay().render(draw_list, camera_,
                                              selection_count, is_hovered_,
                                              ImGui::GetIO().Framerate);
}

} // namespace UI
} // namespace MapEditor
