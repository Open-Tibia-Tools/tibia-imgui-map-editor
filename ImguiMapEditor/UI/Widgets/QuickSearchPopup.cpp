#include "QuickSearchPopup.h"
#include <algorithm>
#include <string_view>
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include "Services/ItemPickerService.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "Rendering/Core/Texture.h"
#include "UI/Utils/UIUtils.hpp"
#include "UI/Utils/PreviewUtils.hpp"
#include "Core/Config.h"

namespace MapEditor::UI {

QuickSearchPopup::QuickSearchPopup() {
    search_buffer_[0] = '\0';
}

void QuickSearchPopup::open() {
    is_open_ = true;
    focus_input_ = true;
    search_buffer_[0] = '\0';
    last_query_.clear();
    results_.clear();
    selected_index_ = 0;
}

void QuickSearchPopup::close() {
    is_open_ = false;
}

void QuickSearchPopup::render() {
    if (!is_open_) return;
    
    // Center popup
    ImVec2 const center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.3f));
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);
    
    ImGuiWindowFlags const flags = ImGuiWindowFlags_NoTitleBar | 
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_AlwaysAutoResize;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    
    if (ImGui::Begin("##QuickSearch", &is_open_, flags)) {
        // Search input
        ImGui::PushItemWidth(-1);
        
        if (focus_input_) {
            ImGui::SetKeyboardFocusHere();
            focus_input_ = false;
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
        bool const input_changed = ImGui::InputTextWithHint(
            "##SearchInput", 
            ICON_FA_MAGNIFYING_GLASS " Search items by name or ID...",
            search_buffer_, 
            sizeof(search_buffer_),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        ImGui::PopStyleVar();
        
        if (input_changed) {
            selectCurrent();
        }
        
        // Tooltip for the input field
        Utils::SetTooltipOnHover(ICON_FA_KEYBOARD " Type to search, use Up/Down arrows to navigate, Enter to select");

        ImGui::PopItemWidth();
        
        // Handle keyboard
        handleKeyboardNavigation();
        
        // Update search if query changed
        std::string const current_query(search_buffer_);
        if (current_query != last_query_) {
            last_query_ = current_query;
            doSearch();
        }
        
        // Results list
        if (!results_.empty()) {
            ImGui::Separator();
            
            for (size_t i = 0; i < std::min(results_.size(), MAX_VISIBLE_RESULTS); ++i) {
                const auto& result = results_[i];
                bool const is_selected = (static_cast<int>(i) == selected_index_);
                
                ImGui::PushID(static_cast<int>(i));
                
                // Selectable row
                if (ImGui::Selectable("##Row", is_selected, 0, ImVec2(0, 32))) {
                    selected_index_ = static_cast<int>(i);
                    selectCurrent();
                }
                
                // Show preview on hover (moved to cover the entire row)
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();

                    bool rendered = false;
                    if (sprite_manager_ && client_data_) {
                        if (result.is_creature) {
                            if (auto preview = Utils::GetCreaturePreview(*client_data_, *sprite_manager_, result.name)) {
                                ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(preview.texture->id())), ImVec2(preview.size, preview.size));
                                rendered = true;
                            }
                        } else if (const auto* item_type = client_data_->getItemTypeByServerId(result.server_id)) {
                            if (auto* texture = Utils::GetItemPreview(*sprite_manager_, item_type)) {
                                const float size = static_cast<float>(std::max(item_type->width, item_type->height)) * Config::UI::PREVIEW_TILE_SIZE;
                                ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(texture->id())), ImVec2(size, size));
                                rendered = true;
                            }
                        }
                    }

                    if (!rendered) {
                        ImGui::TextDisabled("No preview available");
                    }
                    ImGui::TextDisabled(result.is_creature ? "Double-click to place creature" : "Double-click to place item");
                    ImGui::EndTooltip();
                }

                // Draw content on same line
                ImGui::SameLine(8);

                // Selection indicator
                if (is_selected) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_FA_ARROW_POINTER);
                } else {
                    // Reserve space to prevent jitter
                    ImGui::Dummy(ImVec2(ImGui::CalcTextSize(ICON_FA_ARROW_POINTER).x, 0));
                }
                ImGui::SameLine();

                // Icon placeholder (could use sprite later)
                if (result.is_creature) {
                    ImGui::Text(ICON_FA_DRAGON);
                } else {
                    ImGui::Text(ICON_FA_CUBE);
                }
                
                ImGui::SameLine();
                ImGui::Text("%s", result.name.c_str());
                
                ImGui::SameLine(ImGui::GetWindowWidth() - 80);
                ImGui::TextDisabled("ID: %u", result.server_id);
                
                ImGui::PopID();
            }
            
            // Show count if more results
            if (results_.size() > MAX_VISIBLE_RESULTS) {
                ImGui::Separator();
                ImGui::TextDisabled("... and %zu more", 
                    results_.size() - MAX_VISIBLE_RESULTS);
            }
        } else if (std::string_view(search_buffer_).length() >= 2) {
            ImGui::Separator();
            ImGui::TextDisabled(ICON_FA_CIRCLE_EXCLAMATION " No results found");
        } else if (!std::string_view(search_buffer_).empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Type at least 2 characters");
        }
        
        // Help text
        ImGui::Separator();
        ImGui::TextDisabled(ICON_FA_KEYBOARD " Enter: select | " 
                           ICON_FA_ARROW_UP ICON_FA_ARROW_DOWN " Navigate | Esc: close");
    }
    ImGui::End();
    
    ImGui::PopStyleVar(2);
    
    // Close on Escape
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }
}

void QuickSearchPopup::doSearch() {
    results_.clear();
    selected_index_ = 0;
    
    if (!picker_ || std::string_view(search_buffer_).length() < 2) {
        return;
    }
    
    results_ = picker_->search(search_buffer_, SEARCH_LIMIT);
}

void QuickSearchPopup::handleKeyboardNavigation() {
    if (results_.empty()) return;
    
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        selected_index_ = std::min(selected_index_ + 1, 
            static_cast<int>(std::min(results_.size(), MAX_VISIBLE_RESULTS)) - 1);
    }
    
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        selected_index_ = std::max(selected_index_ - 1, 0);
    }
}

void QuickSearchPopup::selectCurrent() {
    if (results_.empty() || selected_index_ < 0 || 
        selected_index_ >= static_cast<int>(results_.size())) {
        return;
    }
    
    const auto& result = results_[selected_index_];
    
    if (on_select_) {
        on_select_(result.server_id, result.is_creature);
    }
    
    close();
}

} // namespace MapEditor::UI
