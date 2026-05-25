#include "UI/Widgets/SearchResultsWidget.h"
#include <algorithm>
#include <ranges>
#include <string_view>
#include <format>
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include "Services/Map/MapSearchService.h"
#include "Services/SpriteManager.h"
#include "Services/ClientDataService.h"
#include "Rendering/Core/Texture.h"
#include "UI/Utils/UIUtils.hpp"
#include "UI/Utils/PreviewUtils.hpp"

namespace MapEditor::UI {

SearchResultsWidget::SearchResultsWidget() {
    search_buffer_[0] = '\0';
}

void SearchResultsWidget::setResults(const std::vector<Domain::Search::MapSearchResult>& results) {
    auto view = results | std::views::take(MAX_RESULTS);
    results_.assign(view.begin(), view.end());
    selected_index_ = results_.empty() ? -1 : 0;
}

void SearchResultsWidget::clear() {
    results_.clear();
    selected_index_ = -1;
    search_buffer_[0] = '\0';
}

void SearchResultsWidget::render(bool* p_open) {
    if (!p_open || !*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(320, 450), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(ICON_FA_MAGNIFYING_GLASS " Search Map###SearchResults", p_open)) {
        // Search bar + buttons on same line
        float icon_button_width = 30.0f;
        float adv_button_width = 45.0f;
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - icon_button_width - adv_button_width - 16);
        bool enter_pressed = ImGui::InputTextWithHint(
            "##SearchInput", "Name or ID...",
            search_buffer_, sizeof(search_buffer_),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        ImGui::PopItemWidth();

        // Clear button if text exists, otherwise Paste button
        if (!std::string_view(search_buffer_).empty()) {
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_XMARK, ImVec2(icon_button_width, 0))) {
                clear();
            }
            Utils::SetTooltipOnHover("Clear search");
        } else {
            if (Utils::RenderPasteButton(search_buffer_, sizeof(search_buffer_), "##PasteSearch", "Paste and search", ImVec2(icon_button_width, 0))) {
                doSearch();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS, ImVec2(icon_button_width, 0)) || enter_pressed) {
            doSearch();
        }
        Utils::SetTooltipOnHover("Search map (Enter)");

        // Advanced Search button (right next to Find)
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS_PLUS, ImVec2(adv_button_width, 0))) {
            if (on_open_advanced_search_) {
                on_open_advanced_search_();
            }
        }
        Utils::SetTooltipOnHover("Advanced Search...");

        // Toggle buttons for Items/Creatures
        ImVec4 active_color = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        ImVec4 normal_color = ImGui::GetStyleColorVec4(ImGuiCol_Button);

        ImGui::PushStyleColor(ImGuiCol_Button, search_items_ ? active_color : normal_color);
        if (ImGui::Button(ICON_FA_CUBE " Items")) {
            search_items_ = !search_items_;
        }
        Utils::SetTooltipOnHover("Include items in search results");
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, search_creatures_ ? active_color : normal_color);
        if (ImGui::Button(ICON_FA_DRAGON " Creatures")) {
            search_creatures_ = !search_creatures_;
        }
        Utils::SetTooltipOnHover("Include creatures in search results");
        ImGui::PopStyleColor();

        ImGui::Separator();

        // Results list
        ImGui::BeginChild("ResultsList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);

        if (results_.empty()) {
            // New Empty State Logic
            bool is_search_active = !std::string_view(search_buffer_).empty();
            const char* icon = is_search_active ? ICON_FA_CIRCLE_EXCLAMATION : ICON_FA_KEYBOARD;
            const char* text = is_search_active ? "No results found" : "Type to search...";

            // Center text
            ImVec2 window_size = ImGui::GetWindowSize();
            std::string full_text = std::format("{} {}", icon, text);
            ImVec2 text_size = ImGui::CalcTextSize(full_text.c_str());

            ImGui::SetCursorPos(ImVec2(
                (window_size.x - text_size.x) * 0.5f,
                (window_size.y - text_size.y) * 0.5f
            ));

            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            ImGui::TextUnformatted(full_text.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(results_.size()));
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& result = results_[i];
                    bool is_selected = (i == selected_index_);

                    const char* icon;
                    if (result.isCreature()) {
                        icon = ICON_FA_DRAGON;
                    } else if (result.is_in_container) {
                        icon = ICON_FA_BOX_OPEN;  // Items inside containers
                    } else {
                        icon = ICON_FA_CUBE;
                    }

                    ImGui::PushID(i);
                    char label_buf[256];
                    auto format_result = std::format_to_n(label_buf, sizeof(label_buf) - 1, "{} {} @ {},{},{}",
                        icon, result.display_name,
                        result.position.x, result.position.y, result.position.z);
                    *format_result.out = '\0';

                    if (ImGui::Selectable(label_buf, is_selected)) {
                        selected_index_ = i;
                    }
                    ImGui::PopID();

                    // Enhanced preview on hover with details
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        renderPreview(result);
                        ImGui::Separator();
                        ImGui::TextUnformatted(result.display_name.c_str());
                        if (result.isItem()) {
                             ImGui::TextDisabled("ID: %u", result.item_id);
                        }
                        ImGui::TextDisabled("Pos: %d, %d, %d", result.position.x, result.position.y, result.position.z);
                        ImGui::Separator();
                        ImGui::TextDisabled(ICON_FA_ARROW_POINTER " Double-click to teleport");
                        ImGui::EndTooltip();
                    }

                    // Double-click to teleport
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        if (on_navigate_) {
                            on_navigate_(result.position);
                        }
                    }
                }
            }
            clipper.End();
        }
        ImGui::EndChild();

        // Footer
        ImGui::Text(ICON_FA_LIST " %zu results", results_.size());
        if (results_.size() >= MAX_RESULTS) {
            ImGui::SameLine();
            ImGui::TextDisabled("(limit reached)");
        }
    }
    ImGui::End();
}

