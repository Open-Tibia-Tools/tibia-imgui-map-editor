#include "BrowseTileWindow.h"
#include "Application/EditorSession.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Creature.h"
#include "Domain/CreatureType.h"
#include "Domain/History/HistoryManager.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Spawn.h"
#include "Domain/Tile.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "Rendering/Core/Texture.h"
#include "Rendering/Tile/CreatureSpriteHelper.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include "UI/Windows/BrowseTile/ItemsListRenderer.h"
#include "UI/Windows/BrowseTile/SpawnCreatureRenderer.h"
#include <cmath>
#include <imgui.h>
#include <map>
#include <spdlog/spdlog.h>

namespace MapEditor::UI {

BrowseTileWindow::BrowseTileWindow() = default;

BrowseTileWindow::~BrowseTileWindow() = default;

void BrowseTileWindow::setMap(Domain::ChunkedMap *map,
                              Services::ClientDataService *clientData,
                              Services::SpriteManager *spriteManager) {
  map_ = map;
  client_data_ = clientData;
  sprite_manager_ = spriteManager;

  // Clear selection if map changed
  if (!map) {
    setSelection(nullptr);
  }

  ensureRenderersInitialized();
}

void BrowseTileWindow::setSession(AppLogic::EditorSession *session) {
  session_ = session;
  ensureRenderersInitialized();
}

void BrowseTileWindow::setSelection(const Services::Selection::SelectionService *selection) {
  selection_ = selection;
}

void BrowseTileWindow::saveState(AppLogic::EditorSession &session) {
  session.getBrowseTileState().is_open = visible_;
}

void BrowseTileWindow::restoreState(const AppLogic::EditorSession &session) {
  visible_ = session.getBrowseTileState().is_open;
}

void BrowseTileWindow::selectItemByServerId(uint16_t server_id) {
  if (!current_tile_)
    return;

  int display_index = 0;

  // Check ground
  if (current_tile_->hasGround()) {
    if (current_tile_->getGround()->getServerId() == server_id) {
      selected_index_ = display_index;
      spawn_selected_ = false;
      creature_selected_ = false;
      return;
    }
    display_index++;
  }

  // Check stacked items
  for (const auto &item_ptr : current_tile_->getItems()) {
    if (item_ptr && item_ptr->getServerId() == server_id) {
      selected_index_ = display_index;
      spawn_selected_ = false;
      creature_selected_ = false;
      return;
    }
    display_index++;
  }
}

void BrowseTileWindow::selectSpawn() {
  if (!current_tile_ || !current_tile_->hasSpawn())
    return;
  spawn_selected_ = true;
  creature_selected_ = false;
  selected_index_ = -1;
}

void BrowseTileWindow::selectCreature() {
  if (!current_tile_ || !current_tile_->hasCreature())
    return;
  creature_selected_ = true;
  spawn_selected_ = false;
  selected_index_ = -1;
}

void BrowseTileWindow::refreshFromSelection() {
  current_tile_ = nullptr;
  current_pos_ = {};

  if (!selection_ || !map_) {
    return;
  }

  // Only show if exactly one tile is selected
  auto positions = selection_->getPositions();
  if (positions.size() != 1) {
    return;
  }

  current_pos_ = positions[0];
  current_tile_ = map_->getTile(current_pos_);
}

Domain::Item *BrowseTileWindow::getSelectedItem() {
  if (!current_tile_ || selected_index_ < 0 || !map_)
    return nullptr;

  // Get mutable tile instead of using const_cast
  Domain::Tile *mutable_tile = map_->getTile(current_pos_);
  if (!mutable_tile)
    return nullptr;

  int index = 0;

  // Ground item at index 0
  if (mutable_tile->hasGround()) {
    if (selected_index_ == index) {
      return mutable_tile->getGround();
    }
    index++;
  }

  // Stacked items
  size_t stack_index = static_cast<size_t>(selected_index_ - index);
  if (stack_index < mutable_tile->getItemCount()) {
    return mutable_tile->getItem(stack_index);
  }

  return nullptr;
}

void BrowseTileWindow::render(bool *p_visible) {
  ImGuiWindowFlags flags = ImGuiWindowFlags_None;

  // Always call Begin/End for ImGui layout persistence (imgui.ini)
  if (!ImGui::Begin("Browse Tile", p_visible, flags)) {
    ImGui::End();
    return;
  }

  // Refresh tile from selection each frame
  refreshFromSelection();

  if (!current_tile_) {
    ImVec2 content_size = ImGui::GetContentRegionAvail();
    float text_width = ImGui::CalcTextSize("Select a single tile to browse").x;
    ImGui::SetCursorPosX((content_size.x - text_width) * 0.5f);
    ImGui::SetCursorPosY(content_size.y * 0.5f);
    ImGui::TextDisabled("Select a single tile to browse");
    ImGui::End();
    return;
  }

  // Ensure renderers are initialized (defensive)
  ensureRenderersInitialized();

  // Reserve space for footer only
  float footer_height = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
  float available_height = ImGui::GetContentRegionAvail().y - footer_height;
  if (available_height < 100.0f)
    available_height = 100.0f;

  // Wrap table in child to constrain height
  ImGui::BeginChild("TableArea", ImVec2(0, available_height), false);

  // Use a table for clean two-column layout
  if (ImGui::BeginTable("BrowseTileTable", 2,
                        ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Items", ImGuiTableColumnFlags_WidthStretch, 0.5f);
    ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch,
                            0.5f);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();

    // LEFT COLUMN: Items + Spawn/Creature
    ImGui::TableSetColumnIndex(0);
    {
      // Toolbar - handle returned actions
      auto action = items_list_renderer_->renderToolbar(
          current_tile_, current_pos_, selected_index_, spawn_selected_,
          creature_selected_);

      // Handle spawn/creature deletion (delegated from ItemsListRenderer)
      if (action == BrowseTile::ToolbarAction::DeleteSpawn && session_ &&
          map_) {
        auto &hm = session_->getHistoryManager();
        auto &selection = session_->getSelectionService();
        hm.beginOperation("Delete spawn", Domain::History::ActionType::Other, &selection);
        hm.recordTileBefore(current_pos_, current_tile_);

        Domain::Tile *mutable_tile = map_->getTile(current_pos_);
        if (mutable_tile)
          mutable_tile->setSpawn(nullptr);

        hm.endOperation(map_, &selection);
        spawn_selected_ = false;
      } else if (action == BrowseTile::ToolbarAction::DeleteCreature &&
                 session_ && map_) {
        auto &hm = session_->getHistoryManager();
        auto &selection = session_->getSelectionService();
        hm.beginOperation("Delete creature",
                          Domain::History::ActionType::Other, &selection);
        hm.recordTileBefore(current_pos_, current_tile_);

        Domain::Tile *mutable_tile = map_->getTile(current_pos_);
        if (mutable_tile)
          mutable_tile->setCreature(nullptr);

        hm.endOperation(map_, &selection);
        creature_selected_ = false;
      }

      // Reserve fixed height for Spawn/Creature box (64px images + labels +
      // padding)
      float spawn_creature_reserved = 140.0f;

      // Items list in scrollable child - leave space for spawn/creature box
      float available = ImGui::GetContentRegionAvail().y;
      float list_height = available - spawn_creature_reserved;
      if (list_height < 50.0f)
        list_height = 50.0f;

      ImGui::BeginChild("ItemsList", ImVec2(0, list_height), true);
      items_list_renderer_->render(current_tile_, current_pos_, selected_index_,
                                   spawn_selected_, creature_selected_);
      ImGui::EndChild();

      // Check if user dragged an item out of the window (must be done in parent
      // context after EndChild)
      items_list_renderer_->checkDragOutDeletion(current_tile_, current_pos_,
                                                 selected_index_);

      // ========== SPAWN/CREATURE BOX ==========
      spawn_creature_renderer_->render(current_tile_, spawn_selected_,
                                       creature_selected_, selected_index_);
    }

    // RIGHT COLUMN: Properties
    ImGui::TableSetColumnIndex(1);
    {
      renderTileProperties();

      ImGui::Spacing();
      ImGui::Separator();

      // Dynamic Properties Panel
      Domain::Item *selected_item = getSelectedItem();

      // Get mutable spawn/creature for property editing via mutable tile
      Domain::Spawn *spawn = nullptr;
      Domain::Creature *creature = nullptr;
      if (map_ && spawn_selected_) {
        if (Domain::Tile *mutable_tile = map_->getTile(current_pos_)) {
          spawn = mutable_tile->getSpawn();
        }
      }
      if (map_ && creature_selected_) {
        if (Domain::Tile *mutable_tile = map_->getTile(current_pos_)) {
          creature = mutable_tile->getCreature();
        }
      }

      uint32_t otbm_version = map_ ? map_->getVersion().otbm_version : 0;

      property_renderer_.setContext(selected_item, spawn, creature,
                                    otbm_version, sprite_manager_,
                                    map_ ? map_->getWidth() : 65535,
                                    map_ ? map_->getHeight() : 65535, map_);

      // Show panel header if something is selected
      if (selected_item || spawn || creature) {
        ImGui::Text("%s", property_renderer_.getPanelName());
        ImGui::Separator();
      }

      property_renderer_.render();
    }

    ImGui::EndTable();
  }
  ImGui::EndChild();

  // Footer with count, position, and house info
  size_t item_count = current_tile_->hasGround() ? 1 : 0;
  item_count += current_tile_->getItemCount();

  if (current_tile_->isHouseTile()) {
    ImGui::Text("Count %zu, Pos: %d,%d,%d | House: %u", item_count,
                current_pos_.x, current_pos_.y, current_pos_.z,
                current_tile_->getHouseId());
  } else {
    ImGui::Text("Count %zu, Pos: %d,%d,%d | House: none", item_count,
                current_pos_.x, current_pos_.y, current_pos_.z);
  }

  ImGui::End();
}

void BrowseTileWindow::renderTileProperties() {
  // Zone flags at top (ribbon style)
  Domain::TileFlag flags = current_tile_->getFlags();
  bool pz = Domain::hasFlag(flags, Domain::TileFlag::ProtectionZone);
  bool no_pvp = Domain::hasFlag(flags, Domain::TileFlag::NoPvp);
  bool no_logout = Domain::hasFlag(flags, Domain::TileFlag::NoLogout);
  bool pvp_zone = Domain::hasFlag(flags, Domain::TileFlag::PvpZone);

  ImVec4 green(0.2f, 0.8f, 0.2f, 1.0f);
  ImVec4 red(0.5f, 0.3f, 0.3f, 1.0f);

  ImGui::Spacing();

  ImGui::PushStyleColor(ImGuiCol_Text, pz ? green : red);
  ImGui::Button(ICON_FA_SHIELD);
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Protection Zone: %s", pz ? "Yes" : "No");
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, no_pvp ? green : red);
  ImGui::Button(ICON_FA_HAND);
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("No PvP: %s", no_pvp ? "Yes" : "No");
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, no_logout ? green : red);
  ImGui::Button(ICON_FA_DOOR_CLOSED);
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("No Logout: %s", no_logout ? "Yes" : "No");
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, pvp_zone ? green : red);
  ImGui::Button(ICON_FA_SKULL);
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("PvP Zone: %s", pvp_zone ? "Yes" : "No");
  }

  // Associations
  if (current_tile_->isHouseTile()) {
    ImGui::Text("House: %u", current_tile_->getHouseId());
  } else {
    ImGui::TextDisabled("House: none");
  }
}

void BrowseTileWindow::ensureRenderersInitialized() {
  // Initialize or update ItemsListRenderer
  if (!items_list_renderer_) {
    items_list_renderer_ = std::make_unique<BrowseTile::ItemsListRenderer>(
        map_, sprite_manager_, session_);
  } else {
    items_list_renderer_->setContext(map_, sprite_manager_, session_);
  }

  // Initialize or update SpawnCreatureRenderer
  if (!spawn_creature_renderer_) {
    spawn_creature_renderer_ =
        std::make_unique<BrowseTile::SpawnCreatureRenderer>(sprite_manager_,
                                                            client_data_);
  } else {
    spawn_creature_renderer_->setContext(sprite_manager_, client_data_);
  }
}

} // namespace MapEditor::UI
