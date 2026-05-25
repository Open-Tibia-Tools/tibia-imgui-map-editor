#include "Controllers/SearchController.h"
#include "Services/ClientDataService.h"
#include "Services/SpriteManager.h"
#include "UI/Widgets/QuickSearchPopup.h"
#include "UI/Dialogs/AdvancedSearchDialog.h"
#include "UI/Widgets/SearchResultsWidget.h"
#include "Domain/ChunkedMap.h"
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

    current_client_data_ = client_data;
}

} // namespace AppLogic
} // namespace MapEditor
