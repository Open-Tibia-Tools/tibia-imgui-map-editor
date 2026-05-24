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
    total_results_ = results.size();
    current_page_ = 0;
    selected_index_ = results_.empty() ? -1 : 0;
    filter_buffer_[0] = '\0';
    filter_dirty_ = true;
    if (active_map_) saveState();
}

void SearchResultsWidget::clear() {
    results_.clear();
    filtered_indices_.clear();
    filtered_count_ = 0;
    selected_index_ = -1;
    current_page_ = 0;
    total_results_ = 0;
    search_buffer_[0] = '\0';
    filter_buffer_[0] = '\0';
    filter_dirty_ = true;
    if (active_map_) saveState();
}

void SearchResultsWidget::invalidateFilter() {
    filter_dirty_ = true;
}

void SearchResultsWidget::setActiveMap(const Domain::ChunkedMap* map) {
    if (active_map_ && active_map_ != map) {
        saveState();
    }
    loadState(map);
    active_map_ = map;
}

void SearchResultsWidget::forgetMap(const Domain::ChunkedMap* map) {
    session_states_.erase(map);
    if (active_map_ == map) {
        active_map_ = nullptr;
        results_.clear();
        total_results_ = 0;
        selected_index_ = -1;
        current_page_ = 0;
        filtered_indices_.clear();
        filtered_count_ = 0;
        filter_dirty_ = true;
    }
}

void SearchResultsWidget::saveState() {
    if (!active_map_) return;
    auto& st = session_states_[active_map_];
    st.results = results_;
    st.total_results = total_results_;
    st.selected_index = selected_index_;
    st.current_page = current_page_;
    memcpy(st.filter_buffer, filter_buffer_, sizeof(filter_buffer_));
    st.search_items = search_items_;
    st.search_creatures = search_creatures_;
}

void SearchResultsWidget::loadState(const Domain::ChunkedMap* map) {
    auto it = session_states_.find(map);
    if (it != session_states_.end()) {
        results_ = std::move(it->second.results);
        total_results_ = it->second.total_results;
        selected_index_ = it->second.selected_index;
        current_page_ = it->second.current_page;
        memcpy(filter_buffer_, it->second.filter_buffer, sizeof(filter_buffer_));
        search_items_ = it->second.search_items;
        search_creatures_ = it->second.search_creatures;
        session_states_.erase(it);
    } else {
        results_.clear();
        total_results_ = 0;
        selected_index_ = -1;
        current_page_ = 0;
        filter_buffer_[0] = '\0';
    }
    filtered_indices_.clear();
    filtered_count_ = 0;
    filter_dirty_ = true;
}

void SearchResultsWidget::rebuildFilter() {
    filtered_indices_.clear();

    auto ci_eq = [](unsigned char a, unsigned char b) {
        return std::tolower(a) == std::tolower(b);
    };

    size_t filter_len = strlen(filter_buffer_);
    for (size_t i = 0; i < results_.size(); ++i) {
        const auto& r = results_[i];
        if (!search_items_ && r.isItem()) continue;
        if (!search_creatures_ && r.isCreature()) continue;
        if (filter_len) {
            auto& dn = r.display_name;
            auto it = std::search(dn.begin(), dn.end(),
                filter_buffer_, filter_buffer_ + filter_len, ci_eq);
            if (it == dn.end()) continue;
        }
        filtered_indices_.push_back(i);
    }
    filtered_count_ = filtered_indices_.size();
    filter_dirty_ = false;
}

void SearchResultsWidget::renderSearchBar(bool& enter_pressed) {
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    enter_pressed = ImGui::InputTextWithHint(
        "##SearchInput", "Name or ID...",
        search_buffer_, sizeof(search_buffer_),
        ImGuiInputTextFlags_EnterReturnsTrue
    );
    ImGui::PopItemWidth();
}

