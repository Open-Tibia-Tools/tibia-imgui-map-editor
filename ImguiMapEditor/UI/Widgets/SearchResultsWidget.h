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
    
    void setResults(const std::vector<Domain::Search::MapSearchResult>& results);
    void clear();
    size_t getResultCount() const { return results_.size(); }
    
    void render(bool* p_open);
    
private:
    void doSearch();
    void renderPreview(const Domain::Search::MapSearchResult& result);
    
    Services::SpriteManager* sprite_manager_ = nullptr;
    Services::ClientDataService* client_data_ = nullptr;
    Services::MapSearchService* search_service_ = nullptr;
    NavigateCallback on_navigate_;
    OpenAdvancedSearchCallback on_open_advanced_search_;
    
    char search_buffer_[256] = {};
    bool search_items_ = true;
    bool search_creatures_ = true;
    
    std::vector<Domain::Search::MapSearchResult> results_;
    int selected_index_ = -1;

    static constexpr size_t MAX_RESULTS = 1000;
};

} // namespace UI
} // namespace MapEditor
