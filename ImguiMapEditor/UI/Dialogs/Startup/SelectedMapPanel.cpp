#include "SelectedMapPanel.h"
#include "UI/Core/Theme.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace MapEditor {
namespace UI {

namespace SC = SemanticColors;

void SelectedMapPanel::render() {
  // Panel header
  ImGui::TextColored(SC::HEADER_TEXT,
                     ICON_FA_CIRCLE_INFO " Selected map information");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (map_info_.valid) {
    // Map Name
    ImGui::TextColored(SC::LABEL, ICON_FA_FILE " Map Name");
    ImGui::TextColored(SC::VALUE, "%s", map_info_.name.c_str());
    ImGui::Spacing();

    // Client Version
    ImGui::TextColored(SC::LABEL, ICON_FA_CODE_BRANCH " Client Version");
    if (map_info_.client_version >= 700) {
      ImGui::TextColored(SC::VALUE, "%u.%02u", map_info_.client_version / 100,
                         map_info_.client_version % 100);
    } else if (map_info_.client_version > 0) {
      ImGui::TextColored(SC::VALUE, "%u", map_info_.client_version);
    } else {
      ImGui::TextColored(SC::EMPTY, "Unknown");
    }
    ImGui::Spacing();

    // Dimensions
    ImGui::TextColored(SC::LABEL, ICON_FA_RULER_COMBINED " Dimensions");
    ImGui::TextColored(SC::VALUE, "%u x %u tiles", map_info_.width,
                       map_info_.height);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // OTBM Version
    ImGui::TextColored(SC::LABEL, ICON_FA_FILE_CODE " OTBM Version");
    ImGui::TextColored(SC::VALUE, "%u", map_info_.otbm_version);
    ImGui::Spacing();

    // Items Major Version
    ImGui::TextColored(SC::LABEL, ICON_FA_CUBES " Items Major Version");
    ImGui::TextColored(SC::VALUE, "%u", map_info_.items_major_version);
    ImGui::Spacing();

    // Items Minor Version
    ImGui::TextColored(SC::LABEL, ICON_FA_CUBE " Items Minor Version");
    ImGui::TextColored(SC::VALUE, "%u", map_info_.items_minor_version);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // House File
    ImGui::TextColored(SC::LABEL, ICON_FA_HOUSE " House File");
    if (!map_info_.house_file.empty()) {
      ImGui::TextColored(SC::VALUE, "%s", map_info_.house_file.c_str());
    } else {
      ImGui::TextColored(SC::EMPTY, "(Not set)");
    }
    ImGui::Spacing();

    // Spawn File
    ImGui::TextColored(SC::LABEL, ICON_FA_SKULL " Spawn File");
    if (!map_info_.spawn_file.empty()) {
      ImGui::TextColored(SC::VALUE, "%s", map_info_.spawn_file.c_str());
    } else {
      ImGui::TextColored(SC::EMPTY, "(Not set)");
    }
    ImGui::Spacing();

    // Description
    ImGui::TextColored(SC::LABEL, ICON_FA_FILE_LINES " Description");
    if (!map_info_.description.empty()) {
      ImGui::TextWrapped("%s", map_info_.description.c_str());
    } else {
      ImGui::TextColored(SC::EMPTY, "(No description)");
    }
  } else {
    ImGui::Spacing();
    ImGui::TextColored(SC::LABEL,
                       "Select a map to view details");
  }
}

} // namespace UI
} // namespace MapEditor
