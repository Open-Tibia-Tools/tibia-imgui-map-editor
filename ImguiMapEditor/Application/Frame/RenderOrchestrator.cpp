#include "Application/Frame/RenderOrchestrator.h"
#include "Application/AppStateManager.h"
#include "Application/ClientVersionManager.h"
#include "Application/DialogContainer.h"
#include "Application/MapOperationHandler.h"
#include "Application/MapTabManager.h"
#include "Controllers/StartupController.h"
#include "Platform/GlfwWindow.h"
#include "Platform/ImGuiBackend.h"
#include "Presentation/Dialogs/CleanupController.h"
#include "Presentation/Dialogs/ImportMapController.h"
#include "Presentation/Dialogs/TownPickController.h"
#include "Presentation/MainWindow.h"
#include "Rendering/Animation/AnimationTicks.h"
#include "Rendering/Frame/RenderingManager.h"
#include "Rendering/Map/MapRenderer.h"
#include "Services/ConfigService.h"
#include "Services/ViewSettings.h"
#include "UI/Dialogs/AdvancedSearchDialog.h"
#include "UI/Dialogs/Startup/StartupDialog.h"
#include "UI/Dialogs/UnsavedChangesModal.h"
#include "UI/Map/MapPanel.h"
#include "UI/Panels/BrushSizePanel.h"
#include "UI/Ribbon/Panels/FilePanel.h"
#include "UI/Ribbon/RibbonController.h"
#include "UI/Widgets/QuickSearchPopup.h"
#include "UI/Widgets/SearchResultsWidget.h"
#include "UI/Widgets/TilesetWidget.h"
#include "UI/Windows/BrowseTile/BrowseTileWindow.h"
#include "UI/Windows/MinimapWindow.h"
#include "UI/Windows/PaletteWindowManager.h"
#include <filesystem>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>

// ImGuiToast integration
#include "ImGuiNotify.hpp"
namespace MapEditor {

void RenderOrchestrator::render(Context &ctx) {
  beginFrame(ctx);

  auto *session = ctx.tab_manager->getActiveSession();

  // Render tiled background for Startup state only
  if (ctx.state_manager->isInState(AppStateManager::State::Startup)) {
    background_renderer_.tryLoad(); // Lazy load on first use
    background_renderer_.render();
  }

  // Render StartupDialog during Startup state
  if (ctx.state_manager->isInState(AppStateManager::State::Startup) &&
      ctx.startup_dialog && ctx.startup_controller) {
    auto recent_maps = ctx.startup_controller->getRecentMaps();
    auto matched_index = ctx.startup_controller->getMatchedClientIndex();
    ctx.startup_dialog->render(recent_maps, matched_index);
  }

  // Only render MainWindow in Editor state
  // (Startup state is handled by StartupDialog directly)
  if (ctx.state_manager->isInState(AppStateManager::State::Editor)) {

    // Calculate global animation ticks
    Rendering::AnimationTicks anim_ticks;
    if (session) {
      auto now = std::chrono::steady_clock::now();
      int64_t frame_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              now.time_since_epoch())
              .count();
      anim_ticks = Rendering::AnimationTicks::calculate(frame_time);
    }

    // Update FilePanel state
    if (ctx.file_panel) {
      ctx.file_panel->SetHasActiveSession(session != nullptr);
    }

    auto *current_map = session ? session->getMap() : nullptr;

    // Render main window (Editor only now)
    ctx.main_window->renderEditor(current_map, ctx.rendering_manager,
                                  &anim_ticks);

    renderEditorState(ctx, session);
    renderDialogs(ctx);
  }

  renderNotifications();
  endFrame(ctx);
}

void RenderOrchestrator::beginFrame(Context &ctx) {
  ctx.imgui_backend->newFrame();
}

void RenderOrchestrator::renderEditorState(Context &ctx,
                                           AppLogic::EditorSession *session) {
  // Render ribbon
  if (ctx.ribbon) {
    ctx.ribbon->Render();
  }

  auto *current_map = session ? session->getMap() : nullptr;

  if (current_map) {
    // Sync minimap with camera
    auto cam_pos = ctx.map_panel->getCameraPosition();
    ctx.minimap->syncWithCamera(static_cast<int32_t>(cam_pos.x),
                                static_cast<int32_t>(cam_pos.y),
                                ctx.map_panel->getCurrentFloor());
    ctx.minimap->render(&ctx.view_settings->show_minimap_window);

    // Browse tile window
    if (ctx.view_settings->show_browse_tile) {
      ctx.browse_tile->setSelection(&session->getSelectionService());
    }
    ctx.browse_tile->render(&ctx.view_settings->show_browse_tile);

    // Tileset/Palettes widget (brush selection)
    // New PaletteWindowManager - render all category and detached windows
    if (ctx.palette_window_manager) {
      ctx.palette_window_manager->renderAllWindows();
    }

    // Brush size panel (dockable)
    if (ctx.brush_size_panel && ctx.view_settings) {
      ctx.brush_size_panel->render(&ctx.view_settings->show_brush_settings);
    }
  }
}

