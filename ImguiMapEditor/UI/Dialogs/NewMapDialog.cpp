#include "NewMapDialog.h"
#include "Core/Config.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include <imgui.h>

namespace MapEditor {
namespace UI {

void NewMapDialog::initialize(Services::ClientVersionRegistry *registry) {
  panel_.initialize(registry);
}

void NewMapDialog::show() {
  visible_ = true;
  state_ = {};
  state_.description = "Made with Tibia Imgui Map Editor!";
  panel_.reset();
}

void NewMapDialog::render() {
  if (!visible_)
    return;

  ImGui::OpenPopup("New Map##EditorModal");

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(
      ImVec2(Config::UI::NEW_MAP_DIALOG_W, Config::UI::NEW_MAP_DIALOG_H),
      ImGuiCond_Appearing);

  // Match startup dialog dark styling
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.12f, 0.14f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.16f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

  if (ImGui::BeginPopupModal("New Map##EditorModal", nullptr,
                              ImGuiWindowFlags_NoResize)) {

    // === TABS ===
    if (ImGui::BeginTabBar("##NewMapTabs")) {
      if (ImGui::BeginTabItem("OTBM")) {
        ImGui::Spacing();
        panel_.render(state_);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("SEC")) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.0f),
                           ICON_FA_CLOCK " Coming soon");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 0.42f, 0.45f, 1.0f),
                           "SEC format support is not yet implemented.");
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // === FOOTER ===
    float button_width = Config::UI::MODAL_BUTTON_W;
    float total_width = button_width * 2 + 10.0f;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_width) / 2.0f);

    if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
      visible_ = false;
      state_ = {};
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine(0, 10.0f);

    bool can_create =
        !state_.map_name.empty() && state_.selected_template_index >= 0;

    if (!can_create) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.70f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.28f, 0.55f, 0.80f, 1.0f));

    if (ImGui::Button(ICON_FA_CHECK " Create Map", ImVec2(button_width, 0)) &&
        can_create) {
      visible_ = false;
      if (on_confirm_) {
        on_confirm_(state_);
      }
      state_ = {};
      ImGui::CloseCurrentPopup();
    }

    ImGui::PopStyleColor(2);

    if (!can_create) {
      ImGui::PopStyleVar();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (state_.map_name.empty()) {
          ImGui::SetTooltip("Enter a map name");
        } else {
          ImGui::SetTooltip("Select a client version first");
        }
      }
    }

    ImGui::EndPopup();
  }

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);
}

} // namespace UI
} // namespace MapEditor
