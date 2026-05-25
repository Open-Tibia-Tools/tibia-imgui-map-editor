#include "WorkspaceController.h"
#include "Application/EditorSession.h"
#include "Brushes/BrushController.h"
#include "Controllers/MapInputController.h"
#include "Controllers/SearchController.h"
#include "Rendering/Map/MapRenderer.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "UI/Map/MapPanel.h"
#include "UI/Widgets/TilesetWidget.h"
#include "UI/Windows/BrowseTile/BrowseTileWindow.h"
#include "UI/Windows/MinimapWindow.h"
#include "UI/Windows/PaletteWindowManager.h"
#include <spdlog/spdlog.h>

namespace MapEditor::Presentation {

WorkspaceController::WorkspaceController(
    UI::MapPanel &map_panel, UI::MinimapWindow &minimap_window,
    UI::BrowseTileWindow &browse_tile_window, UI::TilesetWidget &tileset_widget,
    UI::PaletteWindowManager *palette_window_manager,
    Brushes::BrushController &brush_controller,
    AppLogic::SearchController &search_controller,
    AppLogic::MapInputController &input_controller)
    : map_panel_(map_panel), minimap_window_(minimap_window),
      browse_tile_window_(browse_tile_window), tileset_widget_(tileset_widget),
      palette_window_manager_(palette_window_manager),
      brush_controller_(brush_controller),
      search_controller_(search_controller),
      input_controller_(input_controller) {}

void WorkspaceController::bindSession(
    AppLogic::EditorSession *session, Services::ClientDataService *client_data,
    Services::SpriteManager *sprite_manager,
    Services::ViewSettings *view_settings,
    Domain::Tileset::TilesetRegistry *tileset_registry,
    Domain::Palette::PaletteRegistry *palette_registry,
    const Domain::Position *initial_camera_pos) {
  // 1. Update MapPanel
  map_panel_.setEditorSession(session);
  map_panel_.setClientDataService(client_data);
  if (initial_camera_pos) {
    map_panel_.setCameraCenter(*initial_camera_pos);
  }

  // 2. Update InputController
  input_controller_.setClientDataService(client_data);

  // 3. Update SearchController
  search_controller_.onMapLoaded(session ? session->getMap() : nullptr,
                                 client_data, sprite_manager, view_settings);

  // 4. Update Minimap
  minimap_window_.setMap(session ? session->getMap() : nullptr, client_data);

  // 5. Update BrowseTileWindow
  browse_tile_window_.setMap(session ? session->getMap() : nullptr, client_data,
                             sprite_manager);
  if (session) {
    browse_tile_window_.setSelection(&session->getSelectionService());
    browse_tile_window_.setSession(session);
  }

  // 6. Update TilesetWidget (now requires TilesetRegistry)
  if (tileset_registry) {
    tileset_widget_.initialize(client_data, sprite_manager, &brush_controller_,
                               *tileset_registry);
  }

  // 6b. Initialize PaletteWindowManager (now requires both registries)
  if (palette_window_manager_ && tileset_registry && palette_registry) {
    palette_window_manager_->initialize(client_data, sprite_manager,
                                        &brush_controller_, *tileset_registry,
                                        *palette_registry);
    // Restore previously open palette windows
    palette_window_manager_->restoreState();
  }

  // 7. Update BrushController
  if (session) {
    brush_controller_.initialize(session->getMap(),
                                 &session->getHistoryManager(), client_data);

    // Wire preview service
    brush_controller_.setPreviewService(&session->getPreviewService());

    // Clear selection when brush is activated
    brush_controller_.setOnBrushActivatedCallback(
        [session]() { session->getSelectionService().clear(); });
  }
}

void WorkspaceController::unbindSession() {
  map_panel_.setEditorSession(nullptr);
  map_panel_.setClientDataService(nullptr);

  input_controller_.setClientDataService(nullptr);

  // Reset SearchController
  search_controller_.onMapLoaded(nullptr, nullptr, nullptr, nullptr);

  minimap_window_.setMap(nullptr, nullptr);

  browse_tile_window_.setMap(nullptr, nullptr, nullptr);
  browse_tile_window_.setSelection(nullptr);
  browse_tile_window_.setSession(nullptr);

  // Reset controllers that hold pointers to session/client data
  brush_controller_.initialize(nullptr, nullptr, nullptr);
  // Note: tileset_widget_ keeps its registry reference; it just doesn't have
  // session-specific data to clear
}

} // namespace MapEditor::Presentation
