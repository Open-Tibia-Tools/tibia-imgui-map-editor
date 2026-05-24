#include "SelectedMapPanel.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace MapEditor {
namespace UI {

void SelectedMapPanel::render() const {
  // Panel header
  ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.92f, 1.0f),
                     ICON_FA_CIRCLE_INFO " Selected map information");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (map_info_.valid) {
    const ImVec4 label_color(0.55f, 0.58f, 0.62f, 1.0f);
    const ImVec4 value_color(0.95f, 0.95f, 0.95f, 1.0f);
    const ImVec4 empty_color(0.4f, 0.42f, 0.45f, 1.0f);

    // Map Name
    ImGui::TextColored(label_color, ICON_FA_FILE " Map Name");
    ImGui::TextColored(value_color, "%s", map_info_.name.c_str());
    ImGui::Spacing();

    // Client Version
    ImGui::TextColored(label_color, ICON_FA_CODE_BRANCH " Client Version");
    if (map_info_.client_version >= 700) {
      ImGui::TextColored(value_color, "%u.%02u", map_info_.client_version / 100,
                         map_info_.client_version % 100);
    } else if (map_info_.client_version > 0) {
      ImGui::TextColored(value_color, "%u", map_info_.client_version);
    } else {
      ImGui::TextColored(empty_color, "Unknown");
    }
    ImGui::Spacing();

    // Dimensions
    ImGui::TextColored(label_color, ICON_FA_RULER_COMBINED " Dimensions");
    ImGui::TextColored(value_color, "%u x %u tiles", map_info_.width,
                       map_info_.height);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // OTBM Version
    ImGui::TextColored(label_color, ICON_FA_FILE_CODE " OTBM Version");
    ImGui::TextColored(value_color, "%u", map_info_.otbm_version);
    ImGui::Spacing();

    // Items Major Version
    ImGui::TextColored(label_color, ICON_FA_CUBES " Items Major Version");
    ImGui::TextColored(value_color, "%u", map_info_.items_major_version);
    ImGui::Spacing();

    // Items Minor Version
    ImGui::TextColored(label_color, ICON_FA_CUBE " Items Minor Version");
    ImGui::TextColored(value_color, "%u", map_info_.items_minor_version);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // House File
    ImGui::TextColored(label_color, ICON_FA_HOUSE " House File");
    if (!map_info_.house_file.empty()) {
      ImGui::TextColored(value_color, "%s", map_info_.house_file.c_str());
    } else {
      ImGui::TextColored(empty_color, "(Not set)");
    }
    ImGui::Spacing();

    // Spawn File
    ImGui::TextColored(label_color, ICON_FA_SKULL " Spawn File");
    if (!map_info_.spawn_file.empty()) {
      ImGui::TextColored(value_color, "%s", map_info_.spawn_file.c_str());
    } else {
      ImGui::TextColored(empty_color, "(Not set)");
    }
    ImGui::Spacing();

    // Description
    ImGui::TextColored(label_color, ICON_FA_FILE_LINES " Description");
    if (!map_info_.description.empty()) {
      ImGui::TextWrapped("%s", map_info_.description.c_str());
    } else {
      ImGui::TextColored(empty_color, "(No description)");
    }
  } else {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.52f, 0.55f, 1.0f),
                       "Select a map to view details");
  }
}

} // namespace UI
} // namespace MapEditor
