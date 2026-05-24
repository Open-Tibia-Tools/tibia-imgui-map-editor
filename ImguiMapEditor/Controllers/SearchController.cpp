#include "Controllers/SearchController.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "UI/Widgets/QuickSearchPopup.h"
#include "UI/Dialogs/AdvancedSearchDialog.h"
#include "UI/Widgets/SearchResultsWidget.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Search/MapSearchResult.h"
#include <ranges>
#include <spdlog/spdlog.h>
namespace MapEditor {
namespace AppLogic {

static void sortResultsByName(std::vector<Domain::Search::MapSearchResult>& results) {
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) {
            return std::lexicographical_compare(
                a.display_name.begin(), a.display_name.end(),
                b.display_name.begin(), b.display_name.end(),
                [](unsigned char ca, unsigned char cb) {
                    return std::tolower(ca) < std::tolower(cb);
                });
        });
}

SearchController::SearchController()
    : quick_search_popup_(std::make_unique<UI::QuickSearchPopup>()),
      advanced_search_dialog_(std::make_unique<UI::AdvancedSearchDialog>()),
      search_results_widget_(std::make_unique<UI::SearchResultsWidget>()) {}

SearchController::~SearchController() = default;

UI::QuickSearchPopup* SearchController::getQuickSearchPopup() const { return quick_search_popup_.get(); }
UI::AdvancedSearchDialog* SearchController::getAdvancedSearchDialog() const { return advanced_search_dialog_.get(); }
UI::SearchResultsWidget* SearchController::getSearchResultsWidget() const { return search_results_widget_.get(); }

template<typename F>
void SearchController::launchAsync(F&& searchFn) {
    if (!map_search_service_ || async_search_active_) return;
    async_search_map_ = map_search_service_->getMapAddr();
    async_search_future_ = std::async(std::launch::async,
        [fn = std::forward<F>(searchFn)]() {
            auto results = fn();
            sortResultsByName(results);
            return results;
        });
    async_search_active_ = true;
}

void SearchController::onMapLoaded(
    Domain::ChunkedMap* map,
    Services::ClientDataService* client_data,
    Services::SpriteManager* sprite_manager,
    Services::ViewSettings* view_settings
) {
    cancelAsyncSearch();

    if (map_search_service_) {
        map_search_service_->setMap(map);
    }

    if (!client_data) return;

    if (!item_picker_service_ || current_client_data_ != client_data) {
        item_picker_service_ = std::make_unique<AppLogic::ItemPickerService>(client_data);
        quick_search_popup_->setItemPickerService(item_picker_service_.get());
        advanced_search_dialog_->setItemPickerService(item_picker_service_.get());
    }

    if (!map_search_service_) {
        map_search_service_ = std::make_unique<Services::MapSearchService>();
        search_results_widget_->setMapSearchService(map_search_service_.get());
        advanced_search_dialog_->setMapSearchService(map_search_service_.get());
        advanced_search_dialog_->setSearchResultsWidget(search_results_widget_.get());
    }

    map_search_service_->setClientData(client_data);
    if (map) map_search_service_->setMap(map);

    search_results_widget_->setClientData(client_data);
    search_results_widget_->setSpriteManager(sprite_manager);

    quick_search_popup_->setSpriteManager(sprite_manager);
    quick_search_popup_->setClientDataService(client_data);

    advanced_search_dialog_->setClientDataService(client_data);
    advanced_search_dialog_->setSpriteManager(sprite_manager);

    if (view_settings) {
        advanced_search_dialog_->setShowSearchResultsToggle(&view_settings->show_search_results);
    }

    search_results_widget_->setSearchAsyncCallback(
        [this](const std::string& query, bool search_items, bool search_creatures) {
            searchTextAsync(query, search_items, search_creatures);
        });

    current_client_data_ = client_data;
}

void SearchController::searchUniqueAsync() {
    launchAsync([s = map_search_service_.get()]() { return s->searchByUnique(); });
}
void SearchController::searchActionAsync() {
    launchAsync([s = map_search_service_.get()]() { return s->searchByAction(); });
}
void SearchController::searchContainerAsync() {
    launchAsync([s = map_search_service_.get()]() { return s->searchByContainer(); });
}
void SearchController::searchWriteableAsync() {
    launchAsync([s = map_search_service_.get()]() { return s->searchByWriteable(); });
}

void SearchController::searchTextAsync(const std::string& query, bool search_items, bool search_creatures) {
    if (!map_search_service_ || query.empty()) return;

    if (async_search_active_) {
        pending_text_search_ = PendingTextSearch{query, search_items, search_creatures};
        return;
    }

    launchAsync([service = map_search_service_.get(), query, search_items, search_creatures]() {
        return service->searchMulti(query, search_items, search_creatures);
    });
}

void SearchController::forgetSessionMap(const Domain::ChunkedMap* map) {
    if (search_results_widget_) {
        search_results_widget_->forgetMap(map);
    }
}

void SearchController::cancelAsyncSearch() {
    if (async_search_active_ && async_search_future_.valid()) {
        async_search_future_.wait();
        async_search_active_ = false;
    }
}

void SearchController::processAsyncSearch() {
    if (!async_search_active_ || !async_search_future_.valid()) return;

    auto status = async_search_future_.wait_for(std::chrono::seconds(0));
    if (status != std::future_status::ready) return;

    try {
        auto results = async_search_future_.get();
        // Drop stale results if the session map changed while search was running
        const Domain::ChunkedMap* current_map = map_search_service_ ? map_search_service_->getMapAddr() : nullptr;
        if (async_search_map_ && async_search_map_ != current_map) {
            async_search_active_ = false;
            async_search_map_ = nullptr;
            return;
        }
        if (search_results_widget_) {
            search_results_widget_->setResults(results);
        }
    } catch (const std::exception& e) {
        spdlog::error("Async search failed: {}", e.what());
    } catch (...) {
        // Catch-all safety net — prevents exceptions from the background
        // thread escaping into the main loop's update() and crashing the app.
        spdlog::error("Async search failed: unknown error");
    }

    async_search_active_ = false;
    async_search_map_ = nullptr;

    // Launch pending search if queued
    if (pending_text_search_) {
        auto pending = std::move(*pending_text_search_);
        pending_text_search_.reset();
        searchTextAsync(pending.query, pending.search_items, pending.search_creatures);
    }
}

} // namespace AppLogic
} // namespace MapEditor
