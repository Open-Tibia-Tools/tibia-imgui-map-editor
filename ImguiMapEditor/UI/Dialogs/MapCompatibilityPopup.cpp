#include "MapCompatibilityPopup.h"
#include "UI/Core/Theme.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>


namespace MapEditor {
namespace UI {

namespace SC = SemanticColors;

void MapCompatibilityPopup::show(const MapCompatibilityResult &compat,
                                 const std::filesystem::path &map_path) {
  compat_info_ = compat;
  map_path_ = map_path;
  is_open_ = true;
  result_ = Result::None;
}

MapCompatibilityPopup::Result MapCompatibilityPopup::consumeResult() {
  auto r = result_;
  result_ = Result::None;
  return r;
}

void MapCompatibilityPopup::render() {
  if (!is_open_)
    return;

  ImGui::OpenPopup("Map Compatibility Warning");

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Always);

  if (ImGui::BeginPopupModal("Map Compatibility Warning", &is_open_,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Warning icon and title
    ImGui::TextColored(SC::WARNING,
                       ICON_FA_TRIANGLE_EXCLAMATION " Version Mismatch");
    ImGui::Separator();
    ImGui::Spacing();

    // Map info
    ImGui::Text("Map: %s", map_path_.filename().string().c_str());
    ImGui::Text("Map requires: Items %u.%u", compat_info_.map_items_major,
                compat_info_.map_items_minor);

    ImGui::Spacing();

    // Client info
    ImGui::Text("Loaded client: %u", compat_info_.client_version);
    ImGui::Text("Client provides: Items %u.%u", compat_info_.client_items_major,
                compat_info_.client_items_minor);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Warning message
    ImGui::TextWrapped("This map was created for a different client version. "
                       "Loading it with the current client may cause items "
                       "to display incorrectly or be missing.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons
    float button_width = 130.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button_width * 3 + spacing * 2;
    float offset = (ImGui::GetWindowWidth() - total_width) * 0.5f;

    ImGui::SetCursorPosX(offset);

    if (ImGui::Button(ICON_FA_XMARK " Cancel", ImVec2(button_width, 0))) {
      result_ = Result::Cancel;
      is_open_ = false;
    }

    ImGui::SameLine();

    // Force load button - warning color
    ImGui::PushStyleColor(ImGuiCol_Button, SC::WARNING);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, SC::WARNING);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, SC::WARNING);
    if (ImGui::Button(ICON_FA_BOLT " Force Load", ImVec2(button_width, 0))) {
      result_ = Result::ForceLoad;
      is_open_ = false;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    // Load with new client button - disabled/grayed out as placeholder
    ImGui::BeginDisabled(true);
    if (ImGui::Button(ICON_FA_ROTATE " New Client", ImVec2(button_width, 0))) {
      result_ = Result::LoadWithNewClient;
      is_open_ = false;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Load with matching client (coming soon)");
    }

    ImGui::EndPopup();
  }

  // Handle close via X button
  if (!is_open_) {
    if (result_ == Result::None) {
      result_ = Result::Cancel;
    }
  }
}

} // namespace UI
} // namespace MapEditor
