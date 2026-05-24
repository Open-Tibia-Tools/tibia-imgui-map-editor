#pragma once
#include "Services/ItemPickerService.h"
#include "Services/Map/MapSearchService.h"
#include "Services/ViewSettings.h"
#include <future>
#include <memory>
#include <vector>

namespace MapEditor {

namespace Domain {
namespace Search { struct MapSearchResult; }
}

namespace UI {
class QuickSearchPopup;
class AdvancedSearchDialog;
class SearchResultsWidget;
}

namespace Services {
    class ClientDataService;
    class SpriteManager;
}

namespace Domain {
    class ChunkedMap;
}

namespace AppLogic {

/**
 * Orchestrates search functionality, managing search services and UI widgets.
 */
class SearchController {
public:
    SearchController();
    ~SearchController();

    /**
     * Update search components when a map is loaded.
     * Wires services with the new map and client data.
     */
    void onMapLoaded(
        Domain::ChunkedMap* map,
        Services::ClientDataService* client_data,
        Services::SpriteManager* sprite_manager,
        Services::ViewSettings* view_settings
    );

    /** Launch async map-wide search for items with unique ID. */
    void searchUniqueAsync();

    /** Launch async map-wide search for items with action ID. */
    void searchActionAsync();

    /** Launch async map-wide search for container items. */
    void searchContainerAsync();

    /** Launch async map-wide search for writeable items. */
    void searchWriteableAsync();

    /** Launch async text-based search (name or ID) with smart mode detection. */
    void searchTextAsync(const std::string& query, bool search_items, bool search_creatures);

    /** Process completed async search results. Must be called each frame from the main thread. */
    void processAsyncSearch();

    // Accessors for UI components (needed for rendering and callbacks)
    UI::QuickSearchPopup* getQuickSearchPopup() const;
    UI::AdvancedSearchDialog* getAdvancedSearchDialog() const;
    UI::SearchResultsWidget* getSearchResultsWidget() const;

private:
    // UI Components (unique_ptr to allow forward declarations in header)
    std::unique_ptr<UI::QuickSearchPopup> quick_search_popup_;
    std::unique_ptr<UI::AdvancedSearchDialog> advanced_search_dialog_;
    std::unique_ptr<UI::SearchResultsWidget> search_results_widget_;

    // Services
    std::unique_ptr<AppLogic::ItemPickerService> item_picker_service_;
    std::unique_ptr<Services::MapSearchService> map_search_service_;

    // State tracking
    Services::ClientDataService* current_client_data_ = nullptr;

    // Async search
    std::future<std::vector<Domain::Search::MapSearchResult>> async_search_future_;
    bool async_search_active_ = false;
};

} // namespace AppLogic
} // namespace MapEditor
