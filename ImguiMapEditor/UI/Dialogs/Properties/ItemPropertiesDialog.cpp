#include "ItemPropertiesDialog.h"
#include "Core/Config.h"
#include "Domain/ItemType.h"
#include "Domain/Position.h"
#include "Presentation/NotificationHelper.h"
#include "../../../Utils/StringCopy.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <cmath>
#include <imgui.h>

namespace MapEditor::UI {

ItemPropertiesDialog::ItemPropertiesDialog() = default;

void ItemPropertiesDialog::open(Domain::Item *item, SaveCallback on_save) {
  if (!item)
    return;

  current_item_ = item;
  save_callback_ = on_save;
  is_open_ = true;
  selected_slot_ = -1;

  // Load current values
  action_id_ = item->getActionId();
  unique_id_ = item->getUniqueId();

  // Text
  std::string text = item->getText();
  ::MapEditor::Utils::copyTruncate(text_buffer_, text);

  // Teleport destination
  const Domain::Position *dest = item->getTeleportDestination();
  if (dest) {
    tele_x_ = dest->x;
    tele_y_ = dest->y;
    tele_z_ = dest->z;
  } else {
    tele_x_ = tele_y_ = tele_z_ = 0;
  }

  door_id_ = static_cast<int>(item->getDoorId());
}

void ItemPropertiesDialog::renderContent() {
  if (!current_item_)
    return;

  ImGui::Text("Item ID: %u", current_item_->getServerId());
  ImGui::Separator();

  // Check if this is a container
  const Domain::ItemType *item_type = current_item_->getType();
  const bool is_container = item_type && item_type->volume > 0;

  if (ImGui::BeginTabBar("##ItemPropsTabs")) {
    // Properties tab
    if (ImGui::BeginTabItem("Properties")) {
      // Action ID
      ImGui::InputInt("Action ID", &action_id_);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Script identifier (AID) - Used for scripts and events");
      }
      if (action_id_ < 0)
        action_id_ = 0;
      if (action_id_ > 65535)
        action_id_ = 65535;

      // Unique ID
      ImGui::InputInt("Unique ID", &unique_id_);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Global identifier (UID) - Must be unique across the entire map");
      }
      if (unique_id_ < 0)
        unique_id_ = 0;
      if (unique_id_ > 65535)
        unique_id_ = 65535;

      // Text
      ImGui::InputTextMultiline("Text", text_buffer_, sizeof(text_buffer_));

      // Teleport destination
      ImGui::Text("Teleport Destination:");
      ImGui::PushItemWidth(80);
      ImGui::InputInt("##tele_x", &tele_x_);
      ImGui::SameLine();
      ImGui::InputInt("##tele_y", &tele_y_);
      ImGui::SameLine();
      ImGui::InputInt("##tele_z", &tele_z_);
      ImGui::PopItemWidth();

      // Door ID
      ImGui::InputInt("Door ID", &door_id_);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Links key items to this door (0-255)");
      }
      if (door_id_ < 0)
        door_id_ = 0;
      if (door_id_ > 255)
        door_id_ = 255;

      ImGui::EndTabItem();
    }

    // Contents tab (only for containers)
    if (is_container) {
      if (ImGui::BeginTabItem("Contents")) {
        renderContentsTab();
        ImGui::EndTabItem();
      }
    }

    ImGui::EndTabBar();
  }
}

void ItemPropertiesDialog::onSave() {
    if (current_item_) {
        // Apply changes
        current_item_->setActionId(static_cast<uint16_t>(action_id_));
        current_item_->setUniqueId(static_cast<uint16_t>(unique_id_));
        current_item_->setText(text_buffer_);

        if (tele_x_ > 0 || tele_y_ > 0 || tele_z_ > 0) {
            current_item_->setTeleportDestination(
                Domain::Position{tele_x_, tele_y_, static_cast<int16_t>(tele_z_)});
        }

        current_item_->setDoorId(static_cast<uint32_t>(door_id_));
        Presentation::showSuccess("Item properties saved!");
    }
}

void ItemPropertiesDialog::onClose() {
    BasePropertiesDialog::onClose();
    current_item_ = nullptr;
}

void ItemPropertiesDialog::renderContentsTab() {
  const float SLOT_SIZE = 36.0f;
  const float PADDING = 2.0f;

  const Domain::ItemType *item_type = current_item_->getType();
  if (!item_type || item_type->volume == 0)
    return;

  const uint16_t volume = item_type->volume;

  // Dynamic columns: square-ish layout
  const int cols =
      static_cast<int>(std::ceil(std::sqrt(static_cast<float>(volume))));

  const auto &items = current_item_->getContainerItems();

  ImGui::Text("Container: %u / %u slots", static_cast<unsigned>(items.size()),
              volume);
  ImGui::Separator();

  for (size_t i = 0; i < volume; ++i) {
    if (i % cols != 0)
      ImGui::SameLine(0, PADDING);

    Domain::Item *slot_item = (i < items.size()) ? items[i].get() : nullptr;
    const bool selected = (selected_slot_ == static_cast<int>(i));

    ImGui::PushID(static_cast<int>(i));
    if (renderSlotButton(slot_item, SLOT_SIZE, selected)) {
      selected_slot_ = selected ? -1 : static_cast<int>(i);
    }

    // Context menu for filled slots
    if (slot_item && ImGui::BeginPopupContextItem()) {
      ImGui::Text("Item #%u", slot_item->getServerId());
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_FA_TRASH " Remove")) {
        // TODO: Implement item removal
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }
}

bool ItemPropertiesDialog::renderSlotButton(Domain::Item *item, float size,
                                            bool selected) {
  ImVec2 pos = ImGui::GetCursorScreenPos();
  ImDrawList *dl = ImGui::GetWindowDrawList();

  // Slot background
  ImU32 bg_color =
      selected ? IM_COL32(80, 80, 120, 255) : IM_COL32(40, 40, 40, 255);
  ImU32 border_color =
      selected ? IM_COL32(120, 120, 180, 255) : IM_COL32(80, 80, 80, 255);

  dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), bg_color);
  dl->AddRect(pos, ImVec2(pos.x + size, pos.y + size), border_color);

  // Item indicator (colored square if item present)
  if (item) {
    const float inner_margin = 4.0f;
    dl->AddRectFilled(
        ImVec2(pos.x + inner_margin, pos.y + inner_margin),
        ImVec2(pos.x + size - inner_margin, pos.y + size - inner_margin),
        IM_COL32(100, 150, 200, 255) // Item placeholder color
    );
  }

  return ImGui::InvisibleButton("##slot", ImVec2(size, size));
}

} // namespace MapEditor::UI
