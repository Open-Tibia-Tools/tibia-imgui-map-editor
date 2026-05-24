#pragma once

#include <filesystem>
#include <memory>

// Forward declarations
namespace MapEditor {
class ClientVersionManager;
namespace Rendering {
class RenderingManager;
}
namespace AppLogic {
class MapTabManager;
class MapOperationHandler;
class EditorSession;
} // namespace AppLogic
namespace Domain {
class ChunkedMap;
} // namespace Domain
namespace Services {
class ClientDataService;
class SpriteManager;
class ViewSettings;
} // namespace Services
} // namespace MapEditor

namespace MapEditor {

/**
 * Wires newly loaded map resources to the appropriate managers.
 * Separates resource ownership transfer from UI binding.
 */
class SessionWiringService {
public:
  struct Context {
    ClientVersionManager *version_manager = nullptr;
    Rendering::RenderingManager *rendering_manager = nullptr;
    AppLogic::MapTabManager *tab_manager = nullptr;
    Services::ViewSettings *view_settings = nullptr;
  };

  explicit SessionWiringService(Context ctx) : ctx_(ctx) {}

  /**
   * Wire resources from a newly loaded map.
   * Transfers ownership to appropriate managers.
   * Creates renderer via RenderingManager::createRenderer() factory.
   * @returns the active EditorSession after opening the map
   */
  AppLogic::EditorSession *
  wireResources(std::unique_ptr<Domain::ChunkedMap> map,
                std::unique_ptr<Services::ClientDataService> client_data,
                std::unique_ptr<Services::SpriteManager> sprite_manager,
                const std::filesystem::path &map_path,
                AppLogic::MapOperationHandler *map_operations) const;

private:
  Context ctx_;
};

} // namespace MapEditor
