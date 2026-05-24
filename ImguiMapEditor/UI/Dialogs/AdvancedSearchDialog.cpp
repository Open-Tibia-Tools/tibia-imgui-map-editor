#include "AdvancedSearchDialog.h"
#include "UI/Widgets/SearchResultsWidget.h"
#include "Services/Map/MapSearchService.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "Domain/ItemType.h"
#include "Domain/CreatureType.h"
#include "IO/Readers/DatReaderBase.h"
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include "Rendering/Core/Texture.h"
#include "Presentation/NotificationHelper.h"
#include "UI/Utils/PreviewUtils.hpp"
#include <algorithm>
#include <cctype>

namespace MapEditor::UI {

// PreviewResult helpers
std::string PreviewResult::getDisplayName() const {
    if (is_creature && creature) {
        return creature->name;
    }
    if (item) {
        return item->name.empty() ? "(unnamed)" : item->name;
    }
    return "(unknown)";
}

uint16_t PreviewResult::getServerId() const {
    if (item) return item->server_id;
    return 0;
}

AdvancedSearchDialog::AdvancedSearchDialog() {
    search_buffer_[0] = '\0';
}

void AdvancedSearchDialog::render() {
    if (!is_open_) return;
    
    ImGui::SetNextWindowSize(ImVec2(800, 550), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin(ICON_FA_MAGNIFYING_GLASS_PLUS " Advanced Search###AdvancedSearch", &is_open_)) {
        
        // === 4 COLUMNS ===
        if (ImGui::BeginTable("SearchColumns", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInner)) {
            ImGui::TableSetupColumn("FindBy", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Types", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Results", ImGuiTableColumnFlags_WidthStretch, 1.2f);

            ImGui::TableNextRow();

            // Column 1: Find By
            ImGui::TableSetColumnIndex(0);
            renderFindByColumn();

            // Column 2: Types
            ImGui::TableSetColumnIndex(1);
            renderTypesColumn();

            // Column 3: Properties
            ImGui::TableSetColumnIndex(2);
            renderPropertiesColumn();

            // Column 4: Results
            ImGui::TableSetColumnIndex(3);
            renderResultsColumn();

            ImGui::EndTable();
        }
        
        ImGui::Separator();
        
        // Bottom bar with buttons
        renderBottomBar();
    }
    ImGui::End();
    
    // Close on Escape
    if (is_open_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }
}

void AdvancedSearchDialog::renderFindByColumn() {
    ImGui::BeginChild("FindBy", ImVec2(0, -40), true);
    
    ImGui::Text(ICON_FA_MAGNIFYING_GLASS " Find By");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Search input
    if (focus_input_) {
        ImGui::SetKeyboardFocusHere();
        focus_input_ = false;
    }
    
    ImGui::PushItemWidth(-1);
    if (ImGui::InputTextWithHint("##SearchInput", "Name or ID...", 
                                  search_buffer_, sizeof(search_buffer_))) {
        filters_changed_ = true;
    }
    ImGui::PopItemWidth();
    
    ImGui::Spacing();
    ImGui::TextWrapped(ICON_FA_CIRCLE_INFO " Searches by:");
    ImGui::BulletText("Name (fuzzy)");
    ImGui::BulletText("Server ID");
    ImGui::BulletText("Client ID");
    
    ImGui::Spacing();
    ImGui::TextDisabled("Leave empty to search\nby Types/Properties only");
    
    ImGui::EndChild();
}

void AdvancedSearchDialog::renderTypesColumn() {
    ImGui::BeginChild("Types", ImVec2(0, -40), true);
    
    ImGui::Text(ICON_FA_CUBES " Types");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextDisabled("(OR logic)");
    ImGui::Spacing();
    
    if (ImGui::Checkbox("Depot", &type_filter_.depot)) filters_changed_ = true;
    if (ImGui::Checkbox("Mailbox", &type_filter_.mailbox)) filters_changed_ = true;
    if (ImGui::Checkbox("Trash Holder", &type_filter_.trash_holder)) filters_changed_ = true;
    if (ImGui::Checkbox("Container", &type_filter_.container)) filters_changed_ = true;
    if (ImGui::Checkbox("Door", &type_filter_.door)) filters_changed_ = true;
    if (ImGui::Checkbox("Magic Field", &type_filter_.magic_field)) filters_changed_ = true;
    if (ImGui::Checkbox("Teleport", &type_filter_.teleport)) filters_changed_ = true;
    if (ImGui::Checkbox("Bed", &type_filter_.bed)) filters_changed_ = true;
    if (ImGui::Checkbox("Key", &type_filter_.key)) filters_changed_ = true;
    if (ImGui::Checkbox("Podium", &type_filter_.podium)) filters_changed_ = true;

    ImGui::Separator();
    ImGui::TextDisabled("Combat");
    if (ImGui::Checkbox("Weapon", &type_filter_.weapon)) filters_changed_ = true;
    if (ImGui::Checkbox("Ammo", &type_filter_.ammo)) filters_changed_ = true;
    if (ImGui::Checkbox("Armor", &type_filter_.armor)) filters_changed_ = true;
    if (ImGui::Checkbox("Rune", &type_filter_.rune)) filters_changed_ = true;
    
    ImGui::Separator();
    if (ImGui::Checkbox("Creature", &type_filter_.creature)) filters_changed_ = true;
    
    ImGui::EndChild();
}

void AdvancedSearchDialog::renderPropertiesColumn() {
    ImGui::BeginChild("Properties", ImVec2(0, -40), true);
    
    ImGui::Text(ICON_FA_SLIDERS " Properties");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextDisabled("(AND logic)");
    ImGui::Spacing();
    
    if (ImGui::Checkbox("Unpassable", &property_filter_.unpassable)) filters_changed_ = true;
    if (ImGui::Checkbox("Unmovable", &property_filter_.unmovable)) filters_changed_ = true;
    if (ImGui::Checkbox("Block Missiles", &property_filter_.block_missiles)) filters_changed_ = true;
    if (ImGui::Checkbox("Block Pathfinder", &property_filter_.block_pathfinder)) filters_changed_ = true;
    if (ImGui::Checkbox("Has Elevation", &property_filter_.has_elevation)) filters_changed_ = true;
    if (ImGui::Checkbox("Floor Change", &property_filter_.floor_change)) filters_changed_ = true;
    if (ImGui::Checkbox("Full Tile", &property_filter_.full_tile)) filters_changed_ = true;

    ImGui::Separator();
    ImGui::TextDisabled("Interaction");
    if (ImGui::Checkbox("Readable", &property_filter_.readable)) filters_changed_ = true;
    if (ImGui::Checkbox("Writeable", &property_filter_.writeable)) filters_changed_ = true;
    if (ImGui::Checkbox("Pickupable", &property_filter_.pickupable)) filters_changed_ = true;
    if (ImGui::Checkbox("Force Use", &property_filter_.force_use)) filters_changed_ = true;
    if (ImGui::Checkbox("Dist Read", &property_filter_.allow_dist_read)) filters_changed_ = true;
    if (ImGui::Checkbox("Rotatable", &property_filter_.rotatable)) filters_changed_ = true;
    if (ImGui::Checkbox("Hangable", &property_filter_.hangable)) filters_changed_ = true;
    
    ImGui::Separator();
    ImGui::TextDisabled("Visuals/Misc");
    if (ImGui::Checkbox("Has Light", &property_filter_.has_light)) filters_changed_ = true;
    if (ImGui::Checkbox("Animation", &property_filter_.animation)) filters_changed_ = true;
    if (ImGui::Checkbox("Always Top", &property_filter_.always_on_top)) filters_changed_ = true;
    if (ImGui::Checkbox("Ignore Look", &property_filter_.ignore_look)) filters_changed_ = true;
    if (ImGui::Checkbox("Has Charges", &property_filter_.has_charges)) filters_changed_ = true;
    if (ImGui::Checkbox("Client Charges", &property_filter_.client_charges)) filters_changed_ = true;
    if (ImGui::Checkbox("Decays", &property_filter_.decays)) filters_changed_ = true;
    if (ImGui::Checkbox("Has Speed", &property_filter_.has_speed)) filters_changed_ = true;
    
    ImGui::EndChild();
}

void AdvancedSearchDialog::renderResultsColumn() {
    // Update preview if filters changed
    if (filters_changed_) {
        updatePreviewResults();
        filters_changed_ = false;
    }
    
    ImGui::BeginChild("Results", ImVec2(0, -40), true);
    
    ImGui::Text(ICON_FA_LIST " Result (%zu)", preview_results_.size());
    ImGui::Separator();
    ImGui::Spacing();
    
    if (preview_results_.empty()) {
        ImGui::TextDisabled("No matching items");
        ImGui::TextDisabled("Enter search term or");
        ImGui::TextDisabled("select filters");
    } else {
        ImGui::TextDisabled("Double-click to search map");
        ImGui::Spacing();
        
        // Scrollable list of results
        ImGui::BeginChild("ResultsList", ImVec2(0, 0), false);
        
        constexpr float SPRITE_SIZE = 24.0f;
        constexpr float ROW_HEIGHT = 28.0f;
        
        for (size_t i = 0; i < preview_results_.size(); ++i) {
            const auto& result = preview_results_[i];
            bool const is_selected = (static_cast<int>(i) == selected_preview_index_);
            
            ImGui::PushID(static_cast<int>(i));
            
            // Selectable row with fixed height for sprite
            if (ImGui::Selectable("##Row", is_selected, ImGuiSelectableFlags_AllowDoubleClick, 
                                  ImVec2(0, ROW_HEIGHT))) {
                selected_preview_index_ = static_cast<int>(i);
                
                // Double-click = auto search map
                if (ImGui::IsMouseDoubleClicked(0)) {
                    onSearchMap();
                }
            }
            
            // Draw content on same line (sprite + text)
            ImGui::SameLine(4);
            
            // Sprite thumbnail
            bool sprite_rendered = false;
            if (sprite_manager_ && client_data_) {
                if (result.is_creature && result.creature) {
                    // Creature sprite
                    auto preview = Utils::GetCreaturePreview(*client_data_, *sprite_manager_, result.creature->outfit);
                    if (preview && preview.texture) {
                        // AdvancedSearchDialog forces SPRITE_SIZE (24.0f) for list alignment, ignoring recommended size
                        ImGui::Image((void*)(intptr_t)preview.texture->id(), ImVec2(SPRITE_SIZE, SPRITE_SIZE));
                        sprite_rendered = true;
                    }
                } else if (result.item) {
                    // Item sprite
                    if (auto* texture = Utils::GetItemPreview(*sprite_manager_, result.item)) {
                        ImGui::Image((void*)(intptr_t)texture->id(), ImVec2(SPRITE_SIZE, SPRITE_SIZE));
                        sprite_rendered = true;
                    }
                }
            }
            
            // Fallback icon if no sprite
            if (!sprite_rendered) {
                ImGui::Dummy(ImVec2(SPRITE_SIZE, SPRITE_SIZE));
                ImGui::SameLine(4);
                if (result.is_creature) {
                    ImGui::Text(ICON_FA_DRAGON);
                } else {
                    ImGui::Text(ICON_FA_CUBE);
                }
            }
            
            ImGui::SameLine();
            
            // Text label
            if (result.is_creature) {
                ImGui::Text("%s", result.getDisplayName().c_str());
            } else {
                ImGui::Text("[%u] %s", result.getServerId(), result.getDisplayName().c_str());
            }
            
            // Tooltip with more details
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                if (result.is_creature && result.creature) {
                    ImGui::Text("Creature: %s", result.creature->name.c_str());
                    ImGui::Text("LookType: %u", result.creature->outfit.lookType);
                } else if (result.item) {
                    ImGui::Text("Server ID: %u", result.item->server_id);
                    ImGui::Text("Client ID: %u", result.item->client_id);
                    if (!result.item->name.empty()) {
                        ImGui::Text("Name: %s", result.item->name.c_str());
                    }
                }
                ImGui::EndTooltip();
            }

            // Right-align the copy button
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 30);

            if (ImGui::Button(ICON_FA_COPY)) {
                if (result.is_creature && result.creature) {
                    ImGui::SetClipboardText(result.creature->name.c_str());
                    Presentation::showSuccess("Creature name copied");
                } else if (result.item) {
                    ImGui::SetClipboardText(std::to_string(result.item->server_id).c_str());
                    Presentation::showSuccess("Item ID copied");
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Copy ID/Name to clipboard");
            }
            
            ImGui::PopID();
        }
        
        ImGui::EndChild();
    }
    
