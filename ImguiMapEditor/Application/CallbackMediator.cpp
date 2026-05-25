#include "CallbackMediator.h"
#include "AppStateManager.h"
#include "Brushes/BrushController.h"
#include "ClientVersionManager.h"
#include "Controllers/HotkeyController.h"
#include "Controllers/MapInputController.h"
#include "MapOperationHandler.h"
#include "MapTabManager.h"
#include "Platform/GlfwWindow.h"
#include "Platform/PlatformCallbackRouter.h"
#include "Presentation/MainWindow.h"
#include "Presentation/MenuBar.h"
#include "Presentation/NotificationHelper.h"
#include "Rendering/Frame/RenderingManager.h"
#include "Rendering/Map/MapRenderer.h"
#include "Services/ClientVersionRegistry.h"
#include "Services/ConfigService.h"
#include "Services/RecentLocationsService.h"
#include "Services/SecondaryClientData.h"
#include "Services/ViewSettings.h"
#include "UI/Dialogs/AdvancedSearchDialog.h"
#include "UI/Dialogs/ConfirmationDialog.h"
#include "UI/Dialogs/EditTownsDialog.h"
#include "UI/Dialogs/Import/ImportMapDialog.h"
#include "UI/Dialogs/Import/ImportMonstersDialog.h"
#include "UI/Dialogs/Properties/MapPropertiesDialog.h"
#include "UI/Dialogs/UnsavedChangesModal.h"
#include "UI/Map/MapPanel.h"
#include "UI/Panels/NewMapPanel.h"
#include "UI/PreferencesDialog.h"
#include "UI/Ribbon/Panels/FilePanel.h"
#include "UI/Widgets/QuickSearchPopup.h"
#include "UI/Widgets/SearchResultsWidget.h"
#include "UI/Windows/BrowseTile/BrowseTileWindow.h"
#include "UI/Windows/IngameBoxWindow.h"
#include "UI/Windows/MinimapWindow.h"
#include <spdlog/spdlog.h>