void SearchResultsWidget::renderActionButtons(bool enter_pressed) {
    const char* clipboard = ImGui::GetClipboardText();
    bool has_clipboard = clipboard && clipboard[0];
    constexpr int btn_count = 4;
    float avail = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x * (btn_count - 1);
    float btn_w = (avail - spacing) / btn_count;
    if (btn_w < 50.0f) btn_w = 50.0f;

    if (ImGui::Button(ICON_FA_PASTE " Paste", ImVec2(btn_w, 0))) {
        if (has_clipboard) {
            size_t copy_len = std::min(strlen(clipboard), sizeof(search_buffer_) - 1);
            memcpy(search_buffer_, clipboard, copy_len);
            search_buffer_[copy_len] = '\0';
            doSearch();
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
}

void SearchResultsWidget::renderResultsList(float row_height) {
    if (filter_dirty_) rebuildFilter();

    if (results_.empty()) {
        ImVec2 wsize = ImGui::GetWindowSize();
        const char* msg = search_buffer_[0] ? "No results found" : "Type to search...";
        std::string text = std::format("{} {}", search_buffer_[0] ? ICON_FA_CIRCLE_EXCLAMATION : ICON_FA_KEYBOARD, msg);
        ImVec2 tsize = ImGui::CalcTextSize(text.c_str());
        ImGui::SetCursorPos(ImVec2((wsize.x - tsize.x) * 0.5f, (wsize.y - tsize.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopStyleColor();
        return;
    }

    if (filtered_indices_.empty()) {
        ImVec2 wsize = ImGui::GetWindowSize();
        std::string msg = std::format("{} No matching results", ICON_FA_CIRCLE_EXCLAMATION);
        ImVec2 tsize = ImGui::CalcTextSize(msg.c_str());
        ImGui::SetCursorPos(ImVec2((wsize.x - tsize.x) * 0.5f, (wsize.y - tsize.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextUnformatted(msg.c_str());
        ImGui::PopStyleColor();
        return;
    }

    size_t page_start = static_cast<size_t>(current_page_) * PAGE_SIZE;
    if (page_start >= filtered_count_) {
        current_page_ = static_cast<int>((filtered_count_ - 1) / PAGE_SIZE);
        page_start = static_cast<size_t>(current_page_) * PAGE_SIZE;
    }
    size_t page_end = std::min(page_start + PAGE_SIZE, filtered_count_);
    size_t page_count = page_end - page_start;

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(page_count));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            size_t result_idx = filtered_indices_[page_start + static_cast<size_t>(i)];
            const auto& result = results_[result_idx];
            bool is_selected = (static_cast<size_t>(selected_index_) == result_idx);
            renderResultRow(result_idx, result, is_selected, row_height);
        }
    }
    clipper.End();
}

void SearchResultsWidget::renderFilterFooter(float btn_width) {
    ImVec4 active = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    ImVec4 normal = ImGui::GetStyleColorVec4(ImGuiCol_Button);

    if (ImGui::InputTextWithHint("##FilterInput", "Filter results...",
            filter_buffer_, sizeof(filter_buffer_))) {
        filter_dirty_ = true;
        current_page_ = 0;
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, search_items_ ? active : normal);
    if (ImGui::Button(ICON_FA_CUBE, ImVec2(btn_width, 0))) {
        search_items_ = !search_items_;
        filter_dirty_ = true;
        current_page_ = 0;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle items");

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, search_creatures_ ? active : normal);
    if (ImGui::Button(ICON_FA_DRAGON, ImVec2(btn_width, 0))) {
        search_creatures_ = !search_creatures_;
        filter_dirty_ = true;
        current_page_ = 0;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle creatures");
}

void SearchResultsWidget::renderPagination() {
    if (results_.empty()) return;

    size_t display_count = filtered_count_;
    size_t total_pages = std::max<size_t>(1, (display_count + PAGE_SIZE - 1) / PAGE_SIZE);
    bool capped = total_results_ > MAX_RESULTS;

    ImGui::TextUnformatted(std::format("{} {} results", ICON_FA_LIST, total_results_).c_str());
    if (capped) { ImGui::SameLine(); ImGui::TextDisabled("(first %zu)", MAX_RESULTS); }
    if (filter_buffer_[0]) { ImGui::SameLine(); ImGui::TextDisabled("| filter: %zu", filtered_count_); }

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

void SearchResultsWidget::render(bool* p_open) {
    if (!p_open || !*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(340, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(ICON_FA_MAGNIFYING_GLASS " Search Map###SearchResults", p_open)) {
        float btn_width = 30.0f;

        bool enter_pressed = false;
        renderSearchBar(enter_pressed);
        renderActionButtons(enter_pressed);

        ImGui::Separator();

        float line_h = ImGui::GetTextLineHeightWithSpacing();
        float row_h = std::max(32.0f, line_h * 3) + 10;
        float frame_h = ImGui::GetFrameHeightWithSpacing();
        float footer_h = frame_h * 2 + 10;

        ImGui::BeginChild("ResultsList", ImVec2(0, -footer_h), true);
        renderResultsList(row_h);
        ImGui::EndChild();

        renderFilterFooter(btn_width);
        renderPagination();
    }
    ImGui::End();
}

void SearchResultsWidget::renderResultRow(size_t result_idx, const Domain::Search::MapSearchResult& result, bool is_selected, float row_height) {
    ImVec2 cursor = ImGui::GetCursorPos();
    float line_h = ImGui::GetTextLineHeightWithSpacing();

    ImGui::PushID(static_cast<int>(result_idx));

    // Selectable without AllowOverlap — cursor advances naturally for clipper
    ImGui::Selectable("##row", is_selected, 0, ImVec2(0, row_height));

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

    // Save cursor position after Selectable (for clipper-consistent advancement)
    ImVec2 cursor_end = ImGui::GetCursorPos();

    float sprite_size = 32.0f;
    float text_x = cursor.x + sprite_size + 4;

    // Draw sprite
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

    // Draw text rows overlaid on the selectable
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

    // Restore cursor to after the Selectable and submit Dummy to register the
    // full row height as content area (silences SetCursorPos boundary warnings)
    ImGui::SetCursorPos(cursor_end);
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 0));
    ImGui::PopID();
}

void SearchResultsWidget::renderPreview(const Domain::Search::MapSearchResult& result) {
    bool rendered = false;

    if (result.isItem() && sprite_manager_ && client_data_) {
        auto* item_type = client_data_->getItemTypeByServerId(result.item_id);
        if (item_type) {
            if (auto* texture = Utils::GetItemPreview(*sprite_manager_, item_type)) {
                float preview_size = static_cast<float>(std::max(item_type->width, item_type->height) * 32);
                ImGui::Image((void*)(intptr_t)texture->id(), ImVec2(preview_size, preview_size));
                rendered = true;
            }
        }
    } else if (result.isCreature() && sprite_manager_ && client_data_) {
        auto preview = Utils::GetCreaturePreview(*client_data_, *sprite_manager_, result.creature_name);
        if (preview && preview.texture) {
            float preview_size = preview.size;
            ImGui::Image((void*)(intptr_t)preview.texture->id(), ImVec2(preview_size, preview_size));
            rendered = true;
        }
    }

    if (!rendered) {
        ImGui::Dummy(ImVec2(32, 32));
    }
}

void SearchResultsWidget::doSearch() {
    results_.clear();
    filtered_indices_.clear();
    filtered_count_ = 0;
    total_results_ = 0;
    selected_index_ = -1;
    current_page_ = 0;
    filter_dirty_ = true;

    if (std::string_view(search_buffer_).empty()) return;

    if (on_search_async_) {
        on_search_async_(search_buffer_, search_items_, search_creatures_);
    }
}

} // namespace MapEditor::UI
