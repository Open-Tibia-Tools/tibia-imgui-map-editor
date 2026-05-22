#include "UIFactory.h"
#include "Application/MapOperationHandler.h"
#include "Application/MapTabManager.h"
#include "Brushes/BrushController.h"
#include "Controllers/HotkeyController.h"
#include "Controllers/MapInputController.h"
#include "Controllers/SearchController.h"
#include "Controllers/SimulationController.h"
#include "Controllers/StartupController.h"
#include "Controllers/WorkspaceController.h"
#include "Presentation/MainWindow.h"
#include "Presentation/MenuBar.h"
#include "Services/AppSettings.h"
#include "Services/ClientVersionRegistry.h"
#include "Services/ClipboardService.h"
#include "Services/ConfigService.h"
#include "Services/HotkeyRegistry.h"
#include "Services/RecentLocationsService.h"
#include "Services/TilesetService.h"
#include "Services/ViewSettings.h"
#include "UI/Dialogs/Startup/StartupDialog.h"
#include "UI/Map/MapPanel.h"
#include "UI/Ribbon/Panels/BrushesPanel.h"
#include "UI/Ribbon/Panels/EditPanel.h"
#include "UI/Ribbon/Panels/FilePanel.h"
#include "UI/Ribbon/Panels/PalettesPanel.h"
#include "UI/Ribbon/Panels/SelectionPanel.h"
#include "UI/Ribbon/Panels/ThemePanel.h"
#include "UI/Ribbon/Panels/ViewPanel.h"
#include "UI/Ribbon/RibbonController.h"
#include "UI/Windows/BrowseTile/BrowseTileWindow.h"
#include "UI/Windows/IngameBoxWindow.h"
#include "UI/Windows/MinimapWindow.h"
#include "UI/Windows/PaletteWindowManager.h"


namespace MapEditor {

UIComponentContainer UIFactory::create(const UIFactoryContext &ctx) {
  UIComponentContainer components;

  // Create StartupDialog
  components.startup_dialog = std::make_unique<UI::StartupDialog>();
  components.startup_dialog->initialize(&ctx.version_registry, &ctx.config);

  components.map_panel = std::make_unique<UI::MapPanel>();
  components.map_panel->setViewSettings(ctx.view_settings);

  // Phase 7: Create Windows inside Factory
  components.ingame_box_window = std::make_unique<UI::IngameBoxWindow>();
  components.minimap_window = std::make_unique<UI::MinimapWindow>();
  components.browse_tile_window = std::make_unique<UI::BrowseTileWindow>();

  components.hotkey_controller = std::make_unique<AppLogic::HotkeyController>(
      ctx.hotkey_registry, ctx.view_settings, components.map_panel.get(),
      *components.ingame_box_window, ctx.tab_manager);
  components.hotkey_controller->setBrushController(&ctx.brush_controller);

  components.menu_bar = std::make_unique<Presentation::MenuBar>(
      ctx.view_settings, ctx.selection_settings, components.map_panel.get(),
      &ctx.tab_manager);
  components.menu_bar->setThemePtr(&ctx.app_settings.theme);

  components.input_controller = std::make_unique<AppLogic::MapInputController>(
      ctx.selection_settings, nullptr);
  components.input_controller->setBrushController(&ctx.brush_controller);

  // Wire map panel to input controller immediately as they are created together
  components.map_panel->setInputController(components.input_controller.get());
  components.map_panel->setSelectionSettings(&ctx.selection_settings);
  components.map_panel->setBrushController(&ctx.brush_controller);

  components.simulation_controller =
      std::make_unique<AppLogic::SimulationController>(ctx.view_settings);

  components.clipboard_service = std::make_unique<AppLogic::ClipboardService>(
      ctx.tab_manager.getCopyBuffer());

  // Initialize MapOperationHandler (no longer needs ProjectConfigDialog)
  components.map_operations = std::make_unique<AppLogic::MapOperationHandler>(
      ctx.config, ctx.version_registry, ctx.recent_locations, ctx.view_settings,
      ctx.tab_manager, ctx.brush_registry, ctx.tileset_service);

  // Initialize StartupController
  components.startup_controller = std::make_unique<AppLogic::StartupController>(
      *components.startup_dialog, *components.map_operations, ctx.config,
      ctx.version_registry, ctx.recent_locations, ctx.state_manager);

  // Initialize Ribbon UI
  components.ribbon_controller =
      std::make_unique<UI::Ribbon::RibbonController>();

  // Create and add ribbon panels
  auto file_panel = std::make_unique<UI::Ribbon::FilePanel>();
  components.file_panel_ptr = file_panel.get(); // Store for return

  auto edit_panel = std::make_unique<UI::Ribbon::EditPanel>(&ctx.tab_manager);
  auto view_panel = std::make_unique<UI::Ribbon::ViewPanel>(
      ctx.view_settings, components.map_panel.get());

  auto theme_panel = std::make_unique<UI::Ribbon::ThemePanel>();
  components.theme_panel_ptr = theme_panel.get();
  components.theme_panel_ptr->setThemePtr(&ctx.app_settings.theme);

  auto selection_panel = std::make_unique<UI::Ribbon::SelectionPanel>(
      ctx.selection_settings, &ctx.tab_manager);
  auto brushes_panel = std::make_unique<UI::Ribbon::BrushesPanel>(
      &ctx.brush_controller, ctx.brush_controller.getBrushSettingsService());

  // Create PaletteWindowManager (initialized later when services available)
  components.palette_window_manager =
      std::make_unique<UI::PaletteWindowManager>();
  components.palette_window_manager->setAppSettings(&ctx.app_settings);

  // Create PalettesPanel with window manager and app settings
  auto palettes_panel = std::make_unique<UI::Ribbon::PalettesPanel>(
      components.palette_window_manager.get(),
      ctx.tileset_service.getPaletteRegistry(), &ctx.app_settings);

  components.ribbon_controller->AddPanel(std::move(file_panel));
  components.ribbon_controller->AddPanel(std::move(edit_panel));
  components.ribbon_controller->AddPanel(std::move(view_panel));
  components.ribbon_controller->AddPanel(std::move(theme_panel));
  components.ribbon_controller->AddPanel(std::move(selection_panel));
  components.ribbon_controller->AddPanel(std::move(brushes_panel));
  components.ribbon_controller->AddPanel(std::move(palettes_panel));

  components.search_controller = std::make_unique<AppLogic::SearchController>();

  components.main_window = std::make_unique<Presentation::MainWindow>(
      ctx.view_settings, ctx.version_registry, *components.map_panel,
      *components.ingame_box_window, *components.menu_bar, &ctx.tab_manager);

  components.main_window->setClipboardService(
      components.clipboard_service.get());

  // Initialize WorkspaceController (Extracted from Application.cpp)
  components.workspace_controller =
      std::make_unique<Presentation::WorkspaceController>(
          *components.map_panel, *components.minimap_window,
          *components.browse_tile_window, ctx.tileset_widget,
          components.palette_window_manager.get(), ctx.brush_controller,
          *components.search_controller, *components.input_controller);

  return components;
}

} // namespace MapEditor
