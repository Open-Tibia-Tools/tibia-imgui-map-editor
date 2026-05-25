#pragma once

#include <memory>

// Forward declarations
namespace MapEditor::Domain {
struct Position;
namespace Tileset {
class TilesetRegistry;
}
namespace Palette {
class PaletteRegistry;
}
} // namespace MapEditor::Domain

namespace MapEditor::AppLogic {
class EditorSession;
}

namespace MapEditor::Rendering {
class MapRenderer;
}

namespace MapEditor::Services {
class ClientDataService;
class SpriteManager;
struct ViewSettings;
} // namespace MapEditor::Services

namespace MapEditor::UI {
class MapPanel;
class MinimapWindow;
class BrowseTileWindow;
class TilesetWidget;
class PaletteWindowManager;
} // namespace MapEditor::UI

namespace MapEditor::AppLogic {
class SearchController;
class MapInputController;
} // namespace MapEditor::AppLogic

namespace MapEditor::Brushes {
class BrushController;
}

namespace MapEditor::Presentation {

/**
 * Controller responsible for synchronizing UI workspace tools with the active
 * editor session. Manages the "View" aspect of the current session across
 * multiple components.
 */
class WorkspaceController {
public:
  WorkspaceController(UI::MapPanel &map_panel,
                      UI::MinimapWindow &minimap_window,
                      UI::BrowseTileWindow &browse_tile_window,
                      UI::TilesetWidget &tileset_widget,
                      UI::PaletteWindowManager *palette_window_manager,
                      Brushes::BrushController &brush_controller,
                      AppLogic::SearchController &search_controller,
                      AppLogic::MapInputController &input_controller);

  /**
   * Bind the workspace tools to a new active session.
   * Updates all UI components with the new map, client data, and renderer.
   */
  void bindSession(AppLogic::EditorSession *session,
                   Services::ClientDataService *client_data,
                   Services::SpriteManager *sprite_manager,
                   Services::ViewSettings *view_settings,
                   Domain::Tileset::TilesetRegistry *tileset_registry,
                   Domain::Palette::PaletteRegistry *palette_registry,
                   const Domain::Position *initial_camera_pos = nullptr);

  /**
   * Unbind the workspace tools.
   * Clears references to the session and map data to prevent use-after-free.
   * Should be called when the active session is closed or destroyed.
   */
  void unbindSession();

private:
  UI::MapPanel &map_panel_;
  UI::MinimapWindow &minimap_window_;
  UI::BrowseTileWindow &browse_tile_window_;
  UI::TilesetWidget &tileset_widget_;
  UI::PaletteWindowManager *palette_window_manager_ = nullptr;
  Brushes::BrushController &brush_controller_;
  AppLogic::SearchController &search_controller_;
  AppLogic::MapInputController &input_controller_;
};

} // namespace MapEditor::Presentation
