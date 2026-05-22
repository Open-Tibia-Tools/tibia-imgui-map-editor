#include "ImportMapDialog.h"
#include "../../../Utils/StringCopy.h"
#include <imgui.h>
#include <nfd.hpp>
#include <cstring>
#include "ext/fontawesome6/IconsFontAwesome6.h"

namespace MapEditor {
namespace UI {

void ImportMapDialog::show() {
    should_open_ = true;
    // Reset options
    options_ = ImportOptions{};
    std::memset(path_buffer_, 0, sizeof(path_buffer_));
}

ImportMapDialog::Result ImportMapDialog::render() {
    Result result = Result::None;
    
    if (should_open_) {
        ImGui::OpenPopup("Import Map###ImportMapDialog");
        should_open_ = false;
        is_open_ = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Import Map###ImportMapDialog", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        // File selection
        ImGui::Text(ICON_FA_FILE " Map File");
        ImGui::PushItemWidth(-120);
        ImGui::InputText("##path", path_buffer_, sizeof(path_buffer_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse...")) {
            NFD::UniquePath outPath;
            nfdfilteritem_t filterItem[1] = {{"OTBM Files", "otbm"}};
            nfdresult_t nfdResult = NFD::OpenDialog(outPath, filterItem, 1);
            if (nfdResult == NFD_OKAY) {
                ::MapEditor::Utils::copyTruncate(path_buffer_, outPath.get());
                options_.source_path = path_buffer_;
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select .otbm map file");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Offset position
        ImGui::Text(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Import Offset");
        ImGui::TextDisabled("Position offset for imported tiles");
        
        int offset_x = options_.offset.x;
        int offset_y = options_.offset.y;
        int offset_z = options_.offset.z;
        
        ImGui::PushItemWidth(100);
        if (ImGui::InputInt("X", &offset_x)) {
            options_.offset.x = offset_x;
        }
        ImGui::SameLine();
        if (ImGui::InputInt("Y", &offset_y)) {
            options_.offset.y = offset_y;
        }
        ImGui::SameLine();
        if (ImGui::InputInt("Z", &offset_z)) {
            options_.offset.z = static_cast<int16_t>(std::clamp(offset_z, 0, 15));
        }
        ImGui::PopItemWidth();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Merge mode
        ImGui::Text(ICON_FA_LAYER_GROUP " Merge Mode");
        ImGui::RadioButton("Merge with existing tiles", !options_.overwrite_existing);
        if (ImGui::IsItemClicked()) options_.overwrite_existing = false;
        ImGui::RadioButton("Overwrite existing tiles", options_.overwrite_existing);
        if (ImGui::IsItemClicked()) options_.overwrite_existing = true;
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons
        bool can_import = !options_.source_path.empty() && 
                          std::filesystem::exists(options_.source_path);
        
        ImGui::BeginDisabled(!can_import);
        if (ImGui::Button(ICON_FA_FILE_IMPORT " Import", ImVec2(120, 0))) {
            options_.source_path = path_buffer_;
            result = Result::Confirmed;
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!can_import) {
                ImGui::SetTooltip("Select a map file first");
            } else {
                ImGui::SetTooltip("Start map import");
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK " Cancel", ImVec2(120, 0))) {
            result = Result::Cancelled;
            ImGui::CloseCurrentPopup();
            is_open_ = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Cancel import (Esc)");
        }
        
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
