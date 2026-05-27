#include "UnsavedChangesModal.h"
#include "UI/Core/Theme.h"
#include <imgui.h>
#include "ext/fontawesome6/IconsFontAwesome6.h"

namespace MapEditor {
namespace UI {

void UnsavedChangesModal::show(const std::string& map_name) {
    map_name_ = map_name;
    should_open_ = true;
}

UnsavedChangesModal::Result UnsavedChangesModal::render() {
    Result result = Result::None;
    
    // Open the modal if requested
    if (should_open_) {
        ImGui::OpenPopup("Unsaved Changes###UnsavedChangesModal");
        should_open_ = false;
        is_open_ = true;
    }
    
    // Center the modal
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Unsaved Changes###UnsavedChangesModal", nullptr, 
                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        // Warning icon and message
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION);
        ImGui::SameLine();
        ImGui::Text("The map \"%s\" has unsaved changes.", map_name_.c_str());
        ImGui::Spacing();
        ImGui::Text("Do you want to save before closing?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons - right aligned
        float button_width = 100.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float total_width = button_width * 3 + spacing * 2;
        float start_x = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);
        
        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save", ImVec2(button_width, 0))) {
            result = Result::Save;
            if (on_save_) on_save_();
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save changes to disk");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH " Discard", ImVec2(button_width, 0))) {
            result = Result::Discard;
            if (on_discard_) on_discard_();
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lose unsaved changes");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK " Cancel", ImVec2(button_width, 0))) {
            result = Result::Cancel;
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Keep editing (Esc)");
        
        // Allow Escape to cancel
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            result = Result::Cancel;
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        
        ImGui::EndPopup();
    } else {
        // Modal was closed externally
        if (is_open_) {
            is_open_ = false;
            result = Result::Cancel;
        }
    }
    
    return result;
}

} // namespace UI
} // namespace MapEditor
