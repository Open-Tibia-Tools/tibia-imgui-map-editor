#include "ImportMonstersDialog.h"
#include "../../../Utils/StringCopy.h"
#include <imgui.h>
#include <nfd.hpp>
#include <cstring>
#include "ext/fontawesome6/IconsFontAwesome6.h"

namespace MapEditor {
namespace UI {

void ImportMonstersDialog::show() {
    should_open_ = true;
    options_ = ImportOptions{};
    std::memset(path_buffer_, 0, sizeof(path_buffer_));
}

ImportMonstersDialog::Result ImportMonstersDialog::render() {
    Result result = Result::None;
    
    if (should_open_) {
        ImGui::OpenPopup("Import Monsters/NPC###ImportMonstersDialog");
        should_open_ = false;
        is_open_ = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Import Monsters/NPC###ImportMonstersDialog", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        // File selection
        ImGui::Text(ICON_FA_FILE " Spawns File");
        ImGui::TextDisabled("Select a spawns.xml file to import");
        
        ImGui::PushItemWidth(-120);
        ImGui::InputText("##path", path_buffer_, sizeof(path_buffer_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse...")) {
            NFD::UniquePath outPath;
            nfdfilteritem_t filterItem[1] = {{"XML Files", "xml"}};
            nfdresult_t nfdResult = NFD::OpenDialog(outPath, filterItem, 1);
            if (nfdResult == NFD_OKAY) {
                ::MapEditor::Utils::copyTruncate(path_buffer_, outPath.get());
                options_.source_path = path_buffer_;
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Merge mode
        ImGui::Text(ICON_FA_CODE_MERGE " Merge Mode");
        
        int mode = static_cast<int>(options_.merge_mode);
        if (ImGui::RadioButton("Replace all spawns", mode == 0)) {
            options_.merge_mode = MergeMode::ReplaceAll;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Clear all existing spawns and replace with imported ones");
        }
        
        if (ImGui::RadioButton("Merge with existing", mode == 1)) {
            options_.merge_mode = MergeMode::Merge;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Add imported spawns to existing spawns");
        }
        
        if (ImGui::RadioButton("Skip duplicates", mode == 2)) {
            options_.merge_mode = MergeMode::SkipDuplicates;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Only import spawns at positions without existing spawns");
        }
        
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
                ImGui::SetTooltip("Select a file to import first");
            } else {
                ImGui::SetTooltip("Start import process");
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_BAN " Cancel", ImVec2(120, 0))) {
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
