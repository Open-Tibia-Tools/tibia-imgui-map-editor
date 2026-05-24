#include "MapSearchService.h"
#include "Domain/ChunkedMap.h"
#include "Domain/Tile.h"
#include "Domain/Item.h"
#include "Domain/ItemType.h"
#include "Domain/Creature.h"
#include "Services/ClientDataService.h"
#include <algorithm>
#include <cctype>
#include <format>

namespace MapEditor::Services {

std::vector<Domain::Search::MapSearchResult> MapSearchService::search(
    const std::string& query,
    MapSearchMode mode,
    bool search_items,
    bool search_creatures,
    size_t limit) const {
    
    std::vector<Domain::Search::MapSearchResult> results;
    
    if (!map_ || query.empty()) {
        return results;
    }
    
    std::string query_lower = toLower(query);
    uint16_t search_id = 0;
    
    // Parse ID for ID-based searches
    if (mode == MapSearchMode::ByServerId || mode == MapSearchMode::ByClientId) {
        try {
            search_id = static_cast<uint16_t>(std::stoi(query));
        } catch (...) {
            return results;  // Invalid ID
        }
    }
    
    // Iterate all tiles on the map
    map_->forEachTile([&](const Domain::Tile* tile) {
        if (!tile || results.size() >= limit) return;
        
        // Search items on tile
        if (search_items) {
            // Check ground
            if (auto* ground = tile->getGround()) {
                if (matchesItem(ground, mode, query_lower, search_id)) {
                    results.push_back(createResult(tile, ground));
                    if (results.size() >= limit) return;
                }
            }
            
            // Check stacked items
            for (const auto& item_ptr : tile->getItems()) {
                if (results.size() >= limit) return;
                auto* item = item_ptr.get();
                if (item && matchesItem(item, mode, query_lower, search_id)) {
                    results.push_back(createResult(tile, item));
                }
                
                // Search inside containers recursively
                if (item) {
                    searchContainerItems(item, tile, mode, query_lower, search_id, results, limit);
                }
            }
        }
        
        // Search creature on tile
        if (search_creatures && tile->hasCreature()) {
            auto* creature = tile->getCreature();
            if (creature && mode == MapSearchMode::ByName) {
                if (matchesFuzzy(creature->name, query_lower)) {
                    Domain::Search::MapSearchResult result;
                    result.position = tile->getPosition();
                    result.item_id = 0;
                    result.creature_name = creature->name;
                    result.display_name = creature->name;
                    result.info_line = "Creature";
                    results.push_back(result);
                }
            }
        }
    });
    
    return results;
}

bool MapSearchService::matchesItem(const Domain::Item* item, MapSearchMode mode,
                                    const std::string& query_lower, uint16_t search_id) const {
    if (!item) return false;

    const Domain::ItemType* type = item->getType();
    
    switch (mode) {
        case MapSearchMode::ByServerId:
            return item->getServerId() == search_id;
            
        case MapSearchMode::ByClientId:
            return type && type->client_id == search_id;
            
        case MapSearchMode::ByName:
        default:
            return type && !type->name.empty() && matchesFuzzy(type->name, query_lower);
    }
}

Domain::Search::MapSearchResult MapSearchService::createResult(
    const Domain::Tile* tile, const Domain::Item* item) const {
    
    Domain::Search::MapSearchResult result;
    result.position = tile->getPosition();
    result.item_id = item->getServerId();
    result.info_line = std::format("ID: {}", item->getServerId());

    const Domain::ItemType* type = item->getType();
    if (type && !type->name.empty()) {
        result.display_name = type->name;
    } else {
        result.display_name = "Item " + std::to_string(item->getServerId());
    }
    
    return result;
}

void MapSearchService::searchContainerItems(
    const Domain::Item* container,
    const Domain::Tile* tile,
    MapSearchMode mode,
    const std::string& query_lower,
    uint16_t search_id,
    std::vector<Domain::Search::MapSearchResult>& results,
    size_t limit) const {
    
    for (const auto& item_ptr : container->getContainerItems()) {
        if (results.size() >= limit) return;
        auto* item = item_ptr.get();
        if (item && matchesItem(item, mode, query_lower, search_id)) {
            auto result = createResult(tile, item);
            result.is_in_container = true;  // Mark as found inside container
            results.push_back(result);
        }
        // Recursive search for nested containers
        if (item) {
            searchContainerItems(item, tile, mode, query_lower, search_id, results, limit);
        }
    }
}

bool MapSearchService::matchesFuzzy(const std::string& text, 
                                     const std::string& query) const {
    std::string text_lower = toLower(text);
    return text_lower.find(query) != std::string::npos;
}

std::string MapSearchService::toLower(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::vector<const Domain::ItemType*> MapSearchService::searchItemDatabase(
    const std::string& query,
    const Domain::Search::TypeFilter& types,
    const Domain::Search::PropertyFilter& properties,
    size_t limit) const {
    
    std::vector<const Domain::ItemType*> results;
    
    if (!client_data_) {
        return results;
    }
    
    std::string query_lower = toLower(query);
    uint16_t search_id = 0;
    bool is_numeric = false;
    
    // Auto-detect if query is numeric (server/client ID)
    if (!query.empty()) {
        try {
            search_id = static_cast<uint16_t>(std::stoi(query));
            is_numeric = true;
        } catch (...) {
            is_numeric = false;
        }
    }
    
    // Get all item types from ClientDataService
    const auto& item_types = client_data_->getItemTypes();
    
    for (const auto& item_type : item_types) {
        if (results.size() >= limit) break;
        
        // Skip invalid items
        if (item_type.server_id == 0) continue;
        
        // === QUERY FILTER ===
        bool matches_query = query.empty();  // Empty query matches all
        
        if (!matches_query) {
            if (is_numeric) {
                // Match server ID or client ID
                matches_query = (item_type.server_id == search_id) ||
                               (item_type.client_id == search_id);
            } else {
                // Fuzzy name match
                matches_query = matchesFuzzy(item_type.name, query_lower);
            }
        }
        
        if (!matches_query) continue;
        
        // === TYPE FILTER (OR logic) ===
        if (types.hasAnySelected()) {
            bool matches_type = false;
            
            // Use helper methods that check both OTB group and JSON item_type
            if (types.depot && item_type.isDepot()) matches_type = true;
            if (types.mailbox && item_type.isMailbox()) matches_type = true;
            if (types.trash_holder && item_type.isTrashHolder()) matches_type = true;
            if (types.container && item_type.isContainer()) matches_type = true;
            if (types.door && item_type.isDoor()) matches_type = true;
            if (types.magic_field && item_type.isMagicField()) matches_type = true;
            if (types.teleport && item_type.isTeleport()) matches_type = true;
            if (types.bed && item_type.isBed()) matches_type = true;
            if (types.key && item_type.isKey()) matches_type = true;
            if (types.podium && item_type.isPodium()) matches_type = true;
            // Note: types.creature is handled separately (not an ItemType)
            
            if (!matches_type && !types.creature) continue;
        }
        
        // === PROPERTY FILTER (AND logic) ===
            if (properties.hasAnySelected()) {
                if (properties.unpassable && !item_type.hasFlag(Domain::ItemFlag::Unpassable)) continue;
                if (properties.unmovable && item_type.is_moveable) continue;  // Inverse logic
                if (properties.block_missiles && !item_type.hasFlag(Domain::ItemFlag::BlockMissiles)) continue;
                if (properties.block_pathfinder && !item_type.hasFlag(Domain::ItemFlag::BlockPathfinder)) continue;
                if (properties.readable && !item_type.isReadable()) continue;
                if (properties.writeable && !item_type.isWriteable()) continue;
                if (properties.pickupable && !item_type.is_pickupable) continue;
                if (properties.stackable && !item_type.is_stackable) continue;
                if (properties.rotatable && !item_type.isRotatable()) continue;
                if (properties.hangable && !item_type.is_hangable) continue;
                if (properties.hook_east && !item_type.hook_east) continue;
                if (properties.hook_south && !item_type.hook_south) continue;
                if (properties.has_elevation && !item_type.hasElevation()) continue;
                if (properties.ignore_look && !item_type.hasFlag(Domain::ItemFlag::IgnoreLook)) continue;

                // New Flags
                if (properties.force_use && !item_type.hasFlag(Domain::ItemFlag::ForceUse)) continue;
                if (properties.allow_dist_read && !item_type.hasFlag(Domain::ItemFlag::AllowDistRead)) continue;
                if (properties.full_tile && !item_type.hasFlag(Domain::ItemFlag::FullTile)) continue;
                if (properties.client_charges && !item_type.hasFlag(Domain::ItemFlag::ClientCharges)) continue;
                if (properties.animation && !item_type.hasFlag(Domain::ItemFlag::Animation)) continue;
                if (properties.always_on_top && !item_type.hasFlag(Domain::ItemFlag::AlwaysOnTop)) continue;
                
                if (properties.has_light && item_type.light_level == 0) continue;
                if (properties.has_speed && item_type.speed == 0) continue;
                if (properties.decays && item_type.decayTo == 0 && !item_type.decays) continue;
                
                if (properties.has_charges && 
                    item_type.charges == 0 && 
                    !item_type.extra_chargeable && 
                    !item_type.hasFlag(Domain::ItemFlag::ClientCharges)) continue;

                // Floor change check: check specific boolean flags instead of generic OTB flag
                if (properties.floor_change) {
                    bool is_floor_change = item_type.floor_change || 
                                          item_type.floor_change_down ||
                                          item_type.floor_change_north ||
                                          item_type.floor_change_east ||
                                          item_type.floor_change_south ||
                                          item_type.floor_change_west;
                    if (!is_floor_change) continue;
                }
            }
            
            // Check Combat/Extra Types
            if (types.hasAnySelected()) {
                // ... existing checks for depot, container, etc ...
                // Note: The logic below assumes "OR" behavior for type matches
                // If existing checks return match, good. If not, we check new types.
                // The current structure iterates property filters AND then type filters check?
                // Actually existing code logic for types is implicit in the "hasAnySelected()" 
                // typically implemented as: "if type filter active, item must match AT LEAST ONE selected type"
                // Let's verify how `types` are handled. 
                // Ah, looking at `AdvancedSearchDialog::updatePreviewResults`, it calls searchItemDatabase.
                // But inside `searchItemDatabase`, type checks might be done BEFORE this property block or implicit.
                // Let's look at the surrounding loop context from `view_file` previously.
                // It seems iterate all items -> check name -> check properties -> add to result.
                // Wait, where is the TYPE filter applied? 
                
                // Inspecting previous `view_code_item` of `MapSearchService::searchItemDatabase`:
                // It iterates. 
                // checks name.
                // checks properties (AND logic).
                // checks types (OR logic).
                
                bool type_match = false;
                if (!types.hasAnySelected()) {
                    type_match = true;
                } else {
                     if (types.depot && item_type.isDepot()) type_match = true;
                     else if (types.mailbox && item_type.isMailbox()) type_match = true;
                     else if (types.trash_holder && item_type.isTrashHolder()) type_match = true;
                     else if (types.container && item_type.isContainer()) type_match = true;
                     else if (types.door && item_type.isDoor()) type_match = true;
                     else if (types.magic_field && item_type.isMagicField()) type_match = true;
                     else if (types.teleport && item_type.isTeleport()) type_match = true;
                     else if (types.bed && item_type.isBed()) type_match = true;
                     else if (types.key && item_type.isKey()) type_match = true;
                     else if (types.podium && item_type.isPodium()) type_match = true;
                     
                     // New Types
                     else if (types.weapon && (item_type.group == Domain::ItemGroup::Weapon || item_type.weapon_type != Domain::WeaponType::None)) type_match = true;
                     else if (types.ammo && (item_type.group == Domain::ItemGroup::Ammunition || item_type.weapon_type == Domain::WeaponType::Ammo)) type_match = true;
                     else if (types.armor && (item_type.group == Domain::ItemGroup::Armor || (item_type.slot_position & Domain::SlotPosition::Armor) != Domain::SlotPosition::None)) type_match = true;
                     // Rune logic if needed, usually determined by OTB group or attribute
                }
                
                if (!type_match) continue;
            }
        
        results.push_back(&item_type);
    }
    
    return results;
}

std::vector<Domain::Search::MapSearchResult> MapSearchService::searchByUnique(size_t limit) const {
    return searchByCondition(SearchCondition::Unique, limit);
}

std::vector<Domain::Search::MapSearchResult> MapSearchService::searchByAction(size_t limit) const {
    return searchByCondition(SearchCondition::Action, limit);
}

std::vector<Domain::Search::MapSearchResult> MapSearchService::searchByContainer(size_t limit) const {
    return searchByCondition(SearchCondition::Container, limit);
}

std::vector<Domain::Search::MapSearchResult> MapSearchService::searchByWriteable(size_t limit) const {
    return searchByCondition(SearchCondition::Writeable, limit);
}

std::vector<Domain::Search::MapSearchResult> MapSearchService::searchByCondition(SearchCondition cond, size_t limit) const {
    std::vector<Domain::Search::MapSearchResult> results;
    if (!map_) return results;

    map_->forEachTile([&](const Domain::Tile* tile) {
        if (!tile || results.size() >= limit) return;

        auto processItem = [&](const Domain::Item* item, bool in_container) {
            if (!item || results.size() >= limit) return;

            const Domain::ItemType* type = item->getType();
            std::string type_name = type ? type->name : "Unknown";
            bool matches = false;
            std::string display_name;

            std::string info;
            switch (cond) {
                case SearchCondition::Unique:
                    if (item->getUniqueId() > 0) {
                        matches = true;
                        display_name = type_name;
                        info = std::format("UID: {}", item->getUniqueId());
                    }
                    break;
                case SearchCondition::Action:
                    if (item->getActionId() > 0) {
                        matches = true;
                        display_name = type_name;
                        info = std::format("AID: {}", item->getActionId());
                    }
                    break;
                case SearchCondition::Container:
                    if (item->isContainer()) {
                        matches = true;
                        display_name = type_name;
                        size_t count = item->getContainerItems().size();
                        info = std::format("Container ({} item{})", count, count == 1 ? "" : "s");
                    }
                    break;
                case SearchCondition::Writeable:
                    if (!item->getText().empty()) {
                        matches = true;
                        display_name = type_name;
                        const auto& text = item->getText();
                        std::string truncated = text.length() > 60 ? text.substr(0, 60) + "..." : text;
                        info = std::format("Text: {}", truncated);
                    }
                    break;
            }

            if (matches) {
                Domain::Search::MapSearchResult result;
                result.position = tile->getPosition();
                result.item_id = item->getServerId();
                result.display_name = std::move(display_name);
                result.info_line = std::move(info);
                result.is_in_container = in_container;
                results.push_back(std::move(result));
            }

            if (!item->getContainerItems().empty() && results.size() < limit) {
                processContainerItems(item, tile, cond, results, limit);
            }
        };

        if (auto* ground = tile->getGround()) {
            processItem(ground, false);
        }
        for (const auto& item_ptr : tile->getItems()) {
            if (results.size() >= limit) return;
            processItem(item_ptr.get(), false);
        }
    });

    return results;
}

void MapSearchService::processContainerItems(
    const Domain::Item* container, const Domain::Tile* tile,
    SearchCondition cond, std::vector<Domain::Search::MapSearchResult>& results,
    size_t limit) const {

    for (const auto& child : container->getContainerItems()) {
        if (results.size() >= limit) return;
        const Domain::Item* item = child.get();
        if (!item) continue;

        const Domain::ItemType* type = item->getType();
        std::string type_name = type ? type->name : "Unknown";
        bool matches = false;
        std::string display_name;

        std::string info;
        switch (cond) {
            case SearchCondition::Unique:
                if (item->getUniqueId() > 0) {
                    matches = true;
                    display_name = type_name;
                    info = std::format("UID: {}", item->getUniqueId());
                }
                break;
            case SearchCondition::Action:
                if (item->getActionId() > 0) {
                    matches = true;
                    display_name = type_name;
                    info = std::format("AID: {}", item->getActionId());
                }
                break;
            case SearchCondition::Container:
                if (item->isContainer()) {
                    matches = true;
                    display_name = type_name;
                    size_t count = item->getContainerItems().size();
                    info = std::format("Container ({} item{})", count, count == 1 ? "" : "s");
                }
                break;
            case SearchCondition::Writeable:
                if (!item->getText().empty()) {
                    matches = true;
                    display_name = type_name;
                    const auto& text = item->getText();
                    std::string truncated = text.length() > 60 ? text.substr(0, 60) + "..." : text;
                    info = std::format("Text: {}", truncated);
                }
                break;
        }

        if (matches) {
            Domain::Search::MapSearchResult result;
            result.position = tile->getPosition();
            result.item_id = item->getServerId();
            result.display_name = std::move(display_name);
            result.info_line = std::move(info);
            result.is_in_container = true;
            results.push_back(std::move(result));
        }

        if (item->isContainer() && results.size() < limit) {
            processContainerItems(item, tile, cond, results, limit);
        }
    }
}

} // namespace MapEditor::Services
