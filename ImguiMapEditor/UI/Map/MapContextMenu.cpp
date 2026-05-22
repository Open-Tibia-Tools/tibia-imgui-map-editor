#include "MapContextMenu.h"

#include "Application/EditorSession.h"
#include "Domain/Creature.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Selection/SelectionEntry.h"
#include "Domain/Spawn.h"
#include "Domain/Tile.h"
#include "Presentation/NotificationHelper.h"
#include "Services/ClipboardService.h"
#include "Services/Selection/SelectionService.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <algorithm>
#include <array>
#include <imgui.h>
#include <optional>
#include <ranges>
#include <sstream>

namespace MapEditor::UI {

namespace {

const Domain::Item *getTopItem(const Domain::Tile *tile) {
  if (!tile) {
    return nullptr;
  }

  if (!tile->getItems().empty()) {
    return tile->getItems().back().get();
  }

  return tile->getGround();
}

const Domain::Item *getSelectedContextItem(AppLogic::EditorSession *session,
                                           const Domain::Position &position) {
  if (!session) {
    return nullptr;
  }

  const auto &selection = session->getSelectionService();
  const auto entries = selection.getEntriesAt(position);
  if (entries.size() != 1) {
    return nullptr;
  }

  const auto &entry = entries.front();
  if (entry.getType() == Domain::Selection::EntityType::Item ||
      entry.getType() == Domain::Selection::EntityType::Ground) {
    return static_cast<const Domain::Item *>(entry.entity_ptr);
  }

  return nullptr;
}

const Domain::Item *getActiveItem(const Domain::Tile *tile,
                                  const Domain::Item *selected_item) {
  if (selected_item) {
    return selected_item;
  }
  return getTopItem(tile);
}

const Domain::Item *getTopSelectedItem(AppLogic::EditorSession *session,
                                       const Domain::Tile *tile,
                                       const Domain::Position &position) {
  if (!session || !tile) {
    return nullptr;
  }

  const auto entries = session->getSelectionService().getEntriesAt(position);
  if (entries.empty()) {
    return nullptr;
  }

  const auto isSelectedItem =
      [&entries](const Domain::Item *item,
                 Domain::Selection::EntityType type) -> bool {
    return std::ranges::any_of(entries, [item, type](const auto &entry) {
      return entry.getType() == type && entry.entity_ptr == item;
    });
  };

  for (auto it = tile->getItems().rbegin(); it != tile->getItems().rend(); ++it) {
    if (it->get() && isSelectedItem(it->get(), Domain::Selection::EntityType::Item)) {
      return it->get();
    }
  }

  if (const auto *ground = tile->getGround();
      ground && isSelectedItem(ground, Domain::Selection::EntityType::Ground)) {
    return ground;
  }

  return nullptr;
}

enum class SelectionContextType : uint8_t {
  None,
  Item,
  Ground,
  Creature,
  Spawn,
  Multiple,
};

SelectionContextType getSelectionContextType(AppLogic::EditorSession *session,
                                             const Domain::Position &position) {
  if (!session) {
    return SelectionContextType::None;
  }

  const auto entries = session->getSelectionService().getEntriesAt(position);
  if (entries.empty()) {
    return SelectionContextType::None;
  }
  if (entries.size() > 1) {
    return SelectionContextType::Multiple;
  }

  switch (entries.front().getType()) {
  case Domain::Selection::EntityType::Item:
    return SelectionContextType::Item;
  case Domain::Selection::EntityType::Ground:
    return SelectionContextType::Ground;
  case Domain::Selection::EntityType::Creature:
    return SelectionContextType::Creature;
  case Domain::Selection::EntityType::Spawn:
    return SelectionContextType::Spawn;
  }

  return SelectionContextType::None;
}

struct BrushMenuContext {
  const Domain::Tile *tile = nullptr;
  const Domain::Item *selected_item = nullptr;
  const Domain::Item *top_selected_item = nullptr;
  const Domain::Item *active_item = nullptr;
  const Domain::Item *ground_item = nullptr;
  SelectionContextType selection_type = SelectionContextType::None;
};

BrushMenuContext buildBrushMenuContext(AppLogic::EditorSession *session,
                                       const Domain::Tile *tile,
                                       const Domain::Position &position) {
  const auto *selectedItem = getSelectedContextItem(session, position);
  const auto *topSelectedItem = getTopSelectedItem(session, tile, position);
  return {.tile = tile,
          .selected_item = selectedItem,
          .top_selected_item = topSelectedItem,
          .active_item = getActiveItem(tile, topSelectedItem ? topSelectedItem
                                                             : selectedItem),
          .ground_item = tile ? tile->getGround() : nullptr,
          .selection_type = getSelectionContextType(session, position)};
}

const Domain::Item *getPreferredBrushItem(BrushPickMode mode,
                                          const BrushMenuContext &context) {
  switch (mode) {
  case BrushPickMode::Raw:
    return context.active_item;
  case BrushPickMode::Ground:
    return context.ground_item;
  case BrushPickMode::Wall:
  case BrushPickMode::Door:
  case BrushPickMode::Carpet:
  case BrushPickMode::Table:
  case BrushPickMode::Doodad:
  case BrushPickMode::Collection:
    return context.active_item;
  case BrushPickMode::Smart:
  case BrushPickMode::Creature:
  case BrushPickMode::Spawn:
  case BrushPickMode::House:
  case BrushPickMode::HouseExit:
  case BrushPickMode::Waypoint:
  case BrushPickMode::OptionalBorder:
  case BrushPickMode::ProtectionZone:
  case BrushPickMode::NoPvp:
  case BrushPickMode::NoLogout:
  case BrushPickMode::PvpZone:
    return nullptr;
  }

  return nullptr;
}

bool shouldOfferBrushOption(BrushPickMode mode,
                            const BrushMenuContext &context) {
  if (!context.tile) {
    return false;
  }

  switch (mode) {
  case BrushPickMode::Creature:
    return context.selection_type == SelectionContextType::Creature ||
           context.tile->hasCreature();
  case BrushPickMode::Spawn:
    return context.selection_type == SelectionContextType::Spawn ||
           context.tile->hasSpawn();
  case BrushPickMode::House:
    return context.tile->isHouseTile();
  case BrushPickMode::Raw:
  case BrushPickMode::Wall:
  case BrushPickMode::Carpet:
  case BrushPickMode::Table:
  case BrushPickMode::Doodad:
  case BrushPickMode::Door:
  case BrushPickMode::Ground:
  case BrushPickMode::Collection:
    return true;
  case BrushPickMode::Smart:
  case BrushPickMode::HouseExit:
  case BrushPickMode::Waypoint:
  case BrushPickMode::OptionalBorder:
  case BrushPickMode::ProtectionZone:
  case BrushPickMode::NoPvp:
  case BrushPickMode::NoLogout:
  case BrushPickMode::PvpZone:
    return false;
  }

  return false;
}

Domain::Item *getMutableActiveItem(AppLogic::EditorSession *session,
                                   const Domain::Position &position,
                                   const Domain::Item *preferred_item) {
  if (!session || !session->getMap()) {
    return nullptr;
  }

  auto *tile = session->getMap()->getTile(position);
  if (!tile) {
    return nullptr;
  }

  if (preferred_item) {
    if (tile->getGround() == preferred_item) {
      return tile->getGround();
    }

    for (const auto &item : tile->getItems()) {
      if (item.get() == preferred_item) {
        return item.get();
      }
    }
  }

  if (!tile->getItems().empty()) {
    return tile->getItems().back().get();
  }

  return tile->getGround();
}

void copyTextToClipboard(std::string text, const char *success_message) {
  ImGui::SetClipboardText(text.c_str());
  Presentation::showSuccess(success_message);
}

} // namespace

MapContextMenu::MapContextMenu() = default;

void MapContextMenu::show(const Domain::Position &pos) {
  position_ = pos;
  is_open_ = true;
  ImGui::OpenPopup("MapContextMenu");
}

void MapContextMenu::render(AppLogic::EditorSession *session,
                            AppLogic::ClipboardService *clipboard,
                            PropertiesCallback on_properties,
                            GotoCallback on_goto) {
  properties_callback_ = on_properties;
  goto_callback_ = on_goto;

  if (!is_open_) {
    return;
  }

  current_tile_ = nullptr;
  selected_item_ = nullptr;
  has_single_selection_context_ = false;

  if (session && session->getMap()) {
    current_tile_ = session->getMap()->getTile(position_);
    selected_item_ = getSelectedContextItem(session, position_);
    const auto selected_positions = session->getSelectionService().getPositions();
    has_single_selection_context_ =
        selected_positions.size() == 1 && selected_positions.front() == position_;
  }

  if (ImGui::BeginPopup("MapContextMenu")) {
    renderClipboardActions(session, clipboard);
    renderItemActions(session);
    renderBrushSelectionActions(session);
    renderTileActions(session);
    ImGui::EndPopup();
  } else {
    is_open_ = false;
  }
}

void MapContextMenu::renderClipboardActions(
    AppLogic::EditorSession *session, AppLogic::ClipboardService *clipboard) {
  const bool has_selection =
      session && !session->getSelectionService().isEmpty();
  const bool can_paste = clipboard && clipboard->canPaste();

  if (ImGui::MenuItem(ICON_FA_SCISSORS " Cut", "Ctrl+X", false,
                      has_selection)) {
    if (clipboard && session) {
      const size_t count = clipboard->cut(*session);
      Presentation::showInfo("Cut " + std::to_string(count) + " tiles", 3000);
    }
  }

  if (ImGui::MenuItem(ICON_FA_COPY " Copy", "Ctrl+C", false, has_selection)) {
    if (clipboard && session) {
      const size_t count = clipboard->copy(*session);
      Presentation::showInfo("Copied " + std::to_string(count) + " tiles",
                             3000);
    }
  }

  std::ostringstream pos_str;
  pos_str << position_.x << ", " << position_.y << ", "
          << static_cast<int>(position_.z);

  if (ImGui::MenuItem(ICON_FA_LOCATION_DOT " Copy Position", nullptr, false,
                      has_selection)) {
    copyTextToClipboard(pos_str.str(), "Position copied to clipboard!");
  }

  if (ImGui::MenuItem(ICON_FA_PASTE " Paste", "Ctrl+V", false, can_paste)) {
    if (clipboard && session) {
      const size_t count = clipboard->paste(*session, position_);
      Presentation::showSuccess("Pasted " + std::to_string(count) + " tiles");
    }
  }

  if (ImGui::MenuItem(ICON_FA_TRASH " Delete", "Del", false, has_selection)) {
    if (session) {
      session->deleteSelection();
      Presentation::showWarning("Deleted selection");
    }
  }

  if (has_selection || can_paste) {
    ImGui::Separator();
  }
}

void MapContextMenu::renderTileActions(AppLogic::EditorSession *session) {
  if (!has_single_selection_context_ || !current_tile_) {
    return;
  }

  Domain::Item *mutable_selected_item =
      getMutableActiveItem(session, position_, selected_item_);
  const bool has_spawn = current_tile_->hasSpawn();
  const bool has_creature = current_tile_->hasCreature();
  const bool can_open_properties =
      (has_spawn && static_cast<bool>(spawn_properties_callback_)) ||
      (has_creature && static_cast<bool>(creature_properties_callback_)) ||
      (mutable_selected_item != nullptr &&
       static_cast<bool>(properties_callback_));
  const auto *active_item = getActiveItem(current_tile_, selected_item_);
  const uint16_t browse_item_id =
      active_item ? active_item->getServerId() : 0U;

  ImGui::Separator();

  if (ImGui::MenuItem(ICON_FA_GEAR " Properties", nullptr, false,
                      can_open_properties)) {
    if (has_spawn && spawn_properties_callback_) {
      spawn_properties_callback_(
          const_cast<Domain::Spawn *>(current_tile_->getSpawn()), position_);
    } else if (has_creature && creature_properties_callback_) {
      creature_properties_callback_(
          const_cast<Domain::Creature *>(current_tile_->getCreature()),
          current_tile_->getCreature()->name, position_);
    } else if (properties_callback_ && mutable_selected_item) {
      properties_callback_(mutable_selected_item);
    }
  }

  if (can_open_properties) {
    ImGui::Separator();
  }

  if (ImGui::MenuItem(ICON_FA_MAGNIFYING_GLASS " Browse Field",
                      "Double Click", false, current_tile_ != nullptr)) {
    if (browse_tile_callback_) {
      browse_tile_callback_(position_, browse_item_id);
    }
  }
}

void MapContextMenu::renderItemActions(AppLogic::EditorSession *session) {
  if (!has_single_selection_context_ || !current_tile_) {
    return;
  }

  const auto *selected_item = selected_item_;
  const auto *active_item = getActiveItem(
      current_tile_, getTopSelectedItem(session, current_tile_, position_));
  Domain::Item *mutable_selected_item =
      getMutableActiveItem(session, position_, selected_item);
  const auto *selected_type = selected_item ? selected_item->getType() : nullptr;
  const bool can_rotate =
      selected_item &&
      ((can_rotate_item_callback_ &&
        can_rotate_item_callback_(position_, selected_item)) ||
       (selected_type && selected_type->rotateTo != 0));
  const auto door_state =
      door_state_callback_ ? door_state_callback_(position_, selected_item)
                           : std::nullopt;
  const bool can_switch_door =
      selected_item &&
      ((can_switch_door_callback_ &&
        can_switch_door_callback_(position_, selected_item)) ||
       (selected_type && selected_type->isDoor()));
  const bool has_name = selected_type && !selected_type->name.empty();
  const auto *destination =
      selected_item ? selected_item->getTeleportDestination() : nullptr;
  const bool has_spawn = current_tile_->hasSpawn();
  const bool has_creature = current_tile_->hasCreature();

  if (!active_item && !has_spawn && !has_creature) {
    return;
  }

  if (selected_item &&
      ImGui::MenuItem(ICON_FA_TAG " Copy Server ID")) {
    copyTextToClipboard(std::to_string(selected_item->getServerId()),
                        "Item ID copied to clipboard!");
  }

  if (selected_item &&
      ImGui::MenuItem(ICON_FA_ID_CARD " Copy Item Client ID")) {
    copyTextToClipboard(std::to_string(selected_item->getClientId()),
                        "Item client ID copied to clipboard!");
  }

  if (selected_item && has_name &&
      ImGui::MenuItem(ICON_FA_FILE_LINES " Copy Item Name")) {
    copyTextToClipboard(selected_type->name, "Item name copied to clipboard!");
  }

  if (can_rotate && ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " Rotate Item")) {
    if (rotate_item_callback_ &&
        rotate_item_callback_(position_, selected_item)) {
      if (session) {
        session->setModified(true);
      }
    } else if (mutable_selected_item && selected_type &&
               selected_type->rotateTo != 0) {
      mutable_selected_item->setServerId(selected_type->rotateTo);
      if (session) {
        session->setModified(true);
      }
    }
  }

