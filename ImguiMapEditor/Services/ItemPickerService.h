#pragma once
#include "Domain/Search/ISearchProvider.h"
#include <string>
#include <vector>

namespace MapEditor {
namespace Services { class ClientDataService; }

namespace AppLogic {

/**
 * Service for searching items/creatures available to place.
 * Used by QuickSearch (Ctrl+F) to find items in the catalog.
 */
class ItemPickerService {
public:
    explicit ItemPickerService(Services::ClientDataService* client_data);
    ~ItemPickerService() = default;
    
    // Non-copyable
    ItemPickerService(const ItemPickerService&) = delete;
    ItemPickerService& operator=(const ItemPickerService&) = delete;
    
    /**
     * Search items and creatures by query.
     * Supports:
     * - Name search (fuzzy): "dragon"
     * - Server ID exact: "2492" or pure number
     * - Client ID: "cid:3031"
     * 
     * @param query Search string
     * @param limit Max results
     * @return Matching items/creatures
     */
    std::vector<Domain::Search::PickResult> search(
        const std::string& query, 
        size_t limit = 50) const;
    
private:
    bool matchesFuzzy(const std::string& text, const std::string& query) const;
    static std::string toLower(const std::string& str) ;
    
    Services::ClientDataService* client_data_;  // Non-owning
};

} // namespace AppLogic
} // namespace MapEditor
