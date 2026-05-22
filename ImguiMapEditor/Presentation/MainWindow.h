#pragma once
#include "Application/MapTabManager.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Position.h"
#include "MenuBar.h"
#include "Rendering/Map/MapRenderer.h"
#include "Services/ViewSettings.h"
#include "UI/Dialogs/Properties/CreaturePropertiesDialog.h"
#include "UI/Dialogs/Properties/ItemPropertiesDialog.h"
#include "UI/Dialogs/Properties/SpawnPropertiesDialog.h"
#include "UI/Dialogs/NewMapDialog.h"
#include "UI/Dialogs/OpenSecDialog.h"
#include "UI/Map/MapContextMenu.h"
#include "UI/Map/MapPanel.h"
#include "UI/Windows/IngameBoxWindow.h"
#include <functional>
#include <memory>
#include <optional>

namespace MapEditor::Rendering {
class MapRenderer;
struct AnimationTicks;
class RenderingManager;
} // namespace MapEditor::Rendering

namespace MapEditor {
namespace AppLogic {
class ClipboardService;
}
namespace Services {
class ClientVersionRegistry;
}
namespace Presentation {

/**
 * Main window UI orchestrator - Editor mode only.
 * 
 * StartupDialog handles Startup state directly.
 * MainWindow is now focused solely on the map editing interface.
 */
class MainWindow {
public:
  MainWindow(Services::ViewSettings &view_settings,
             Services::ClientVersionRegistry &version_registry,
             UI::MapPanel &map_panel, UI::IngameBoxWindow &ingame_box_window,
             MenuBar &menu_bar, AppLogic::MapTabManager *tab_manager = nullptr);

  ~MainWindow() = default;

  // Non-copyable
  MainWindow(const MainWindow &) = delete;
  MainWindow &operator=(const MainWindow &) = delete;

  /**
   * Render the editor UI.
   * Called only when in Editor state.
   */
  void renderEditor(Domain::ChunkedMap *current_map,
                    Rendering::RenderingManager *rendering_manager,
                    const Rendering::AnimationTicks *anim_ticks = nullptr);

  /**
   * Set clipboard service for context menu operations.
   */
  void setClipboardService(AppLogic::ClipboardService *clipboard) {
    clipboard_ = clipboard;
  }

  /**
   * Open the properties dialog for a specific item.
   */
  void openPropertiesDialog(Domain::Item *item);
  void openSpawnPropertiesDialog(Domain::Spawn *spawn,
                                 const Domain::Position &pos);
  void openCreaturePropertiesDialog(Domain::Creature *creature,
                                    const std::string &name,
                                    const Domain::Position &creature_pos);

  void setCloseTabCallback(std::function<void(int)> callback) {
    on_close_requested_ = std::move(callback);
  }

  void setBrowseTileCallback(
      std::function<void(const Domain::Position &, uint16_t)> callback) {
    context_menu_.setBrowseTileCallback(std::move(callback));
  }

  void setSelectBrushCallback(UI::MapContextMenu::SelectBrushCallback callback) {
    context_menu_.setSelectBrushCallback(std::move(callback));
  }

  void setRotateItemCallback(UI::MapContextMenu::RotateItemCallback callback) {
    context_menu_.setRotateItemCallback(std::move(callback));
  }

  void setCanRotateItemCallback(
      UI::MapContextMenu::CanRotateItemCallback callback) {
    context_menu_.setCanRotateItemCallback(std::move(callback));
  }

  void setSwitchDoorCallback(UI::MapContextMenu::SwitchDoorCallback callback) {
    context_menu_.setSwitchDoorCallback(std::move(callback));
  }

  void setCanSwitchDoorCallback(
      UI::MapContextMenu::CanSwitchDoorCallback callback) {
    context_menu_.setCanSwitchDoorCallback(std::move(callback));
  }

  void setDoorStateCallback(UI::MapContextMenu::DoorStateCallback callback) {
    context_menu_.setDoorStateCallback(std::move(callback));
  }

  void setSpawnPropertiesCallback(
      UI::MapContextMenu::SpawnPropertiesCallback callback) {
    context_menu_.setSpawnPropertiesCallback(std::move(callback));
  }

  void setCreaturePropertiesCallback(
      UI::MapContextMenu::CreaturePropertiesCallback callback) {
    context_menu_.setCreaturePropertiesCallback(std::move(callback));
  }

  void setCanSelectBrushCallback(
      UI::MapContextMenu::CanSelectBrushCallback callback) {
    context_menu_.setCanSelectBrushCallback(std::move(callback));
  }

  // === Editor-state modal dialogs ===
  void showNewMapDialog();
  void showOpenSecDialog();
  
  void setNewMapCallback(std::function<void(const UI::NewMapPanel::State&)> callback) {
    new_map_callback_ = std::move(callback);
  }
  void setOpenSecMapCallback(std::function<void(const std::filesystem::path&, uint32_t)> callback) {
    open_sec_callback_ = std::move(callback);
  }

private:
  std::function<void(int)> on_close_requested_;

  Services::ViewSettings &view_settings_;
  Services::ClientVersionRegistry &version_registry_;  // Constructor-injected reference
  UI::MapPanel &map_panel_;
  UI::IngameBoxWindow &ingame_box_window_;
  MenuBar &menu_bar_;
  AppLogic::MapTabManager *tab_manager_ = nullptr;
  AppLogic::ClipboardService *clipboard_ = nullptr;

  // Context menu for right-click actions
  UI::MapContextMenu context_menu_;

  // Item properties dialog
  UI::ItemPropertiesDialog properties_dialog_;
  UI::SpawnPropertiesDialog spawn_properties_dialog_;
  UI::CreaturePropertiesDialog creature_properties_dialog_;
  
  // Editor-state modal dialogs
  UI::NewMapDialog new_map_dialog_;
  UI::OpenSecDialog open_sec_dialog_;
  std::function<void(const UI::NewMapPanel::State&)> new_map_callback_;
  std::function<void(const std::filesystem::path&, uint32_t)> open_sec_callback_;
};

} // namespace Presentation
} // namespace MapEditor
