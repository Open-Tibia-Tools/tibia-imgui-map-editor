#include "MapContextMenu.h"
#include "Services/ClipboardService.h"
#include "Application/EditorSession.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Tile.h"
#include "Presentation/NotificationHelper.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>
#include <sstream>

namespace MapEditor::UI {

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

  if (!is_open_)
    return;

  // Get tile at position
  current_tile_ = nullptr;
  selected_item_ = nullptr;

  if (session && session->getMap()) {
    current_tile_ = session->getMap()->getTile(position_);
  }

  if (ImGui::BeginPopup("MapContextMenu")) {
    renderTileActions(session);
    ImGui::Separator();
    renderItemActions(session);
    ImGui::Separator();
    renderClipboardActions(session, clipboard);
    ImGui::Separator();
    renderNavigationActions(session);

    ImGui::EndPopup();
  } else {
    is_open_ = false;
  }
}

void MapContextMenu::renderTileActions(AppLogic::EditorSession *session) {
  // Copy Position
  std::ostringstream pos_str;
  pos_str << position_.x << ", " << position_.y << ", " << (int)position_.z;

  if (ImGui::MenuItem(ICON_FA_LOCATION_DOT " Copy Position")) {
    std::string const text = pos_str.str();
    ImGui::SetClipboardText(text.c_str());
    Presentation::showSuccess("Position copied to clipboard!");
  }

  if (ImGui::MenuItem(ICON_FA_COPY " Copy Ground ID", nullptr, false,
                      current_tile_ && current_tile_->getGround())) {
    if (current_tile_->getGround()) {
      std::string const id =
          std::to_string(current_tile_->getGround()->getServerId());
      ImGui::SetClipboardText(id.c_str());
      Presentation::showSuccess("Ground ID copied to clipboard!");
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!current_tile_ || !current_tile_->getGround())
      ImGui::SetTooltip("No ground tile at this location");
    else
      ImGui::SetTooltip("Copy the ground item ID to clipboard");
  }

  // Hint at keyboard shortcut if available (usually Double Click)
  if (ImGui::MenuItem(ICON_FA_MAGNIFYING_GLASS " Browse Tile", "Double Click", false,
                      current_tile_ != nullptr)) {
    if (browse_tile_callback_) {
      // Pass top item server ID (0 if no items)
      uint16_t top_item_id = 0;
      if (current_tile_ && !current_tile_->getItems().empty()) {
        top_item_id = current_tile_->getItems().back()->getServerId();
      }
      browse_tile_callback_(position_, top_item_id);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!current_tile_)
      ImGui::SetTooltip("Empty tile");
    else
      ImGui::SetTooltip("Inspect tile contents");
  }
}

void MapContextMenu::renderItemActions(AppLogic::EditorSession *session) {
  bool const has_items = current_tile_ && !current_tile_->getItems().empty();

  // Check if top item is rotatable
  bool can_rotate = false;
  if (has_items) {
    auto &items = current_tile_->getItems();
    if (!items.empty() && items.back()->getType()) {
      can_rotate = items.back()->getType()->isRotatable();
    }
  }

  // Check if top item is a door
  bool is_door = false;
  if (has_items) {
    auto &items = current_tile_->getItems();
    if (!items.empty() && items.back()->getType()) {
      is_door = items.back()->getType()->isDoor();
    }
  }

  if (ImGui::MenuItem(ICON_FA_TAG " Copy Server ID", nullptr, false,
                      has_items)) {
    if (current_tile_ && !current_tile_->getItems().empty()) {
      auto &items = current_tile_->getItems();
      if (!items.empty()) {
        std::string const id = std::to_string(items.back()->getServerId());
        ImGui::SetClipboardText(id.c_str());
        Presentation::showSuccess("Item ID copied to clipboard!");
      }
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_items)
      ImGui::SetTooltip("No items on tile");
    else
      ImGui::SetTooltip("Copy the item ID to clipboard");
  }

  if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " Rotate Item", nullptr, false,
                      can_rotate)) {
    if (session && current_tile_ && !current_tile_->getItems().empty()) {
      auto &items = current_tile_->getItems();
      if (!items.empty()) {
        auto *item = items.back().get();
        const auto *type = item->getType();
        if (type && type->rotateTo != 0) {
          // Get the mutable tile from the map
          auto *mutable_tile = session->getMap()->getTile(position_);
          if (mutable_tile && !mutable_tile->getItems().empty()) {
            auto &mutable_items = mutable_tile->getItems();
            auto *mutable_item = mutable_items.back().get();
            mutable_item->setServerId(type->rotateTo);
            session->setModified(true);
          }
        }
      }
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_items)
      ImGui::SetTooltip("No items on tile");
    else if (!can_rotate)
      ImGui::SetTooltip("Item is not rotatable");
    else
      ImGui::SetTooltip("Rotate item to next direction/ID");
  }

  if (ImGui::MenuItem(ICON_FA_DOOR_OPEN " Switch Door", nullptr, false,
                      is_door)) {
    // Door switching typically uses door_id attribute or swaps between
    // open/closed item IDs For now, just mark as modified - full implementation
    // requires door open/close mapping
    if (session) {
      session->setModified(true);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_items)
      ImGui::SetTooltip("No items on tile");
    else if (!is_door)
      ImGui::SetTooltip("Item is not a door");
    else
      ImGui::SetTooltip("Toggle door open/closed state");
  }

  // Add keyboard shortcut hint for Properties (Ctrl+Enter is common, but let's check input controller later. For now just "Enter")
  if (ImGui::MenuItem(ICON_FA_GEAR " Properties...", "Enter", false,
                      has_items)) {
    if (properties_callback_ && current_tile_ &&
        !current_tile_->getItems().empty()) {
      properties_callback_(current_tile_->getItems().back().get());
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_items)
      ImGui::SetTooltip("No items on tile");
    else
      ImGui::SetTooltip("View/edit item properties");
  }
}

