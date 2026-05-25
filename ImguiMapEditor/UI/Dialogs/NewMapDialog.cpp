#include "NewMapDialog.h"
#include "Core/Config.h"
#include <imgui.h>

namespace MapEditor {
namespace UI {

void NewMapDialog::initialize(Services::ClientVersionRegistry* registry) {
  panel_.initialize(registry, nullptr);
}

void NewMapDialog::show() {
  visible_ = true;
  state_ = {}; // Reset state for new dialog
}

void NewMapDialog::render() {
  if (!visible_) return;

  ImGui::OpenPopup("New Map##EditorModal");

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(Config::UI::NEW_MAP_DIALOG_W, Config::UI::NEW_MAP_DIALOG_H), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("New Map##EditorModal", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f),
                       "Configure your new map:");
    ImGui::Separator();
    ImGui::Spacing();

    panel_.render(state_);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Action buttons
    float button_width = Config::UI::MODAL_BUTTON_W;
    float total_width = button_width * 2 + 10.0f;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_width) / 2.0f);

    if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
      visible_ = false;
      state_ = {};
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine(0, 10.0f);

    bool can_create = state_.selected_client_index > 0;
    if (!can_create) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }

    if (ImGui::Button("Create Map", ImVec2(button_width, 0)) && can_create) {
      visible_ = false;
      if (on_confirm_) {
        on_confirm_(state_);
      }
      state_ = {};
      ImGui::CloseCurrentPopup();
    }

    if (!can_create) {
      ImGui::PopStyleVar();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Select a client version first");
      }
    }

    ImGui::EndPopup();
  }
}

} // namespace UI
} // namespace MapEditor