void RenderOrchestrator::renderDialogs(Context &ctx) {
  auto &dialogs = *ctx.dialogs;

  // Unsaved changes modal
  auto modal_result = dialogs.unsaved_changes.render();
  switch (modal_result) {
  case UI::UnsavedChangesModal::Result::Save: {
    bool can_close = *ctx.pending_close_all
                         ? !ctx.tab_manager->hasUnsavedChanges()
                         : (ctx.tab_manager->getActiveSession() &&
                            !ctx.tab_manager->getActiveSession()->isModified());
    if (can_close) {
      if (*ctx.pending_close_all) {
        *ctx.pending_close_all = false;
        if (ctx.on_perform_version_switch)
          ctx.on_perform_version_switch();
      } else {
        if (ctx.on_request_close_tab)
          ctx.on_request_close_tab(ctx.get_active_tab_index());
      }
    }
    break;
  }
  case UI::UnsavedChangesModal::Result::Discard:
    if (*ctx.pending_close_all) {
      *ctx.pending_close_all = false;
      if (ctx.on_perform_version_switch)
        ctx.on_perform_version_switch();
    } else {
      if (ctx.on_request_close_tab)
        ctx.on_request_close_tab(ctx.get_active_tab_index());
    }
    break;
  case UI::UnsavedChangesModal::Result::Cancel:
    *ctx.pending_close_all = false;
    break;
  case UI::UnsavedChangesModal::Result::None:
    break;
  }

  // Import map dialog
  dialogs.import_controller.processResult(
      {ctx.tab_manager, ctx.version_manager->getClientData(),
       ctx.rendering_manager, // Pass rendering_manager instead of renderer
       &dialogs.import_map});

  dialogs.import_monsters.render();
  dialogs.preferences.render();

  // Edit towns dialog
  dialogs.edit_towns.render();
  dialogs.town_pick_controller.processPickMode(
      {&dialogs.edit_towns, ctx.map_panel});

  dialogs.map_properties.render();

  // Cleanup confirmation
  dialogs.cleanup_controller.processResult(
      {ctx.tab_manager, ctx.version_manager->getClientData(),
       ctx.rendering_manager, // Pass rendering_manager instead of renderer
       &dialogs.cleanup_confirm});

  // Quick Search popup (Ctrl+F)
  if (ctx.quick_search_popup) {
    ctx.quick_search_popup->render();
  }

  // Advanced Search dialog (Ctrl+Shift+F)
  if (ctx.advanced_search_dialog) {
    ctx.advanced_search_dialog->render();
  }

  // Search Results widget (dockable)
  if (ctx.search_results_widget && ctx.view_settings) {
    ctx.search_results_widget->render(&ctx.view_settings->show_search_results);
  }

  // Map compatibility popup (for second map loading)
  if (ctx.map_operations) {
    auto &popup = ctx.map_operations->getCompatibilityPopup();
    popup.render();

    if (popup.hasResult()) {
      auto result = popup.consumeResult();
      auto map_path = popup.getMapPath();

      switch (result) {
      case UI::MapCompatibilityPopup::Result::ForceLoad:
        spdlog::info("Force loading incompatible map: {}", map_path.string());
        ctx.config->addRecentFile(map_path.string());
        // Defer map loading until after frame to avoid mid-frame state changes
        ctx.map_operations->requestDeferredMapLoad(
            map_path, ctx.map_operations->getCurrentVersion());
        break;
      case UI::MapCompatibilityPopup::Result::LoadWithNewClient:
        spdlog::info("Load with new client requested (not implemented)");
        break;
      case UI::MapCompatibilityPopup::Result::Cancel:
      case UI::MapCompatibilityPopup::Result::None:
        break;
      }
    }
  }
}

void RenderOrchestrator::renderNotifications() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.f);
  ImGui::RenderNotifications();
  ImGui::PopStyleVar(1);
}

void RenderOrchestrator::endFrame(Context &ctx) {
  ImGui::Render();

  int w, h;
  ctx.window->getFramebufferSize(w, h);
  glViewport(0, 0, w, h);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  ctx.imgui_backend->renderDrawData();
  ctx.window->swapBuffers();
}

} // namespace MapEditor
