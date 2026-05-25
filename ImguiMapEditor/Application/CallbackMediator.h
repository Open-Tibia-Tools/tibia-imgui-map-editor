#pragma once

#include <filesystem>
#include <functional>
#include <memory>

// Required for template implementation in wireCloseMapLogic
#include "Application/MapOperationHandler.h"
#include "Application/AppStateManager.h"
#include "UI/Dialogs/UnsavedChangesModal.h"

// Forward declarations for pointers and references only
namespace MapEditor {
class AppStateManager;
class ClientVersionManager;

namespace Platform {
class GlfwWindow;
class PlatformCallbackRouter;
} // namespace Platform

namespace AppLogic {
class MapTabManager;
class MapOperationHandler;
class HotkeyController;
class MapInputController;
class EditorSession;
class ItemPickerService;
class MapSearchService;
} // namespace AppLogic

namespace Brushes {
class BrushController;
}

namespace Services {
class ConfigService;
class ClientVersionRegistry;
class RecentLocationsService;
class ClientDataService;
class SpriteManager;
struct ViewSettings;
} // namespace Services

namespace Rendering {
class RenderingManager;
}

namespace Domain {
struct Position;
class ChunkedMap;
} // namespace Domain

namespace UI {
class MapPanel;
class MinimapWindow;
class IngameBoxWindow;
class BrowseTileWindow;
class UnsavedChangesModal;
class ImportMapDialog;
class ImportMonstersDialog;
class PreferencesDialog;
class EditTownsDialog;
class MapPropertiesDialog;
class ConfirmationDialog;
class QuickSearchPopup;
class AdvancedSearchDialog;
class SearchResultsWidget;

namespace Ribbon {
class FilePanel;
}
} // namespace UI

namespace Presentation {
class MainWindow;
class MenuBar;
} // namespace Presentation
} // namespace MapEditor

namespace MapEditor {

/**
 * Mediator that wires all inter-component callbacks.
 * Extracts the wireCallbacks() logic from Application.
 */
class CallbackMediator {
public:
  /**
   * Context containing all component references needed for callback wiring.
   * Uses raw pointers for non-owning references.
   */
  struct Context {
    // Platform
    Platform::GlfwWindow *window = nullptr;
    Platform::PlatformCallbackRouter *callback_router = nullptr;

    // Core managers
    AppStateManager *state_manager = nullptr;
    ClientVersionManager *version_manager = nullptr;
    AppLogic::MapTabManager *tab_manager = nullptr;

    // Services
    Services::ConfigService *config = nullptr;
    Services::ClientVersionRegistry *versions = nullptr;
    Services::RecentLocationsService *recent = nullptr;
    Services::ViewSettings *view_settings = nullptr;

    // Rendering
    Rendering::RenderingManager *rendering_manager = nullptr;

    // UI Components
    UI::MapPanel *map_panel = nullptr;
    UI::MinimapWindow *minimap = nullptr;
    UI::IngameBoxWindow *ingame_box = nullptr;
    UI::BrowseTileWindow *browse_tile = nullptr;
    Presentation::MainWindow *main_window = nullptr;
    Presentation::MenuBar *menu_bar = nullptr;
    UI::Ribbon::FilePanel *file_panel = nullptr;

    // Controllers
    AppLogic::HotkeyController *hotkey = nullptr;
    AppLogic::MapInputController *input_controller = nullptr;
    AppLogic::MapOperationHandler *map_operations = nullptr;
    Brushes::BrushController *brush_controller = nullptr;

    // Dialogs
    UI::UnsavedChangesModal *unsaved_modal = nullptr;
    UI::ImportMapDialog *import_map = nullptr;
    UI::ImportMonstersDialog *import_monsters = nullptr;
    UI::PreferencesDialog *preferences = nullptr;
    UI::EditTownsDialog *edit_towns = nullptr;
    UI::MapPropertiesDialog *map_properties = nullptr;

    // Search components
    UI::QuickSearchPopup *quick_search = nullptr;
    UI::AdvancedSearchDialog *advanced_search = nullptr;
    UI::SearchResultsWidget *search_results = nullptr;
    UI::ConfirmationDialog *cleanup_confirm = nullptr;

    // Callbacks back to Application (simple types only)
    std::function<void()> quit_callback;
    std::function<void()> change_version_callback;
    std::function<void(int)> request_close_tab;
    std::function<void()> trigger_invalid_items_cleanup;
    std::function<void()> trigger_house_items_cleanup;

    // MapOperationHandler callbacks (captures complex types via std::function)
    // NOTE: MapRenderer not included - caller uses
    // RenderingManager::createRenderer()
    std::function<void(std::unique_ptr<Domain::ChunkedMap>,
                       std::unique_ptr<Services::ClientDataService>,
                       std::unique_ptr<Services::SpriteManager>,
                       const Domain::Position &)>
        on_map_loaded;

    std::function<void(int, const std::string &)> on_notification;
  };

  /**
   * Wire all callbacks between components.
   */
  void wireAll(Context &ctx);

private:
  void wirePlatformCallbacks(Context &ctx);
  void wireTabCallbacks(Context &ctx);
  void wireMapOperationCallbacks(Context &ctx);
  void wireMenuCallbacks(Context &ctx);
  void wireSecondaryClientCallbacks(Context &ctx);
  void wireRibbonCallbacks(Context &ctx);
  void wireCleanupCallbacks(Context &ctx);
  void wireSearchCallbacks(Context &ctx);
  void wireInputCallbacks(Context &ctx);
  void wireMinimapCallbacks(Context &ctx);

  /**
   * Helper to wire close map logic (handles unsaved changes checks).
   * @param ctx Context containing required managers
   * @param set_callback_fn Function to set the callback on the target component (e.g. MenuBar::setCloseMapCallback)
   */
  template<typename F>
  void wireCloseMapLogic(Context &ctx, F set_callback_fn) {
      set_callback_fn([ctx]() {
          auto *session = ctx.tab_manager->getActiveSession();
          int index = ctx.tab_manager->getActiveTabIndex();

          auto perform_close = [ctx, index]() {
              if (index >= 0) {
                  ctx.tab_manager->closeTab(index);
                  if (ctx.tab_manager->getTabCount() == 0) {
                      ctx.state_manager->transition(AppStateManager::State::Startup);
                  }
              }
          };

          if (session && session->isModified()) {
              ctx.unsaved_modal->setSaveCallback(
                  [ctx, perform_close]() {
                      ctx.map_operations->handleSaveMap();
                      perform_close();
                  });
              ctx.unsaved_modal->setDiscardCallback(perform_close);
              ctx.unsaved_modal->show(session->getDisplayName());
          } else {
              perform_close();
          }
      });
  }
};

} // namespace MapEditor