    ImGui::EndChild();
}

void AdvancedSearchDialog::renderBottomBar() {
    ImGui::Spacing();
    
    float const button_width = 120.0f;
    float const total_buttons_width = button_width * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
    float const start_x = (ImGui::GetContentRegionAvail().x - total_buttons_width) * 0.5f;
    
    ImGui::SetCursorPosX(start_x);
    
    // Search Map button
    bool const can_search = search_service_ && selected_preview_index_ >= 0;
    ImGui::BeginDisabled(!can_search);
    if (ImGui::Button(ICON_FA_MAP " Search Map", ImVec2(button_width, 0))) {
        onSearchMap();
    }
    ImGui::EndDisabled();
    
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !can_search) {
        ImGui::SetTooltip("Select an item from results first");
    }
    
    ImGui::SameLine();
    
    // Select Item button (placeholder)
    ImGui::BeginDisabled(true);  // Always disabled - placeholder
    if (ImGui::Button(ICON_FA_HAND_POINTER " Select Item", ImVec2(button_width, 0))) {
        onSelectItem();
    }
    ImGui::EndDisabled();
    
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Select item as brush (Coming Soon)");
    }
    
    ImGui::SameLine();
    
    // Cancel button
    if (ImGui::Button(ICON_FA_XMARK " Cancel", ImVec2(button_width, 0))) {
        close();
    }
}

