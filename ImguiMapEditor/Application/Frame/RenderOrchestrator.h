#pragma once
#include "Rendering/Passes/BackgroundRenderer.h"
#include <functional>
#include <memory>

// Forward declarations
namespace MapEditor {

namespace Platform {
class ImGuiBackend;
class GlfwWindow;
} // namespace Platform

namespace Services {
class ConfigService;
struct ViewSettings;
} // namespace Services

namespace AppLogic {
class MapTabManager;
class EditorSession;
class StartupController;
class MapOperationHandler;
} // namespace AppLogic

namespace Rendering {
class MapRenderer;
class RenderingManager;
} // namespace Rendering

namespace UI {
class MapPanel;
class MinimapWindow;
class BrowseTileWindow;
class TilesetWidget;
class QuickSearchPopup;
class AdvancedSearchDialog;
class SearchResultsWidget;
class StartupDialog;
class PaletteWindowManager;
struct RecentFileEntry;

namespace Ribbon {
class RibbonController;
class FilePanel;
} // namespace Ribbon
} // namespace UI

namespace Presentation {
class MainWindow;
}

namespace Brushes {
class BrushController;
class BrushSystem;
} // namespace Brushes

namespace UI {
namespace Panels {
class BrushSizePanel;
} // namespace Panels
} // namespace UI

class AppStateManager;
class ClientVersionManager;
class DialogContainer;

/**
 * Orchestrates the render loop, delegating UI rendering to components.
 * Extracted from Application to reduce coupling and improve testability.
 */
class RenderOrchestrator {
public:
  /**
   * Context containing all dependencies needed for rendering.
   * Non-owning pointers to Application's owned resources.
   */
  struct Context {
    // Platform
    Platform::ImGuiBackend *imgui_backend = nullptr;
    Platform::GlfwWindow *window = nullptr;

    // State
    AppStateManager *state_manager = nullptr;
    AppLogic::MapTabManager *tab_manager = nullptr;
    ClientVersionManager *version_manager = nullptr;

    // Services
    Services::ConfigService *config = nullptr;
    Services::ViewSettings *view_settings = nullptr;

    // Rendering
    Rendering::RenderingManager *rendering_manager = nullptr;

    // UI Components
    UI::MapPanel *map_panel = nullptr;
    UI::MinimapWindow *minimap = nullptr;
    UI::BrowseTileWindow *browse_tile = nullptr;
    UI::Ribbon::RibbonController *ribbon = nullptr;
    UI::Ribbon::FilePanel *file_panel = nullptr;
    Presentation::MainWindow *main_window = nullptr;
    UI::QuickSearchPopup *quick_search_popup = nullptr;
    UI::AdvancedSearchDialog *advanced_search_dialog = nullptr;
    UI::SearchResultsWidget *search_results_widget = nullptr;
    UI::TilesetWidget *tileset_widget = nullptr;
    UI::PaletteWindowManager *palette_window_manager = nullptr;
    UI::StartupDialog *startup_dialog = nullptr;
    AppLogic::StartupController *startup_controller = nullptr;

    // Dialogs
    DialogContainer *dialogs = nullptr;

    // Brush system (for preview rendering)
    Brushes::BrushController *brush_controller = nullptr;
    UI::Panels::BrushSizePanel *brush_size_panel = nullptr;

    // Map operations (for compatibility popup)
    AppLogic::MapOperationHandler *map_operations = nullptr;

    // Callbacks for actions that require Application state
    std::function<void()> on_perform_version_switch;
    std::function<void(int)> on_request_close_tab;
    std::function<int()> get_active_tab_index;

    // Pending state (passed by reference from Application)
    bool *pending_close_all = nullptr;
  };

  /**
   * Execute one render frame.
   */
  void render(Context &ctx);

private:
  void beginFrame(Context &ctx);
  void renderEditorState(Context &ctx, AppLogic::EditorSession *session);
  void renderDialogs(Context &ctx);
  void renderNotifications();
  void endFrame(Context &ctx);

  // Startup background image renderer
  Rendering::BackgroundRenderer background_renderer_;
};

} // namespace MapEditor
