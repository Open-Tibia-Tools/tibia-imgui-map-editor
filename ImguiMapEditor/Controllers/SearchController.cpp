#include "Controllers/SearchController.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "UI/Widgets/QuickSearchPopup.h"
#include "UI/Dialogs/AdvancedSearchDialog.h"
#include "UI/Widgets/SearchResultsWidget.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Search/MapSearchResult.h"
#include <ranges>
namespace MapEditor {
namespace AppLogic {

SearchController::SearchController()
    : quick_search_popup_(std::make_unique<UI::QuickSearchPopup>()),
      advanced_search_dialog_(std::make_unique<UI::AdvancedSearchDialog>()),
      search_results_widget_(std::make_unique<UI::SearchResultsWidget>()) {}

SearchController::~SearchController() = default;

UI::QuickSearchPopup* SearchController::getQuickSearchPopup() const {
    return quick_search_popup_.get();
}

UI::AdvancedSearchDialog* SearchController::getAdvancedSearchDialog() const {
    return advanced_search_dialog_.get();
}

UI::SearchResultsWidget* SearchController::getSearchResultsWidget() const {
    return search_results_widget_.get();
}

void SearchController::onMapLoaded(
    Domain::ChunkedMap* map,
    Services::ClientDataService* client_data,
    Services::SpriteManager* sprite_manager,
    Services::ViewSettings* view_settings
) {
    if (!client_data) return;

    // Only recreate ItemPickerService if client data changed (it doesn't support setting new data)
    if (!item_picker_service_ || current_client_data_ != client_data) {
        item_picker_service_ = std::make_unique<AppLogic::ItemPickerService>(client_data);
        quick_search_popup_->setItemPickerService(item_picker_service_.get());
        advanced_search_dialog_->setItemPickerService(item_picker_service_.get());
    }

    // Initialize MapSearchService if needed
    if (!map_search_service_) {
        map_search_service_ = std::make_unique<Services::MapSearchService>();

        // Wire up UI components that depend on the service instance
        search_results_widget_->setMapSearchService(map_search_service_.get());
        advanced_search_dialog_->setMapSearchService(map_search_service_.get());
        advanced_search_dialog_->setSearchResultsWidget(search_results_widget_.get());
    }

    // Always update dependencies for MapSearchService
    map_search_service_->setClientData(client_data);

    // Update MapSearchService with current map
    if (map) {
        map_search_service_->setMap(map);
    }

    // Update other UI components
    search_results_widget_->setClientData(client_data);
    search_results_widget_->setSpriteManager(sprite_manager);

    // Update QuickSearchPopup dependencies
    quick_search_popup_->setSpriteManager(sprite_manager);
    quick_search_popup_->setClientDataService(client_data);

    advanced_search_dialog_->setClientDataService(client_data);
    advanced_search_dialog_->setSpriteManager(sprite_manager);

    if (view_settings) {
        advanced_search_dialog_->setShowSearchResultsToggle(&view_settings->show_search_results);
    }

    // Wire async text search
    search_results_widget_->setSearchAsyncCallback(
        [this](const std::string& query, bool search_items, bool search_creatures) {
            searchTextAsync(query, search_items, search_creatures);
        });

    current_client_data_ = client_data;
}

void SearchController::searchUniqueAsync() {
    if (!map_search_service_) return;
    async_search_future_ = std::async(std::launch::async,
        [service = map_search_service_.get()]() {
            return service->searchByUnique();
        });
    async_search_active_ = true;
}

void SearchController::searchActionAsync() {
    if (!map_search_service_) return;
    async_search_future_ = std::async(std::launch::async,
        [service = map_search_service_.get()]() {
            return service->searchByAction();
        });
    async_search_active_ = true;
}

void SearchController::searchContainerAsync() {
    if (!map_search_service_) return;
    async_search_future_ = std::async(std::launch::async,
        [service = map_search_service_.get()]() {
            return service->searchByContainer();
        });
    async_search_active_ = true;
}

void SearchController::searchWriteableAsync() {
    if (!map_search_service_) return;
    async_search_future_ = std::async(std::launch::async,
        [service = map_search_service_.get()]() {
            return service->searchByWriteable();
        });
    async_search_active_ = true;
}

void SearchController::searchTextAsync(const std::string& query, bool search_items, bool search_creatures) {
    if (!map_search_service_ || query.empty()) return;

    bool is_number = !query.empty() && std::all_of(query.begin(), query.end(), ::isdigit);

    async_search_future_ = std::async(std::launch::async,
        [service = map_search_service_.get(), query, search_items, search_creatures, is_number]()
            -> std::vector<Domain::Search::MapSearchResult> {
            
            std::vector<Domain::Search::MapSearchResult> results;
            static constexpr size_t limit = 100000;

            auto append = [&](const auto& source) {
                if (results.size() >= limit) return;
                auto needed = limit - results.size();
                std::ranges::copy(source | std::views::take(needed), std::back_inserter(results));
            };

            if (is_number) {
                append(service->search(query, Services::MapSearchMode::ByServerId, search_items, false, limit));
                append(service->search(query, Services::MapSearchMode::ByClientId, search_items, false, limit));
            }

            append(service->search(query, Services::MapSearchMode::ByName, search_items, search_creatures, limit));

            return results;
        });
    async_search_active_ = true;
}

void SearchController::processAsyncSearch() {
    if (!async_search_active_ || !async_search_future_.valid()) return;

    auto status = async_search_future_.wait_for(std::chrono::seconds(0));
    if (status != std::future_status::ready) return;

    auto results = async_search_future_.get();
    async_search_active_ = false;

    if (search_results_widget_) {
        search_results_widget_->setResults(results);
    }
}

} // namespace AppLogic
} // namespace MapEditor