void AdvancedSearchDialog::updatePreviewResults() {
    preview_results_.clear();
    selected_preview_index_ = -1;
    
    // Only search if we have query or filters
    bool const has_query = strlen(search_buffer_) > 0;
    bool const has_type_filter = type_filter_.hasAnySelected();
    bool const has_property_filter = property_filter_.hasAnySelected();
    
    if (!has_query && !has_type_filter && !has_property_filter) {
        return;  // No filters, no results
    }
    
    std::string query_lower = search_buffer_;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    // Search items (only if not creature-only filter)
    bool const search_items = !type_filter_.creature || type_filter_.hasAnySelected();
    if (search_items && search_service_ && !property_filter_.hasAnySelected() || !type_filter_.creature) {
        // If only creature is selected, skip item search
        if (!(type_filter_.creature && !type_filter_.depot && !type_filter_.mailbox && 
              !type_filter_.trash_holder && !type_filter_.container && !type_filter_.door &&
              !type_filter_.magic_field && !type_filter_.teleport && !type_filter_.bed &&
              !type_filter_.key && !type_filter_.podium)) {
            
            auto item_results = search_service_->searchItemDatabase(
                search_buffer_,
                type_filter_,
                property_filter_,
                10000  // No practical limit
            );
            
            for (const auto* item : item_results) {
                PreviewResult result;
                result.is_creature = false;
                result.item = item;
                preview_results_.push_back(result);
            }
        }
    }
    
    // Search creatures (if creature type selected OR query and no type filter)
    bool const search_creatures = type_filter_.creature || (!has_type_filter && has_query);
    if (search_creatures && client_data_ && !property_filter_.hasAnySelected()) {
        const auto& creatures = client_data_->getCreatures();
        
        for (const auto& creature_ptr : creatures) {
            if (!creature_ptr) continue;
            const auto* creature = creature_ptr.get();
            
            // Match by name if we have a query
            if (has_query) {
                std::string name_lower = creature->name;
                std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                
                if (name_lower.find(query_lower) == std::string::npos) {
                    continue;
                }
            }
            
            PreviewResult result;
            result.is_creature = true;
            result.creature = creature;
            preview_results_.push_back(result);
        }
    }
}

void AdvancedSearchDialog::onSearchMap() {
    if (!search_service_ || selected_preview_index_ < 0 || 
        selected_preview_index_ >= static_cast<int>(preview_results_.size())) {
        return;
    }
    
    const auto& selected = preview_results_[selected_preview_index_];
    
    std::vector<Domain::Search::MapSearchResult> results;
    
    if (selected.is_creature && selected.creature) {
        // Search map for this creature by name
        results = search_service_->search(
            selected.creature->name,
            Services::MapSearchMode::ByName,
            false,  // search_items
            true,   // search_creatures
            1000
        );
    } else if (selected.item) {
        // Search map for this item by server ID
        results = search_service_->search(
            std::to_string(selected.item->server_id),
            Services::MapSearchMode::ByServerId,
            true,   // search_items
            false,  // search_creatures
            1000
        );
    }
    
    // Output to SearchResultsWidget
    if (results_widget_) {
        results_widget_->setResults(results);
    }
    
    // Auto-show the search results widget
    if (view_settings_) {
        *view_settings_ = true;
    }
}

void AdvancedSearchDialog::onSelectItem() {
    // Placeholder for future brush selection functionality
    // Will be implemented when brush system is ready
}

} // namespace MapEditor::UI
