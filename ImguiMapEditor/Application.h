#pragma once
#include "Application/AppStateManager.h"
#include "Application/CallbackMediator.h"
#include "Application/ClientVersionManager.h"
#include "Application/Coordination/VersionSwitchCoordinator.h"
#include "Application/DialogContainer.h"
#include "Application/MapOperationHandler.h"
#include "Application/MapTabManager.h"
#include "Application/PersistenceManager.hpp"
#include "Application/PlatformManager.hpp"
#include "Application/ServiceContainer.h"
#include "Application/SessionLifecycleManager.h"
#include "Application/UIComponentContainer.h"
#include "Brushes/BrushController.h"
#include "Brushes/BrushRegistry.h"
#include "Brushes/BrushSystem.h"
#include "Controllers/HotkeyController.h"
#include "Controllers/MapInputController.h"
#include "Controllers/SearchController.h"
#include "Controllers/SimulationController.h"
#include "Controllers/StartupController.h"
#include "Controllers/WindowController.h"
#include "Controllers/WorkspaceController.h"
#include "Domain/SelectionSettings.h"
#include "Application/Frame/RenderOrchestrator.h"
#include "Rendering/Frame/RenderingManager.h"
#include "Services/SessionWiringService.h"
#include "Services/SettingsRegistry.h"
#include "UI/Widgets/TilesetWidget.h"
#include "UI/Windows/BrowseTile/BrowseTileWindow.h"
#include "UI/Windows/IngameBoxWindow.h"
#include "UI/Windows/MinimapWindow.h"
#include "UI/Windows/PaletteWindowManager.h"
#include <memory>


// Forward declarations to reduce compilation dependencies
namespace MapEditor {
class SessionWiringService;

namespace AppLogic {
class HotkeyController;
class MapInputController;
class MapOperationHandler;
class SearchController;
class SessionLifecycleManager;
class SimulationController;
class ClipboardService;
} // namespace AppLogic

namespace Domain {
class ChunkedMap;
class Position;
} // namespace Domain

namespace Presentation {
class MainWindow;
class MenuBar;
class WorkspaceController;
} // namespace Presentation

namespace Services {
class ClientDataService;
class SpriteManager;
} // namespace Services

namespace UI {
class MapPanel;

namespace Ribbon {
class RibbonController;
class FilePanel;
class ThemePanel;
} // namespace Ribbon
} // namespace UI
} // namespace MapEditor

namespace MapEditor {

/**
 * Main application class - orchestrates components
 * Manages application lifecycle, services, and UI flow
 */
class Application {
public:
  Application();
  ~Application();

  // No copy/move
  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;

  bool initialize();
  int run();
  void quit();

private:
  // === Initialization ===
  bool initializePlatform();
  void initializeServices();
  void initializeLifecycleManagers();
  void initializeUIComponents();
  void wireCallbacks();

  // === Configuration ===
  void shutdown();

  // Main loop
  void processEvents();
  void update();
  void render();

  // State-specific update handlers (registered with AppStateManager)
  void onUpdateStartup();
  void onUpdateEditor();

  // Map loaded callback - renderer is created via
  // RenderingManager::createRenderer()
  void onMapLoaded(std::unique_ptr<Domain::ChunkedMap> map,
                   std::unique_ptr<Services::ClientDataService> client_data,
                   std::unique_ptr<Services::SpriteManager> sprite_manager,
                   const Domain::Position &camera_center);

  // Persistence Manager (Extracted Phase 5)
  PersistenceManager persistence_manager_;

  // Platform Manager (Extracted Phase 4)
  PlatformManager platform_manager_;

  bool should_quit_ = false;

  // Services
  std::unique_ptr<Services::SettingsRegistry> settings_registry_;

  // Client version resources (Phase 4 extraction)
  ClientVersionManager version_manager_;

  // State
  AppStateManager state_manager_;
  AppLogic::MapTabManager tab_manager_;
  CallbackMediator callback_mediator_;
  RenderOrchestrator render_orchestrator_;

  // UI components & Controllers (Phase 6 Extraction)
  UIComponentContainer ui_;

  // Brush system (Phase 2-3)
  std::unique_ptr<Brushes::BrushSystem> brush_system_;

  // Selection & Rendering
  Rendering::RenderingManager rendering_manager_;

  // Dialogs & Controllers (Phase 5 container)
  DialogContainer dialogs_;

  // Lifecycle Management
  std::unique_ptr<AppLogic::SessionLifecycleManager> session_lifecycle_;
  std::unique_ptr<SessionWiringService> session_wiring_;

  // Close All flow tracking (for unsaved changes modal)
  bool pending_close_all_ = false;

  /**
   * Process deferred actions (session destruction) at end of frame.
   * Called after render() to ensure OpenGL resources are destroyed safely.
   */
  void processDeferredActions();

  /**
   * Request closing a tab by index. Defers destruction to
   * processDeferredActions. Safe to call during render loop.
   */
  void requestCloseTab(int index);

  // Helper to create the coordinator
  Coordination::VersionSwitchCoordinator createVersionCoordinator();
};

} // namespace MapEditor
