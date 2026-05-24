#pragma once
#include "Domain/Position.h"
#include "UI/Widgets/Properties/PropertyPanelRenderer.h"
#include <cstdint>
#include <memory>

// Forward declarations for new renderers
namespace MapEditor::UI::BrowseTile {
class ItemsListRenderer;
class SpawnCreatureRenderer;
} // namespace MapEditor::UI::BrowseTile

namespace MapEditor {
namespace Services::Selection {
class SelectionService;
}

namespace Domain {
class Tile;
class Item;
class ChunkedMap;
} // namespace Domain

namespace Services {
class ClientDataService;
class SpriteManager;
} // namespace Services

namespace AppLogic {
class EditorSession;
}

namespace UI {

/**
 * Dockable widget to browse items on the selected tile.
 *
 * Features:
 * - Two-column layout: items list (left), tile properties (right)
 * - Shows ground item + stacked items
 * - Displays tile flags, position, house/spawn associations
 * - Prepared for future properties panel integration
 */
class BrowseTileWindow {
public:
  BrowseTileWindow();
  ~BrowseTileWindow();

  /**
   * Set map and client data sources (called on tab switch)
   */
  void setMap(Domain::ChunkedMap *map, Services::ClientDataService *clientData,
              Services::SpriteManager *spriteManager);

  /**
   * Set editor session for item operations (undo/redo)
   */
  void setSession(AppLogic::EditorSession *session);

  /**
   * Set current selection (called when selection changes)
   */
  void setSelection(const Services::Selection::SelectionService *selection);

  /**
   * Render the window
   * @param p_visible Optional visibility flag for ImGui to manage
   */
  void render(bool *p_visible = nullptr);

  // Window visibility
  bool isVisible() const { return visible_; }
  void setVisible(bool visible) { visible_ = visible; }
  void toggleVisible() { visible_ = !visible_; }

  // Session state persistence
  void saveState(AppLogic::EditorSession &session) const;
  void restoreState(const AppLogic::EditorSession &session);

  // Programmatic selection (for syncing with map viewport clicks)
  void selectItemByServerId(uint16_t server_id);
  void selectSpawn();
  void selectCreature();

private:
  void renderTileProperties();
  void refreshFromSelection();
  void ensureRenderersInitialized(); // DRY helper for renderer init

  Domain::ChunkedMap *map_ = nullptr;                  // Non-owning
  Services::ClientDataService *client_data_ = nullptr; // Non-owning
  Services::SpriteManager *sprite_manager_ = nullptr;  // Non-owning
  const Services::Selection::SelectionService *selection_ =
      nullptr; // Non-owning

  // Cached tile data (refreshed when selection changes)
  Domain::Position current_pos_;
  const Domain::Tile *current_tile_ = nullptr;

  // Item selection state
  int selected_index_ = -1;     // Index in display list, -1 = none, 0 = ground
  bool spawn_selected_ = false; // True if spawn row selected
  bool creature_selected_ = false; // True if creature row selected

  // Editor session for undo/redo actions
  AppLogic::EditorSession *session_ = nullptr;

  // Property panel renderer for right column
  Properties::PropertyPanelRenderer property_renderer_;

  // Delegated Renderers
  std::unique_ptr<BrowseTile::ItemsListRenderer> items_list_renderer_;
  std::unique_ptr<BrowseTile::SpawnCreatureRenderer> spawn_creature_renderer_;

  // Helper to get the currently selected item
  Domain::Item *getSelectedItem();

  bool visible_ = true;
};

} // namespace UI
} // namespace MapEditor
