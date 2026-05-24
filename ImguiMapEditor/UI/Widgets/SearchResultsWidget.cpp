#include "UI/Widgets/SearchResultsWidget.h"
#include <algorithm>
#include <cctype>
#include <ranges>
#include <string_view>
#include <format>
#include "ext/fontawesome6/IconsFontAwesome6.h"
#include "Services/Map/MapSearchService.h"
#include "Services/SpriteManager.h"
#include "Services/ClientDataService.h"
#include "Rendering/Core/Texture.h"
#include "UI/Utils/PreviewUtils.hpp"

namespace MapEditor::UI {

SearchResultsWidget::SearchResultsWidget() {
    search_buffer_[0] = '\0';
    filter_buffer_[0] = '\0';
}

void SearchResultsWidget::setResults(const std::vector<Domain::Search::MapSearchResult>& results) {
    auto view = results | std::views::take(MAX_RESULTS);
    results_.assign(view.begin(), view.end());
    std::sort(results_.begin(), results_.end(),
        [](const auto& a, const auto& b) {
            std::string na = a.display_name;
            std::string nb = b.display_name;
            std::transform(na.begin(), na.end(), na.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(nb.begin(), nb.end(), nb.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return na < nb;
        });
    total_results_ = results.size();
    current_page_ = 0;
    selected_index_ = results_.empty() ? -1 : 0;
    filter_buffer_[0] = '\0';
}

void SearchResultsWidget::clear() {
    results_.clear();
    selected_index_ = -1;
    current_page_ = 0;
    total_results_ = 0;
    search_buffer_[0] = '\0';
    filter_buffer_[0] = '\0';
}

void SearchResultsWidget::render(bool* p_open) {
    if (!p_open || !*p_open) return;
    
    ImGui::SetNextWindowSize(ImVec2(340, 500), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin(ICON_FA_MAGNIFYING_GLASS " Search Map###SearchResults", p_open)) {
        float btn_width = 30.0f;

        // --- Search bar (full width) ---
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        bool enter_pressed = ImGui::InputTextWithHint(
            "##SearchInput", "Name or ID...",
            search_buffer_, sizeof(search_buffer_),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        ImGui::PopItemWidth();

        // --- Action buttons row (scale with window) ---
        const char* clipboard = ImGui::GetClipboardText();
        bool has_clipboard = clipboard && clipboard[0];
        constexpr int btn_count = 4;
        float avail = ImGui::GetContentRegionAvail().x;
        float spacing = ImGui::GetStyle().ItemSpacing.x * (btn_count - 1);
        float btn_w = (avail - spacing) / btn_count;
        if (btn_w < 50.0f) btn_w = 50.0f;

        if (ImGui::Button(ICON_FA_PASTE " Paste", ImVec2(btn_w, 0))) {
            if (has_clipboard) {
                size_t len = strlen(clipboard);
                if (len < sizeof(search_buffer_)) {
                    memcpy(search_buffer_, clipboard, len + 1);
                    doSearch();
                }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(has_clipboard ? "Paste and search" : "Clipboard empty");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK " Clear", ImVec2(btn_w, 0))) {
            clear();
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS "##SearchBtn", ImVec2(btn_w, 0)) || enter_pressed) {
            doSearch();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Search map (Enter)");

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS_PLUS "##AdvBtn", ImVec2(btn_w, 0))) {
            if (on_open_advanced_search_) on_open_advanced_search_();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Advanced Search...");
        
        ImGui::Separator();

        // --- Build filtered indices ---
        std::string filter_lower;
        if (filter_buffer_[0]) {
            filter_lower = filter_buffer_;
            std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        std::vector<size_t> filtered_indices;
        for (size_t i = 0; i < results_.size(); ++i) {
            const auto& r = results_[i];
            if (!search_items_ && r.isItem()) continue;
            if (!search_creatures_ && r.isCreature()) continue;
            if (!filter_lower.empty()) {
                std::string dn = r.display_name;
                std::transform(dn.begin(), dn.end(), dn.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (dn.find(filter_lower) == std::string::npos) continue;
            }
            filtered_indices.push_back(i);
        }

        size_t filtered_count = filtered_indices.size();
        size_t page_start = static_cast<size_t>(current_page_) * PAGE_SIZE;
        size_t page_end = std::min(page_start + PAGE_SIZE, filtered_count);
        if (current_page_ > 0 && page_start >= filtered_count && filtered_count > 0) {
            current_page_ = static_cast<int>((filtered_count - 1) / PAGE_SIZE);
            page_start = static_cast<size_t>(current_page_) * PAGE_SIZE;
            page_end = std::min(page_start + PAGE_SIZE, filtered_count);
        }

        // --- Results list ---
        float line_h = ImGui::GetTextLineHeightWithSpacing();
        float row_h = std::max(32.0f, line_h * 3) + 10;
        float frame_h = ImGui::GetFrameHeightWithSpacing();
        float footer_h = frame_h * 2 + 10;
        ImGui::BeginChild("ResultsList", ImVec2(0, -footer_h), true);

        if (results_.empty()) {
            ImVec2 wsize = ImGui::GetWindowSize();
            std::string msg = std::format("{} {}", ICON_FA_KEYBOARD, "Type to search...");
            ImVec2 tsize = ImGui::CalcTextSize(msg.c_str());
            ImGui::SetCursorPos(ImVec2((wsize.x - tsize.x) * 0.5f, (wsize.y - tsize.y) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            ImGui::TextUnformatted(msg.c_str());
            ImGui::PopStyleColor();
        } else if (filtered_indices.empty() && filter_buffer_[0]) {
            ImVec2 wsize = ImGui::GetWindowSize();
            std::string msg = std::format("{} No matching results", ICON_FA_CIRCLE_EXCLAMATION);
            ImVec2 tsize = ImGui::CalcTextSize(msg.c_str());
            ImGui::SetCursorPos(ImVec2((wsize.x - tsize.x) * 0.5f, (wsize.y - tsize.y) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            ImGui::TextUnformatted(msg.c_str());
            ImGui::PopStyleColor();
        } else {
            size_t page_count = page_end - page_start;
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(page_count), row_h);
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    size_t result_idx = filtered_indices[page_start + static_cast<size_t>(i)];
                    const auto& result = results_[result_idx];
                    bool is_selected = (static_cast<size_t>(selected_index_) == result_idx);
                    renderResultRow(result_idx, result, is_selected, row_h);
                }
            }
            clipper.End();
        }
        ImGui::EndChild();

        // --- Filter row ---
        ImVec4 active = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        ImVec4 normal = ImGui::GetStyleColorVec4(ImGuiCol_Button);

        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - btn_width * 2 - 12);
        ImGui::InputTextWithHint("##FilterInput", "Filter results...",
            filter_buffer_, sizeof(filter_buffer_));
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, search_items_ ? active : normal);
        if (ImGui::Button(ICON_FA_CUBE, ImVec2(btn_width, 0))) search_items_ = !search_items_;
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle items");

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, search_creatures_ ? active : normal);
        if (ImGui::Button(ICON_FA_DRAGON, ImVec2(btn_width, 0))) search_creatures_ = !search_creatures_;
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle creatures");

        // --- Pagination row ---
        if (!results_.empty()) {
            size_t display_count = filter_buffer_[0] ? filtered_count : results_.size();
            size_t total_pages = std::max<size_t>(1, (display_count + PAGE_SIZE - 1) / PAGE_SIZE);
            bool capped = total_results_ > MAX_RESULTS;

            ImGui::TextUnformatted(std::format("{} {} results", ICON_FA_LIST, total_results_).c_str());
            if (capped) { ImGui::SameLine(); ImGui::TextDisabled("(first %zu)", MAX_RESULTS); }
            if (filter_buffer_[0]) { ImGui::SameLine(); ImGui::TextDisabled("| filter: %zu", filtered_count); }

            if (total_pages > 1) {
                ImGui::SameLine();
                if (ImGui::SmallButton(ICON_FA_CHEVRON_LEFT) && current_page_ > 0) {
                    current_page_--; selected_index_ = -1;
                }
                ImGui::SameLine();
                ImGui::Text("Page %d/%zu", current_page_ + 1, total_pages);
                ImGui::SameLine();
                if (ImGui::SmallButton(ICON_FA_CHEVRON_RIGHT) && current_page_ < static_cast<int>(total_pages) - 1) {
                    current_page_++; selected_index_ = -1;
                }
            }
        }
    }
    ImGui::End();
}

void SearchResultsWidget::renderResultRow(size_t result_idx, const Domain::Search::MapSearchResult& result, bool is_selected, float row_height) {
    ImVec2 cursor = ImGui::GetCursorPos();
    float line_h = ImGui::GetTextLineHeightWithSpacing();

    ImGui::PushID(static_cast<int>(result_idx));
    ImGui::Selectable("##row", is_selected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, row_height));

    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseDoubleClicked(0) && on_navigate_) {
            on_navigate_(result.position);
        }
        ImGui::BeginTooltip();
        renderPreview(result);
        ImGui::Separator();
        ImGui::TextUnformatted(result.display_name.c_str());
        if (!result.info_line.empty()) ImGui::TextDisabled("%s", result.info_line.c_str());
        ImGui::TextDisabled("Pos: %d, %d, %d", result.position.x, result.position.y, result.position.z);
        ImGui::Separator();
        ImGui::TextDisabled(ICON_FA_ARROW_POINTER " Double-click to teleport");
        ImGui::EndTooltip();
    }