namespace MapEditor {

void CallbackMediator::wireAll(Context &ctx) {
  wirePlatformCallbacks(ctx);
  wireTabCallbacks(ctx);
  wireMapOperationCallbacks(ctx);
  wireMenuCallbacks(ctx);
  wireSecondaryClientCallbacks(ctx);
  wireRibbonCallbacks(ctx);
  wireCleanupCallbacks(ctx);
  wireSearchCallbacks(ctx);
  wireInputCallbacks(ctx);
  wireMinimapCallbacks(ctx);
  spdlog::info("CallbackMediator: All callbacks wired");
}

void CallbackMediator::wireInputCallbacks(Context &ctx) {
  if (ctx.input_controller) {
    ctx.input_controller->setOpenItemPropertiesCallback(
        [main_window = ctx.main_window](Domain::Item *item) {
          if (main_window) {
            main_window->openPropertiesDialog(item);
          }
        });
    ctx.input_controller->setOpenSpawnPropertiesCallback(
        [main_window = ctx.main_window](Domain::Spawn *spawn,
                                        const Domain::Position &pos) {
          if (main_window) {
            main_window->openSpawnPropertiesDialog(spawn, pos);
          }
        });
    ctx.input_controller->setOpenCreaturePropertiesCallback(
        [main_window = ctx.main_window](Domain::Creature *creature,
                                        const std::string &name,
                                        const Domain::Position &creature_pos) {
          if (main_window) {
            main_window->openCreaturePropertiesDialog(creature, name,
                                                      creature_pos);
          }
        });
  }
}

void CallbackMediator::wireMinimapCallbacks(Context &ctx) {
  if (ctx.minimap) {
    ctx.minimap->setViewportSyncCallback(
        [map_panel = ctx.map_panel](int32_t x, int32_t y, int16_t z) {
          if (map_panel) {
            map_panel->setCameraCenter(x, y, z);
          }
        });
  }
}

void CallbackMediator::wirePlatformCallbacks(Context &ctx) {
  GLFWwindow *native = static_cast<GLFWwindow *>(ctx.window->getNativeHandle());
  ctx.callback_router->initialize(native, ctx.hotkey, [ctx]() {
    return ctx.state_manager->isInState(AppStateManager::State::Editor);
  });
}

void CallbackMediator::wireTabCallbacks(Context &ctx) {
  // Wire MainWindow close callback
  if (ctx.main_window) {
    ctx.main_window->setCloseTabCallback(
        [ctx](int index) { ctx.request_close_tab(index); });

    ctx.main_window->setBrowseTileCallback(
        [ctx](const Domain::Position &pos, uint16_t item_server_id) {
          if (auto *session = ctx.tab_manager->getActiveSession()) {
            session->clearSelection();
            session->getSelectionService().selectTile(session->getMap(), pos);
            ctx.view_settings->show_browse_tile = true;

            // Auto-select the clicked item in Browse Tile widget
            if (ctx.browse_tile && item_server_id > 0) {
              ctx.browse_tile->selectItemByServerId(item_server_id);
            }
          }
        });
  }

  // Hotkey save
  ctx.hotkey->setSaveCallback([ctx]() {
    if (ctx.map_operations)
      ctx.map_operations->handleSaveMap();
  });

  // Hotkey file operations
  // NewMap from Editor state uses standalone dialog
  ctx.hotkey->setNewMapCallback([ctx]() {
    if (ctx.main_window)
      ctx.main_window->showNewMapDialog();
  });
  ctx.hotkey->setOpenMapCallback([ctx]() {
    if (ctx.map_operations)
      ctx.map_operations->handleOpenMap();
  });
  ctx.hotkey->setSaveAsMapCallback([ctx]() {
    if (ctx.map_operations)
      ctx.map_operations->handleSaveAsMap();
  });
  wireCloseMapLogic(ctx, [ctx](std::function<void()> cb) {
      ctx.hotkey->setCloseMapCallback(cb);
  });

  // Hotkey map menu
  ctx.hotkey->setEditTownsCallback([ctx]() {
    auto *session = ctx.tab_manager->getActiveSession();
    if (session && session->getMap()) {
      ctx.edit_towns->setGoToCallback([ctx](const Domain::Position &pos) {
        ctx.map_panel->setCameraCenter(pos.x, pos.y, pos.z);
      });
      ctx.edit_towns->show(session->getMap());
    }
  });
  ctx.hotkey->setMapPropertiesCallback([ctx]() {
    auto *session = ctx.tab_manager->getActiveSession();
    if (session && session->getMap()) {
      ctx.map_properties->show(session->getMap());
    }
  });

  // Tab change callback
  ctx.tab_manager->setTabChangedCallback([ctx](int old_index, int new_index) {
    if (old_index >= 0) {
      if (auto *old_session = ctx.tab_manager->getSession(old_index)) {
        auto &state = old_session->getViewState();
        state.camera_x = ctx.map_panel->getCameraPosition().x;
        state.camera_y = ctx.map_panel->getCameraPosition().y;
        state.zoom = ctx.map_panel->getZoom();
        state.current_floor = ctx.map_panel->getCurrentFloor();
        state.lighting_enabled = ctx.view_settings->map_lighting_enabled;
        state.ambient_light = ctx.view_settings->map_ambient_light;
        state.show_ingame_box = ctx.view_settings->show_ingame_box;
        state.show_minimap = ctx.view_settings->show_minimap_window;

        ctx.minimap->saveState(*old_session);
        ctx.ingame_box->saveState(*old_session);
        ctx.browse_tile->saveState(*old_session);
      }
    }

    auto *session = ctx.tab_manager->getSession(new_index);
    ctx.map_panel->setEditorSession(session);

    if (session) {
      const auto &state = session->getViewState();
      ctx.map_panel->setCameraPosition(state.camera_x, state.camera_y);
      ctx.map_panel->setZoom(state.zoom);
      ctx.map_panel->setCurrentFloor(static_cast<int16_t>(state.current_floor));
      ctx.view_settings->map_lighting_enabled = state.lighting_enabled;
      ctx.view_settings->map_ambient_light = state.ambient_light;
      ctx.view_settings->show_ingame_box = state.show_ingame_box;
      ctx.view_settings->show_minimap_window = state.show_minimap;
      ctx.minimap->setMap(session->getMap(),
                          ctx.version_manager->getClientData());

      ctx.minimap->restoreState(*session);
      ctx.ingame_box->restoreState(*session);
      ctx.browse_tile->restoreState(*session);

      ctx.browse_tile->setMap(session->getMap(),
                              ctx.version_manager->getClientData(),
                              ctx.version_manager->getSpriteManager());
      ctx.browse_tile->setSelection(&session->getSelectionService());
      ctx.browse_tile->setSession(session);

      // Initialize brush controller with session's map and history manager
      if (ctx.brush_controller) {
        ctx.brush_controller->initialize(session->getMap(),
                                         &session->getHistoryManager(),
                                         ctx.version_manager->getClientData());
      }
    }
  });

  // Session modification callback
  // Session modification callback
  ctx.tab_manager->setSessionModifiedCallback([ctx](bool modified) {
    if (auto *session = ctx.tab_manager->getActiveSession()) {
      if (ctx.rendering_manager) {
        if (auto *state =
                ctx.rendering_manager->getRenderState(session->getID())) {
          state->invalidateAll();
        }
      }
    }
  });
}

void CallbackMediator::wireMapOperationCallbacks(Context &ctx) {
  if (!ctx.map_operations)
    return;

  // Wire map loaded callback
  ctx.map_operations->setMapLoadedCallback([ctx](auto map, auto client_data,
                                                 auto sprite_manager,
                                                 const auto &center) {
    if (ctx.on_map_loaded) {
      ctx.on_map_loaded(std::move(map), std::move(client_data),
                        std::move(sprite_manager), center);
    }
  });

  // Wire notification callback
  ctx.map_operations->setNotificationCallback(
      [ctx](auto type, const std::string &message) {
        if (ctx.on_notification) {
          ctx.on_notification(static_cast<int>(type), message);
        }
      });

  // Wire MainWindow dialog callbacks to MapOperationHandler
  if (ctx.main_window) {
    ctx.main_window->setNewMapCallback([ctx](const UI::NewMapPanel::State& config) {
      ctx.map_operations->handleNewMapDirect(config.map_name, config.map_width,
                                             config.map_height, config.selected_version);
    });
    ctx.main_window->setOpenSecMapCallback([ctx](const std::filesystem::path& folder, uint32_t version) {
      ctx.map_operations->handleOpenSecMapDirect(folder, version);
    });
  }
}

void CallbackMediator::wireMenuCallbacks(Context &ctx) {
  // NewMap from Editor state uses standalone dialog
  ctx.menu_bar->setNewMapCallback(
      [ctx]() { if (ctx.main_window) ctx.main_window->showNewMapDialog(); });
  ctx.menu_bar->setOpenMapCallback(
      [ctx]() { ctx.map_operations->handleOpenMap(); });
  ctx.menu_bar->setOpenSecMapCallback(
      [ctx]() { if (ctx.main_window) ctx.main_window->showOpenSecDialog(); });
  ctx.menu_bar->setSaveMapCallback(
      [ctx]() { ctx.map_operations->handleSaveMap(); });
  ctx.menu_bar->setSaveAsMapCallback(
      [ctx]() { ctx.map_operations->handleSaveAsMap(); });

  wireCloseMapLogic(ctx, [ctx](std::function<void()> cb) {
      ctx.menu_bar->setCloseMapCallback(cb);
  });

  ctx.menu_bar->setImportMapCallback([ctx]() { ctx.import_map->show(); });
  ctx.menu_bar->setImportMonstersCallback(
      [ctx]() { ctx.import_monsters->show(); });
  ctx.menu_bar->setPreferencesCallback([ctx]() { ctx.preferences->show(); });
  ctx.menu_bar->setCloseAllMapsCallback(
      [ctx]() { ctx.change_version_callback(); });

  ctx.menu_bar->setQuitCallback([ctx]() { ctx.quit_callback(); });

  // Recent files
  ctx.menu_bar->setRecentFilesService(ctx.recent);
  ctx.menu_bar->setOpenRecentCallback([ctx](const std::filesystem::path &path) {
    ctx.map_operations->handleOpenRecentMap(path.string(), 0);
  });

  // Map menu
  ctx.menu_bar->setEditTownsCallback([ctx]() {
    auto *session = ctx.tab_manager->getActiveSession();
    if (session && session->getMap()) {
      ctx.edit_towns->setGoToCallback([ctx](const Domain::Position &pos) {
        ctx.map_panel->setCameraCenter(pos.x, pos.y, pos.z);
      });
      ctx.edit_towns->setPickPositionCallback([ctx]() -> bool {
        Presentation::showInfo("Click on map to select temple position", 2000);
        return true;
      });
      ctx.edit_towns->show(session->getMap());
    }
  });

  ctx.menu_bar->setMapPropertiesCallback([ctx]() {
    auto *session = ctx.tab_manager->getActiveSession();
    if (session && session->getMap()) {
      ctx.map_properties->show(session->getMap());
    }
  });

  // ID conversion callbacks
  ctx.menu_bar->setConvertToServerIdCallback([ctx]() {
    if (ctx.map_operations)
      ctx.map_operations->handleConvertToServerId();
  });
  ctx.menu_bar->setConvertToClientIdCallback([ctx]() {
    if (ctx.map_operations)
      ctx.map_operations->handleConvertToClientId();
  });
}

void CallbackMediator::wireSecondaryClientCallbacks(Context &ctx) {
  ctx.preferences->setLoadSecondaryCallback(
      [ctx](const std::filesystem::path &folder_path) -> bool {
        auto secondary = std::make_unique<Services::SecondaryClientData>();
        auto result = secondary->loadFromFolder(folder_path, *ctx.versions);
        if (!result.success) {
          spdlog::error("Failed to load secondary client: {}", result.error);
          ctx.preferences->setSecondaryClientProvider(nullptr);
          return false;
        }

        ctx.version_manager->setSecondaryClient(std::move(secondary));

        // Create provider lambda that queries ClientVersionManager for current
        // secondary client
        auto secondary_provider = [ctx]() -> Services::SecondaryClientData * {
          return ctx.version_manager->getSecondaryClient();
        };
        ctx.preferences->setSecondaryClientProvider(secondary_provider);

        if (ctx.version_manager->getSpriteManager()) {
          auto spr_reader_provider = [ctx]() -> IO::SprReader * {
            auto *sec = ctx.version_manager->getSecondaryClient();
            return sec ? sec->getSpriteReader() : nullptr;
          };
          ctx.version_manager->getSpriteManager()
              ->setSecondarySpriteReaderProvider(spr_reader_provider);
        }

        if (ctx.rendering_manager && ctx.rendering_manager->getRenderer()) {
          ctx.rendering_manager->getRenderer()
              ->getTileRenderer()
              .setSecondaryClientProvider(secondary_provider);
          ctx.rendering_manager->invalidateCache();
        }

        spdlog::info("Secondary client v{} loaded: {} items",
                     result.client_version, result.item_count);
        Presentation::showSuccess(
            "Secondary client v" + std::to_string(result.client_version / 100) +
            "." + (result.client_version % 100 < 10 ? "0" : "") +
            std::to_string(result.client_version % 100) + " loaded (" +
            std::to_string(result.item_count) + " items)");
        return true;
      });

  ctx.preferences->setUnloadSecondaryCallback([ctx]() {
    if (ctx.version_manager->hasSecondaryClient()) {
      ctx.version_manager->clearSecondaryClient();

      // Clear providers by setting empty providers
      ctx.preferences->setSecondaryClientProvider(nullptr);

      if (ctx.version_manager->getSpriteManager()) {
        ctx.version_manager->getSpriteManager()
            ->setSecondarySpriteReaderProvider(nullptr);
      }
      if (ctx.rendering_manager && ctx.rendering_manager->getRenderer()) {
        ctx.rendering_manager->getRenderer()
            ->getTileRenderer()
            .setSecondaryClientProvider(nullptr);
        ctx.rendering_manager->invalidateCache();
      }

      spdlog::info("Secondary client unloaded");
      Presentation::showInfo("Secondary client unloaded", 2000);
    }
  });

  ctx.preferences->setToggleSecondaryCallback([ctx](bool active) {
    if (ctx.version_manager->hasSecondaryClient()) {
      ctx.version_manager->getSecondaryClient()->setActive(active);
      if (ctx.rendering_manager) {
        ctx.rendering_manager->invalidateCache();
      }
      spdlog::info("Secondary client {}", active ? "activated" : "deactivated");
    }
  });

  // Set up initial provider for secondary client (may be null initially)
  ctx.preferences->setSecondaryClientProvider(
      [ctx]() -> Services::SecondaryClientData * {
        return ctx.version_manager->getSecondaryClient();
      });
}

void CallbackMediator::wireRibbonCallbacks(Context &ctx) {
  if (ctx.file_panel) {
    // NewMap from Editor state uses standalone dialog  
    ctx.file_panel->SetNewMapCallback(
        [ctx]() { if (ctx.main_window) ctx.main_window->showNewMapDialog(); });
    ctx.file_panel->SetOpenMapCallback(
        [ctx]() { ctx.map_operations->handleOpenMap(); });
    ctx.file_panel->SetSaveMapCallback(
        [ctx]() { ctx.map_operations->handleSaveMap(); });
    ctx.file_panel->SetSaveAsMapCallback(
        [ctx]() { ctx.map_operations->handleSaveAsMap(); });

    wireCloseMapLogic(ctx, [ctx](std::function<void()> cb) {
        ctx.file_panel->SetCloseMapCallback(cb);
    });

    // Check modified state for visual indicator
    ctx.file_panel->SetCheckModifiedCallback([ctx]() -> bool {
        if (!ctx.tab_manager) return false;
        auto* session = ctx.tab_manager->getActiveSession();
        return session && session->isModified();
    });

    // Check loading state
    ctx.file_panel->SetCheckLoadingCallback([ctx]() -> bool {
        return ctx.map_operations && ctx.map_operations->isLoading();
    });
  }
}

void CallbackMediator::wireCleanupCallbacks(Context &ctx) {
  ctx.menu_bar->setCleanInvalidItemsCallback([ctx]() {
    auto *session = ctx.tab_manager->getActiveSession();
    if (session && session->getMap() && ctx.version_manager->hasClientData()) {
      ctx.trigger_invalid_items_cleanup();
    }
  });

  ctx.menu_bar->setCleanHouseItemsCallback([ctx]() {
    auto *session = ctx.tab_manager->getActiveSession();
    if (session && session->getMap() && ctx.version_manager->hasClientData()) {
      ctx.trigger_house_items_cleanup();
    }
  });
}

void CallbackMediator::wireSearchCallbacks(Context &ctx) {
  // Wire Ctrl+F quick search callback
  if (ctx.hotkey && ctx.quick_search) {
    ctx.hotkey->setQuickSearchCallback(
        [qs = ctx.quick_search]() { qs->open(); });
  }

  // Wire Ctrl+Shift+F to toggle Search Results Widget (not Advanced Search)
  if (ctx.hotkey && ctx.view_settings) {
    ctx.hotkey->setAdvancedSearchCallback(
        [vs = ctx.view_settings]() { vs->show_search_results = true; });
  }

  // Wire QuickSearch selection callback
  if (ctx.quick_search) {
    ctx.quick_search->setSelectCallback(
        [](uint16_t server_id, bool is_creature) {
          spdlog::info("QuickSearch selected: {} (ID: {})",
                       is_creature ? "creature" : "item", server_id);
          Presentation::showInfo(std::string("Selected ") +
                                     (is_creature ? "creature" : "item") +
                                     " ID: " + std::to_string(server_id),
                                 2000);
        });
  }

  // Wire SearchResultsWidget navigate callback
  if (ctx.search_results && ctx.map_panel) {
    ctx.search_results->setNavigateCallback(
        [mp = ctx.map_panel](const Domain::Position &pos) {
          mp->setCameraCenter(pos.x, pos.y, pos.z);
        });
  }

  // Wire SearchResultsWidget's Advanced Search button
  if (ctx.search_results && ctx.advanced_search) {
    ctx.search_results->setOpenAdvancedSearchCallback(
        [ads = ctx.advanced_search]() { ads->open(); });
  }
}

} // namespace MapEditor
