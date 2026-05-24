#pragma once
#include "Services/ItemPickerService.h"
#include "Services/Map/MapSearchService.h"
#include "Services/ViewSettings.h"
#include <future>
#include <memory>
#include <optional>
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
 *
 * ## Thread Safety — Known Limitation
 *
 * Async searches (searchUniqueAsync, searchTextAsync, etc.) run on background
 * threads via std::async. These read ChunkedMap tiles and items concurrently
 * with the main thread, which may modify tiles during editing (brush strokes,
 * property edits, undo/redo).
 *
 * ChunkedMap is NOT currently synchronized. A data race exists if the user
 * edits tiles while a search is in progress. In practice searches complete in
 * seconds, during which map edits are unlikely. No crashes have been observed.
 *
 * A proper fix would require a read-write lock on ChunkedMap or a snapshot
 * mechanism for searches (follow-up issue).
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

    /** Cancel any in-flight async search. Blocks until the background task finishes. */
    void cancelAsyncSearch();

    /** Remove cached results for a session about to be destroyed. */
    void forgetSessionMap(const Domain::ChunkedMap* map);

    // Accessors for UI components (needed for rendering and callbacks)
    UI::QuickSearchPopup* getQuickSearchPopup() const;
    UI::AdvancedSearchDialog* getAdvancedSearchDialog() const;
    UI::SearchResultsWidget* getSearchResultsWidget() const;

private:
    template<typename F> void launchAsync(F&& searchFn);

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
    const Domain::ChunkedMap* async_search_map_ = nullptr;

    // Pending search queue (stored when a new query arrives while one is running)
    struct PendingTextSearch {
        std::string query;
        bool search_items = true;
        bool search_creatures = true;
    };
    std::optional<PendingTextSearch> pending_text_search_;
};

} // namespace AppLogic
} // namespace MapEditor
