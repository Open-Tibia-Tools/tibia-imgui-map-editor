#include "RecentMapsPanel.h"
#include "UI/Core/Theme.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace MapEditor {
namespace UI {

namespace SC = SemanticColors;

namespace SC = SemanticColors;

void RecentMapsPanel::render(const std::vector<RecentMapEntry> &entries) {
  // Panel header
  ImGui::TextColored(SC::HEADER_TEXT, "Recent Maps List");
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Scrollable list of recent maps
  ImGui::BeginChild("##RecentMapsList", ImVec2(0, 0), false);

  for (size_t i = 0; i < entries.size(); ++i) {
    const auto &entry = entries[i];
    ImGui::PushID(static_cast<int>(i));

    bool is_selected = (selected_index_ == static_cast<int>(i));

    // Calculate item height for selectable
    float item_height = 60.0f;

    // Style for selected/hover
    if (is_selected) {
      ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
    } else {
      ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_Header));
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
    }

    // Grayed out if file doesn't exist
    if (!entry.exists) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, SC::DISABLED_ALPHA);
    }

    // Make selectable span full width
    ImVec2 item_size = ImVec2(ImGui::GetContentRegionAvail().x, item_height);

    if (ImGui::Selectable("##MapEntry", is_selected,
                          ImGuiSelectableFlags_AllowDoubleClick, item_size)) {
      selected_index_ = static_cast<int>(i);

      if (on_selection_) {
        on_selection_(static_cast<int>(i), entry);
      }

      // Double-click to load
      if (ImGui::IsMouseDoubleClicked(0) && load_enabled_ && on_double_click_) {
        on_double_click_(static_cast<int>(i), entry);
      }
    }

    // Draw content over the selectable (rewind cursor)
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - item_height);
    ImGui::Indent(8.0f);

    // Map icon (standard size)
    ImGui::BeginGroup();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, SC::INFO);
    ImGui::Text(ICON_FA_MAP);
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    ImGui::SameLine();

    // Map name and date
    ImGui::BeginGroup();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::TextColored(SC::VALUE, "%s",
                       entry.filename.c_str());
    ImGui::TextColored(SC::LABEL, "%s",
                       entry.last_modified.c_str());
    ImGui::EndGroup();

    ImGui::Unindent(8.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + item_height - 44);

    if (!entry.exists) {
      ImGui::PopStyleVar();
    }
    ImGui::PopStyleColor(2);

    ImGui::PopID();
    ImGui::Spacing();
  }

  if (entries.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(SC::LABEL, "No recent maps");
  }

  ImGui::EndChild();
}

} // namespace UI
} // namespace MapEditor
