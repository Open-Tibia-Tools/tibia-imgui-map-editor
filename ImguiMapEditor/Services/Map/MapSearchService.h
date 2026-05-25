#pragma once
#include "Domain/Search/MapSearchResult.h"
#include "Domain/Search/SearchFilterTypes.h"
#include <string>
#include <vector>

namespace MapEditor {

namespace Domain { 
    class ChunkedMap; 
    class Tile;
    class Item;
    class ItemType;
}

namespace Services {

class ClientDataService;

/**
 * Mode for map search operations.
 */
enum class MapSearchMode {
    ByName,       // Fuzzy name match
    ByServerId,   // Exact server ID
    ByClientId    // Exact client ID
};

/**
 * Service for searching items/creatures ON THE MAP.
 * Iterates map tiles to find matching entities.
 */
class MapSearchService {
public:
    MapSearchService() = default;
    ~MapSearchService() = default;
    
    // Non-copyable
    MapSearchService(const MapSearchService&) = delete;
    MapSearchService& operator=(const MapSearchService&) = delete;
    
    /**
     * Set the map to search
     */
    void setMap(const Domain::ChunkedMap* map) { map_ = map; }
    
    /**
     * Set client data for item name lookup
     */
    void setClientData(const Services::ClientDataService* data) { client_data_ = data; }
    
    /**
     * Search the map for items/creatures matching query.
     * @param query Search string (name or ID)
     * @param mode Search mode (name, server ID, client ID)
     * @param search_items Include items in search
     * @param search_creatures Include creatures in search
     * @param limit Maximum results
     * @return Vector of map search results
     */
    std::vector<Domain::Search::MapSearchResult> search(
        const std::string& query,
        MapSearchMode mode = MapSearchMode::ByName,
        bool search_items = true,
        bool search_creatures = true,
        size_t limit = 1000
    ) const;
    
    /**
     * Search the ITEM DATABASE (not map) for items matching filters.
     * Used for Advanced Search preview - shows matching item types.
     * @param query Fuzzy text or numeric ID
     * @param types Type filter (OR logic)
     * @param properties Property filter (AND logic)
     * @param limit Maximum results
     * @return Vector of matching ItemType pointers
     */
    std::vector<const Domain::ItemType*> searchItemDatabase(
        const std::string& query,
        const Domain::Search::TypeFilter& types,
        const Domain::Search::PropertyFilter& properties,
        size_t limit = 100
    ) const;
    
private:
    bool matchesFuzzy(const std::string& text, const std::string& query) const;
    bool matchesItem(const Domain::Item* item, MapSearchMode mode,
                     const std::string& query_lower, uint16_t search_id) const;
    void searchContainerItems(
        const Domain::Item* container,
        const Domain::Tile* tile,
        MapSearchMode mode,
        const std::string& query_lower,
        uint16_t search_id,
        std::vector<Domain::Search::MapSearchResult>& results,
        size_t limit) const;
    Domain::Search::MapSearchResult createResult(
        const Domain::Tile* tile, const Domain::Item* item) const;
    std::string toLower(const std::string& str) const;
    
    const Domain::ChunkedMap* map_ = nullptr;
    const ClientDataService* client_data_ = nullptr;
};

} // namespace Services
} // namespace MapEditor