  const char *doorActionLabel = ICON_FA_DOOR_OPEN " Switch Door";
  if (door_state.has_value()) {
    doorActionLabel = *door_state ? ICON_FA_DOOR_CLOSED " Close Door"
                                  : ICON_FA_DOOR_OPEN " Open Door";
  }

  if (ImGui::MenuItem(doorActionLabel, nullptr, false, can_switch_door)) {
    if (switch_door_callback_ &&
        switch_door_callback_(position_, selected_item)) {
      if (session) {
        session->setModified(true);
      }
    } else {
      Presentation::showWarning("Item is not a door", 2500);
    }
  }

  if (destination &&
      ImGui::MenuItem(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE " Go To Destination")) {
    if (goto_callback_) {
      goto_callback_(*destination);
    }
  }
}

void MapContextMenu::renderNavigationActions() {
  // Integrated into renderItemActions to preserve wx menu order.
}

void MapContextMenu::renderBrushSelectionActions(
    AppLogic::EditorSession *session) {
  const bool callbacks_ready = static_cast<bool>(select_brush_callback_) &&
                               static_cast<bool>(can_select_brush_callback_);
  if (!callbacks_ready || !current_tile_ || !has_single_selection_context_) {
    return;
  }

  struct BrushOption {
    const char *label;
    BrushPickMode mode;
  };

  constexpr std::array brush_options{
      BrushOption{"Select Creature", BrushPickMode::Creature},
      BrushOption{"Select Spawn", BrushPickMode::Spawn},
      BrushOption{"Select RAW", BrushPickMode::Raw},
      BrushOption{"Select Wallbrush", BrushPickMode::Wall},
      BrushOption{"Select Carpetbrush", BrushPickMode::Carpet},
      BrushOption{"Select Tablebrush", BrushPickMode::Table},
      BrushOption{"Select Doodadbrush", BrushPickMode::Doodad},
      BrushOption{"Select Doorbrush", BrushPickMode::Door},
      BrushOption{"Select Groundbrush", BrushPickMode::Ground},
      BrushOption{"Select Collection", BrushPickMode::Collection},
      BrushOption{"Select House", BrushPickMode::House},
  };

  const auto brush_context = buildBrushMenuContext(session, current_tile_, position_);

  const auto can_select = [this, &brush_context](BrushPickMode mode) {
    const auto *preferredItem = getPreferredBrushItem(mode, brush_context);
    return shouldOfferBrushOption(mode, brush_context) &&
           can_select_brush_callback_(position_, preferredItem, mode);
  };

  const auto try_select_brush = [this, &brush_context](BrushPickMode mode) {
    const auto *preferredItem = getPreferredBrushItem(mode, brush_context);
    const auto selected_brush =
        select_brush_callback_(position_, preferredItem, mode);
    if (selected_brush.empty()) {
      Presentation::showWarning("No matching brush found for this tile", 2500);
      return;
    }

    Presentation::showSuccess("Selected brush: " + selected_brush, 2000);
  };

  const bool has_any = std::ranges::any_of(
      brush_options, [&can_select](const BrushOption &option) {
        return can_select(option.mode);
      });
  if (!has_any) {
    return;
  }

  for (const auto &option : brush_options) {
    if (can_select(option.mode) && ImGui::MenuItem(option.label)) {
      try_select_brush(option.mode);
    }
  }
}

} // namespace MapEditor::UI
