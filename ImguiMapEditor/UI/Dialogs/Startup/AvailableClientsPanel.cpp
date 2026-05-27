#include "AvailableClientsPanel.h"
#include "UI/Core/Theme.h"
#include <IconsFontAwesome6.h>
#include <filesystem>
#include <imgui.h>

namespace MapEditor {
namespace UI {

namespace SC = SemanticColors;

// Custom list-item header colors for the dark-theme clients panel
constexpr ImVec4 kSelectedHeader       { 0.25f, 0.45f, 0.70f, 0.90f };
constexpr ImVec4 kSelectedHeaderHover  { 0.30f, 0.50f, 0.75f, 1.00f };
constexpr ImVec4 kUnselectedHeader     { 0.18f, 0.20f, 0.24f, 0.60f };
constexpr ImVec4 kUnselectedHeaderHover{ 0.22f, 0.25f, 0.30f, 0.80f };

void AvailableClientsPanel::render() {
  ImGui::TextColored(SC::HEADER_TEXT, "Available Clients");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::BeginChild("##ClientsList", ImVec2(0, 0), false);

  int total_count = 0;
  if (registry_) {
    auto all_versions = registry_->getAllVersions();

    for (const auto *client : all_versions) {
      if (!client)
        continue;

      if (client->getMetadataFile().empty() || client->getSpritesFile().empty())
        continue;
      if (!std::filesystem::exists(client->getMetadataFile()) || !std::filesystem::exists(client->getSpritesFile()))
        continue;

      total_count++;
      uint32_t index = client->getIndex();
      bool is_selected = (selected_client_index_ == index);

      ImGui::PushID(static_cast<int>(index));

      if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Header, kSelectedHeader);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kSelectedHeaderHover);
      } else {
        ImGui::PushStyleColor(ImGuiCol_Header, kUnselectedHeader);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kUnselectedHeaderHover);
      }

      float item_height = 60.0f;
      ImVec2 item_size = ImVec2(ImGui::GetContentRegionAvail().x, item_height);

      if (ImGui::Selectable("##ClientEntry", is_selected,
                            ImGuiSelectableFlags_AllowDoubleClick, item_size)) {
        selected_client_index_ = index;
        if (on_selection_) {
          on_selection_(index);
        }
      }

      ImGui::SetCursorPosY(ImGui::GetCursorPosY() - item_height);
      ImGui::Indent(8.0f);

      // Bookmark icon
      ImGui::BeginGroup();
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f);
      ImGui::PushStyleColor(ImGuiCol_Text, SC::GOLD);
      ImGui::Text(ICON_FA_BOOKMARK);
      ImGui::PopStyleColor();
      ImGui::EndGroup();

      ImGui::SameLine();

      // Client name and version info
      ImGui::BeginGroup();
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
      ImGui::TextColored(SC::SAVED, "%s",
                         client->getName().c_str());

      const char* type_str = "???";
      switch (client->getDataSource()) {
        case Domain::ItemDataSource::OTB: type_str = "OTB"; break;
        case Domain::ItemDataSource::SRV: type_str = "SRV"; break;
        case Domain::ItemDataSource::DAT: type_str = "DAT"; break;
      }
      ImGui::TextColored(SC::LABEL, "%u | %s",
                         client->getVersion(), type_str);
      ImGui::EndGroup();

      ImGui::Unindent(8.0f);
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + item_height - 44);

      ImGui::PopStyleColor(2);
      ImGui::PopID();
      ImGui::Spacing();
    }
  }

  if (total_count == 0) {
    ImGui::Spacing();
    ImGui::TextColored(SC::LABEL, "No clients in database.");
    ImGui::TextColored(SC::EMPTY,
                       "Use 'Client Config' to add clients.");
  }

  ImGui::EndChild();
}

} // namespace UI
} // namespace MapEditor
