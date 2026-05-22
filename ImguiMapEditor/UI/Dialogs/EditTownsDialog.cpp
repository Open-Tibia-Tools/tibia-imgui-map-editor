#include "EditTownsDialog.h"
#include "../../Utils/StringCopy.h"
#include <imgui.h>
#include <IconsFontAwesome6.h>
#include <algorithm>
#include <ranges>

namespace MapEditor {
namespace UI {

void EditTownsDialog::show(Domain::ChunkedMap* map) {
    if (!map) return;
    
    map_ = map;
    should_open_ = true;
    is_picking_position_ = false;
    
    loadTownsFromMap();
}

EditTownsDialog::Result EditTownsDialog::render() {
    Result result = Result::None;
    
    if (should_open_) {
        is_open_ = true;
        should_open_ = false;
    }
    
    if (!is_open_) return result;
    
    // Center dialog on first appearance
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Edit Towns###EditTownsDialog", &is_open_,
                     ImGuiWindowFlags_NoCollapse)) {
        
        // === Town List ===
        ImGui::Text("Towns:");
        
        // Listbox with towns
        ImVec2 listbox_size(ImGui::GetContentRegionAvail().x, 150);
        if (ImGui::BeginListBox("##TownList", listbox_size)) {
            // C++20: Use iota for index-based iteration
            for (size_t i : std::views::iota(size_t(0), towns_.size())) {
                const auto& town = towns_[i];
                
                // Format: "ID: Name"
                char label[300];
                snprintf(label, sizeof(label), "%u: %s", town.id, town.name.c_str());
                
                bool is_selected = (static_cast<int>(i) == selected_index_);
                if (ImGui::Selectable(label, is_selected)) {
                    selected_index_ = static_cast<int>(i);
                    updateSelectionBuffers();
                }
            }
            ImGui::EndListBox();
        }
        
        // Add/Remove buttons
        if (ImGui::Button(ICON_FA_PLUS " Add")) {
            TownEntry new_town;
            new_town.id = next_town_id_++;
            new_town.name = "New Town";
            new_town.temple_position = Domain::Position(0, 0, 7);
            new_town.is_new = true;
            
            towns_.push_back(new_town);
            selected_index_ = static_cast<int>(towns_.size() - 1);
            updateSelectionBuffers();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create a new town entry");
        }
        
        ImGui::SameLine();
        
        bool can_remove = canRemoveSelectedTown();
        ImGui::BeginDisabled(!can_remove);
        if (ImGui::Button(ICON_FA_TRASH " Remove")) {
            if (selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size())) {
                show_delete_confirm_ = true;
                ImGui::OpenPopup("Remove Town?");
            }
        }
        ImGui::EndDisabled();
        
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (!can_remove && selected_index_ >= 0) {
                ImGui::SetTooltip("Cannot remove town with associated houses");
            } else {
                ImGui::SetTooltip("Delete selected town");
            }
        }

        // Delete Confirmation Modal
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("Remove Town?", &show_delete_confirm_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            if (selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size())) {
                ImGui::Text(ICON_FA_TRIANGLE_EXCLAMATION " Are you sure you want to remove:");
                ImGui::TextDisabled("ID %u: %s", towns_[selected_index_].id, towns_[selected_index_].name.c_str());
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

                if (ImGui::Button(ICON_FA_TRASH " Yes, Remove", ImVec2(120, 0))) {
                    towns_.erase(towns_.begin() + selected_index_);
                    if (selected_index_ >= static_cast<int>(towns_.size())) {
                        selected_index_ = static_cast<int>(towns_.size()) - 1;
                    }
                    updateSelectionBuffers();
                    show_delete_confirm_ = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    show_delete_confirm_ = false;
                    ImGui::CloseCurrentPopup();
                }
                // UX: Default focus on Cancel to prevent accidental enter-key deletion
                ImGui::SetItemDefaultFocus();
            } else {
                // Safety: selection invalid, close popup
                show_delete_confirm_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        ImGui::Separator();
        
        // === Edit Section ===
        bool has_selection = selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size());
        
        ImGui::BeginDisabled(!has_selection);
        
        // Town Name
        ImGui::Text("Name:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##TownName", name_buffer_, sizeof(name_buffer_))) {
            if (has_selection) {
                towns_[selected_index_].name = name_buffer_;
            }
        }
        
        // Town ID (read-only display)
        ImGui::Text("ID:");
        ImGui::SameLine(100);
        if (has_selection) {
            ImGui::Text("%u", towns_[selected_index_].id);
        } else {
            ImGui::TextDisabled("-");
        }
        
        ImGui::Separator();
        
        // Temple Position
        ImGui::Text("Temple Position:");
        
        ImGui::Text("X:");
        ImGui::SameLine(30);
        ImGui::SetNextItemWidth(80);
        if (ImGui::InputInt("##TempleX", &temple_x_, 0, 0)) {
            if (has_selection) {
                towns_[selected_index_].temple_position.x = temple_x_;
            }
        }
        
        ImGui::SameLine();
        ImGui::Text("Y:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::InputInt("##TempleY", &temple_y_, 0, 0)) {
            if (has_selection) {
                towns_[selected_index_].temple_position.y = temple_y_;
            }
        }
        
        ImGui::SameLine();
        ImGui::Text("Z:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        if (ImGui::InputInt("##TempleZ", &temple_z_, 0, 0)) {
            temple_z_ = std::clamp(temple_z_, 0, 15);
            if (has_selection) {
                towns_[selected_index_].temple_position.z = static_cast<int16_t>(temple_z_);
            }
        }
        
        // Pick from map button
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_CROSSHAIRS " Pick")) {
            if (on_pick_position_ && on_pick_position_()) {
                is_picking_position_ = true;
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click on map to set temple position");
        }
        
        // Go To button
        if (ImGui::Button(ICON_FA_LOCATION_DOT " Go To")) {
            if (has_selection && on_go_to_) {
                on_go_to_(towns_[selected_index_].temple_position);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Move camera to temple position");
        }
        
        ImGui::EndDisabled();
        
        // Show pick mode indicator
        if (is_picking_position_) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), 
                             ICON_FA_CROSSHAIRS " Click on map to select position...");
        }
        
        ImGui::Separator();
        
        // === OK / Cancel ===
        float button_width = 120.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float total_width = button_width * 2 + spacing;
        float start_x = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);
        
        if (ImGui::Button(ICON_FA_CHECK " OK", ImVec2(button_width, 0))) {
            applyChangesToMap();
            result = Result::Applied;
            is_open_ = false;
            is_picking_position_ = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Apply changes and close");
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button(ICON_FA_XMARK " Cancel", ImVec2(button_width, 0))) {
            result = Result::Cancelled;
            is_open_ = false;
            is_picking_position_ = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Discard changes (Esc)");
        }
        
        // Escape to close
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            result = Result::Cancelled;
            is_open_ = false;
            is_picking_position_ = false;
        }
    }
    
    ImGui::End();
    
    // Check if window was closed via X button
    if (!is_open_) {
        is_picking_position_ = false;
        if (result == Result::None) {
            result = Result::Cancelled;
        }
    }
    
    return result;
}

