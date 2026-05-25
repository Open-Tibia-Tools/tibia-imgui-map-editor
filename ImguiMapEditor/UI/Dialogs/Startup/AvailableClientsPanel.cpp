#include "AvailableClientsPanel.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace MapEditor {
namespace UI {

void AvailableClientsPanel::render() {
  ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.92f, 1.0f), "Available Clients");
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

      total_count++;
      uint32_t index = client->getIndex();
      bool is_selected = (selected_client_index_ == index);

      ImGui::PushID(static_cast<int>(index));

      if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.45f, 0.70f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(0.30f, 0.50f, 0.75f, 1.0f));
      } else {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.20f, 0.24f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(0.22f, 0.25f, 0.30f, 0.8f));
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
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.65f, 0.30f, 1.0f));
      ImGui::Text(ICON_FA_BOOKMARK);
      ImGui::PopStyleColor();
      ImGui::EndGroup();

      ImGui::SameLine();

      // Client name and version info
      ImGui::BeginGroup();
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
      ImGui::TextColored(ImVec4(0.43f, 0.82f, 0.43f, 1.0f), "%s",
                         client->getName().c_str());

      const char* type_str = "???";
      switch (client->getDataSource()) {
        case Domain::ItemDataSource::OTB: type_str = "OTB"; break;
        case Domain::ItemDataSource::SRV: type_str = "SRV"; break;
        case Domain::ItemDataSource::DAT: type_str = "DAT"; break;
      }
      ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.0f), "%u | %s",
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
    ImGui::TextColored(ImVec4(0.5f, 0.52f, 0.55f, 1.0f), "No clients in database.");
    ImGui::TextColored(ImVec4(0.4f, 0.42f, 0.45f, 1.0f),
                       "Use 'Client Config' to add clients.");
  }

  ImGui::EndChild();
}

} // namespace UI
} // namespace MapEditor
