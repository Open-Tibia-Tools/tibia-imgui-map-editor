#include "ConfirmationDialog.h"
#include "UI/Core/Theme.h"
#include <imgui.h>
#include <IconsFontAwesome6.h>

namespace MapEditor {
namespace UI {

void ConfirmationDialog::show(const std::string& title, 
                               const std::string& message,
                               const std::string& confirm_label) {
    title_ = title;
    message_ = message;
    confirm_label_ = confirm_label;
    should_open_ = true;
}

ConfirmationDialog::Result ConfirmationDialog::render() {
    Result result = Result::None;
    
    if (should_open_) {
        ImGui::OpenPopup(title_.c_str());
        should_open_ = false;
        is_open_ = true;
    }
    
    // Center dialog
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal(title_.c_str(), nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize)) {
        
        // Warning icon and message
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", message_.c_str());
        
        ImGui::Separator();
        
        // Buttons
        float button_width = 100.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float total_width = button_width * 2 + spacing;
        float start_x = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);
        
        if (ImGui::Button(confirm_label_.c_str(), ImVec2(button_width, 0))) {
            result = Result::Confirmed;
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Confirm action");
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
            result = Result::Cancelled;
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Cancel action (Esc)");
        }
        
        // Escape to cancel
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            result = Result::Cancelled;
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        
        ImGui::EndPopup();
    } else if (is_open_) {
        is_open_ = false;
        result = Result::Cancelled;
    }
    
    return result;
}

} // namespace UI
} // namespace MapEditor
