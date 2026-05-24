#pragma once
#include "Domain/Search/MapSearchResult.h"
#include <imgui.h>
#include <vector>
#include <functional>
#include <string>

namespace MapEditor {
namespace Services { 
    class SpriteManager; 
    class ClientDataService;
}
namespace Services { class MapSearchService; }

namespace UI {

/**
 * Dockable widget for searching items/creatures on the map.
 * Smart search: auto-detects name vs ID, searches all modes.
 */
class SearchResultsWidget {
public:
    using NavigateCallback = std::function<void(const Domain::Position&)>;
    using OpenAdvancedSearchCallback = std::function<void()>;
    using SearchAsyncCallback = std::function<void(const std::string& query, bool search_items, bool search_creatures)>;
    
    SearchResultsWidget();
    ~SearchResultsWidget() = default;
    
    // Non-copyable
    SearchResultsWidget(const SearchResultsWidget&) = delete;
    SearchResultsWidget& operator=(const SearchResultsWidget&) = delete;
    
    void setSpriteManager(Services::SpriteManager* sprites) { sprite_manager_ = sprites; }
    void setClientData(Services::ClientDataService* data) { client_data_ = data; }
    void setMapSearchService(Services::MapSearchService* service) { search_service_ = service; }
    void setNavigateCallback(NavigateCallback cb) { on_navigate_ = std::move(cb); }
    void setOpenAdvancedSearchCallback(OpenAdvancedSearchCallback cb) { on_open_advanced_search_ = std::move(cb); }
    void setSearchAsyncCallback(SearchAsyncCallback cb) { on_search_async_ = std::move(cb); }
    
    void setResults(const std::vector<Domain::Search::MapSearchResult>& results);
    void clear();
    void invalidateFilter();
    size_t getResultCount() const { return total_results_; }

    static constexpr size_t PAGE_SIZE = 10000;
    static constexpr size_t MAX_RESULTS = 100000;
    
    void render(bool* p_open);
    
private:
    void doSearch();
    void renderPreview(const Domain::Search::MapSearchResult& result);
    
    Services::SpriteManager* sprite_manager_ = nullptr;
    Services::ClientDataService* client_data_ = nullptr;
    Services::MapSearchService* search_service_ = nullptr;
    NavigateCallback on_navigate_;
    OpenAdvancedSearchCallback on_open_advanced_search_;
    SearchAsyncCallback on_search_async_;
    
    char search_buffer_[256] = {};
    char filter_buffer_[256] = {};
    bool search_items_ = true;
    bool search_creatures_ = true;
    
    std::vector<Domain::Search::MapSearchResult> results_;
    int selected_index_ = -1;
    int current_page_ = 0;
    size_t total_results_ = 0;

    std::vector<size_t> filtered_indices_;
    size_t filtered_count_ = 0;
    bool filter_dirty_ = true;

    void rebuildFilter();
    void renderSearchBar(bool& enter_pressed);
    void renderActionButtons(bool enter_pressed);
    void renderResultsList(float row_height);
    void renderFilterFooter(float btn_width);
    void renderPagination();
    void renderResultRow(size_t result_idx, const Domain::Search::MapSearchResult& result, bool is_selected, float row_height);
};

} // namespace UI
} // namespace MapEditor
