#include "AvailableClientsPanel.h"
#include <IconsFontAwesome6.h>
#include <algorithm>
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

    constexpr ImVec4 kGreenStatus = ImVec4(0.43f, 0.82f, 0.43f, 1.0f);
    constexpr ImVec4 kTextMuted    = ImVec4(0.67f, 0.70f, 0.75f, 1.0f);

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
        ImGui::PushStyleColor(ImGuiCol_Header,
                              ImVec4(0.25f, 0.45f, 0.70f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(0.30f, 0.50f, 0.75f, 1.0f));
      } else {
        ImGui::PushStyleColor(ImGuiCol_Header,
                              ImVec4(0.18f, 0.20f, 0.24f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(0.22f, 0.25f, 0.30f, 0.8f));
      }

      ImVec2 item_size = ImVec2(ImGui::GetContentRegionAvail().x, 44.0f);

      if (ImGui::Selectable("##ClientEntry", is_selected, 0, item_size)) {
        selected_client_index_ = index;
        if (on_selection_) {
          on_selection_(index);
        }
      }

      ImVec2 sel_min = ImGui::GetItemRectMin();

      const char* type_str = "???";
      switch (client->getDataSource()) {
        case Domain::ItemDataSource::OTB: type_str = "OTB"; break;
        case Domain::ItemDataSource::SRV: type_str = "SRV"; break;
        case Domain::ItemDataSource::DAT: type_str = "DAT"; break;
      }

      ImGui::SetCursorScreenPos(ImVec2(sel_min.x + 8.0f, sel_min.y + 4.0f));
      ImGui::TextColored(kGreenStatus, "%s", client->getName().c_str());

      ImGui::SetCursorScreenPos(ImVec2(sel_min.x + 8.0f, sel_min.y + 22.0f));
      ImGui::TextColored(kTextMuted, "%u | %s", client->getVersion(), type_str);

      ImGui::SetCursorScreenPos(ImVec2(sel_min.x, sel_min.y + item_size.y));

      ImGui::PopStyleColor(2);
      ImGui::PopID();
    }
  }

  if (total_count == 0) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.52f, 0.55f, 1.0f),
                       "No clients in database.");
    ImGui::TextColored(ImVec4(0.4f, 0.42f, 0.45f, 1.0f),
                       "Use 'Client Config' to add clients.");
  }

  ImGui::EndChild();
}

} // namespace UI
} // namespace MapEditor
