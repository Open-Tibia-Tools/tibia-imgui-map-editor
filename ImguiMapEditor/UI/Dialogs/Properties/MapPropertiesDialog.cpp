#include "MapPropertiesDialog.h"
#include "Presentation/NotificationHelper.h"
#include "../../../Utils/StringCopy.h"
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <imgui.h>

namespace MapEditor {
namespace UI {

void MapPropertiesDialog::show(Domain::ChunkedMap *map) {
  if (!map)
    return;

  map_ = map;
  should_open_ = true;
  loadFromMap();
}

MapPropertiesDialog::Result MapPropertiesDialog::render() {
  Result result = Result::None;

  if (should_open_) {
    ImGui::OpenPopup("Map Properties###MapPropertiesDialog");
    should_open_ = false;
    is_open_ = true;
  }

  // Center dialog
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(450, 420), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("Map Properties###MapPropertiesDialog", nullptr,
                             ImGuiWindowFlags_NoResize)) {

    // === Description ===
    ImGui::Text(ICON_FA_FILE_LINES " Description:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextMultiline("##Description", description_buffer_,
                              sizeof(description_buffer_), ImVec2(-1, 80));

    ImGui::Separator();

    // === Dimensions ===
    ImGui::Text(ICON_FA_RULER_COMBINED " Map Size:");

    ImGui::Text("Width:");
    ImGui::SameLine(80);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("##Width", &width_, 0, 0);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Map width in tiles (Min: 256, Max: 65535)");
    }
    width_ = std::clamp(width_, 256, 65535);

    ImGui::SameLine();
    ImGui::Text("Height:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("##Height", &height_, 0, 0);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Map height in tiles (Min: 256, Max: 65535)");
    }
    height_ = std::clamp(height_, 256, 65535);

    ImGui::Separator();

    // === Version Info (read-only for now) ===
    ImGui::Text(ICON_FA_CODE_BRANCH " Version Information:");
    ImGui::TextDisabled("(Version conversion coming in future update)");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("To change map version, create a new map and copy "
                        "content.\nDirect conversion is not yet supported.");
    }

    ImGui::Text("OTBM Version:");
    ImGui::SameLine(120);
    ImGui::Text("%u", otbm_version_);

    ImGui::Text("Client Version:");
    ImGui::SameLine(120);
    ImGui::Text("%u", client_version_);

    ImGui::Separator();

    // === External Files ===
    ImGui::Text(ICON_FA_LINK " External Files:");

    ImGui::Text("House File:");
    ImGui::SameLine(100);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##HouseFile", house_filename_, sizeof(house_filename_));
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "External XML file for house data (e.g., map-houses.xml)");
    }

    ImGui::Text("Spawn File:");
    ImGui::SameLine(100);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##SpawnFile", spawn_filename_, sizeof(spawn_filename_));
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "External XML file for spawn data (e.g., map-spawns.xml)");
    }

    ImGui::Separator();

    // === OK / Cancel ===
    float button_width = 120.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button_width * 2 + spacing;
    float start_x = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);

    if (ImGui::Button(ICON_FA_CHECK " OK", ImVec2(button_width, 0))) {
      applyToMap();
      result = Result::Applied;
      Presentation::showSuccess("Map properties updated!");
      ImGui::CloseCurrentPopup();
      is_open_ = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply changes and close");

    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_BAN " Cancel", ImVec2(button_width, 0))) {
      result = Result::Cancelled;
      ImGui::CloseCurrentPopup();
      is_open_ = false;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Discard changes (Esc)");

    // Escape to close
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      result = Result::Cancelled;
      ImGui::CloseCurrentPopup();
      is_open_ = false;
    }

    ImGui::EndPopup();
  } else if (is_open_) {
    // Popup was closed externally
    is_open_ = false;
    result = Result::Cancelled;
  }

  return result;
}

void MapPropertiesDialog::loadFromMap() {
  if (!map_)
    return;

  // Description
  ::MapEditor::Utils::copyTruncate(description_buffer_, map_->getDescription());

  // Dimensions
  width_ = map_->getWidth();
  height_ = map_->getHeight();

  // Version info
  const auto &version = map_->getVersion();
  otbm_version_ = version.otbm_version;
  client_version_ = version.client_version;

  // External files
  ::MapEditor::Utils::copyTruncate(house_filename_, map_->getHouseFile());

  ::MapEditor::Utils::copyTruncate(spawn_filename_, map_->getSpawnFile());
}

void MapPropertiesDialog::applyToMap() {
  if (!map_)
    return;

  map_->setDescription(description_buffer_);
  map_->setSize(static_cast<uint16_t>(width_), static_cast<uint16_t>(height_));
  map_->setHouseFile(house_filename_);
  map_->setSpawnFile(spawn_filename_);
}

} // namespace UI
} // namespace MapEditor