void MapContextMenu::renderClipboardActions(
    AppLogic::EditorSession *session, AppLogic::ClipboardService *clipboard) {
  bool const has_selection = session && !session->getSelectionService().isEmpty();
  bool const can_paste = clipboard && clipboard->canPaste();

  if (ImGui::MenuItem(ICON_FA_SCISSORS " Cut", "Ctrl+X", false,
                      has_selection)) {
    if (clipboard && session) {
      size_t const count = clipboard->cut(*session);
      Presentation::showInfo("Cut " + std::to_string(count) + " tiles", 3000);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_selection)
      ImGui::SetTooltip("Select tiles first");
    else
      ImGui::SetTooltip("Cut selection to clipboard");
  }

  if (ImGui::MenuItem(ICON_FA_COPY " Copy", "Ctrl+C", false, has_selection)) {
    if (clipboard && session) {
      size_t const count = clipboard->copy(*session);
      Presentation::showInfo("Copied " + std::to_string(count) + " tiles",
                             3000);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_selection)
      ImGui::SetTooltip("Select tiles first");
    else
      ImGui::SetTooltip("Copy selection to clipboard");
  }

  if (ImGui::MenuItem(ICON_FA_PASTE " Paste", "Ctrl+V", false, can_paste)) {
    if (clipboard && session) {
      size_t const count = clipboard->paste(*session, position_);
      Presentation::showSuccess("Pasted " + std::to_string(count) + " tiles");
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!can_paste)
      ImGui::SetTooltip("Clipboard is empty");
    else
      ImGui::SetTooltip("Paste from clipboard");
  }

  if (ImGui::MenuItem(ICON_FA_TRASH " Delete", "Del", false, has_selection)) {
    if (session) {
      session->deleteSelection();
      // Delete doesn't return count, but we can assume selection
      Presentation::showWarning("Deleted selection");
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_selection)
      ImGui::SetTooltip("Select tiles first");
    else
      ImGui::SetTooltip("Delete selected tiles");
  }
}

void MapContextMenu::renderNavigationActions(AppLogic::EditorSession *session) {
  // Check for teleporter
  bool has_teleporter = false;
  Domain::Position dest{0, 0, 0};

  if (current_tile_) {
    for (const auto &item : current_tile_->getItems()) {
      const Domain::Position *tele_dest =
          item ? item->getTeleportDestination() : nullptr;
      if (tele_dest) {
        has_teleporter = true;
        dest = *tele_dest;
        break;
      }
    }
  }

  if (ImGui::MenuItem(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE " Goto Destination",
                      nullptr, false, has_teleporter)) {
    if (goto_callback_) {
      goto_callback_(dest);
    }
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_teleporter)
      ImGui::SetTooltip("No teleport destination found");
    else
      ImGui::SetTooltip("Teleport camera to destination");
  }
}

} // namespace MapEditor::UI
