#include "SessionWiringService.h"
#include "Application/ClientVersionManager.h"
#include "Domain/ChunkedMap.h"
#include "Application/MapOperationHandler.h"
#include "Application/MapTabManager.h"
#include "Rendering/Frame/RenderingManager.h"
#include "Rendering/Map/MapRenderer.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "Services/ViewSettings.h"
#include <spdlog/spdlog.h>

namespace MapEditor {

AppLogic::EditorSession *SessionWiringService::wireResources(
    std::unique_ptr<Domain::ChunkedMap> map,
    std::unique_ptr<Services::ClientDataService> client_data,
    std::unique_ptr<Services::SpriteManager> sprite_manager,
    const std::filesystem::path &map_path,
    AppLogic::MapOperationHandler *map_operations) const {

  // Architecture trace: show what SessionWiringService receives
  spdlog::info("[SessionWiringService] wireResources() called with:");
  spdlog::info("  - map: {} (tiles: {})", map ? "valid" : "null",
               map ? map->getTileCount() : 0);
  spdlog::info("  - client_data: {}",
               client_data ? "valid (OWNERSHIP TRANSFER)" : "null");
  spdlog::info("  - sprite_manager: {}",
               sprite_manager ? "valid (OWNERSHIP TRANSFER)" : "null");
  spdlog::info("  - map_path: {}", map_path.string());

  // Step 1: Transfer client data ownership BEFORE opening map
  // so sessions get valid client_data for lighting
  if (client_data) {
    spdlog::info("[SessionWiringService] Step 1: Transferring client_data to "
                 "ClientVersionManager");
    ctx_.version_manager->setClientData(std::move(client_data));
    ctx_.tab_manager->setClientData(ctx_.version_manager->getClientData());
  }

  // Ensure TabManager checks RenderingManager
  ctx_.tab_manager->setRenderingManager(ctx_.rendering_manager);

  // Step 2: Open map in tab manager
  spdlog::info("[SessionWiringService] Step 2: Opening map in MapTabManager");
  ctx_.tab_manager->openMap(std::move(map), map_path);

  // Step 3: Create renderer using factory and transfer sprite manager ownership
  if (sprite_manager) {
    spdlog::info("[SessionWiringService] Step 3: Transferring sprite_manager "
                 "to ClientVersionManager");
    ctx_.version_manager->setSpriteManager(std::move(sprite_manager));

    // Create renderer using factory pattern - this is the key DI fix!
    spdlog::info("[SessionWiringService] Step 4: Creating renderer via "
                 "RenderingManager::createRenderer() FACTORY");
    spdlog::info("  - Passing client_data: {}",
                 ctx_.version_manager->getClientData() ? "valid" : "null");
    spdlog::info("  - Passing sprite_manager: {}",
                 ctx_.version_manager->getSpriteManager() ? "valid" : "null");

    auto renderer = ctx_.rendering_manager->createRenderer(
        ctx_.version_manager->getClientData(),
        ctx_.version_manager->getSpriteManager());
    renderer->setViewSettings(ctx_.view_settings);

    spdlog::info(
        "[SessionWiringService] Step 5: Setting renderer on RenderingManager");
    ctx_.rendering_manager->setRenderer(
        std::move(renderer),
        ctx_.version_manager->getSpriteManager() // Wires callback internally
    );

    // Step 6: Update MapOperationHandler resources
    if (map_operations) {
      spdlog::info("[SessionWiringService] Step 6: Updating "
                   "MapOperationHandler existing resources");
      map_operations->setExistingResources(
          ctx_.version_manager->getClientData(),
          ctx_.version_manager->getSpriteManager());
    }
  } else {
    // Second map load - reusing existing client_data/sprite_manager/renderer
    spdlog::info(
        "[SessionWiringService] REUSING existing resources (second map):");
    spdlog::info("  - Existing client_data: {}",
                 ctx_.version_manager->getClientData() ? "valid" : "null");
    spdlog::info("  - Existing sprite_manager: {}",
                 ctx_.version_manager->getSpriteManager() ? "valid" : "null");
    spdlog::info("  - Existing renderer: {}",
                 ctx_.rendering_manager->getRenderer() ? "valid" : "null");
  }

  spdlog::info("[SessionWiringService] wireResources() complete - returning "
               "active session");
  return ctx_.tab_manager->getActiveSession();
}

} // namespace MapEditor