void SearchResultsWidget::renderPreview(const Domain::Search::MapSearchResult& result) {
    bool rendered = false;

    if (result.isItem() && sprite_manager_ && client_data_) {
        // Item sprite - size based on item dimensions
        auto* item_type = client_data_->getItemTypeByServerId(result.item_id);
        if (item_type) {
            if (auto* texture = Utils::GetItemPreview(*sprite_manager_, item_type)) {
                // Dynamic size: max(width, height) * 32
                float preview_size = static_cast<float>(std::max(item_type->width, item_type->height) * 32);
                ImGui::Image((void*)(intptr_t)texture->id(), ImVec2(preview_size, preview_size));
                rendered = true;
            }
        }
    } else if (result.isCreature() && sprite_manager_ && client_data_) {
        // Creature outfit sprite using helper
        auto preview = Utils::GetCreaturePreview(*client_data_, *sprite_manager_, result.creature_name);
        if (preview && preview.texture) {
            float preview_size = preview.size;
            ImGui::Image((void*)(intptr_t)preview.texture->id(), ImVec2(preview_size, preview_size));
            rendered = true;
        }
    }

    // Fallback if no sprite
    if (!rendered) {
        ImGui::Dummy(ImVec2(32, 32));
    }
}

void SearchResultsWidget::doSearch() {
    results_.clear();
    selected_index_ = -1;

    if (!search_service_ || std::string_view(search_buffer_).empty()) {
        return;
    }

    std::string query = search_buffer_;

    auto name_results = search_service_->search(
        query, Services::MapSearchMode::ByName,
        search_items_, search_creatures_, MAX_RESULTS);

    bool is_number = !query.empty() && std::all_of(query.begin(), query.end(), ::isdigit);

    auto append_results = [&](const auto& source) {
        if (results_.size() >= MAX_RESULTS) return;
        auto needed = MAX_RESULTS - results_.size();
        std::ranges::copy(source | std::views::take(needed), std::back_inserter(results_));
    };

    if (is_number) {
        auto server_results = search_service_->search(
            query, Services::MapSearchMode::ByServerId,
            search_items_, false, MAX_RESULTS);
        auto client_results = search_service_->search(
            query, Services::MapSearchMode::ByClientId,
            search_items_, false, MAX_RESULTS);

        append_results(server_results);
        append_results(client_results);
    }

    append_results(name_results);

    selected_index_ = results_.empty() ? -1 : 0;
}

} // namespace MapEditor::UI