void EditTownsDialog::setPickedPosition(const Domain::Position& pos) {
    if (!is_picking_position_ || selected_index_ < 0 || 
        selected_index_ >= static_cast<int>(towns_.size())) {
        return;
    }
    
    towns_[selected_index_].temple_position = pos;
    temple_x_ = pos.x;
    temple_y_ = pos.y;
    temple_z_ = pos.z;
    is_picking_position_ = false;
}

void EditTownsDialog::loadTownsFromMap() {
    towns_.clear();
    selected_index_ = -1;
    next_town_id_ = 1;
    
    if (!map_) return;
    
    const auto& map_towns = map_->getTowns();
    towns_.reserve(map_towns.size());
    
    // Modern C++20: Use transform with lambda
    std::ranges::transform(map_towns, std::back_inserter(towns_), [](const auto& town) {
        return TownEntry{town.id, town.name, town.temple_position, false};
    });
    
    // Sort by ID for consistent display
    std::ranges::sort(towns_, {}, &TownEntry::id);

    if (!towns_.empty()) {
        selected_index_ = 0;
        updateSelectionBuffers();

        // Modern C++20: Use max_element with projection
        auto max_town = std::ranges::max_element(towns_, {}, &TownEntry::id);

        // Restore original "ratchet" logic: only update if higher than current
        if (max_town->id >= next_town_id_) {
            next_town_id_ = max_town->id + 1;
        }
    }
}

void EditTownsDialog::applyChangesToMap() {
    if (!map_) return;
    
    // Get current town IDs from map via view projection (C++20)
    // Use generic lambda to avoid namespace assumptions
    auto current_ids_view = map_->getTowns() | std::views::transform([](const auto& t) { return t.id; });
    
    // Identify deleted towns: IDs present in map but not in our list
    std::vector<uint32_t> ids_to_remove;
    
    // C++20: Use copy_if to filter IDs that are no longer present
    std::ranges::copy_if(current_ids_view, std::back_inserter(ids_to_remove), [this](uint32_t old_id) {
        return std::ranges::find(towns_, old_id, &TownEntry::id) == towns_.end();
    });

    // Apply removals
    for (uint32_t id : ids_to_remove) {
        map_->removeTown(id);
    }
    
    // Add/update remaining towns
    for (const auto& entry : towns_) {
        auto* existing = map_->getTown(entry.id);
        if (existing) {
            map_->updateTown(entry.id, entry.name, entry.temple_position);
        } else {
            map_->addTown(entry.id, entry.name, entry.temple_position);
        }
    }
}

void EditTownsDialog::updateSelectionBuffers() {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(towns_.size())) {
        const auto& town = towns_[selected_index_];
        ::MapEditor::Utils::copyTruncate(name_buffer_, town.name);
        temple_x_ = town.temple_position.x;
        temple_y_ = town.temple_position.y;
        temple_z_ = town.temple_position.z;
    } else {
        name_buffer_[0] = '\0';
        temple_x_ = 0;
        temple_y_ = 0;
        temple_z_ = 7;
    }
}

bool EditTownsDialog::canRemoveSelectedTown() const {
    if (selected_index_ < 0 || selected_index_ >= static_cast<int>(towns_.size())) {
        return false;
    }
    
    if (!map_) return true;
    
    uint32_t town_id = towns_[selected_index_].id;
    
    // Check if any house belongs to this town
    return !map_->hasTownWithHouses(town_id);
}

} // namespace UI
} // namespace MapEditor
