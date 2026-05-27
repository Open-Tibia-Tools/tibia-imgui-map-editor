#include "RecentMapsPanel.h"
#include "UI/Core/Theme.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace MapEditor {
namespace UI {

namespace SC = SemanticColors;

// Custom list-item header colors for the dark-theme recent maps panel
constexpr ImVec4 kSelectedHeader       { 0.25f, 0.45f, 0.70f, 0.90f };
constexpr ImVec4 kSelectedHeaderHover  { 0.30f, 0.50f, 0.75f, 1.00f };
constexpr ImVec4 kUnselectedHeader     { 0.18f, 0.20f, 0.24f, 0.60f };
constexpr ImVec4 kUnselectedHeaderHover{ 0.22f, 0.25f, 0.30f, 0.80f };

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
      ImGui::PushStyleColor(ImGuiCol_Header, kSelectedHeader);
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kSelectedHeaderHover);
    } else {
      ImGui::PushStyleColor(ImGuiCol_Header, kUnselectedHeader);
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kUnselectedHeaderHover);
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