    if (ImGui::IsItemClicked()) {
        selected_index_ = static_cast<int>(result_idx);
    }

    // 2-column layout: left = sprite (32px), right = text
    float sprite_size = 32.0f;
    float text_x = cursor.x + sprite_size + 4;

    // Left column: sprite
    ImGui::SetCursorPos(ImVec2(cursor.x, cursor.y + 2));
    GLuint tex_id = 0;
    if (result.isItem() && sprite_manager_ && client_data_) {
        auto* item_type = client_data_->getItemTypeByServerId(result.item_id);
        if (item_type) {
            if (auto* tex = Utils::GetItemPreview(*sprite_manager_, item_type)) {
                tex_id = tex->id();
            }
        }
    } else if (result.isCreature() && sprite_manager_ && client_data_) {
        auto preview = Utils::GetCreaturePreview(*client_data_, *sprite_manager_, result.creature_name);
        if (preview.texture) tex_id = preview.texture->id();
    }
    if (tex_id) {
        ImGui::Image((void*)(intptr_t)tex_id, ImVec2(sprite_size, sprite_size));
    }

    // Right column: 3 text rows
    ImGui::SetCursorPos(ImVec2(text_x, cursor.y));
    ImGui::TextUnformatted(result.display_name.c_str());

    ImGui::SetCursorPos(ImVec2(text_x + 8, cursor.y + line_h));
    if (!result.info_line.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextUnformatted(result.info_line.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorPos(ImVec2(text_x + 8, cursor.y + line_h * 2));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::Text("%d, %d, %d", result.position.x, result.position.y, result.position.z);
    ImGui::PopStyleColor();

    // End of row: submit Dummy to properly extend content boundaries
    ImGui::SetCursorPos(ImVec2(cursor.x, cursor.y + row_height + 2));
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 0));
    ImGui::PopID();
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
    selected_index_ = -1;
    current_page_ = 0;
    
    if (std::string_view(search_buffer_).empty()) return;
    
    if (on_search_async_) {
        on_search_async_(search_buffer_, search_items_, search_creatures_);
    }
}

} // namespace MapEditor::UI
